#include "esp_httpd_test.h"

#define SERBAUD 115200
#define SVRPORT 80


/********************************************************
   Global Variables
 ********************************************************/

uint32_t msBlink;
STATUSES status = STATUS_OK;

/********************************************************
   Routes
 ********************************************************/

HttpRoute httpRoutes[] = {
  {HTTP_GET, "/favicon.ico", cgiFavicon, NULL},
  {HTTP_ANY, "/static", cgiStatic, NULL},
  {HTTP_GET, "/test?*", cgiGet, NULL},
  {HTTP_POST, "/test", cgiPost, NULL},
  {HTTP_GET, "/", httpd_dirHandler, NULL},
  // {HTTP_GET, "/", httpd_fileHandler, (void*) "/dirlist.htm"},
  {HTTP_GET, "*", httpd_fileHandler, NULL},
  {HTTP_NONE, NULL, NULL, NULL}
};

/********************************************************
   Setup
 ********************************************************/

void setup() {
  // Give us something pretty to look at.
  pinMode(PIN_HB_LED, OUTPUT);           // Set Heartbeat LED as output
  digitalWrite(PIN_HB_LED, LED_ON);      // Turn LED on
  msBlink = millis();

  // Open a serial connection and wait for a keypress.
  SB(115200);
  int msWait = millis();
  SP("\nWaiting for MOTU...");
  while(!SA() && millis() - msWait < 10000) ;
  SP("\nStarting...");

  // Initialize the WiFi.
  if(!initWifi()) {
    SPN("Failed to init Wifi");
    status = STATUS_ERR;
    return;
  }

  // Start the web server.
  httpd_init(httpRoutes, SVRPORT);

  digitalWrite(PIN_HB_LED, LED_OFF);     // Turn LED off
}

/********************************************************
   Blinking Loop
 ********************************************************/

void loop() {
  unsigned long msNow = millis();
  if(msNow - msBlink > (status == STATUS_ERR ? 200 : 1000)) {
    digitalWrite(PIN_HB_LED, !digitalRead(PIN_HB_LED));
    msBlink = millis();
  }

  if(status == STATUS_ERR) return;

  // Do other stuff here.
}

/********************************************************
   Request Handlers
 ********************************************************/

bool cgiFavicon(espconn* pEspconn, HttpRequest &httpReq, void* handlerArg) {
 SPN("\n*** cgiFavicon");
 httpd_send(pEspconn, 404);
 return true;  // Handler indicates that it has handled the request.
}

bool cgiStatic(espconn* pEspconn, HttpRequest &httpReq, void* handlerArg) {
 SPN("\n*** cgiStatic");
 httpd_send(pEspconn, 200, "text/html", "<html><body><h3>cgiStatic Worked!</h3></body></html>");
 return true;  // Handler indicates that it has handled the request.
}

bool cgiGet(espconn* pEspconn, HttpRequest &httpReq, void* handlerArg) {
  SPN("\n*** cgiGet");
  httpd_parseParams(httpReq, HTTP_QUERY);
  // Tally the size of the buffer needed for the resulting HTML page.
  unsigned int lData = 70;
  for(uint8_t i = 0; i < httpReq.argCount; i++) {
    lData += strlen(httpReq.args[i].key) + strlen(httpReq.args[i].value) + 8;
  }
  char* pData = (char*) zalloc(lData);

  // Build the response.
  strcpy(pData, "<html>\n<body>\n<h3>cgiGet Worked!</h3>\n<p>\n");
  char* ptr = pData + 42;
  for(uint8_t i = 0; i < httpReq.argCount; i++) {
    sprintf(ptr, "%s = %s<br>\n", httpReq.args[i].key, httpReq.args[i].value);
    ptr += strlen(httpReq.args[i].key) + strlen(httpReq.args[i].value) + 8;
  }
  strcpy(ptr, "</p>\n</body>\n</html>\n");

  // Send the response.
  httpd_send(pEspconn, 200, "text/html", pData);
  return true;  // Handler indicates that it has handled the request.
}

bool cgiPost(espconn* pEspconn, HttpRequest &httpReq, void* handlerArg) {
  SPN("\n*** cgiPost");
  httpd_parseParams(httpReq, HTTP_DATA);
  // Tally the size of the buffer needed for the resulting HTML page.
  unsigned int lData = 70;
  for(uint8_t i = 0; i < httpReq.argCount; i++) {
    lData += strlen(httpReq.args[i].key) + strlen(httpReq.args[i].value) + 8;
  }
  char* pData = (char*) zalloc(lData);

  // Build the response.
  strcpy(pData, "<html>\n<body>\n<h3>cgiPost Worked!</h3>\n<p>\n");
  char* ptr = pData + 43;
  for(uint8_t i = 0; i < httpReq.argCount; i++) {
    sprintf(ptr, "%s = %s<br>\n", httpReq.args[i].key, httpReq.args[i].value);
    ptr += strlen(httpReq.args[i].key) + strlen(httpReq.args[i].value) + 8;
  }
  strcpy(ptr, "</p>\n</body>\n</html>\n");

  // Send the response.
  httpd_send(pEspconn, 200, "text/html", pData);
  return true;  // Handler indicates that it has handled the request.
}

bool cgiTest(espconn* pEspconn, HttpRequest &httpReq, void* handlerArg) {
  SPN("\n*** cgiTest");
  if(strcmp(httpReq.uri, "/test") == 0) {
    httpd_send(pEspconn, 200, "text/html", "<html><body><h3>cgiTest Worked! /test</h3></body></html>");
    return true;  // Handler indicates that it has handled the request.
  }
  // A simple form of URL rewriting
  if(strcmp(httpReq.uri, "/") == 0) httpReq.uri = (char*)"/menu.htm";

  if(httpd_fileHandler(pEspconn, httpReq, NULL)) {
    return true;  // Handler indicates that it has handled the request.
  }
  return false;
}
