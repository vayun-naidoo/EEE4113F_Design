#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <SPI.h>
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

// WiFi credentials
const char* ssid = "DESKTOP-QJB68QR 3134";
const char* password = "1{9H99s4";

// Google Apps Script URL
const char* scriptURL = "https://script.google.com/macros/s/AKfycbxIj88NqBZIBS-fbqbhHV8SmKTibE933pB9KW0l4JE8wAVCAaLMIScBh6R33fsgKQOK/exec";

// UART pins (optional)
#define UART_TX 17
#define UART_RX 16

// Data
String captureTimestamps[3] = { "--", "--", "--" };
String unitNumber = "--";
SemaphoreHandle_t screenSemaphore;

void initialiseScreen() {
  Serial.println("=== Screen Init ===");
  Serial.println("Connecting WiFi...");
  Serial.println("===================");
}

void connectWifi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  configTime(3600 * 2, 0, "pool.ntp.org", "time.nist.gov");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.println("Waiting for time sync...");
    delay(1000);
  }
}

void writeScreen(const char* currentTime) {
  Serial.println();
  Serial.println("==== Display Output ====");
  Serial.println(String("Time: ") + currentTime);
  Serial.println(String("Unit: ") + unitNumber);
  Serial.println("-------------------------");
  Serial.println("Recent Captures:");
  Serial.println("1. " + captureTimestamps[2]);
  Serial.println("2. " + captureTimestamps[1]);
  Serial.println("3. " + captureTimestamps[0]);
  Serial.println("=========================");
}

void readFromGoogleSheet() {
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(scriptURL);
  int code = http.GET();

  if (code == 200) {
    String payload = http.getString();
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      Serial.println("JSON parsing failed!");
      return;
    }

  JsonArray tsArray = doc["timestamps"];
  for (int i = 0; i < 3; i++) {
    if (i < tsArray.size()) {
      captureTimestamps[i] = tsArray[i].as<String>();
    } else {
      captureTimestamps[i] = "--";
    }
  }

  unitNumber = doc["unitNumber"] | "--";


  } else {
    Serial.println("HTTP error: " + String(code));
  }
  http.end();
}

void sendToGoogleSheet(String timestamp) {
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(scriptURL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  timestamp.replace(" ", "%20");
  timestamp.replace(":", "%3A");
  String postData = "timestamp=" + timestamp;
  Serial.println("Sending to sheet: " + postData);
  int code = http.POST(postData);
  Serial.println("POST status: " + String(code));
  Serial.println("Response: " + http.getString());

  http.end();
}

void displayTask(void* param) {
  while (true) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char buffer[32];
      strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
      if (xSemaphoreTake(screenSemaphore, portMAX_DELAY)) {
        writeScreen(buffer);
        xSemaphoreGive(screenSemaphore);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void networkTask(void* param) {
  while (true) {
    readFromGoogleSheet();
    vTaskDelay(pdMS_TO_TICKS(15000));
  }
}

void setup() {
  Serial.begin(115200);
  // Serial2.begin(9600, SERIAL_8N1, UART_RX, UART_TX); // Optional

  screenSemaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(screenSemaphore);

  initialiseScreen();
  connectWifi();
  readFromGoogleSheet();

  xTaskCreatePinnedToCore(displayTask, "Display Task", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(networkTask, "Network Task", 8192, NULL, 1, NULL, 1);
  // xTaskCreatePinnedToCore(uartSenderTask, "UART Sender", 2048, NULL, 1, NULL, 1);
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'T') {
      struct tm now;
      if (getLocalTime(&now)) {
        char stamp[32];
        strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &now);
        sendToGoogleSheet(String(stamp));
      }
    }
  }
}
