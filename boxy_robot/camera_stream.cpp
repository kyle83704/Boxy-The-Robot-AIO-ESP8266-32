#include "hardware_config.h"

#if defined(ESP32) && BOXY_ENABLE_CAMERA
#include <Arduino.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include <stdio.h>
#include <string.h>

namespace {

#define CAMERA_PART_BOUNDARY "boxycamframe"

const char *STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" CAMERA_PART_BOUNDARY;
const char *STREAM_BOUNDARY = "\r\n--" CAMERA_PART_BOUNDARY "\r\n";
const char *STREAM_PART =
    "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

esp_err_t streamHandler(httpd_req_t *request) {
  esp_err_t result = httpd_resp_set_type(request, STREAM_CONTENT_TYPE);
  if (result != ESP_OK) {
    return result;
  }
  httpd_resp_set_hdr(request, "Access-Control-Allow-Origin", "*");

  char partHeader[64];
  while (true) {
    camera_fb_t *frame = esp_camera_fb_get();
    if (frame == nullptr) {
      Serial.println("Camera capture failed");
      return ESP_FAIL;
    }

    result = httpd_resp_send_chunk(request, STREAM_BOUNDARY,
                                   strlen(STREAM_BOUNDARY));
    if (result == ESP_OK) {
      const int headerLength =
          snprintf(partHeader, sizeof(partHeader), STREAM_PART,
                   static_cast<unsigned int>(frame->len));
      result = httpd_resp_send_chunk(request, partHeader, headerLength);
    }
    if (result == ESP_OK) {
      result = httpd_resp_send_chunk(request,
                                     reinterpret_cast<const char *>(frame->buf),
                                     frame->len);
    }
    esp_camera_fb_return(frame);
    if (result != ESP_OK) {
      break;
    }
  }

  httpd_resp_send_chunk(request, nullptr, 0);
  return result;
}

bool initializeCamera() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sccb_sda = 26;
  config.pin_sccb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 16;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  }

  const esp_err_t error = esp_camera_init(&config);
  if (error != ESP_OK) {
    Serial.printf("Camera initialization failed: 0x%x\n", error);
    return false;
  }
  return true;
}

}  // namespace

bool startCameraStream() {
  if (!initializeCamera()) {
    return false;
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 81;
  httpd_handle_t server = nullptr;
  const esp_err_t startError = httpd_start(&server, &config);
  if (startError != ESP_OK) {
    Serial.printf("Camera HTTP server failed: 0x%x\n", startError);
    return false;
  }

  httpd_uri_t streamUri = {};
  streamUri.uri = "/stream";
  streamUri.method = HTTP_GET;
  streamUri.handler = streamHandler;
  streamUri.user_ctx = nullptr;
  const esp_err_t registerError = httpd_register_uri_handler(server, &streamUri);
  if (registerError != ESP_OK) {
    Serial.printf("Camera stream route failed: 0x%x\n", registerError);
    httpd_stop(server);
    return false;
  }

  Serial.println("Camera stream available at http://<camera-ip>:81/stream");
  return true;
}
#else
bool startCameraStream() {
  return false;
}
#endif
