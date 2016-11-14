#include "esp_httpd.h"

/********************************************************
   Global Variables
 ********************************************************/

HttpRequest httpd_requests[MAX_HTTP_CONNECTIONS];
HttpRoute* httpd_routes;

/********************************************************
   Web Functions
 ********************************************************/

void httpd_init(HttpRoute* pHttpRoutes, int port) {
  SP("\nhttpd_init...");

  // Zero the HTTP requests for good measure.
  for(uint8_t r = 0; r < MAX_HTTP_CONNECTIONS; r++) {
    memset(&httpd_requests[r], 0, sizeof(HttpRequest));
    httpd_requests[r].method = HTTP_NONE; // Not really needed because HTTP_NONE = 0.
  }

  httpd_routes = pHttpRoutes;

  espconn vEspconn;
  esp_tcp espTcp;
  // Fill the connection structure, including "listen" port
  vEspconn.type = ESPCONN_TCP;
  vEspconn.state = ESPCONN_NONE;
  vEspconn.proto.tcp = &espTcp;
  vEspconn.proto.tcp->local_port = port;
  vEspconn.recv_callback = NULL;
  vEspconn.sent_callback = NULL;

  // Register the connection timeout(0=no timeout)
  espconn_regist_time(&vEspconn, 5, 0);

  // Register connection callbacks
  espconn_regist_connectcb(&vEspconn, httpd_connect);
  espconn_regist_disconcb(&vEspconn, httpd_discon);
  espconn_regist_reconcb(&vEspconn, httpd_recon);
  espconn_regist_recvcb(&vEspconn, httpd_recv);
  espconn_regist_sentcb(&vEspconn, httpd_sent);
  espconn_regist_write_finish(&vEspconn, httpd_write_finish);

  // Start Listening for connections
  espconn_accept(&vEspconn);
  SPN("Web Server initialized");
}

int8_t httpd_findAvailHttpReq() {
  uint msNow = millis();
  for(uint8_t r = 0; r < MAX_HTTP_CONNECTIONS; r++) {
    if(httpd_requests[r].method == HTTP_NONE) return r;
    if(msNow - httpd_requests[r].msLast > CONNECTION_EXPIRE_MS) return r;
  }
  return NOT_FOUND;
}

void httpd_connect(void* arg) {
  SPN("\n*** httpd_connected");
  espconn* pEspconn = (espconn*) arg;
  httpd_dumpEspconn(pEspconn);

  espconn_set_opt(pEspconn, ESPCONN_REUSEADDR);
  int8_t r = httpd_findAvailHttpReq();
  if(r == NOT_FOUND) {
    SPN("No connection recs avail");
    // status = STATUS_ERR;
    return;
  }
  SPF("Using connection %d at %p\n", r, &httpd_requests[r]);
  memcpy(httpd_requests[r].remote_ip, pEspconn->proto.tcp->remote_ip, 4);
  httpd_requests[r].remote_port = pEspconn->proto.tcp->remote_port;
  httpd_requests[r].method = HTTP_ANY;
  httpd_requests[r].msLast = millis();
  httpd_requests[r].uri = NULL;
  httpd_requests[r].auth = NULL;
  httpd_requests[r].lenData = 0;   // Size of incoming or outgoing data.
  httpd_requests[r].lenSoFar = 0;  // Length of data received or sent so far.
  httpd_requests[r].data = NULL;
  httpd_requests[r].argCount = 0;
  // httpd_requests[r].args = (void *) NULL;
}

int8_t httpd_findHttpReq(espconn* pEspconn) {
  for(uint8_t r = 0; r < MAX_HTTP_CONNECTIONS; r++) {
    if(httpd_requests[r].remote_port == pEspconn->proto.tcp->remote_port &&
      httpd_requests[r].remote_ip[0] == pEspconn->proto.tcp->remote_ip[0] &&
      httpd_requests[r].remote_ip[1] == pEspconn->proto.tcp->remote_ip[1] &&
      httpd_requests[r].remote_ip[2] == pEspconn->proto.tcp->remote_ip[2] &&
      httpd_requests[r].remote_ip[3] == pEspconn->proto.tcp->remote_ip[3]) return r;
  }
  return NOT_FOUND;
}

void httpd_discon(void* arg) {
  SPN("\n*** httpd_disconnected");
  espconn* pEspconn = (espconn*) arg;
  httpd_dumpEspconn(pEspconn);

  int8_t r = httpd_findHttpReq(pEspconn);
  if(r == NOT_FOUND) {
    SPN("Connection rec not found");
    // status = STATUS_ERR;
    return;
  }
  SPF("Freeing connection %d\n", r);
  httpd_requests[r].method = HTTP_NONE;
  free(httpd_requests[r].uri);
  httpd_requests[r].uri = NULL;
  free(httpd_requests[r].data);
  httpd_requests[r].data = NULL;
  if(httpd_requests[r].argCount > 0) {
    free(httpd_requests[r].args);
    httpd_requests[r].argCount = 0;
  }
  httpd_dumpHttpReq(httpd_requests[r]);
}

void httpd_recon(void* arg, int8_t err) {
  SPN("\n*** httpd_recon");
  espconn* pEspconn = (espconn*) arg;
  httpd_dumpEspconn(pEspconn);
}

void httpd_recv(void* arg, char* pData, unsigned short len) {
  SPN("\n*** httpd_recv");
  espconn* pEspconn = (espconn*) arg;
  httpd_dumpEspconn(pEspconn);

  int8_t r = httpd_findHttpReq(pEspconn);
  if(r == NOT_FOUND) {
    SPN("Connection rec not found");
    // status = STATUS_ERR;
    return;
  }
  SPF("Using connection %d\n", r);

  if(httpd_requests[r].method == HTTP_ANY) {  // This is the first recv for this connection so this is assumed to be the header.
    // First line of the header is assumed to have the following format:
    // METHOD <space> URI <space> HTTP...
    // We look for the two spaces first.
    char* space1 = strchr(pData, ' ');
    if(!space1) {
      SPN("Invalid request 1");
      return;
    }
    char* space2 = strchr(space1 + 1, ' ');
    if(!space2) {
      SPN("Invalid request 2");
      return;
    }
    // Compare the start of the string to the available methods.
    if(strncmp("GET", pData, 3) == 0) httpd_requests[r].method = HTTP_GET;
    else if(strncmp(pData, "POST", 4) == 0) httpd_requests[r].method = HTTP_POST;
    else if(strncmp(pData, "PUT", 3) == 0) httpd_requests[r].method = HTTP_PUT;
    else if(strncmp(pData, "PATCH", 5) == 0) httpd_requests[r].method = HTTP_PATCH;
    else if(strncmp(pData, "DELETE", 6) == 0) httpd_requests[r].method = HTTP_DELETE;
    else {
      SPN("Invalid method");
      return;
    }

    // Allocate memory for the URI and associate it with the HTTP request.
    httpd_requests[r].uri = (char*) malloc(space2 - space1);
    if(!httpd_requests[r].uri) {
      SPN("Failed to malloc");
      return;
    }
    // Copy the URI to the HTTP request.
    memcpy(httpd_requests[r].uri, space1 + 1, space2 - space1 - 1);
    httpd_requests[r].uri[space2 - space1 - 1] = '\0';
    SPF("uri:%s<\n", httpd_requests[r].uri);

    // Loop through the remainder of the header, line by line,
    // looking for headers of interest.
    char* ptrFrom = strstr(space2 + 2, "\r\n");
    char* ptrTo;
    while(ptrTo = strstr(ptrFrom, "\r\n")) {
      // SPF("ptrFrom: %d ptrTo: %d\n", ptrFrom, ptrTo);
      if(strncmp("Content-Length:", ptrFrom, 15) == 0) {
        SPF("Content-Length: %s\n", ptrFrom + 15);
        httpd_requests[r].lenData = atoi(ptrFrom + 15);
        SPF("lenData: %d\n", httpd_requests[r].lenData);
      } else if(strncmp("Authorization: ", ptrFrom, 15) == 0) {
        // Assuming there is a space after the :.
        httpd_requests[r].auth = (char*) malloc(ptrTo - ptrFrom - 15 + 1);
        if(!httpd_requests[r].auth) {
          SPN("Failed to malloc");
          return;
        }
        // Copy the auth string to the HTTP request.
        memcpy(httpd_requests[r].auth, ptrFrom + 15, ptrTo - ptrFrom - 15);
        httpd_requests[r].auth[ptrTo - ptrFrom - 15] = '\0';
        SPF("auth:%s<\n", httpd_requests[r].auth);
      }
      ptrFrom = ptrTo + 2;
    }
  } else {
    // This is not the first chunk so we assume it is data, either the initial data
    // or a continuation of the data.
    if(httpd_requests[r].data) {
      // More data - append it to the data buffer.
      char* newPtr = (char*) realloc(httpd_requests[r].data, httpd_requests[r].lenSoFar + len + 1);
      if(!newPtr) {
        SPN("Failed to realloc");
        return;
      }
      memcpy(newPtr + httpd_requests[r].lenSoFar, pData, len);
      httpd_requests[r].data = newPtr;
      httpd_requests[r].lenSoFar += len;
      httpd_requests[r].data[httpd_requests[r].lenSoFar] = '\0';
    } else {
      // Initial data - copy it to the data buffer.
      char* newPtr = (char*) malloc(len + 1);
      if(!newPtr) {
        SPN("Failed to malloc");
        return;
      }
      strcpy(newPtr, pData);
      httpd_requests[r].data = newPtr;
      httpd_requests[r].lenSoFar = len;
      httpd_requests[r].data[len] = '\0';
    }
  }

  httpd_dumpHttpReq(httpd_requests[r]);

  if(httpd_requests[r].lenSoFar == httpd_requests[r].lenData) {
    // All the data has been received.
    SPN("All the data has been received");

    // Call the httpd_router
    httpd_router(pEspconn, httpd_requests[r]);
  }
}

void httpd_sent(void* arg) {
  SPN("\n*** httpd_sent");
  espconn* pEspconn = (espconn*) arg;
  httpd_dumpEspconn(pEspconn);

  int8_t r = httpd_findHttpReq(pEspconn);
  if(r == NOT_FOUND) {
    SPN("Connection rec not found");
    // status = STATUS_ERR;
    return;
  }
  SPF("Using connection %d\n", r);

  if(httpd_requests[r].method == HTTP_SENDING) {
    if(httpd_requests[r].lenSoFar < httpd_requests[r].lenData) {
       // There is more data to send so we assume it is a file and return control to the httpd_fileHandler
       // to pick up where it left off.
      httpd_fileHandler(pEspconn, httpd_requests[r], NULL);
    } else {
      // All data has been sent so we set the method to HTTP_NONE, effectively returning the
      // httpd_requests to the pool.
      httpd_requests[r].method = HTTP_NONE;
    }
  }
}

// The purpose of this callback is unclear.
// I have not seen it called in all of my testing.
void httpd_write_finish(void* arg) {
  SPN("\n*** httpd_write_finish");
  espconn* pEspconn = (espconn*) arg;
  httpd_dumpEspconn(pEspconn);
}

/********************************************************
   Send-related functions
 ********************************************************/

void httpd_router(espconn* pEspconn, HttpRequest &httpd_request) {
  SPN("\n*** httpd_router");
  httpd_dumpEspconn(pEspconn);

  int8_t i = 0;
  //Look up URI in the routing table.
  while(httpd_routes[i].method != HTTP_NONE) {
    // Is this route the correct method?
    if(httpd_routes[i].method == HTTP_ANY || httpd_routes[i].method == httpd_request.method) {
      if(
        // See if there's a literal match...
        strcmp(httpd_routes[i].uri, httpd_request.uri) == 0 ||
        // See if there's a wildcard match...
        (
          httpd_routes[i].uri[strlen(httpd_routes[i].uri) - 1] == '*' &&
          strncmp(httpd_routes[i].uri, httpd_request.uri, strlen(httpd_routes[i].uri) - 1) == 0
        )
      ) {
        SPF("Routing to handler: %d, method: %s, uri: %s...\n", i, httpd_methodToString(httpd_routes[i].method), httpd_routes[i].uri);
        if(httpd_routes[i].handlerFunc(pEspconn, httpd_request, httpd_routes[i].handlerArg)) break;
        SPF("Route %d's handler didn't handle it after all.\n", i);
      }
    }
    i++;
  }
  if(httpd_routes[i].method == HTTP_NONE) {
      // Dang, we're at the end of the URI table.
      // Generate a built-in 404 to handle this.
      SPF("\n%s not found. 404!\n", httpd_request.uri);
      httpd_send(pEspconn, 404);
  }
}

void httpd_send(espconn* pEspconn, uint responseCode) {
  httpd_send(pEspconn, responseCode, NULL, NULL, 0);
}

void httpd_send(espconn* pEspconn, uint responseCode, const char* pMime, const char* pData) {
  httpd_send(pEspconn, responseCode, pMime, pData, strlen(pData));
}

void httpd_send(espconn* pEspconn, uint responseCode, const char* pMime, const char* pData, uint lData) {
  SPN("\n*** httpd_send");
  // SPF("Sending - code: %d, mime: %s, len: %d, data:\n%s\n", responseCode, pMime, lData, pData);

  char* pBuf = NULL;
  char httphead[256];
  memset(httphead, 0, 256);

  sprintf(httphead,
    "HTTP/1.0 %d %s\r\nContent-Length: %d\r\nServer: %s\r\nAccess-Control-Allow-Origin: *\r\n",
    responseCode,
    httpd_responseCodeToString(responseCode),
    lData,
    HTTPD_SERVER
  );

  if(pData) {
    sprintf(httphead + strlen(httphead),
      "Content-type: %s\r\nExpires: Fri, 10 Apr 2015 14:00:00 GMT\r\nPragma: no-cache\r\n\r\n", pMime);
    uint lHead = strlen(httphead);
    pBuf = (char*) malloc(lData + lHead + 1);
    memcpy(pBuf, httphead, lHead);
    memcpy(pBuf + lHead, pData, lData);
    *(pBuf + lHead + lData) = '\0';
    espconn_send(pEspconn, (uint8 *)pBuf, lData + lHead);
    SPF("Sent:\n%s\n", pBuf);
  } else {
    sprintf(httphead + strlen(httphead), "\r\n");
    espconn_send(pEspconn, (uint8 *)httphead, strlen(httphead));
    SPF("Sent:\n%s\n", httphead);
  }

  if(pBuf) {
    free(pBuf);
  }
}

/********************************************************
   File Handling Functions
 ********************************************************/

bool httpd_fileHandler(espconn* pEspconn, HttpRequest &httpd_request, void* handlerArg) {
  SPN("\nFile Handler");
  char* uri;
  if(handlerArg) {
    uri = (char*) handlerArg;
  } else {
    uri = httpd_request.uri;
  }
  if(strcmp(uri, "/") == 0) {
    uri = (char*) "/dirlist.htm";
  }
  SPF("uri: %s\n", uri);

  if (!SPIFFS.begin()) {
    SPN("Failed to start SPIFFS");
    return false;
  }

  if(!SPIFFS.exists(uri)) {
    SPN("file not found");
    return false;
  }

  File f = SPIFFS.open(uri, "r");
  if(!f) {
    SPN("file open failed");
    return false;
  }

  if(httpd_request.method != HTTP_SENDING) {
    const char* mime = httpd_mimetype(uri);
    SPF("Mime type: %s\n", mime);

    httpd_request.lenData = f.size();
    httpd_request.lenSoFar = 0;
    char httphead[128];
    memset(httphead, 0, 128);

    sprintf(httphead,
      "HTTP/1.0 200 OK\r\nContent-Length: %d\r\nServer: %s\r\nContent-type: %s\r\n\r\n",
      httpd_request.lenData,
      HTTPD_SERVER,
      mime
    );

    SPF("lenData: %d sending header: %d\n", httpd_request.lenData, strlen(httphead));
    espconn_send(pEspconn, (uint8 *)httphead, strlen(httphead));
    httpd_request.method = HTTP_SENDING;
  } else if(httpd_request.lenSoFar == httpd_request.lenData) {
    SPN(" ... File sent");
  } else {
    uint lenToSend = httpd_request.lenData - httpd_request.lenSoFar;
    if(lenToSend > FILE_BUFFER_SIZE) lenToSend = FILE_BUFFER_SIZE;
    SPF("lenData: %d lenSoFar: %d lenToSend: %d\n", httpd_request.lenData, httpd_request.lenSoFar, lenToSend);
    char buf[lenToSend];
    f.seek(httpd_request.lenSoFar, SeekSet);
    f.readBytes(buf, lenToSend);
    espconn_send(pEspconn, (uint8 *)buf, lenToSend);
    httpd_request.lenSoFar += lenToSend;
  }

  f.close();
  return true;
}

bool httpd_dirHandler(espconn* pEspconn, HttpRequest &httpd_request, void* handlerArg) {
  SPN("\nDir Handler");

  if (!SPIFFS.begin()) {
    SPN("Failed to start SPIFFS");
    return false;
  }

  File f = SPIFFS.open("/dirlist.htm", "w");
  if(!f) {
    SPN("dirlist.htm open failed");
    return false;
  }

  f.print("<html>\n<body>\n");

  Dir dir = SPIFFS.openDir("/");
  char fn[128];
  while (dir.next()) {
    strcpy(fn, dir.fileName().c_str());
    if(strcmp(fn, "/dirlist.htm") == 0) continue;
    f.printf("<a href=\"%s\">%s</a><br>\n", fn, fn);
  }

  f.print("</body>\n</html>\n");
  f.close();
  SPN("Listing saved to /dirlist.htm");

  return httpd_fileHandler(pEspconn, httpd_request, NULL);
}

/********************************************************
   Utility Functions
 ********************************************************/

void httpd_parseParams(HttpRequest &httpd_request, ParamLocation where) {
  SPN("\n*** httpd_parseParams");
  httpd_request.argCount = 0;
  if(httpd_request.method == HTTP_ANY) return;
  char* ptrStart;
  if(where == HTTP_QUERY) {
    if(!(ptrStart = strstr(httpd_request.uri, "?"))) return;
    *ptrStart = '\0';
    ptrStart++;
    SPF("uri: %s query: %s\n", httpd_request.uri, ptrStart);
  } else {
    if(strlen(httpd_request.data) == 0) return;
    ptrStart = httpd_request.data;
  }

  // Check for an empty query string.
  if(*ptrStart == '\0') {
    httpd_request.args = 0;
    httpd_dumpHttpReq(httpd_request);
    return;
  }

  // Count the number of parameters
  uint8_t c = 1;
  uint i=0;
  while(ptrStart[i]) {
    if(ptrStart[i] == '&') c++;
    i++;
  }
  httpd_request.args = (RequestArgument*) calloc(c, sizeof(RequestArgument));
  char* ptrEquals;
  char* ptrAmp;
  for(i = 0; i < c; i++) {
    httpd_request.args[i].key = ptrStart;
    ptrEquals = strstr(ptrStart, "=");
    ptrAmp = strstr(ptrStart, "&");
    if(ptrAmp) *ptrAmp = '\0';
    else ptrAmp = ptrStart + strlen(ptrStart);
    if(ptrEquals && ptrEquals < ptrAmp) {
      *ptrEquals = '\0';
      httpd_request.args[i].value = ptrEquals + 1;
    } else {
      httpd_request.args[i].value = ptrAmp;
    }
    ptrStart = ptrAmp + 1;
  }
  httpd_request.argCount = c;
  httpd_dumpHttpReq(httpd_request);
}

const char* httpd_responseCodeToString(uint responseCode) {
  switch(responseCode) {
  case 200:
    return "OK";
  case 404:
    return "Not Found";
  case 500:
    return "Internal Server Error";
  }
  return "Unknown Error";
}

const char* httpd_mimetype(const char* filename) {
  char* ext = strrchr(filename, '.');
  SPF("ext: %s\n", ext);
  if(!ext) {
    return "text/plain";
  }
  ext++;
  if(strcmp(ext, "htm") == 0) return "text/html";
  else if(strcmp(ext, "css") == 0) return "text/css";
  else if(strcmp(ext, "jpg") == 0) return "image/jpeg";
  else if(strcmp(ext, "png") == 0) return "image/png";
  else if(strcmp(ext, "gif") == 0) return "image/gif";
  else if(strcmp(ext, "js") == 0) return "application/javascript";

  return "text/plain";
}

/********************************************************
   Dubugging Functions
 ********************************************************/

const char* httpd_methodToString(HTTPMethod method) {
  switch(method) {
  case HTTP_ANY:
    return "<any>";
  case HTTP_GET:
    return "GET";
  case HTTP_POST:
    return "POST";
  case HTTP_PUT:
    return "PUT";
  case HTTP_PATCH:
    return "PATCH";
  case HTTP_DELETE:
    return "DELETE";
  }
  return "Unknown";
}

void httpd_dumpHttpReq(HttpRequest &httpd_request) {
#ifdef ESP_HTTPD_VERBOSE
  SPN("\n>>> HttpRequest <<<");
  SPF("httpd_request at %p\n", httpd_request);
  SPF("->remote: %d.%d.%d.%d:%d\n",
    httpd_request.remote_ip[0],
    httpd_request.remote_ip[1],
    httpd_request.remote_ip[2],
    httpd_request.remote_ip[3],
    httpd_request.remote_port
  );
  SPF("->method: %s\n", httpd_methodToString(httpd_request.method));
  SPF("->uri: %s\n", httpd_request.uri);
  SPF("->lenData: %d\n", httpd_request.lenData);
  SPF("->lenSoFar: %d\n", httpd_request.lenSoFar);
  SPF("->data: %s\n", httpd_request.data);
  SPF("->argCount: %d\n", httpd_request.argCount);
  RequestArgument* args = httpd_request.args;
  for(uint8_t i = 0; i < httpd_request.argCount; i++) {
    SPF("->%s: %s\n", args[i].key, args[i].value);
  }
#endif
  SPF("\nHeap: %d\n", ESP.getFreeHeap());
}

void httpd_dumpEspconn(espconn* pEspconn) {
#ifdef ESP_HTTPD_VERBOSE
  SPN("\n### espconn ###");
  SPF("espconn at %p\n", pEspconn);
  SPF("->type: %d\n", pEspconn->type);
  SPF("->state: %d\n", pEspconn->state);
  SPF("->remote: %d.%d.%d.%d:%d\n",
    pEspconn->proto.tcp->remote_ip[0],
    pEspconn->proto.tcp->remote_ip[1],
    pEspconn->proto.tcp->remote_ip[2],
    pEspconn->proto.tcp->remote_ip[3],
    pEspconn->proto.tcp->remote_port
  );
  SPF("->link_cnt: %d\n", pEspconn->link_cnt);
#endif
}
