// Interface with the Youtube Data Api
//
// Created in 2013 by Get to Know Society and TakingITGlobal
// for the Explore150 project, www.explore150.ca
//
// Public domain

#pragma once

#include "HttpClient.h"
#include "s3eFile.h"

//Exchange Google OAuth2 Refresh token for an Access token
class GoogleOAuthRequest : public HttpPost {
public:
	GoogleOAuthRequest(const char* clientID, const char* clientToken, const char* refreshToken);
	virtual ~GoogleOAuthRequest() {};
};

//ask Youtube for a resumable session URI
class YoutubeSessionRequest: public HttpPostJson {
public:
	YoutubeSessionRequest(std::string accessToken, int videoFileSize, std::string title, std::string description, int category, std::string privacyStatus);
	virtual ~YoutubeSessionRequest() {};
};

//Upload video to Youtube with a PUT request
class YoutubeUploadRequest : public HttpRequest {
public:
	YoutubeUploadRequest(std::string resumableURI, std::string accessToken, std::string filepath, int videoFileSize);
	virtual ~YoutubeUploadRequest() {};
	
	virtual long Worker_GetUploadSize() const { return m_fileSize; }
	virtual size_t Worker_HandleUpload(const unsigned char* pData, size_t fillSize);
	virtual size_t Worker_HandleData(const unsigned char* contents, size_t size);
	virtual void Worker_HandleCleanup() { if (m_workerResponseBuffer) free((void*)m_workerResponseBuffer); if(m_pUploadFile) s3eFileClose(m_pUploadFile); }
	virtual void HandleResponse(bool success, int httpStatusCode);
	
	size_t GetBytesUploaded() { return m_bytesUploaded; }
	const json::Object& GetResponse() const { return m_responseData; }
	
protected:
	int m_fileSize;
	
	size_t m_bytesUploaded;
	const char* m_workerResponseBuffer; // Buffer used by the worker thread to store the response as it comes in.
	size_t m_workerResponseSize;
	
	s3eFile* m_pUploadFile;
	std::string m_filePath;
	json::UnknownElement m_responseData;
};
