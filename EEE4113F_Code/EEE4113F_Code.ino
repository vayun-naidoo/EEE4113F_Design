#include <U8g2lib.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <SPI.h>
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

//const char* ssid = "VODAFONEB148";
//const char* password = "3UQKAAAPT9KC3R6D";

const char* ssid = "DESKTOP-QJB68QR 3134";
const char* password = "1{9H99s4";

const char* scriptURL = "https://script.google.com/macros/s/AKfycbwil1MuIU0Xj2V5rJBMQx64NSohBkdznGxqrt04VPp9lV2vf7ML4d5337gh3deFFrSj/exec";

#define D0 18     //D0 = Clock
#define D1 23     //D1 = Data
#define RES 27    //RES = Reset
#define DC 26     //DC = Data/Command
#define CS 5      //CS = Chip Select

U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI u8g2(U8G2_R0, CS, DC, RES); //constructor to initialise screen with hardware spi with full buffer

unsigned long lastSheetCheck = 0;
unsigned long lastDisplayUpdate = 0;
const unsigned long sheetInterval = 15000; 
const unsigned long displayInterval = 1000;

String captureTimestamps[3] = {"--", "--", "--"};  // fallback defaults

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

  // Display confirmation
  u8g2.clearBuffer();
  u8g2.drawStr(0, 12, "WiFi Connected!");
  u8g2.sendBuffer();

  // Configure time using NTP
  configTime(3600 * 2, 0, "pool.ntp.org", "time.nist.gov"); //UTC + 2 config
  // Wait for time to sync
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
void sendToGoogleSheet(String timestamp) {
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(scriptURL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded"); // Not JSON!

  // Send data as key=value
  timestamp.replace(" ", "%20");
  timestamp.replace(":", "%3A");
  String postData = "timestamp=" + timestamp;
  Serial.println("payload: " + postData);
  int code = http.POST(postData);
  Serial.print("POST code: "); Serial.println(code);
  String response = http.getString();
  Serial.println("Response: " + response);

  http.end();
}
void readFromGoogleSheet() {
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);  // Important line
  http.begin(scriptURL);
  int code = http.GET();
  Serial.print("GET code: "); Serial.println(code);
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
void setup() {
  //Setup serial for debugging
  Serial.begin(115200);

  //Initialise screen
  initialiseScreen();
  // Connect to Wi-Fi
  connectWifi();
  readFromGoogleSheet(); //populate on startup
}

void loop() {

  unsigned long now = millis();
  if (now - lastDisplayUpdate >= displayInterval)
  {
    lastDisplayUpdate = now;
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    u8g2.clearBuffer();

    u8g2.firstPage();
    do {
      writeScreen(buffer);
    } while (u8g2.nextPage());
    } else {
    Serial.println("Failed to get time");
    }
  }

  // Check sheet every 15 seconds
  if (now - lastSheetCheck >= sheetInterval) {
    lastSheetCheck = now;
    readFromGoogleSheet();
  }

  // Example: simulate a UART trigger (replace with real UART check)
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'T') {
      struct tm now;
      if (getLocalTime(&now)) {
        char stamp[32];
        strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &now);
        Serial.println(String(stamp));
        sendToGoogleSheet(String(stamp));
      }
    }
  }


}
