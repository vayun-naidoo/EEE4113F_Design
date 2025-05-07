#include <U8g2lib.h>
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
const char* scriptURL = "https://script.google.com/macros/s/AKfycbwil1MuIU0Xj2V5rJBMQx64NSohBkdznGxqrt04VPp9lV2vf7ML4d5337gh3deFFrSj/exec";  // your full URL

// Display Pins
#define D0 18
#define D1 23
#define RES 27
#define DC 26
#define CS 5

// Display object
U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI u8g2(U8G2_R0, CS, DC, RES);

// Data
String captureTimestamps[3] = {"--", "--", "--"};
SemaphoreHandle_t screenSemaphore;

void initialiseScreen() {
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(0, 12, "Connecting WiFi...");
  u8g2.sendBuffer();
}

void connectWifi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected!");

  // Confirm WiFi connection
  if (xSemaphoreTake(screenSemaphore, portMAX_DELAY)) {
    u8g2.clearBuffer();
    u8g2.drawStr(0, 12, "WiFi Connected!");
    u8g2.sendBuffer();
    xSemaphoreGive(screenSemaphore);
  }

  // Sync time from NTP (UTC+2)
  configTime(3600 * 2, 0, "pool.ntp.org", "time.nist.gov");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.println("Waiting for time...");
    delay(1000);
  }
}

void writeScreen(const char* currentTime) {
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(0, 12, currentTime);
  u8g2.drawStr(0, 20, "Unit: NDXVAY001");
  u8g2.drawFrame(0, 23, 100, 1);
  u8g2.drawFrame(100, 0, 1, 24);

  char line1[32], line2[32], line3[32];
  snprintf(line1, sizeof(line1), "1. %s", captureTimestamps[2].c_str());
  snprintf(line2, sizeof(line2), "2. %s", captureTimestamps[1].c_str());
  snprintf(line3, sizeof(line3), "3. %s", captureTimestamps[0].c_str());

  u8g2.drawStr(0, 35, line1);
  u8g2.drawStr(0, 45, line2);
  u8g2.drawStr(0, 55, line3);
}

void readFromGoogleSheet() {
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(scriptURL);
  int code = http.GET();
  if (code == 200) {
    String payload = http.getString();
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      Serial.println("JSON parsing failed!");
      return;
    }

    for (int i = 0; i < 3; i++) {
      if (i < doc.size()) {
        captureTimestamps[i] = doc[i][0].as<String>();
      } else {
        captureTimestamps[i] = "--";
      }
    }
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
  Serial.println("payload: " + postData);
  int code = http.POST(postData);
  Serial.print("POST code: "); Serial.println(code);
  Serial.println("Response: " + http.getString());

  http.end();
}

void displayTask(void *param) {
  while (true) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char buffer[32];
      strftime(buffer, sizeof(buffer),"%Y-%m-%d %H:%M:%S", &timeinfo);
      if (xSemaphoreTake(screenSemaphore, portMAX_DELAY)) {
        u8g2.clearBuffer();
        u8g2.firstPage();
        do {
          writeScreen(buffer);
        } while (u8g2.nextPage());
        xSemaphoreGive(screenSemaphore);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));  // Update every second
  }
}

void networkTask(void *param) {
  while (true) {
    readFromGoogleSheet();
    vTaskDelay(pdMS_TO_TICKS(15000));  // Every 15 seconds
  }
}

void setup() {
  Serial.begin(115200);
  screenSemaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(screenSemaphore);  // Initially available

  initialiseScreen();
  connectWifi();
  readFromGoogleSheet();  // Initial population

  // Start tasks
  xTaskCreatePinnedToCore(displayTask, "Display Task", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(networkTask, "Network Task", 8192, NULL, 1, NULL, 1);
}

void loop() {
  // UART listener: T = trigger
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
