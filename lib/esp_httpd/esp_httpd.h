#ifndef ESP_HTTPD_H
#define ESP_HTTPD_H

#define MAX_HTTP_CONNECTIONS 4
#define CONNECTION_EXPIRE_MS 30000
#define FILE_BUFFER_SIZE 1400

#define NOT_FOUND -1

#define HTTPD_SERVER "ESP_httpd"

// #define NO_PRINT
#define ESP_HTTPD_VERBOSE

#include <Arduino.h>
#include <serial.print.h>
#include <FS.h>

// Include API-Headers
extern "C" {
  // #include "ets_sys.h"
  // #include "os_type.h"
  // #include "osapi.h"
  // #include "mem_manager.h"
  // #include "mem.h"
  #include "user_interface.h"
  // #include "cont.h"
  #include "espconn.h"
  // #include "eagle_soc.h"
  // void*  pvPortZalloc(int size, char* , int);
}

#define zalloc(n) calloc(n, 1)

// HTTPMethod is also used to indicate the state of the HTTP request.
enum HTTPMethod { HTTP_NONE, HTTP_ANY, HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_PATCH, HTTP_DELETE, HTTP_SENDING };
enum ParamLocation { HTTP_QUERY, HTTP_DATA };

// Parsed arguments will be returned as an array of RequestArgument's.
struct RequestArgument {
  char* key;
  char* value;
};

// An array of HttpRequest's is used to track HTTP connections between callback function invocations.
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

// Prototype for the request handler functions.
typedef bool (*HandlerFunc)(espconn* pEspconn, HttpRequest &httpd_request, void* handlerArg);

// Each HTTP is checked against an array of HttpRoute's to determine if there is one or more suitable handlers.
struct HttpRoute {
  HTTPMethod method;
    const char* uri;
    HandlerFunc handlerFunc;
    void* handlerArg;
};

/********************************************************
  Function Prototypes
 ********************************************************/

void httpd_init(HttpRoute* pHttpRoutes, int port);
// Callbacks called by ESP
void httpd_connect(void* arg);
void httpd_discon(void* arg);
void httpd_recon(void* arg, int8_t err);
void httpd_recv(void* arg, char* pData, unsigned short len);
void httpd_sent(void* arg);
void httpd_write_finish(void* arg);
// Send-related functions
void httpd_router(espconn* pEspconn, HttpRequest &httpd_request);
void httpd_send(espconn* pEspconn, uint responseCode);
void httpd_send(espconn* pEspconn, uint responseCode, const char *pMime, const char *pData);
void httpd_send(espconn* pEspconn, uint responseCode, const char *pMime, const char *pData, uint lData);
// File Handling Functions
bool httpd_fileHandler(espconn* pEspconn, HttpRequest &httpd_request, void* handlerArg);
bool httpd_dirHandler(espconn* pEspconn, HttpRequest &httpd_request, void* handlerArg);
// Utility functions
void httpd_parseParams(HttpRequest &httpd_request, ParamLocation where);
const char* httpd_responseCodeToString(uint responseCode);
const char* httpd_mimetype(const char* filename);
// Debug functions
const char* httpd_methodToString(HTTPMethod method);
void httpd_dumpHttpReq(HttpRequest &httpd_request);
void httpd_dumpEspconn(espconn* pEspconn);

#endif
