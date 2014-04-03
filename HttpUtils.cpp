// HttpUtils example.
// Created by the Get to Know Society
// Public domain

#include "s3e.h"
#include "IwDebug.h"
#include "HttpClient.h"

// Main entry point for the application
int main()
{
	//Initialise modules
	HttpClient::GlobalInit(); // Put this BEFORE IwGxInit or IwUIInit
	
	
	
	
	// You can now create one or more HttpClient objects, each of which can spawn one or more worker threads
	HttpClient* g_httpClient = new HttpClient(5 /* # of worker threads */, "HttpUtils Example Client v1.0" /* User Agent */);
	
	// Let's download a file:
	Ptr<HttpDownload> p_download = new HttpDownload("https://www.madewithmarmalade.com/sites/all/themes/marmalade/images/marmalade-header.png", "marmalade_logo.png");
	g_httpClient->QueueRequest(p_download);
	
	
	
	// Loop forever, until the user or the OS performs some action to quit the app
	while (!s3eDeviceCheckQuitRequest())
	{
		//Update the input systems
		//s3eKeyboardUpdate();
		//s3ePointerUpdate();

		
		// Your rendering/app code goes here.
		
		// Wait for the download to finish... (NOTE: You can also do this using callbacks, which is obviously better in general)
		if (p_download->GetStatus() == HttpRequest::DONE) {
			s3eDebugAssertShow(S3E_MESSAGE_CONTINUE, "Image was downloaded!");
			break;
		} else if (p_download->GetStatus() == HttpRequest::ERROR) {
			s3eDebugErrorShow(S3E_MESSAGE_CONTINUE, "Image failed to downloaded.");
			break;
		}
		
		// You must update any HttpClients as part of your main loop
		g_httpClient->Update();

		// Sleep for 0ms to allow the OS to process events etc.
		s3eDeviceYield(0);
	}
	
	p_download = nullptr;
	
	delete g_httpClient;
	g_httpClient = nullptr;

	//Terminate modules being used
	HttpClient::GlobalCleanup(); // Put after IwUITerminate etc.
	
	// Return
	return 0;
}
