#ifndef ESP_HTTPD_TEST_H
#define ESP_HTTPD_TEST_H

#define LED_OFF     1
#define LED_ON      0

// Heartbeat LED
// #define PIN_HB_LED  LED_BUILTIN
#define PIN_HB_LED  2

// #define NO_PRINT

#include <Arduino.h>
#include <serial.print.h>
#include <wifi.h>
#include <esp_httpd.h>

enum STATUSES { STATUS_OK = 0, STATUS_ERR };

/********************************************************
  Function Prototypes
 ********************************************************/

// Request handlers
bool cgiFavicon(espconn* pEspconn, HttpRequest &httpReq, void* handlerArg);
bool cgiStatic(espconn* pEspconn, HttpRequest &httpReq, void* handlerArg);
bool cgiGet(espconn* pEspconn, HttpRequest &httpReq, void* handlerArg);
bool cgiPost(espconn* pEspconn, HttpRequest &httpReq, void* handlerArg);
bool cgiTest(espconn* pEspconn, HttpRequest &httpReq, void* handlerArg);

#endif
