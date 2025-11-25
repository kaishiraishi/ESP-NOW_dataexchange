#include <WiFi.h>
#include <ArduinoOTA.h>
#include "OTA_Handler.h"

const char* ssid = "IA4-411";
const char* password = "gEdCx5Rdm9J9WNAJ7xN7";

void setupOTA() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Fail. Rebooting...");
    delay(3000);
    ESP.restart();
  }

  ArduinoOTA.setPassword("0000");
  ArduinoOTA.setHostname("WifiOTA_NWstudio");
  ArduinoOTA.begin();
  Serial.println("OTA Ready");
}

void handleOTA() {
  ArduinoOTA.handle();
}
