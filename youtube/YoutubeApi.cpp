// Interface with the Youtube Data Api
//
// Created in 2013 by Get to Know Society and TakingITGlobal
// for the Explore150 project, www.explore150.ca
//
// Public domain

#include "YoutubeApi.h"
#include "iohelpers.h"

using std::string;

// From our "stringhelpers.h" file:
// To create an STL string using printf syntax
// http://stackoverflow.com/questions/2342162/stdstring-formating-like-sprintf
static inline string string_format(const char* const fmt, ...) {
	int size=100; // guess at initial buffer length needed
	string str;
	va_list ap;
	while (1) {
		str.resize(size);
		va_start(ap, fmt);
		int n = vsnprintf((char *)str.c_str(), size, fmt, ap);
		va_end(ap);
		if (n > -1 && n < size) {
			str.resize(n);
			return str;
		}
		if (n > -1)
			size=n+1;
		else
			size*=2;
	}
}


GoogleOAuthRequest::GoogleOAuthRequest(const char* clientID, const char* clientToken, const char* refreshToken)
: HttpPost("https://accounts.google.com/o/oauth2/token")
{
	SetValue("client_id", clientID);
	SetValue("client_secret", clientToken);
	SetValue("refresh_token", refreshToken);
	SetValue("grant_type", "refresh_token");
}

YoutubeSessionRequest::YoutubeSessionRequest(string accessToken, int videoFileSize, string title, string description, int category, string privacyStatus)
: HttpPostJson("https://www.googleapis.com/upload/youtube/v3/videos?uploadType=resumable&part=snippet,status"){
	SetHeader("Authorization", string_format("Bearer %s", accessToken.c_str()));
	SetHeader("Content-Type", "application/json; charset=UTF-8");
	SetHeader("X-upload-content-type", "video/*");
	SetHeader("X-Upload-Content-Length", string_format("%d", videoFileSize));
	json::Object video_resource;
	json::Object video_snippet;
	video_snippet["title"] = (json::String) title;
	video_snippet["description"] = (json::String) description;
	video_snippet["categoryId"] = (json::Number) category;
	video_resource["snippet"] = video_snippet;
	
	json::Object video_status;
	video_status["privacyStatus"] = (json::String) privacyStatus;
	video_resource["status"] = video_status;

	SetPostData(video_resource);
}

YoutubeUploadRequest::YoutubeUploadRequest(string resumableURI, string accessToken, string filepath, int videoFileSize)
: HttpRequest(PUT, resumableURI.c_str()),
m_bytesUploaded(0),
m_workerResponseSize(0),
m_workerResponseBuffer(NULL),
m_pUploadFile(nullptr),
m_filePath(filepath)
{
	SetHeader("Authorization", string_format("Bearer %s", accessToken.c_str()));
	SetHeader("Content-Type", "video/*");
	SetHeader("Content-Length", string_format("%d", videoFileSize));
	
	m_fileSize = videoFileSize;
	//Trace("YoutubeUploadRequest videoFileSize %d", videoFileSize);
}

size_t YoutubeUploadRequest::Worker_HandleUpload(const unsigned char* pData, size_t fillSize) {
	size_t ncopy = 0;
	if(!m_pUploadFile) {
		m_pUploadFile = s3eFileOpen(m_filePath.c_str(), "rb");
		if (m_pUploadFile == nullptr)
			throw std::runtime_error("Unable to open file for uploading!");
	}
	if(m_pUploadFile) {
		ncopy = s3eFileRead((void*)pData, 1, fillSize, m_pUploadFile);
	}
	m_bytesUploaded += ncopy;
	//Trace("uploaded %d/%d for %0.2f", m_bytesUploaded, m_fileSize, m_bytesUploaded/((double) m_fileSize));
	return ncopy;
}

size_t YoutubeUploadRequest::Worker_HandleData(const unsigned char* contents, size_t size) {
	if (size > 0) {
		m_workerResponseBuffer = (const char*)realloc((void*)m_workerResponseBuffer, m_workerResponseSize + size);
		if (m_workerResponseBuffer) {
			memcpy((void*)(m_workerResponseBuffer + m_workerResponseSize), (void*)contents, size);
			m_workerResponseSize += size;
		}
	}
	return size;
}

void YoutubeUploadRequest::HandleResponse(bool success, int httpStatusCode) {
	HttpRequest::HandleResponse(success, httpStatusCode);
	string response(m_workerResponseBuffer, m_workerResponseSize);
	if (success) {
		s3eDebugTracePrintf("Youtube upload request succeeded (%s %s)", GetMethodStr(), m_url.c_str());
		if (response.empty()) {
			s3eDebugTracePrintf("Warning: Empty response body from youtube upload call.");
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
			s3eDebugTracePrintf("Response: %s", string(json::String(response)).c_str());
		}
		
	} else {
		s3eDebugTracePrintf("Error with Youtube upload request. Response code %d", httpStatusCode);
		s3eDebugTraceLine(response.c_str());
	}
}
