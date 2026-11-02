#include <Arduino.h>
#include "hardware_config.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESP8266mDNS.h>
#if BOXY_ENABLE_SERVO
#include <Servo.h>
#endif
#else
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPmDNS.h>
#if BOXY_ENABLE_SERVO
#include <ESP32Servo.h>
#endif
#endif
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <queue>

#if defined(ESP8266)
#define DEBUG_PRINT(...) ((void)0)
#define DEBUG_PRINTLN(...) ((void)0)
#else
#define DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
#define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#endif

#include "music_player.h"
#include "web_ui.h"
#if BOXY_ENABLE_CAMERA
#include "camera_stream.h"
#endif

// === WiFi Configuration ===
#ifndef BOXY_STA_SSID
#define BOXY_STA_SSID ""
#endif
#ifndef BOXY_STA_PASSWORD
#define BOXY_STA_PASSWORD ""
#endif
const char *STA_SSID = BOXY_STA_SSID;
const char *STA_PASSWORD = BOXY_STA_PASSWORD;

const char *AP_SSID = "Boxy-AP";
const char *AP_PASSWORD = "";

const char *hostname = "geff"; // host name will how you find your robor on your network "hostname.local"

#if BOXY_ENABLE_NEOPIXEL
Adafruit_NeoPixel pixels(NUM_NEOPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
#endif

// === Globals ===
AsyncWebServer server(80);
AsyncWebSocket webSocket("/ws");
#if BOXY_ENABLE_SERVO
Servo myServo;
#endif

int maxSpeed = 255;
int hornFreq = 800;
bool isHornOn = false;
bool ledState = false;
int currentsweepAngle = 90;

// === Sensor Globals ===
unsigned long lastSensorReadTime = 0;
const unsigned long sensorReadInterval = 200;

// --- Byline Globals ---
std::queue<String> byLineCommandQueue;
enum ByLineCommandState {
  BYLINE_IDLE,
  BYLINE_EXECUTING_COMMAND
};
ByLineCommandState byLineCurrentCommandState = BYLINE_IDLE;
unsigned long currentByLineCommandEndTime = 0;

// --- Roam Mode Globals ---
int roamSpeed = 150;
int sweepAngle = 90;
int sweepDirection = 1;

const int SWEEP_UPDATE_INTERVAL = 10;
const int SWEEP_MIN_ANGLE = 20;
const int SWEEP_MAX_ANGLE = 160;
const int OBSTACLE_THRESHOLD = 30; // in CM
const int CLOSE_OBSTACLE_THRESHOLD = 15; // in CM

unsigned long lastActionTime = 0;
const unsigned long REVERSE_DURATION = 500;
const unsigned long TURN_DURATION = 500;
const unsigned long ULTRASONIC_CHECK_DURATION = 250;
unsigned long wanderTimer = 0;
const unsigned long MIN_WANDER_DURATION = 2000;
const unsigned long MAX_WANDER_DURATION = 5000;

enum RoamState {
  ROAM_FORWARD,
  ROAM_REVERSING,
  ROAM_CHECKING_RIGHT,
  ROAM_CHECKING_LEFT,
  ROAM_TURNING
};
RoamState roamState = ROAM_FORWARD;
long rightDistance = 0;
long leftDistance = 0;
unsigned long roamStateStartTime = 0;

enum RobotMode {
  MODE_JOYSTICK = 0,
  MODE_DUAL_JOYSTICK = 1,
  MODE_ROAM = 2,
  MODE_BYLINE = 3
};
int currentMode = MODE_JOYSTICK;

// === Neopixel Color Definitions ===
uint32_t joystickColor1 = 0xFF0000;
uint32_t joystickColor2 = 0x00FF00;
uint32_t dualJoystickColor1 = 0x0000FF;
uint32_t dualJoystickColor2 = 0xFFFF00;
uint32_t roamColor1 = 0x800080;
uint32_t roamColor2 = 0xFFA500;
uint32_t bylineColor1 = 0xFFFFFF;
uint32_t bylineColor2 = 0x000000;

// === Function Declarations (Prototypes) ===
void setMotor(int pinA, int pinB, int pwm);
void stopMotors();
void driveMotors(int joyX, int joyY);
void playQuickTone(int duration, int frequency);
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void setupWiFi();
long readUltrasonicDistance();
void sendSensorData();
void roamMoveForward();
void roamTurnRight();
void roamTurnLeft();
void roamReverse();
void executeByLineCommand(String command);
void processByLineCommandQueue();
void sendByLineErrorMessage(const String &message);
void byLineMoveForward(int duration);
void byLineMoveBackward(int duration);
void byLineTurnLeft(int duration);
void byLineTurnRight(int duration);
void setNeopixelModeColors();

// === Motor Control Function ===
void setMotor(int pinA, int pinB, int pwm) {
#if BOXY_ENABLE_MOTORS
  pwm = constrain(pwm, -maxSpeed, maxSpeed);

  if (pwm > 0) {
    analogWrite(pinA, pwm);
    analogWrite(pinB, 0);
  } else if (pwm < 0) {
    analogWrite(pinA, 0);
    analogWrite(pinB, -pwm);
  } else {
    analogWrite(pinA, 0);
    analogWrite(pinB, 0);
  }
#else
  (void)pinA;
  (void)pinB;
  (void)pwm;
#endif
}

void stopMotors() {
  setMotor(LEFTA, LEFTB, 0);
  setMotor(RIGHTA, RIGHTB, 0);
  byLineCurrentCommandState = BYLINE_IDLE;
  currentByLineCommandEndTime = 0;
  boxyNoTone();
}

void driveMotors(int joyX, int joyY) {
  int targetX = map(joyX, -100, 100, -maxSpeed, maxSpeed);
  int targetY = map(joyY, -100, 100, -maxSpeed, maxSpeed);

  int leftMotorSpeed = targetY + targetX;
  int rightMotorSpeed = targetY - targetX;

  leftMotorSpeed = constrain(leftMotorSpeed, -maxSpeed, maxSpeed);
  rightMotorSpeed = constrain(rightMotorSpeed, -maxSpeed, maxSpeed);

  if (joyX == 0 && joyY == 0) {
    stopMotors();
    return;
  }

  setMotor(LEFTA, LEFTB, leftMotorSpeed);
  setMotor(RIGHTA, RIGHTB, rightMotorSpeed);
}

void playQuickTone(int duration, int frequency) {
  boxyTone(frequency);
  delay(duration);
  boxyNoTone();
}

void setServoAngle(int angle) {
  currentsweepAngle = angle;
#if BOXY_ENABLE_SERVO
  myServo.write(angle);
#endif
}

// Neopixel Control Function
void setNeopixelModeColors() {
#if BOXY_ENABLE_NEOPIXEL
  uint32_t color1, color2;
  switch (currentMode) {
    case MODE_JOYSTICK:
      color1 = joystickColor1;
      color2 = joystickColor2;
      break;
    case MODE_DUAL_JOYSTICK:
      color1 = dualJoystickColor1;
      color2 = dualJoystickColor2;
      break;
    case MODE_ROAM:
      color1 = roamColor1;
      color2 = roamColor2;
      break;
    case MODE_BYLINE:
      color1 = bylineColor1;
      color2 = bylineColor2;
      break;
    default:
      color1 = 0;
      color2 = 0;
      break;
  }
  
  for (int i = 0; i < NUM_NEOPIXELS; i++) {
    if (i % 2 == 0) {
      pixels.setPixelColor(i, color1);
    } else {
      pixels.setPixelColor(i, color2);
    }
  }
  pixels.show();
#endif
}

// Roam Mode Movement Functions
void roamMoveForward() {
  setMotor(LEFTA, LEFTB, roamSpeed);
  setMotor(RIGHTA, RIGHTB, roamSpeed);
}

void roamTurnRight() {
  setMotor(LEFTA, LEFTB, roamSpeed);
  setMotor(RIGHTA, RIGHTB, -roamSpeed);
}

void roamTurnLeft() {
  setMotor(LEFTA, LEFTB, -roamSpeed);
  setMotor(RIGHTA, RIGHTB, roamSpeed);
}

void roamReverse() {
  setMotor(LEFTA, LEFTB, -roamSpeed);
  setMotor(RIGHTA, RIGHTB, -roamSpeed);
}

// ByLine Mode Functions
void sendByLineErrorMessage(const String &message) {
  webSocket.textAll("ERROR: " + message);
}

void byLineMoveForward(int duration) {
  setMotor(LEFTA, LEFTB, maxSpeed);
  setMotor(RIGHTA, RIGHTB, maxSpeed);
  byLineCurrentCommandState = BYLINE_EXECUTING_COMMAND;
  currentByLineCommandEndTime = millis() + duration;
}

void byLineMoveBackward(int duration) {
  setMotor(LEFTA, LEFTB, -maxSpeed);
  setMotor(RIGHTA, RIGHTB, -maxSpeed);
  byLineCurrentCommandState = BYLINE_EXECUTING_COMMAND;
  currentByLineCommandEndTime = millis() + duration;
}

void byLineTurnLeft(int duration) {
  setMotor(LEFTA, LEFTB, -maxSpeed);
  setMotor(RIGHTA, RIGHTB, maxSpeed);
  byLineCurrentCommandState = BYLINE_EXECUTING_COMMAND;
  currentByLineCommandEndTime = millis() + duration;
}

void byLineTurnRight(int duration) {
  setMotor(LEFTA, LEFTB, maxSpeed);
  setMotor(RIGHTA, RIGHTB, -maxSpeed);
  byLineCurrentCommandState = BYLINE_EXECUTING_COMMAND;
  currentByLineCommandEndTime = millis() + duration;
}

void executeByLineCommand(String command) {
  DEBUG_PRINT("Executing ByLine command: ");
  DEBUG_PRINTLN(command);

  int duration = 0;
  String baseCommand;

  int commaIndex = command.indexOf(',');
  if (commaIndex != -1) {
    baseCommand = command.substring(0, commaIndex);
    String durationString = command.substring(commaIndex + 1);
    duration = durationString.toInt();
    DEBUG_PRINT("Parsed baseCommand: ");
    DEBUG_PRINTLN(baseCommand);
    DEBUG_PRINT("Parsed duration: ");
    DEBUG_PRINTLN(duration);
  } else {
    baseCommand = command;
    duration = 0;
    DEBUG_PRINT("Parsed baseCommand: ");
    DEBUG_PRINTLN(baseCommand);
  }

  // Your original code had this stop. I've re-added it here
  // to ensure a clean transition between commands.
  stopMotors(); 
  byLineCurrentCommandState = BYLINE_IDLE; 

  baseCommand.toLowerCase();

  if (baseCommand == "f") {
    if (duration == 0) duration = 500;
    byLineMoveForward(duration);
  } else if (baseCommand == "b") {
    if (duration == 0) duration = 500;
    byLineMoveBackward(duration);
  } else if (baseCommand == "l") {
    if (duration == 0) duration = 300;
    byLineTurnLeft(duration);
  } else if (baseCommand == "r") {
    if (duration == 0) duration = 300;
    byLineTurnRight(duration);
  } else if (baseCommand == "stop") {
    stopMotors();
    byLineCurrentCommandState = BYLINE_IDLE;
  } else if (baseCommand == "p") {
    int newPwm = constrain(duration, 0, 255);
    maxSpeed = newPwm;
    JsonDocument speedDoc;
    speedDoc["maxSpeed"] = maxSpeed;
    String speedJson;
    serializeJson(speedDoc, speedJson);
    webSocket.textAll(speedJson);
    byLineCurrentCommandState = BYLINE_IDLE;
  } else if (baseCommand == "s") {
    int servoPos = constrain(duration, 0, 180);
    setServoAngle(servoPos);
    currentsweepAngle = servoPos;
    JsonDocument servoDoc;
    servoDoc["servoAngle"] = currentsweepAngle;
    String servoJson;
    serializeJson(servoDoc, servoJson);
    webSocket.textAll(servoJson);
    byLineCurrentCommandState = BYLINE_IDLE;
  } else {
    sendByLineErrorMessage("Unknown command: " + command);
    byLineCurrentCommandState = BYLINE_IDLE;
  }
}

void processByLineCommandQueue() {
  if (byLineCurrentCommandState == BYLINE_EXECUTING_COMMAND) {
    if (millis() >= currentByLineCommandEndTime) {
      stopMotors();
      byLineCurrentCommandState = BYLINE_IDLE;
      DEBUG_PRINTLN("ByLine command completed.");
    }
  }

  if (byLineCurrentCommandState == BYLINE_IDLE && !byLineCommandQueue.empty()) {
    String nextCommand = byLineCommandQueue.front();
    byLineCommandQueue.pop();
    executeByLineCommand(nextCommand);
  }
}

long readUltrasonicDistance() {
#if BOXY_ENABLE_ULTRASONIC
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long duration = pulseIn(echo, HIGH, ULTRASONIC_TIMEOUT);
  if (duration == 0) {
    return -1;
  }
  return duration * 0.034 / 2;
#else
  return -1;
#endif
}

void sendSensorData() {
  if (millis() - lastSensorReadTime >= sensorReadInterval) {
    lastSensorReadTime = millis();
    JsonDocument sensorDoc;
    sensorDoc["ultrasonicCM"] = readUltrasonicDistance();
#if BOXY_ENABLE_IR_SENSORS
    sensorDoc["irL"] = digitalRead(lir);
    sensorDoc["irR"] = digitalRead(rir);
#else
    sensorDoc["irL"] = -1;
    sensorDoc["irR"] = -1;
#endif
    sensorDoc["ledState"] = ledState;
    String sensorJson;
    serializeJson(sensorDoc, sensorJson);
    webSocket.textAll(sensorJson);
  }
}

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
            AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    JsonDocument doc;
    doc["ledState"] = ledState;
    doc["maxSpeed"] = maxSpeed;
    doc["servoAngle"] = currentsweepAngle;
    doc["hornFrequency"] = hornFreq;
    doc["currentMode"] = currentMode;
    String jsonString;
    serializeJson(doc, jsonString);
    webSocket.text(client->id(), jsonString);
  } else if (type == WS_EVT_DISCONNECT) {
    stopMotors();
    boxyNoTone();
    isHornOn = false;
    resetMusic();
    while (!byLineCommandQueue.empty()) {
      byLineCommandQueue.pop();
    }
    byLineCurrentCommandState = BYLINE_IDLE;
    if (currentMode == MODE_ROAM) {
      currentMode = MODE_JOYSTICK;
      setNeopixelModeColors(); // Update colors on disconnect
    }
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      String msg = String((char *)data);
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, msg);
      
      // Handle start and stop messages for all modes.
      if (msg.startsWith("start roam")) {
        currentMode = MODE_ROAM;
        setNeopixelModeColors(); // Update colors
        stopMotors();
        resetMusic();
        roamState = ROAM_FORWARD;
        roamStateStartTime = millis();
        wanderTimer = random(MIN_WANDER_DURATION, MAX_WANDER_DURATION);
        JsonDocument modeDoc;
        modeDoc["currentMode"] = currentMode;
        String modeJson;
        serializeJson(modeDoc, modeJson);
        webSocket.textAll(modeJson);
        return;
      } else if (msg.startsWith("stop roam")) {
        currentMode = MODE_JOYSTICK;
        setNeopixelModeColors(); // Update colors
        stopMotors();
        boxyNoTone();
        isHornOn = false;
        resetMusic();
        roamState = ROAM_FORWARD;
        roamStateStartTime = millis();
        JsonDocument modeDoc;
        modeDoc["currentMode"] = currentMode;
        String modeJson;
        serializeJson(modeDoc, modeJson);
        webSocket.textAll(modeJson);
        return;
      } else if (msg.startsWith("stop byline")) {
        currentMode = MODE_JOYSTICK;
        setNeopixelModeColors(); // Update colors
        stopMotors();
        while (!byLineCommandQueue.empty()) {
          byLineCommandQueue.pop();
        }
        byLineCurrentCommandState = BYLINE_IDLE;
        JsonDocument modeDoc;
        modeDoc["currentMode"] = currentMode;
        String modeJson;
        serializeJson(modeDoc, modeJson);
        webSocket.textAll(modeJson);
        return;
      }

      if (error) {
        return;
      }
      String command = doc["command"] | "";

      // This logic now implicitly handles the mode switch
      // when a byLine command is received.
      if (command == "byLineCmd") {
        currentMode = MODE_BYLINE;
        setNeopixelModeColors(); // Update colors
        String action = doc["action"] | "";
        if (action == "custom") {
          String customCmds = doc["value"] | "";
          int startIndex = 0;
          int endIndex = -1;
          String cmd;
          while (startIndex < static_cast<int>(customCmds.length())) {
            endIndex = customCmds.indexOf(';', startIndex);
            if (endIndex == -1) {
              endIndex = customCmds.indexOf(',', startIndex);
            }
            if (endIndex == -1) {
              cmd = customCmds.substring(startIndex);
              startIndex = customCmds.length();
            } else {
              cmd = customCmds.substring(startIndex, endIndex);
              startIndex = endIndex + 1;
            }
            cmd.trim();
            if (cmd.length() > 0) {
              byLineCommandQueue.push(cmd);
            }
          }
        } else {
          String queuedCmd = action;
          if (doc["value"].is<int>()) {
            queuedCmd += "," + String(doc["value"].as<int>());
          }
          byLineCommandQueue.push(queuedCmd);
        }
      } else if (command == "maxSpeed") {
        int uiMaxSpeed = doc["value"].as<int>();
        maxSpeed = constrain(uiMaxSpeed, 0, 255);
        JsonDocument speedDoc;
        speedDoc["maxSpeed"] = maxSpeed;
        String speedJson;
        serializeJson(speedDoc, speedJson);
        webSocket.textAll(speedJson);
      } else if (command == "motor") {
        currentMode = MODE_JOYSTICK;
        setNeopixelModeColors(); // Update colors
        int joyX = doc["joyX"].as<String>().toInt();
        int joyY = doc["joyY"].as<String>().toInt();
        driveMotors(joyX, joyY);
      } else if (command == "dualMotor") {
        currentMode = MODE_DUAL_JOYSTICK;
        setNeopixelModeColors(); // Update colors
        int joy1Y = doc["joy1Y"].as<String>().toInt();
        int joy2Y = doc["joy2Y"].as<String>().toInt();
        int leftMotorSpeed = map(joy1Y, -100, 100, -maxSpeed, maxSpeed);
        int rightMotorSpeed = map(joy2Y, -100, 100, -maxSpeed, maxSpeed);
        setMotor(LEFTA, LEFTB, leftMotorSpeed);
        setMotor(RIGHTA, RIGHTB, rightMotorSpeed);
        if (joy1Y == 0 && joy2Y == 0) {
          stopMotors();
        }
      } else if (command == "toggleLED") {
        ledState = !ledState;
#if BOXY_ENABLE_STATUS_LED
        if (LED_PIN >= 0) {
          digitalWrite(LED_PIN, ledState);
        }
#endif
      } else if (command == "horn") {
        String state = doc["state"] | "";
        hornFreq = doc["frequency"] | 800;
        if (state == "on") {
          boxyTone(hornFreq);
          isHornOn = true;
        } else {
          boxyNoTone();
          isHornOn = false;
        }
      } else if (command == "servo") {
        int angle = constrain(doc["angle"].as<int>(), 0, 180);
        setServoAngle(angle);
      } else if (command == "get_state") {
        JsonDocument stateDoc;
        stateDoc["ledState"] = ledState;
        stateDoc["maxSpeed"] = maxSpeed;
        stateDoc["servoAngle"] = currentsweepAngle;
        stateDoc["hornFrequency"] = hornFreq;
        stateDoc["currentMode"] = currentMode;
        String stateJson;
        serializeJson(stateDoc, stateJson);
        webSocket.text(client->id(), stateJson);
      }
    }
  }
}

void setupWiFi() {
  if (STA_SSID[0] != '\0') {
    WiFi.begin(STA_SSID, STA_PASSWORD);
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
      delay(500);
    }
  }
  if (WiFi.status() == WL_CONNECTED) {
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
  }
}

void setup() {
#if !defined(ESP8266)
  Serial.begin(115200);
#else
  analogWriteRange(255);
#endif

  // Initialize Neopixels
#if BOXY_ENABLE_NEOPIXEL
  pixels.begin();
  pixels.clear();
  pixels.show();
#endif
  setNeopixelModeColors();

#if BOXY_ENABLE_STATUS_LED
  if (LED_PIN >= 0) {
    pinMode(LED_PIN, OUTPUT);
  }
#endif
#if BOXY_ENABLE_BUZZER
  pinMode(BUZZER_PIN, OUTPUT);
#endif
#if BOXY_ENABLE_MOTORS
  pinMode(LEFTA, OUTPUT);
  pinMode(LEFTB, OUTPUT);
  pinMode(RIGHTA, OUTPUT);
  pinMode(RIGHTB, OUTPUT);
#endif
#if BOXY_ENABLE_IR_SENSORS
  pinMode(lir, INPUT_PULLUP);
  pinMode(rir, INPUT_PULLUP);
#endif
#if BOXY_ENABLE_ULTRASONIC
  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);
#endif

#if BOXY_ENABLE_STATUS_LED
  if (LED_PIN >= 0) {
    digitalWrite(LED_PIN, LOW);
  }
#endif
  stopMotors();
  boxyNoTone();
#if BOXY_ENABLE_ULTRASONIC
  digitalWrite(trig, LOW);
#endif

#if BOXY_ENABLE_SERVO
  myServo.attach(SERVO_PIN, 500, 2500);
  myServo.write(currentsweepAngle);
#endif

  setupWiFi();

#if BOXY_ENABLE_CAMERA
  if (!startCameraStream()) {
    DEBUG_PRINTLN("Camera stream is unavailable");
  }
#endif

  if (MDNS.begin(hostname)) {
    DEBUG_PRINTLN("mDNS responder started");
  } else {
    DEBUG_PRINTLN("Error setting up mDNS");
  }

  registerWebUi(server);

  webSocket.onEvent(onWebSocketEvent);
  server.addHandler(&webSocket);
  server.begin();

  randomSeed(analogRead(0));
}

// === Main Loop ===
void loop() {
  webSocket.cleanupClients();
  sendSensorData();
  handleMusic(currentMode == MODE_ROAM && roamState == ROAM_FORWARD, isHornOn);

  if (currentMode == MODE_JOYSTICK || currentMode == MODE_DUAL_JOYSTICK) {
    // Motor control handled by WebSocket events
  } else if (currentMode == MODE_BYLINE) {
    processByLineCommandQueue();
  } else if (currentMode == MODE_ROAM) {
    // Check for a very close obstacle first.
    bool closeObstacleDetected = false;
#if BOXY_ENABLE_IR_SENSORS
    closeObstacleDetected = digitalRead(lir) == HIGH || digitalRead(rir) == HIGH;
#endif
    if (closeObstacleDetected) {
      if (roamState != ROAM_REVERSING) {
        stopMotors();
        roamReverse();
        roamStateStartTime = millis();
        roamState = ROAM_REVERSING;
      }
    } else {
      // --- Normal Roam State Machine ---
      switch (roamState) {
        case ROAM_FORWARD: {
          if (millis() - roamStateStartTime >= wanderTimer) {
            stopMotors();
            if (random(2) == 0) {
              roamTurnLeft();
            } else {
              roamTurnRight();
            }
            roamStateStartTime = millis();
            roamState = ROAM_TURNING;
            break;
          }
          roamMoveForward();
          if (millis() - lastActionTime >= SWEEP_UPDATE_INTERVAL) {
            lastActionTime = millis();
            setServoAngle(sweepAngle);
            long distance = readUltrasonicDistance();
            if (distance > 0 && distance < OBSTACLE_THRESHOLD && currentsweepAngle > 70 && currentsweepAngle < 110) {
              stopMotors();
              roamReverse();
              roamStateStartTime = millis();
              roamState = ROAM_REVERSING;
              break;
            }
            sweepAngle += sweepDirection;
            if (sweepAngle >= SWEEP_MAX_ANGLE || sweepAngle <= SWEEP_MIN_ANGLE) {
              sweepDirection *= -1;
            }
          }
          break;
        }

        case ROAM_REVERSING: {
          if (millis() - roamStateStartTime >= REVERSE_DURATION) {
            stopMotors();
            setServoAngle(SWEEP_MAX_ANGLE);
            roamStateStartTime = millis();
            roamState = ROAM_CHECKING_RIGHT;
          }
          break;
        }

        case ROAM_CHECKING_RIGHT: {
          if (millis() - roamStateStartTime >= ULTRASONIC_CHECK_DURATION) {
            rightDistance = readUltrasonicDistance();
            setServoAngle(SWEEP_MIN_ANGLE);
            roamStateStartTime = millis();
            roamState = ROAM_CHECKING_LEFT;
          }
          break;
        }

        case ROAM_CHECKING_LEFT: {
          if (millis() - roamStateStartTime >= ULTRASONIC_CHECK_DURATION) {
            leftDistance = readUltrasonicDistance();
            stopMotors();
            if (rightDistance > leftDistance) {
              roamTurnRight();
            } else {
              roamTurnLeft();
            }
            roamStateStartTime = millis();
            roamState = ROAM_TURNING;
          }
          break;
        }

        case ROAM_TURNING: {
          // Fixed the missing state transition here.
          if (millis() - roamStateStartTime >= TURN_DURATION) {
            stopMotors();
            roamState = ROAM_FORWARD; 
            setServoAngle(90);
            roamStateStartTime = millis();
            wanderTimer = random(MIN_WANDER_DURATION, MAX_WANDER_DURATION);
          }
          break;
        }
      }
    }
  }
}