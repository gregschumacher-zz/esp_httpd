#include "wifi.h"
#include "wifi_credentials"

/* Put credentials here, or in an include file.
Credentials credentials[] = {
	{"ssid1", "password1"},
	{"ssid2", "password2"}
};
*/

bool initWifi() {
  uint8_t i;
  int msWait, msDot;
  for (i = 0; i < sizeof(credentials) / sizeof(credentials[0]); i++) {
    SP("\nLooking for ");
    SP(credentials[i].ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(credentials[i].ssid, credentials[i].pwd);
    // Wait for connection
    msWait = msDot = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - msWait < 20000) {
      yield();
      if(millis() - msDot >=1000) {
        msDot = millis();
        SP(".");
      }
    }
    if (WiFi.status() == WL_CONNECTED) break;
  }
  if (WiFi.status() != WL_CONNECTED) {
    SPN("\nFailed to connect to any network!");
    return false;
  }
  SPFN("\nConnected to: %s", credentials[i].ssid);
  SP("IP address: ");
  SPN(WiFi.localIP());
  return true;
}
