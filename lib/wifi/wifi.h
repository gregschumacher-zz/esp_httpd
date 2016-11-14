#include <ESP8266WiFi.h>
#include <serial.print.h>

struct Credentials {
  const char *ssid;
  const char *pwd;
};

bool initWifi();
