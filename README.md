HttpUtils for Marmalade
=======================

A Marmalade-friendly wrapper around libcurl. `HttpClient` will automatically
spawn and manage worker threads to allow fast parallel downloads.

Usage: see example in `HttpUtils.cpp` 

You can also subclass `HttpClient` to create things like: 
 * A file downloader
 * A YouTube API Client (we have done this to upload videos from iOS/Android,
   and it works really well)
 * A client for your custom API
 * etc.

Or, you can use the included `HttpClient` or `HttpDownloader` for basic GET/POST/PUT requests.

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
