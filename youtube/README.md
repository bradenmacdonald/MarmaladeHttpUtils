HttpUtils YouTube API Example
=============================

This implements the YouTube API, or at least enough of it to upload a video on
a background thread and display the upload progress to the user during the
upload.


Example usage:
```c++
class MyAppModule {
	// Authenticate with Google's YouTube API:
	void HandleSubmitButton() {
		Ptr<GoogleOAuthRequest> p_oauth_request = new GoogleOAuthRequest("MY_CLIENT_ID", "MY_CLIENT_TOKEN", "MY_REFRESH_TOKEN");
		GetApp()->GetAPIClient()->QueueRequest(p_oauth_request, new HttpCallback<MyAppModule>(this, &MyAppModule::TokenRequestResponse));
		// Update UI to say "Uploading..."
	}

	// We should now be authenticated:
	void TokenRequestResponse(Ptr<HttpRequest> _pRequest) {
		Ptr<GoogleOAuthRequest> pRequest = dynamic_cast<GoogleOAuthRequest*>(_pRequest.ptr());
		bool success = false;
		if (pRequest->GetStatus() == ApiRequest::DONE) {
			try {
				const json::Object& data = pRequest->GetResponse();
				m_accessToken = string((json::String)data["access_token"]);
				SessionRequest();
			} catch (const json::Exception& e) { s3eDebugTracePrintf("Error: %s", e.what()); }
		} else {
			// Report error, ask user if they want to retry or not.
		}
	}

	// Start a YouTube resumable upload session:
	void SessionRequest() {
		Ptr<YoutubeSessionRequest> p_session_request = new YoutubeSessionRequest(m_accessToken, m_videoFileSize, m_videoTitleString, m_videoDescription, 22, "unlisted");
		
		// Now use any old instance of HttpClient to handle this.
		// In this example, a global instance is available via `GetAppSingleton()->GetAPIClient()`
		GetAppSingleton()->GetAPIClient()->QueueRequest(p_session_request, new HttpCallback<MyAppModule>(this, &MyAppModule::SessionRequestResponse));
	}

	// Handle the session response
	void SessionRequestResponse(Ptr<HttpRequest> _pRequest) {
		Ptr<YoutubeSessionRequest> pRequest = dynamic_cast<YoutubeSessionRequest*>(_pRequest.ptr());
		if(pRequest->GetStatus() == ApiRequest::DONE) {
			try {
				string resumable_uri = pRequest->GetResponseHeaders()["Location"];
				s3eDebugTracePrintf("Got Youtube resumable session URI: %s", resumable_uri.c_str());
				UploadRequest(resumable_uri, m_accessToken);
			} catch (const json::Exception& e) { s3eDebugTracePrintf("Error: %s", e.what()); }
		} else {
			// Report error, ask user if they want to retry or not.
		}
	}

	// Start the upload:
	void UploadRequest(string resumableURI, string accessToken) {
		m_pUploadRequest = new YoutubeUploadRequest(resumableURI, accessToken, m_videoFilePath, m_videoFileSize);

		GetApp()->GetAPIClient()->QueueRequest(m_pUploadRequest, new HttpCallback<MyAppModule>(this, &MyAppModule::UploadRequestResponse));

		// In the meantime, you can use m_pUploadRequest->GetUploadFraction() to show an updating progress bar.
	}

	// Our upload has finished!
	void UploadRequestResponse(Ptr<HttpRequest> _pRequest) {
		Ptr<YoutubeUploadRequest> pRequest = dynamic_cast<YoutubeUploadRequest*>(_pRequest.ptr());
		if(pRequest->GetStatus() == HttpRequest::DONE) {
			try {
				const json::Object& data = pRequest->GetResponse();
				s3eDebugTracePrintf("Uploaded video with Youtube ID %s, title %s", string((json::String)data["id"]).c_str(), string((json::String)data["snippet"]["title"]).c_str());
				// Notify the user of success
			} catch (const json::Exception& e) { s3eDebugTracePrintf("Error: %s", e.what()); }
		} else {
			// Report error, ask user if they want to retry or not.
		}
	}
}
```
