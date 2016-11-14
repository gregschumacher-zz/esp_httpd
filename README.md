# esp_httpd

A simple, entirely event-driven HTTP server for serving files and responding to RESTful requests, intended for use in Arduino-based projects.

#Goal

First and foremost, esp_httpd needed to be entirely event-driven. An ESP8266's underlying networking framework presents an event-driven interface so it was decided that all interaction with the networking framework would use this interface.

To be useful in a variety of projects it was decided that esp_httpd should be able to:

* Serve files stored in flash using SPIFFS. These files will be static HTML, CSS, JS and images that comprise the host device's web interface.
* Differentiate between GET, POST, PUT, and DELETE requests. This is how the host device will correctly respond to AJAX requests.
* Parse key-value pairs in the URI or body. Data to be parsed can be submitted in the URI as a typical GET query string, or as key-value pairs in the body as in a typical POST. The data parsing should not be automatic in case raw or JSON-encoded data is sent in the body.
* Route to different handlers based on the URI furnished, with support for a trailing wildcard and cascading handlers.
* Use C-strings exclusively in place of Arduino String objects in an effort to minimize the potential for heap fragmentation.

#Limitations/Assumptions

esp_httpd is limited in some respects, in part because it was built using the author's knowledge of HTTP and a little reverse engineering. It is not built to strict compliance with the relevant RFC's, but serves pages to modern browsers as expected.

Some of the assumptions on which esp_httpd is built include:

* The HTTP header will be received in its entirety in the first packet. Assuming there is not a lot of cookie data this should not be an issue.
* Dynamic data to be sent fits in a single packet. Because the anticipated dynamically-generated responses will be sensor readings or other state information this should not be an issue.
* The number of simultaneous connections will be limited. It is not anticipated that the server will be accessed by multiple clients simultaneously.
* Data, in the form of key-value pairs, will appear in the query string or body of the HTTP request, but not both places in the same request.
* esp_httpd has been built using the author's knowledge of HTTP and a little reverse engineering. It is not built to strict compliance with the relevant RFC's, but has served pages to modern browsers as expected.

#Inspiration

This solution was greatly inspired by libesphttpd (https://github.com/Spritetm/libesphttpd). In fact, I originally intended to use libesphttpd in my own projects but ran into troubles I couldn't overcome due to my own limitations, not the library's. Because I knew I didn't need all the functionality, and the resulting complexity, that libesphttpd offers I decided to start a new httpd library from scratch. At least this way I "should" understand what it is doing and why.

The aspect of libesphttpd that really caught my attention was the routing array. I appreciate how the array codifies URI's to which the server is prepared to respond. Because esp_httpd is intended to be used in a RESTful manner it expands the routes specification to include the required method, if any.

# What is included here

The only files you need to include in your project in order to use `esp_httpd ` are located in `/lib/esp_httpd/`. Other files in this repository include:

* `/src/esp_httpd_test.cpp` An Arduino program for creating a simple web server based on `esp_httpd`.
* `/lib/serial.print/serial.print.h` A set of preprocessor macros for printing to the Serial port (see **Troubleshooting/Seeing what is going on** below).
* `/lib/wifi/wifi.cpp` A function for initializing WiFi.
* `/data/` A folder containing sample HTML and graphic files for testing purposes.
* `rebuildfs` A short batch file useful in the PlatformIO IDE for building and uploading the `/data/` directory to the ESP8266 device as a SPIFFS-based file system.

#Programming Guide

Creating a program that utilizes esp_httpd will require some knowledge of HTTP (for example the difference between a GET and a POST request), how HTTP headers work, what a mime-type is and so on.

## Initialing the web server

The web server is typically initialized in you program's `setup()` function by a call to `httpd_init(HttpRoute* pHttpRoutes, int port)`. This passes your list of routes to the server and starts it listening on the port specified.

`HttpRoute* pHttpRoutes` is an array of type `HttpRoute`. Below is an example.

```
struct HttpRoute {
  HTTPMethod method;
  const char* uri;
  HandlerFunc handlerFunc;
  void* handlerArg;
};

HttpRoute httpRoutes[] = {
  {HTTP_ANY, "/static", cgiStatic, NULL},
  {HTTP_GET, "/test?*", cgiGet, NULL},
  {HTTP_POST, "/test", cgiPost, NULL},
  {HTTP_GET, "/", httpd_dirHandler, NULL},
  {HTTP_GET, "*", httpd_fileHandler, NULL},
  {HTTP_NONE, NULL, NULL, NULL}
};
```

When the web server receives a new request it compares the request against each route, in the order provided. When it finds one that matches it gives the handler specified the opportunity to respond. If the handler indicates that it has not respinded to the request the web server continues down the list of routes looking for others that match. If none are found, or those that match don't respond to the request, it returns a `404 Not Found` response to the client.

Each route indicates the method that is appropriate, or `HTTP_ANY` if any method is accepted.

The `uri` can include a wildcard in the form of a `*` at the end. This is particularly useful for GET requests with query strings, or where you want the handler to use more sophisticated logic to inturpret the URI.

Each handler is a function with the following prototype:

`bool handlerFunc(espconn* pEspconn, HttpRequest &httpd_request, void* handlerArg)`

The pointer to the `espconn` struct is required when sending a response back via the networking framework. The `HttpRequest` struct is used to track the HTTP request between function calls, and is passed to the handler so it has the details about the HTTP request.

The last entry in the httpRoutes array is the sentinel entry. It's method must be set to `HTTP_NONE `. This entry indicates to the web server that it has reached the end of the array.

## Writing a handler

The role of the handler is to respond to the client's request. When the handler is called the client is waiting for a response. esp_httpd provides three functions for sending data back to the client.

`httpd_send(espconn* pEspconn, uint responseCode)` sends just a header, which includes the response code provided.

`httpd_send(espconn* pEspconn, uint responseCode, const char *pMime, const char *pData)` sends data in the form of a C-string (null terminated). The mime type, also supplied as a C-string, is sent in the `Content-type` header.

`httpd_send(espconn* pEspconn, uint responseCode, const char* pMime, const char* pData, uint lData)` sends data of the length specified. This version is well suited for sending binary data that may include null values.

A handler has access to the `httpd_request` data, which is defined as follows:

```
struct HttpRequest {
  uint8_t remote_ip[4];
  uint remote_port;
  uint msLast;
  HTTPMethod method;
  char* uri;
  char* auth;
  uint lenData;
  uint lenSoFar;
  char* data;
  uint8_t argCount;
  RequestArgument* args;
};
```

Some HTTP requests include data in the form of key-value pairs in the query string (example: part of a GET URI following the `?`) or in the body of the request (example: data sent from a form where `method="POST"`). The `httpd_parseParams` function can be called to parse this data. The resulting keys and values are available from the `args` array. This function is not called automatically. 

It should be noted that `httpd_parseParams` does not allocate additional memory for the keys and values it finds. Instead, it replaces the delimiters (`&` and `=`) with `'\0'` and creates an array of `RequestArgument` structs that point to the strings where they exist - in the URI or data.

## Serving Files

esp_httpd includes two built-in handlers, both related to serving files.

`httpd_fileHandler` serves any file located in flash that is accessable through SPIFFS. It can handle files that are larger than the size of a single packet. Files larger than 1400 bytes are sent in chunks, with subsequent chunks being sent in the `httpd_sent` callback.

`httpd_fileHandler` performs one potential URI rewrite, replacing `/` with `/dirlist.htm`. A file named `dirlist.htm` can be included in those uploaded to the SPIFFS file system, or a route can be added so requests for `/` are sent to `httpd_dirHandler`.

`httpd_dirHandler` creates or updates `dirlist.htm ` based on the contents of the file system, then calls `httpd_fileHandler` to serve the file. This can be useful if you want easy access to log files that have been written to the file system.

# Troubleshooting/Seeing what is going on

If you are like me you put a lot of print statements in your code, at least at first, to track what it is doing. This library includes a few functions intended to dump key structures in order to add visibility into what is going on. 

It also includes as an approach to including `Serial.print`, and related print functions, using short preprocessor macros that expend to the commonly used function calls. By setting these macros to empty strings the print statements can be removed without cluttering the code with `#ifdef` statements. See `serial.print.h` for the supported macros.

# Feedback

This is very much a work in progress - my first GitHub submitted projects. Your feedback is welcome and greatly appreciated.

_An Arduino-minded hobbiest in a C++ world._ - gregschumacher