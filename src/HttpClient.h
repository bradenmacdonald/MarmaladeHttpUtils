// HttpClient:
// Marmalade-specific wrapper around libcurl
//
// Created by the Get to Know Society
// Public domain

#pragma once

#include <list>
#include <queue>

#include "util/fastdelegate.h"
#include "HttpRequest.h"

struct HttpClient_Worker;

///////////////////////////////////////////////////////////////////////////////
// Callback types, used to notify the requestee when an HTTP request has
// either finished or failed.

class HttpCallbackBase : public IRefCounted {
public:
	HttpCallbackBase() {}
	virtual void Call(Ptr<HttpRequest> pRequest)=0;
};

template<typename WatcherType>
class HttpCallback : public HttpCallbackBase {
public:
	HttpCallback(ObservingPtr<WatcherType> pWatcher, void (WatcherType::*pMethod)(Ptr<HttpRequest>))
	: m_pWatcher(pWatcher), m_pDelegate(fastdelegate::MakeDelegate(pWatcher.ptr(), pMethod)) {}
	virtual void Call(Ptr<HttpRequest> pRequest) { if (m_pWatcher) m_pDelegate(pRequest); }
private:
	ObservingPtr<WatcherType> m_pWatcher;
	fastdelegate::FastDelegate1< Ptr<HttpRequest> > m_pDelegate;
};

class HttpStaticCallback : public HttpCallbackBase {
public:
	HttpStaticCallback(void (*pMethod)(Ptr<HttpRequest>)) : m_pDelegate(pMethod) {}
	virtual void Call(Ptr<HttpRequest> pRequest) { m_pDelegate(pRequest); }
private:
	fastdelegate::FastDelegate1< Ptr<HttpRequest> > m_pDelegate;
};

///////////////////////////////////////////////////////////////////////////////
// HttpClient:

class HttpClient {
public:
	static void GlobalInit(); // This must be called as early as possible in program execution
	static void GlobalCleanup(); // Call as late as possible following program termination and after all instances of HttpClient are freed.
	
	// Constructor:
	// Call this to create a new HttpClient.
	// numWorkers specifies the maximum number of worker threads that this
	//            instance will use (Set >1 to allow concurrent requests)
	//            Don't set higher than 3 if you're mostly using one server.
	// userAgent  specifies the HTTP User Agent header. (e.g. "MyApp API Client")
	HttpClient(uint numWorkers, const char* userAgentStr); // Initialize
	virtual ~HttpClient(); // Terminate
	
	// Update:
	// Updates the request queue, calls callbacks, and manages worker threads.
	// This must be called fairly regularly, although HTTP requests will
	// still continue on worker threads even if you aren't calling this.
	void Update();

	// QueueRequest:
	// Send the request pRequest as soon as a worker thread is available.
	// Will optionally call pCallback once a response has been received.
	//
	// Note: pCallback holds an ObservingPtr so if the instance of some
	// class that is expecting a callback gets freed in the meantime,
	// pCallback will never get called, even if the request completes.
	// To avoid that, one option is to use a HttpStaticCallback
	// which does not contain an observing pointer.
	void QueueRequest(Ptr<HttpRequest> pRequest, Ptr<HttpCallbackBase> pCallback = nullptr) {
		if (pRequest->GetStatus() == HttpRequest::BUILDING)
			pRequest->CompileRequest();
		IwAssert(HTTP_CLIENT, pRequest->GetStatus() == HttpRequest::PENDING);
		m_pendingRequests.push(pRequest);
		if (pCallback)
			m_pendingCallbacks.push_back(std::pair< Ptr<HttpRequest>, Ptr<HttpCallbackBase> >(pRequest, pCallback));
	}

private:
	const std::string m_userAgent;
	typedef HttpClient_Worker Worker;
	Worker* m_workers; // Array of Worker threads
	const uint NUM_WORKERS;
	std::queue< Ptr<HttpRequest> > m_pendingRequests;
	std::list< std::pair< Ptr<HttpRequest>, Ptr<HttpCallbackBase> > > m_pendingCallbacks; // Callbacks to call once a request has finished.
};
