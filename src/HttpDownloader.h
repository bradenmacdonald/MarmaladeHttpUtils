// HttpDownloader:
// Equivalent to HttpClient, except this makes sure you don't try to
// download the same file simultaneously.
//
// Created by the Get to Know Society
// Public domain

#pragma once

#include "HttpClient.h"

class HttpDownloader : private HttpClient, public IObservable {
public:
	HttpDownloader(const char* userAgentStr, uint numWorkers = 3) : HttpClient(numWorkers, userAgentStr), m_activeDownloads() {} // Initialize
	virtual ~HttpDownloader() { m_activeDownloads.clear(); }
	
	Ptr<HttpRequest> DownloadFile(std::string url, std::string destFile) {
		if (m_activeDownloads.count(url))
			return m_activeDownloads[url];
		Ptr<HttpRequest> p_request = new HttpDownload(url, destFile);
		QueueRequest(p_request, new HttpCallback<HttpDownloader>(this, &HttpDownloader::HandleDownloadDone));
		m_activeDownloads[url] = p_request;
		return p_request;
	}
	
	void Update() { HttpClient::Update(); }
	
	void HandleDownloadDone(Ptr<HttpRequest> pRequest) {
		// Whether the download succeeded or failed, we remove it from the list of active
		// downloads:
		for (auto it = m_activeDownloads.begin(); it != m_activeDownloads.end(); it++) {
			if (it->second == pRequest) {
				m_activeDownloads.erase(it);
				break;
			}
		}
	}
	
private:
	std::map<std::string, Ptr<HttpRequest> > m_activeDownloads;
};
