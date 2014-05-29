// HttpClient:
// Marmalade-specific wrapper around libcurl
//
// Created by the Get to Know Society
// Public domain

#include "HttpClient.h"

#include <errno.h>
#include <stdio.h>
#include <pthread.h>
#include <curl/curl.h>
#include <openssl/ssl.h>
#include <stdexcept>

using std::string;

// Data for the Worker threads - shared by the worker thread and the HttpClient master thread:
//
// Note: it's really important to understand that the worker threads and the app threads are
// in totally different memory environments. They can each read each other's memory no problem
// but they must never modify any non-POD data belonging to the other thread, or bad things happen.
// This is because the app thread uses the s3e malloc/realloc/free pool, while the worker threads
// use the system's default memory methods.
// BWM: I tried changing the threads to use the same s3e memory methods, but it caused issues
// on debug builds due to the high number of memory operations the worker threads make.
//
// All communication between worker threads and the app thread happens using the following struct:
struct HttpClient_Worker {
	const char* userAgent;
	
	CURL *pCurl; // Re-usable worker, created in app thread, modified by worker thread.
	pthread_t thread_id;
	Ptr<HttpRequest> pRequest; // Set by app thread; must never by modified when status is ACTIVE
	pthread_cond_t wakeCond; // Condition used to wake the worker thread so that it will start the next job or quit.
	pthread_mutex_t wakeMutex; // Mutex that gets locked when we access or modify wakeCond
	volatile bool cancelAndQuit; // If set true by app thread, cancel and quit ASAP, interrupting downloads if needed.
	volatile enum StatusCode {// Worker Status:
		UNUSED, // initialized to UNUSED in app thread. This means the worker thread has not been created yet.
		ACTIVE, // The worker thread is processing a request
		DONE,   // The worker thread has finished processing a request. It has gone to sleep and is waiting for the app thread to finish processing the result.
		CLEANUP,// The app thread has finished processing the result, and is now waking the worker thread up so it can cleanup and get ready for a new request.
		READY   // The worker thread has completed and cleaned up its first request and is ready to process a new request.
	} status;
	// Response Headers: Managed by the worker thread as a super simple one-way linked list of key-value pairs:
	volatile bool responseHeadersDone; // Set true by the worker once we've received the response headers
	typedef HttpRequest::RH RH;
	RH* pResponseHeaders; // The headers get stored in this linked list by the worker
	CURLcode result; // CURL's result code. Should be CURLE_OK
	long responseStatusCode; // Http response status code, e.g. 200
	// The worker thread needs to reset this worker data instance before starting each request:
	void Reset() { responseHeadersDone = false; RH* p_rh = pResponseHeaders; while(p_rh) { RH* del = p_rh; p_rh = p_rh->next; delete del; } pResponseHeaders = nullptr; responseStatusCode = 0; }
	// Constructor and methods for use by the app thread:
	HttpClient_Worker() : status(UNUSED), pRequest(nullptr), pCurl(nullptr), cancelAndQuit(false), pResponseHeaders(nullptr), responseHeadersDone(false), responseStatusCode(0), result(CURLE_OK) {}
	void CancelAndQuit() { pthread_mutex_lock(&wakeMutex); cancelAndQuit = true; pthread_cond_signal(&wakeCond); pthread_mutex_unlock(&wakeMutex); /* Wake just in case we are currently sleeping */ }
	void WakeToStatus(StatusCode sc) { pthread_mutex_lock(&wakeMutex); status = sc; pthread_cond_signal(&wakeCond); pthread_mutex_unlock(&wakeMutex); }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Our libcurl callbacks. We need to specify extern C since libcurl and pthreads expect C calling convention to be used.
extern "C"  {

static size_t HttpClient_WorkerThread_WriteCallback(void *contents, size_t size, size_t nmemb, void *_pWorker) {
	HttpClient_Worker* pWorker = reinterpret_cast<HttpClient_Worker*>(_pWorker);
	if (pWorker->cancelAndQuit)
		return 0;
	size_t realsize = size * nmemb;
	return pWorker->pRequest->Worker_HandleData((const unsigned char*)contents, realsize);
}

static size_t HttpClient_WorkerThread_ReadCallback(void *data, size_t size, size_t nmemb, void *_pWorker) {
	HttpClient_Worker* pWorker = reinterpret_cast<HttpClient_Worker*>(_pWorker);
	if (pWorker->cancelAndQuit)
		return CURL_READFUNC_ABORT;
	size_t realsize = size * nmemb;
	return pWorker->pRequest->Worker_HandleUpload((const unsigned char*)data, realsize);
}

static size_t HttpClient_WorkerThread_HeaderCallback(void *pHeader, size_t size, size_t nmemb, void *_pWorker) {
	HttpClient_Worker* pWorker = reinterpret_cast<HttpClient_Worker*>(_pWorker);
	if (pWorker->cancelAndQuit)
		return 0;
	size_t realsize = size * nmemb;
	std::string header((const char*)pHeader, realsize);
	// We want to store the response headers in the HttpRequest
	// Tricky since we're in a different memory environment (on the worker thread).
	// So for now we store the headers in pWorker, and let the HttpClient
	// later copy them into the request, while on the app thread.
	//s3eDebugTracePrintf("Header: %s", header.c_str());
	if (header.size() >= 2 && header.substr(header.size()-2, 2) == "\r\n")
		header.resize(header.size() - 2); // Drop the trailing \r\n
	auto colon_pos = header.find(':');
	if (header.empty()) {
		// This indicates the end of the headers.
		//s3eDebugTracePrintf("ALL HEADERS RECEIVED");
		pWorker->responseHeadersDone = true;
	} else if (colon_pos != string::npos) {
		string key = header.substr(0, colon_pos++);
		while (header[colon_pos] == ' ')
			colon_pos++;
		string val = header.substr(colon_pos, header.size() - colon_pos);
		pWorker->pResponseHeaders = new HttpClient_Worker::RH(key, val, pWorker->pResponseHeaders);
	} else {
		// This is probably the first line of the headers - e.g. 'HTTP/1.1 200 OK'
		if (header.substr(0,4) == "HTTP") {
			pWorker->pResponseHeaders = new HttpClient_Worker::RH("HTTP", header, pWorker->pResponseHeaders);
		} else {
			s3eDebugTracePrintf("HttpClient: Unexpected/invalid header: %s", header.c_str());
		}
	}
	return realsize;
}

static int HttpClient_WorkerThread_ProgressCallback(void *_pWorker, double dltotal, double dlnow, double ultotal, double ulnow) {
	HttpClient_Worker* pWorker = reinterpret_cast<HttpClient_Worker*>(_pWorker);
	if (pWorker->cancelAndQuit)
		return 1; // Return non-zero to indicate that we want to abort the transfer
	//s3eDebugTracePrintf("Progress: %f/%f, %f/%f", ulnow, ultotal, dlnow, dltotal);
	pWorker->pRequest->Worker_UpdateProgress(dltotal, dlnow, ultotal, ulnow);
	// During the request, we should yield this thread from time to time, so the progress callback seems like a good chance:
	s3eDeviceYield();
	pthread_yield();
	return 0;
}

static void* HttpClient_WorkerThread(void *_pWorker) {
	HttpClient_Worker* pWorker = reinterpret_cast<HttpClient_Worker*>(_pWorker);
	
	pWorker->pCurl = curl_easy_init();
	curl_easy_setopt(pWorker->pCurl, CURLOPT_USERAGENT, pWorker->userAgent);
	
	// SSL: certificates must be configured correctly (not done out of the box by us),
	// OR for testing purposes, you can disable peer certificate verification with this 
	// line (obviously, this is insecure and should not be used in production apps):
	//curl_easy_setopt(pWorker->pCurl, CURLOPT_SSL_VERIFYPEER, 0L);
	
	while (!pWorker->cancelAndQuit) {
		const Ptr<HttpRequest>& pRequest = pWorker->pRequest; // Note, it's very important that we don't change the HttpRequest object's reference count from this thread

		// Set request type:
		curl_easy_setopt(pWorker->pCurl, CURLOPT_HTTPGET, pRequest->GetMethod() == HttpRequest::GET ? 1L : 0L);
		curl_easy_setopt(pWorker->pCurl, CURLOPT_NOBODY, pRequest->GetMethod() == HttpRequest::HEAD ? 1L : 0L);
		curl_easy_setopt(pWorker->pCurl, CURLOPT_UPLOAD, pRequest->GetMethod() == HttpRequest::PUT ? 1L : 0L);
		curl_easy_setopt(pWorker->pCurl, CURLOPT_POST, pRequest->GetMethod() == HttpRequest::POST ? 1L : 0L);
		// Set the URL:
		curl_easy_setopt(pWorker->pCurl, CURLOPT_URL, pRequest->GetURL().c_str());
		
		// Set the request headers
		curl_slist *p_hdr_list = nullptr;
		for (auto it = pWorker->pRequest->GetRequestHeaders().begin(); it != pWorker->pRequest->GetRequestHeaders().end(); it++) {
			p_hdr_list = curl_slist_append(p_hdr_list, string(it->first).append(": ").append(it->second).c_str());
		}
		curl_easy_setopt(pWorker->pCurl, CURLOPT_HTTPHEADER, p_hdr_list);

		// Set callbacks, link them to the HttpRequest virtuals
		curl_easy_setopt(pWorker->pCurl, CURLOPT_WRITEFUNCTION, HttpClient_WorkerThread_WriteCallback);
		curl_easy_setopt(pWorker->pCurl, CURLOPT_WRITEDATA, _pWorker);
		curl_easy_setopt(pWorker->pCurl, CURLOPT_HEADERFUNCTION, HttpClient_WorkerThread_HeaderCallback);
		curl_easy_setopt(pWorker->pCurl, CURLOPT_HEADERDATA, _pWorker);
		
		curl_easy_setopt(pWorker->pCurl, CURLOPT_PROGRESSFUNCTION, HttpClient_WorkerThread_ProgressCallback);
		curl_easy_setopt(pWorker->pCurl, CURLOPT_PROGRESSDATA, _pWorker);
		curl_easy_setopt(pWorker->pCurl, CURLOPT_NOPROGRESS, 0L);
		
		if (pRequest->GetMethod() == HttpRequest::POST) {
			curl_easy_setopt(pWorker->pCurl, CURLOPT_READFUNCTION, HttpClient_WorkerThread_ReadCallback);
			curl_easy_setopt(pWorker->pCurl, CURLOPT_READDATA, _pWorker);
			curl_easy_setopt(pWorker->pCurl, CURLOPT_POSTFIELDSIZE, pWorker->pRequest->Worker_GetUploadSize());
		}
		
		//s3eDebugTracePrintf("Performing request from thread %ld", pWorker->thread_id);
		
		pWorker->result = curl_easy_perform(pWorker->pCurl);
		curl_easy_getinfo(pWorker->pCurl, CURLINFO_RESPONSE_CODE, &pWorker->responseStatusCode);
		if (pWorker->result != CURLE_OK) {
			s3eDebugTracePrintf("HttpClient: Error occurred: %s", curl_easy_strerror(pWorker->result));
		}
		pRequest->Worker_HandleDone(pWorker->result == CURLE_OK, (int)pWorker->responseStatusCode);
	
		curl_slist_free_all(p_hdr_list);
		
		// Now, we need go to sleep and wait for the app thread to process any response data
		// it needs before we finish cleaning up the request response data
		pthread_mutex_lock(&pWorker->wakeMutex);
		pWorker->status = HttpClient_Worker::DONE;
		pthread_cond_wait(&pWorker->wakeCond, &pWorker->wakeMutex); // unlocks the mutex and waits for the condition, then locks the mutex again
		pthread_mutex_unlock(&pWorker->wakeMutex);
		IwAssert(HTTP_CLIENT, pWorker->status == HttpClient_Worker::CLEANUP); // Status should be set back to this by the app
		
		// Do any cleanup that must be done on the worker thread, after the app thread has processed the response:
		pRequest->Worker_HandleCleanup();
		// Reset our cached response headers, etc.:
		pWorker->Reset();
		if (pWorker->cancelAndQuit)
			break;
		
		// Sleep until we have something else to do:
		pthread_mutex_lock(&pWorker->wakeMutex);
		pWorker->status = HttpClient_Worker::READY;
		while (pWorker->status == HttpClient_Worker::READY && !pWorker->cancelAndQuit) {
			timespec ts;
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_nsec += 100 * 1E6; // sleep 100 ms
			int rc = pthread_cond_timedwait(&pWorker->wakeCond, &pWorker->wakeMutex, &ts); // unlocks the mutex and waits for the condition
			//if (rc == ETIMEDOUT)
			//	s3eDebugTracePrintf("(Worker thread %ld is sleeping)", pWorker->thread_id);
			s3eDeviceYield(); // We must call s3eDeviceYield from time to time, or other threads might be blocked waiting for this thread to yield.
		}
		// Now, the condition is set and the mutex has automatically been locked again
		IwAssert(HTTP_CLIENT, (pWorker->status == HttpClient_Worker::ACTIVE) || (pWorker->cancelAndQuit));
		pthread_mutex_unlock(&pWorker->wakeMutex);
	}
	
	curl_easy_cleanup(pWorker->pCurl);
	pWorker->pCurl = nullptr;
	
	return 0;
}
	
} // End of extern "C"
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HttpClient::GlobalInit() {
	curl_global_init(CURL_GLOBAL_SSL);
}

void HttpClient::GlobalCleanup() {
	curl_global_cleanup();
	// Unfortunately there is a memory leak in CURL+OpenSSL
	// so we have to specifically free the OpenSSL compression methods stack:
	CRYPTO_w_lock(CRYPTO_LOCK_SSL);
	if (STACK_OF(SSL_COMP) *ssl_comp_methods = SSL_COMP_get_compression_methods())
		sk_SSL_COMP_free(ssl_comp_methods);
	CRYPTO_w_unlock(CRYPTO_LOCK_SSL);
}

HttpClient::HttpClient(uint numWorkers, const char* userAgentStr)
	: NUM_WORKERS(numWorkers), m_userAgent(userAgentStr)
{
	m_workers = new Worker[NUM_WORKERS];
}

HttpClient::~HttpClient() {
	for (uint i = 0; i < NUM_WORKERS; i++) {
		if (m_workers[i].status != Worker::UNUSED) {
			// Signal to the thread to cancel any pending requests:
			m_workers[i].CancelAndQuit();
			pthread_join(m_workers[i].thread_id, nullptr); // Wait for the thread to finish and then free its resources
			pthread_mutex_destroy(&m_workers[i].wakeMutex);
			pthread_cond_destroy(&m_workers[i].wakeCond);
			m_workers[i].pCurl = nullptr;
		}
	}
	delete[] m_workers;
	if (!m_pendingRequests.empty()) {
		s3eDebugTraceLine("HttpClient: WARNING: Terminating HttpClient instance before all requests were processed.");
		while (!m_pendingRequests.empty())
			m_pendingRequests.pop();
	}
	m_pendingCallbacks.clear();
}

void HttpClient::Update() {
	Worker* p_free_worker = nullptr;
	// First, check if any of our worker threads are free (not currently processing a request)
	// or have recently finished:
	for (uint i=0; i < NUM_WORKERS; i++) {
		Worker& worker = m_workers[i];
		if (worker.status == Worker::ACTIVE) {
			if (worker.pRequest->GetStatus() == HttpRequest::SENDING && worker.responseHeadersDone) {
				// We have now received all the response headers.
				// Since we are on the app thread, we can now update the reqest "responseHeaders" map:
				worker.pRequest->HandleResponseHeaders(m_workers[i].pResponseHeaders);
				// The above method should also mark the request's status as HttpRequest::HEADERS
			}
		} else if (worker.status == Worker::DONE) {
			// This request has *just* finished.
			// If the response was returned all at once, we may not yet have called HandleResponseHeaders()
			if (worker.pRequest->GetStatus() == HttpRequest::SENDING)
				worker.pRequest->HandleResponseHeaders(m_workers[i].pResponseHeaders);
			// Now call the request's response handling code:
			bool success = (worker.result == CURLE_OK) && (worker.responseStatusCode < 400);
			worker.pRequest->HandleResponse(success, (int)worker.responseStatusCode);
			// Now call any registered callbacks:
			for (auto it = m_pendingCallbacks.begin(); it != m_pendingCallbacks.end();) {
				if (it->first == worker.pRequest) {
					it->second->Call(worker.pRequest);
					it = m_pendingCallbacks.erase(it);
				} else {
					it++;
				}
			}
			// Now, wake the worker up and tell it to cleanup:
			worker.WakeToStatus(Worker::CLEANUP);
		} else if (worker.status == Worker::CLEANUP) {
			// We are waiting for the worker to finish cleaning up the request it just finished.
		} else {
			// Worker status is either UNUSED or READY. This worker can receive a new request:
			if (!p_free_worker)
				p_free_worker = &worker; // This worker is free
			if (worker.pRequest)
				worker.pRequest = nullptr; // Free the HttpRequest object, which we no longer need.
		}
	}
	
	if (p_free_worker && !m_pendingRequests.empty()) {
		auto p_request = m_pendingRequests.front();
		m_pendingRequests.pop();
		if (p_request->GetStatus() == HttpRequest::PENDING) {
			p_free_worker->pRequest = p_request;
			p_free_worker->pRequest->HandleRequestStart();

			if (p_free_worker->status == Worker::UNUSED) {
				// This worker has not been initialized
				// We're now going to create a new worker thread
				IwAssert(HTTP_CLIENT, p_free_worker->pCurl == nullptr);
				p_free_worker->userAgent = m_userAgent.c_str(); // We want the thread to be able to read this userAgent string, so we pass it via the Worker struct
				
				// We'll need a mutex and a condition that we can use to wake up the
				// worker thread when/if it's sleeping:
				pthread_mutex_init(&p_free_worker->wakeMutex, nullptr);
				pthread_cond_init(&p_free_worker->wakeCond, nullptr);
				
				p_free_worker->status = Worker::ACTIVE;
				int result = pthread_create(&p_free_worker->thread_id, nullptr, HttpClient_WorkerThread, (void *)p_free_worker);
				if (result != 0) {
					p_free_worker->status = Worker::DONE;
					throw std::runtime_error("Unable to spawn a new HttpClient worker thread.");
				} else {
					s3eDebugTracePrintf("HttpClient: Spawned worker thread (TID %ld)", p_free_worker->thread_id);
				}
			} else {
				IwAssert(HTTP_CLIENT, p_free_worker->status == Worker::READY);
				// The worker was sleeping. Wake it up and put it to work:
				p_free_worker->WakeToStatus(Worker::ACTIVE);
			}
		} else {
			// This request was cancelled. Remove it from the queue but don't do anything with it.
			IwAssert(HTTP_CLIENT, p_request->GetStatus() == HttpRequest::CANCELLED);
		}
	}
}
