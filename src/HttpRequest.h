// HttpRequest:
// Encapsulates/represents a single HTTP request.
// Used with HttpClient
//
// Created by the Get to Know Society
// Public domain

#pragma once

#include <map>
#include <string>

#include <IwDebug.h>

#include "util/Ptr.h"
#include "util/json.h"

struct s3eFile;

class HttpRequest : public IRefCounted {
public:
	enum Method {
		GET,
		POST,
		HEAD,
		PUT
	};
	
	enum Status {
		BUILDING, // This request is not ready to be sent - it is being constructed.
		PENDING,
		SENDING, // We are sending the request to the server / uploading files
		HEADERS, // Respopnse headers have been received but not the response data yet.
		DONE,
		ERROR,
		CANCELLED // Request was cancelled before receiving any response
	};
	
	HttpRequest(Method method, const char* url) : m_status(BUILDING), m_method(method), m_url(url) {}
	virtual ~HttpRequest() {}
	
	Status GetStatus() const { return m_status; }
	
	const std::map<std::string, std::string>& GetResponseHeaders() const { return m_responseHeaders; }
	
	// If actively uploading or downloading data, you can use this to get the progress, if known:
	double GetUploadFraction() const { return m_uploadBytesTotal ? m_uploadBytesNow / m_uploadBytesTotal : 0; }
	double GetDownloadFraction() const { return m_downloadBytesTotal ? m_downloadBytesNow / m_downloadBytesTotal : 0; }
	
	const std::string& GetURL() const { return m_url; }
	Method GetMethod() const { return m_method; };
	const char* GetMethodStr() const { return m_method == GET ? "GET" : m_method == POST ? "POST" : m_method == HEAD ? "HEAD" : m_method == PUT ? "PUT" : "???"; }
	const std::map<std::string, std::string>& GetRequestHeaders() { return m_requestHeaders; }
	// Get the response headers, if available:
	const std::map<std::string, std::string>& GetResponseHeaders() { IwAssert(HTTP_CLIENT, m_status == HEADERS || m_status == DONE || m_status == ERROR); return m_responseHeaders; }
	
	// You can attempt to cancel an API call if it hasn't started yet:
	virtual void Cancel() { if (m_status == PENDING) m_status = CANCELLED; }

	// Set a header for this request:
	void SetHeader(const std::string& header, const std::string& value) { IwAssert(HTTP_CLIENT, m_status == BUILDING); m_requestHeaders[header] = value; }
	
	
	
	
	/////// Internal methods used by HttpClient and friends ///////
	// The HttpClient calls this method before adding this request to the
	// requests queue.
	virtual void CompileRequest() { IwAssert(HTTP_CLIENT, m_status == BUILDING); m_status = PENDING; }
	// Called immediately as the request begins to be transmitted:
	virtual void HandleRequestStart() { IwAssert(HTTP_CLIENT, m_status == PENDING); m_status = SENDING; }
	// Called to handle response headers once they are all received:
	struct RH { const std::string header; const std::string value; RH* next; RH(std::string h, std::string v, RH* next) : header(h), value(v), next(next) {} };
	virtual void HandleResponseHeaders(const RH* pFirstHeader) {
		IwAssert(HTTP_CLIENT, m_status == SENDING);
		m_status = HEADERS;
		// We must copy the headers from the RH* struct in the worker thread's memory environment to m_responseHeaders in the app's memory environment:
		for (const RH* p_header = pFirstHeader; p_header != nullptr; p_header = p_header->next) { m_responseHeaders[p_header->header] = p_header->value; }
	}
	// Called after the request has finished. Process the data that Worker_HandleData() has been receiving.
	// Success will be true unless the HTTP response code was >400 or an error occurred. If a network/curl/ApiClient error occured, httpStatusCode will be zero.
	virtual void HandleResponse(bool success, int httpStatusCode) { IwAssert(HTTP_CLIENT, m_status == HEADERS); m_status = success ? DONE : ERROR; }
	///////////////////////////////////////////////////////
	// Note - these are called from a worker thread!
	// ** The Worker_ methods must NOT modify any of the members not marked as "volatile". **
	virtual void Worker_UpdateProgress(double dltotal, double dlnow, double ultotal, double ulnow) { m_downloadBytesNow = dlnow; m_downloadBytesTotal = dltotal; m_uploadBytesNow = ulnow; m_uploadBytesTotal = ultotal;
		//Trace("Worker_UpdateProgress m_downloadBytesNow %d, m_downloadBytesTotal %d, m_uploadBytesNow %d m_pUploadBytesTotal %d", m_downloadBytesNow, m_downloadBytesTotal, m_uploadBytesNow, m_uploadBytesTotal);
	}
	// For receiving data:
	virtual size_t Worker_HandleData(const unsigned char* contents, size_t size) = 0; // Process response data from the server. Should return value of "size" if successful.
	// For sending data:
	virtual long Worker_GetUploadSize() const { return 0L; } /* For POST/PUT requests, return the length of the body data that we are planning to upload */
	virtual size_t Worker_HandleUpload(const unsigned char* pData, size_t fillSize) { return 0; } // Fill memory area pData with next "fillSize" bytes of data to upload. Return a non-zero # of bytes actually filled.
	// If any cleanup needs to be done by the worker thread:
	virtual void Worker_HandleDone(bool success, int httpStatusCode) {} // Note: this gets called before the app thread calls HandleResponse()
	virtual void Worker_HandleCleanup() {} // This gets called after the app thread has done HandleResponse()
	
	// Helper methods:
	static std::string UrlEncode(const std::string &value, bool strict = true); // URL-Encode a string (e.g. "test test&t" becomes "test+test%26t" or "test%20test%26t" (strict mode)
	///////////////////////////////////////////////////////
	
protected:
	const Method m_method;
	const std::string m_url;
	Status m_status;
	// The following may be read by any thread but should only be modified by a CURL worker thread:
	volatile double m_uploadBytesNow; // How many bytes have been uploaded so far
	volatile double m_uploadBytesTotal; // How many bytes will be uploaded total, if known. Otherwise this will be 0.
	volatile double m_downloadBytesNow; // How many bytes have been uploaded so far
	volatile double m_downloadBytesTotal; // How many bytes will be uploaded total, if known. Otherwise this will be 0.
private:
	std::map<std::string, std::string> m_requestHeaders;
	std::map<std::string, std::string> m_responseHeaders;
};

///////////////////////////////////////////////////////////////////////////////
// HttpRequest is built to be subclassed for more specific behaviour.
// A few standard subclasses follow:


// HttpDownload: Simple request to download a file.
class HttpDownload : public HttpRequest {
public:
	HttpDownload(const std::string& url, const std::string& destFile);
	~HttpDownload();
	
	virtual size_t Worker_HandleData(const unsigned char* contents, size_t size);
	virtual void Worker_HandleDone(bool success, int httpStatusCode);
protected:
	const std::string m_destFile;
	s3eFile* m_pTmpFile;
};


// HttpPost: Simple URL-encoded POST request (like a normal HTML form; cannot upload files)
class HttpPost : public HttpRequest {
public:
	HttpPost(const std::string& url);
	~HttpPost() {};
	
	virtual HttpPost& SetValue(const char* key, const char* value) { m_data[key] = value; return *this; }
	const std::string& GetValue(const char* key) { return m_data[key]; }
	
	virtual void CompileRequest();
	
	virtual long Worker_GetUploadSize() const { return m_postData.size(); }
	virtual size_t Worker_HandleUpload(const unsigned char* pData, size_t fillSize);
	virtual size_t Worker_HandleData(const unsigned char* contents, size_t size);
	virtual void Worker_HandleCleanup() { if (m_workerResponseBuffer) free((void*)m_workerResponseBuffer); }
	virtual void HandleResponse(bool success, int httpStatusCode);
	
	const json::Object& GetResponse() const { return m_responseData; }
protected:
	std::map<std::string, std::string> m_data; // Key-value pairs that we want to submit as the POST data
	//std::string m_postDataUrlEncoded;
	std::string m_postData;
	size_t m_bytesUploaded;
	const char* m_workerResponseBuffer; // Buffer used by the worker thread to store the response as it comes in.
	size_t m_workerResponseSize;
	
	json::UnknownElement m_responseData;
};

//HttpPostJson: Sends JSON objects in POST request
class HttpPostJson : public HttpPost {
public:
	HttpPostJson(const std::string& url);
	~HttpPostJson() {};
	HttpPostJson& SetPostData(const json::Object& jsonObj) { IwAssert(API_CLIENT, m_status == BUILDING); m_postDataJson = jsonObj; return *this; }
	const json::Object& GetPostData() const { return m_postDataJson; }
	
	virtual void CompileRequest();
	
protected:
	json::Object m_postDataJson;
};
