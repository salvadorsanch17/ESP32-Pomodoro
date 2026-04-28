/*
 * Pomodoro Timer with AWS API Integration
 * ESP32 + OLED Display
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "SSD1306Wire.h"

SSD1306Wire display(0x3C, 21, 22);

// WiFi credentials - CHANGE THESE
const char* ssid = "WiFi_NAME";
const char* password = "WiFi_PASSWORD";

// API endpoint - CHANGE THIS to your API Gateway URL
const char* apiEndpoint = "";

// Button pin
#define BUTTON_START 23

// Pomodoro settings
#define WORK_TIME 25 * 60
#define SHORT_BREAK 5 * 60
#define LONG_BREAK 15 * 60
#define CYCLES_BEFORE_LONG 4

// State variables
enum State { WORK, SHORT_BREAK_STATE, LONG_BREAK_STATE };
State currentState = WORK;
int timeRemaining = WORK_TIME;
int cyclesCompleted = 0;
bool isRunning = false;
unsigned long lastUpdate = 0;
unsigned long lastApiCheck = 0;
const unsigned long apiCheckInterval = 5000; // Check API every 5 seconds

// Button handling
bool buttonWasPressed = false;

// Custom message
String customMessage = "";
unsigned long messageDisplayTime = 0;
const unsigned long messageDisplayDuration = 5000; // Show message for 5 seconds

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_START, INPUT);
  
  // Initialize display
  display.init();
  display.flipScreenVertically();
  display.setContrast(255);
  
  // Show connecting message
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "Connecting to WiFi...");
  display.display();
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    
    display.clear();
    display.drawString(0, 0, "WiFi Connected!");
    display.drawString(0, 15, WiFi.localIP().toString());
    display.display();
    delay(2000);
  } else {
    Serial.println("\nFailed to connect to WiFi");
    display.clear();
    display.drawString(0, 0, "WiFi Failed");
    display.drawString(0, 15, "Offline Mode");
    display.display();
    delay(2000);
  }
  
  updateDisplay();
  Serial.println("Pomodoro Timer Started");
}

void loop() {
  handleButton();
  checkApiCommands();
  
  if (isRunning) {
    unsigned long currentMillis = millis();
    
    if (currentMillis - lastUpdate >= 1000) {
      lastUpdate = currentMillis;
      timeRemaining--;
      
      if (timeRemaining <= 0) {
        handleTimerComplete();
      }
      
      // Send status update every 10 seconds
      if (timeRemaining % 10 == 0) {
        sendStatusUpdate();
      }
      
      updateDisplay();
    }
  }
  
  // Check if custom message display time is over
  if (customMessage != "" && millis() - messageDisplayTime > messageDisplayDuration) {
    customMessage = "";
    updateDisplay();
  }
}

void handleButton() {
  int reading = digitalRead(BUTTON_START);
  
  if (reading == HIGH && !buttonWasPressed) {
    delay(300);
    
    if (digitalRead(BUTTON_START) == HIGH) {
      isRunning = !isRunning;
      
      Serial.print("Button pressed! isRunning: ");
      Serial.println(isRunning);
      
      if (isRunning) {
        lastUpdate = millis();
      }
      
      sendStatusUpdate();
      updateDisplay();
      
      buttonWasPressed = true;
    }
  }
  
  if (reading == LOW) {
    buttonWasPressed = false;
  }
}

void checkApiCommands() {
  // Only check if WiFi is connected and enough time has passed
  if (WiFi.status() != WL_CONNECTED) return;
  if (millis() - lastApiCheck < apiCheckInterval) return;
  
  lastApiCheck = millis();
  
  HTTPClient http;
  String url = String(apiEndpoint) + "/command";
  
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("API Response: " + payload);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    
    JsonArray commands = doc["commands"];
    
    for (JsonObject cmd : commands) {
      String type = cmd["type"].as<String>();
      
      if (type == "control") {
        String action = cmd["action"].as<String>();
        handleRemoteControl(action);
      } else if (type == "message") {
        String message = cmd["text"].as<String>();
        showCustomMessage(message);
      }
    }
  } else if (httpCode > 0) {
    Serial.print("API Error: ");
    Serial.println(httpCode);
  }
  
  http.end();
}

void handleRemoteControl(String action) {
  Serial.println("Remote control: " + action);
  
  if (action == "start") {
    if (!isRunning) {
      isRunning = true;
      lastUpdate = millis();
    }
  } else if (action == "pause") {
    isRunning = false;
  } else if (action == "reset") {
    isRunning = false;
    cyclesCompleted = 0;
    currentState = WORK;
    timeRemaining = WORK_TIME;
  }
  
  updateDisplay();
  sendStatusUpdate();
}

void showCustomMessage(String message) {
  customMessage = message;
  messageDisplayTime = millis();
  updateDisplay();
}

void sendStatusUpdate() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  HTTPClient http;
  String url = String(apiEndpoint) + "/status";
  
  DynamicJsonDocument doc(512);
  doc["id"] = "device_status";
  doc["isRunning"] = isRunning;
  doc["timeRemaining"] = timeRemaining;
  doc["currentState"] = currentState == WORK ? "work" : 
                        (currentState == SHORT_BREAK_STATE ? "short_break" : "long_break");
  doc["cyclesCompleted"] = cyclesCompleted;
  doc["timestamp"] = millis();
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(jsonString);
  
  if (httpCode > 0) {
    Serial.println("Status update sent");
  }
  
  http.end();
}

void handleTimerComplete() {
  isRunning = false;
  
  if (currentState == WORK) {
    cyclesCompleted++;
    
    if (cyclesCompleted >= CYCLES_BEFORE_LONG) {
      currentState = LONG_BREAK_STATE;
      timeRemaining = LONG_BREAK;
      cyclesCompleted = 0;
    } else {
      currentState = SHORT_BREAK_STATE;
      timeRemaining = SHORT_BREAK;
    }
  } else {
    currentState = WORK;
    timeRemaining = WORK_TIME;
  }
  
  sendStatusUpdate();
  updateDisplay();
}

void updateDisplay() {
  display.clear();
  
  // Show custom message if present
  if (customMessage != "") {
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, "Message:");
    display.setFont(ArialMT_Plain_16);
    
    // Word wrap the message
    int lineHeight = 18;
    int y = 15;
    int maxWidth = 128;
    
    String remaining = customMessage;
    while (remaining.length() > 0 && y < 60) {
      int splitPos = remaining.length();
      
      // Find good split point
      for (int i = 0; i < remaining.length(); i++) {
        if (display.getStringWidth(remaining.substring(0, i)) > maxWidth - 5) {
          // Find last space before this point
          int lastSpace = remaining.lastIndexOf(' ', i);
          if (lastSpace > 0) {
            splitPos = lastSpace;
          } else {
            splitPos = i;
          }
          break;
        }
      }
      
      String line = remaining.substring(0, splitPos);
      display.drawString(0, y, line);
      
      remaining = remaining.substring(splitPos);
      remaining.trim();
      y += lineHeight;
    }
    
    display.display();
    return;
  }
  
  // Normal timer display
  display.setFont(ArialMT_Plain_16);
  if (currentState == WORK) {
    display.drawString(0, 0, "WORK TIME");
  } else if (currentState == SHORT_BREAK_STATE) {
    display.drawString(0, 0, "SHORT BREAK");
  } else if (currentState == LONG_BREAK_STATE) {
    display.drawString(0, 0, "LONG BREAK");
  }
  
  int minutes = timeRemaining / 60;
  int seconds = timeRemaining % 60;
  
  display.setFont(ArialMT_Plain_24);
  String timeStr = "";
  if (minutes < 10) timeStr += "0";
  timeStr += String(minutes);
  timeStr += ":";
  if (seconds < 10) timeStr += "0";
  timeStr += String(seconds);
  
  display.drawString(10, 25, timeStr);
  
  display.setFont(ArialMT_Plain_10);
  if (isRunning) {
    display.drawString(0, 52, "Running...");
  } else {
    display.drawString(0, 52, "Paused");
  }
  
  String cycleStr = "Cycle: " + String(cyclesCompleted);
  display.drawString(70, 52, cycleStr);
  
  // WiFi indicator
  if (WiFi.status() == WL_CONNECTED) {
    display.drawString(110, 0, "WiFi");
  }
  
  display.display();
}
