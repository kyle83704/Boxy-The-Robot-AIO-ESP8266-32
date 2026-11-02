#include <Arduino.h>
#include <WiFi.h>

#include "camera_stream.h"

#ifndef BOXY_STA_SSID
#define BOXY_STA_SSID ""
#endif
#ifndef BOXY_STA_PASSWORD
#define BOXY_STA_PASSWORD ""
#endif
#ifndef BOXY_CAMERA_AP_SSID
#define BOXY_CAMERA_AP_SSID "Boxy-Cam"
#endif
#ifndef BOXY_CAMERA_AP_PASSWORD
#define BOXY_CAMERA_AP_PASSWORD "boxycam1234"
#endif

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  if (BOXY_STA_SSID[0] != '\0') {
    WiFi.begin(BOXY_STA_SSID, BOXY_STA_PASSWORD);
    const unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000) {
      delay(250);
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(BOXY_CAMERA_AP_SSID, BOXY_CAMERA_AP_PASSWORD)) {
      Serial.println("Camera access point startup failed");
      return;
    }
    Serial.print("Camera AP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.print("Camera Wi-Fi IP: ");
    Serial.println(WiFi.localIP());
  }

  startCameraStream();
}

void loop() {
  delay(1000);
}
