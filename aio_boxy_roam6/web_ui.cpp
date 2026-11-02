#include <Arduino.h>

#include "hardware_config.h"
#include "modes.h"
#include "web_ui.h"

String uiBuildOptionsScript() {
  String script = "window.BOXY_UI_OPTIONS={singleInterface:";
  script += String(BOXY_SINGLE_INTERFACE);
  script += ",theme:'";
#if BOXY_LIGHT_THEME
  script += "theme-light";
#else
  script += "theme-dark";
#endif
  script += "',features:{motors:";
  script += String(BOXY_ENABLE_MOTORS);
  script += ",servo:";
  script += String(BOXY_ENABLE_SERVO);
  script += ",ultrasonic:";
  script += String(BOXY_ENABLE_ULTRASONIC);
  script += ",infrared:";
  script += String(BOXY_ENABLE_IR_SENSORS);
  script += ",buzzer:";
  script += String(BOXY_ENABLE_BUZZER);
  script += ",statusLed:";
  script += String(BOXY_ENABLE_STATUS_LED);
  script += "}};";
  return script;
}

void registerWebUi(AsyncWebServer &server) {
  server.on("/ui-config.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/javascript", uiBuildOptionsScript());
  });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
#if defined(ESP8266)
    request->send_P(200, "text/html", modes_html);
#else
    request->send(200, "text/html", modes_html);
#endif
  });
}
