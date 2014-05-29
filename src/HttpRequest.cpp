// HttpRequest:
// Encapsulates/represents a single HTTP request.
// Used with HttpClient
//
// Created by the Get to Know Society
// Public domain

#include <IwMath.h>
#include "HttpRequest.h"
#include "util/iohelpers.h"

using std::string;
using std::ostringstream;

///////////////////////////////////////////////////////////////////////////////
// HttpRequest:

string HttpRequest::UrlEncode(const string& value, bool strict) {
	// strict=true is better for POST data that is URL-encoded (application/x-www-form-urlencoded)
	// strict=false is better for URL-encoding data to put in an actual URL.
    ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
	for (auto i = value.begin(); i != value.end(); i++) {
		const char c = (*i);
		if (isalnum(c)) {
			escaped << c;
		} else if ((c == '-' || c == '_' || c == '.') && !strict) {
			escaped << c;
		} else if (c == ' ' && !strict)  {
			escaped << '+';
		} else {
			escaped << '%' << std::setw(2) << ((int) c) << std::setw(0);
		}
	}
    return escaped.str();
}

///////////////////////////////////////////////////////////////////////////////
// HttpDownload:

HttpDownload::HttpDownload(const string& url, const string& destFile) :
	HttpRequest(GET, url.c_str()),
	m_destFile(destFile),
	m_pTmpFile(nullptr)
{
	string download_folder = DirName(m_destFile);
	if (!IsDir(download_folder))
		MakePath(download_folder);
	m_status = PENDING;
}

HttpDownload::~HttpDownload() {}

size_t HttpDownload::Worker_HandleData(const unsigned char* contents, size_t size) {
	if (!m_pTmpFile) {
		m_pTmpFile = s3eFileOpen(string(m_destFile).append(".tmp").c_str(), "w");
		if (m_pTmpFile == nullptr)
			throw std::runtime_error("Unable to open file for downloading!");
	}
	if (m_pTmpFile)
		s3eFileWrite(contents, 1, size, m_pTmpFile);
	return size;
}

void HttpDownload::Worker_HandleDone(bool success, int httpStatusCode) {
	if (m_pTmpFile) {
		s3eFileClose(m_pTmpFile);
		m_pTmpFile = nullptr;
		if (success && httpStatusCode == 200)
			s3eFileRename(string(m_destFile).append(".tmp").c_str(), m_destFile.c_str());
		else
			s3eFileDelete(string(m_destFile).append(".tmp").c_str());
	}
	HttpRequest::Worker_HandleDone(success, httpStatusCode);
}

///////////////////////////////////////////////////////////////////////////////
// HttpPost:

HttpPost::HttpPost(const std::string& url)
	: HttpRequest(POST, url.c_str()), m_bytesUploaded(0), m_workerResponseSize(0), m_workerResponseBuffer(NULL)
{
	SetHeader("Content-Type", "application/x-www-form-urlencoded");
}

void HttpPost::CompileRequest() {
	IwAssert(HTTP_CLIENT, m_postData.empty());
	for (auto it = m_data.begin(); it != m_data.end(); it++) {
		if (it != m_data.begin())
			m_postData.append("&");
		std::string key = UrlEncode(it->first);
		std::string val = UrlEncode(it->second);
		m_postData.append(key).append("=").append(val);
	}
	// Now m_postData should look like "name=bob&age=35&gender=M" ...
	//Trace("Compiled request post body:%s", m_postData.c_str());
	HttpRequest::CompileRequest();
}

size_t HttpPost::Worker_HandleUpload(const unsigned char* pData, size_t fillSize) {
	size_t ncopy = MIN(m_postData.size() - m_bytesUploaded, fillSize);
	memcpy((void*)pData, (void*)(m_postData.c_str() + m_bytesUploaded), ncopy);
	m_bytesUploaded += ncopy;
	return ncopy;
}

size_t HttpPost::Worker_HandleData(const unsigned char* contents, size_t size) {
	if (size > 0) {
		m_workerResponseBuffer = (const char*)realloc((void*)m_workerResponseBuffer, m_workerResponseSize + size);
		if (m_workerResponseBuffer) {
			memcpy((void*)(m_workerResponseBuffer + m_workerResponseSize), (void*)contents, size);
			m_workerResponseSize += size;
		}
	}
	return size;
}

void HttpPost::HandleResponse(bool success, int httpStatusCode) {
	HttpRequest::HandleResponse(success, httpStatusCode);
	string response(m_workerResponseBuffer, m_workerResponseSize);
	if (success) {
		s3eDebugTracePrintf("API Request succeeded (%s %s)", GetMethodStr(), m_url.c_str());
		if (response.empty()) {
			s3eDebugTracePrintf("Warning: Empty response body from API call.");
			m_responseData = json::Null();
		} else if (response[0] == '[' || response[0] == '{') {
			// Parse the response as a JSON object or array:
			try {
				std::istringstream json_istream(response);
				json::Reader::Read(m_responseData, json_istream);
			} catch(const json::Exception &e) {
				s3eDebugTracePrintf("Error: Unable to parse JSON response: %s", e.what());
				m_status = ERROR;
			}
		} else {
			m_responseData = json::String(response);
		}
		
	} else {
		s3eDebugTracePrintf("Error with API call. Response code %d", httpStatusCode);
		s3eDebugTraceLine(response.c_str());
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////
//HttpPostJson
HttpPostJson::HttpPostJson(const std::string& url) :
	HttpPost(url)
{
	SetHeader("Content-Type", "application/json");
}

void HttpPostJson::CompileRequest() {
	std::ostringstream str_stream;
	json::Writer::Write(m_postDataJson, str_stream);
	m_postData = str_stream.str();
	
	char size_as_string[40];
	snprintf(size_as_string, sizeof(size_as_string), "%zu", m_postData.size()),
	SetHeader("Content-Length", size_as_string);
	HttpRequest::CompileRequest();
}
