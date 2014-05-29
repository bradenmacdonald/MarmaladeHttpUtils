HttpUtils for Marmalade
=======================

A Marmalade-friendly wrapper around libcurl. `HttpClient` will automatically
spawn and manage worker threads to allow fast parallel downloads.

Usage & Example
---------------
For general usage info, see the example in [`HttpUtils.cpp`](HttpUtils.cpp)

You can also subclass `HttpClient` and/or `HttpRequest` to create things like:
 * A file downloader
 * A YouTube API Client (example included in the `youtube` folder)
 * A client for your custom API
 * etc.

Or, you can use the included `HttpClient` or `HttpDownloader` for basic GET/POST/PUT requests.

HTTPS Support
-------------
HTTPS support is included and enabled, however it will not work out of the box
unless you correctly configure OpenSSL/CURL to be able to verify the
certificates of the server[s] you use. See `HttpClient_WorkerThread()` in
[`HttpClient.cpp`](src/HttpClient.cpp) for a comment describing how to
disable certificate checks if you want a quick-and-dirty insecure way to test
an HTTPS connection.

Curl patch notes
----------------

This includes a patched version of libcurl 7.34.0.

The patch is applied to `source/http.c` line 2425 in `Curl_http()`:

```c
// Due to an unfortunate bug with Marmalade, any attempt to pass
// "long long" types like curl_off_t as a variadic parameter
// corrupt the value on GCC ARM RELEASE builds on iOS.
// So we have to downcast to an int and hope the user is not trying
// any really large files (over 4GB)
unsigned long int size_fix = (unsigned long int)postsize;
result = Curl_add_bufferf(req_buffer, "Content-Length: %ld\r\n",
                          size_fix);
//result = Curl_add_bufferf(req_buffer,
//                          "Content-Length: %" FORMAT_OFF_T"\r\n",
//                          postsize);
```

License
-------
Most original files here are public domain. But check inside each file/folder
as the licenses and copyrights vary. All are suitable for use in commercial
projects, however.
