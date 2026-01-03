#include <Arduino.h>
#include <GxEPD2_3C.h>
#include <GxEPD2.h>
#include <FastCRC.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>
#include <string>
#include <sbb_logo.h>

#define COLORED     GxEPD_BLACK
#define UNCOLORED   GxEPD_WHITE
#define RED         GxEPD_RED
#define ROW_HEIGHT  44
#define HEADER_OFFSET 60

WiFiClient socket;
HTTPClient https;
// GxEPD2_3C<GxEPD2_420, GxEPD2_420::HEIGHT> epd(GxEPD2_420(/*CS=*/ 15, /*DC=*/ 4, /*RST=*/ 5, /*BUSY=*/ 16));
GxEPD2_3C<GxEPD2_420c, GxEPD2_420c::HEIGHT> epd(GxEPD2_420c(/*CS=*/ 15, /*DC=*/ 4, /*RST=*/ 5, /*BUSY=*/ 16));
FastCRC32 CRC32;

int pixelShift = 0;
uint32_t lastChecksum = 0;
uint32_t currentChecksum = 0;

JsonDocument fetchTrainData() {
  JsonDocument doc;

  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[WiFi] Not connected, status: %d\n", WiFi.status());
    return doc; // return empty doc
  }

  Serial.println("[WiFi] Connected, proceeding with HTTPS request");

  // Try HTTP first as fallback
  Serial.println("[HTTP] Trying HTTP first...");
  WiFiClient httpClient;
  if (https.begin(httpClient, "http://transport.opendata.ch/v1/connections?from=8503099&to=8503000&transportations[]=train&limit=5&&fields[]=connections/from/departure&fields[]=connections/from/delay")) {
    https.addHeader("Content-Type", "application/json");
    https.useHTTP10(true);
    
    Serial.println("[HTTP] Sending GET request...");
    int httpCode = https.GET();
    Serial.printf("[HTTP] GET response code: %d\n", httpCode);
    
    if (httpCode == HTTP_CODE_OK) {
      String response = https.getString();
      Serial.println("[HTTP] Received response string, parsing JSON...");
      
      currentChecksum = CRC32.crc32((const uint8_t*)response.c_str(), response.length());
      Serial.printf("[CRC32] Response checksum: 0x%08X\n", currentChecksum);

      DeserializationError err = deserializeJson(doc, response);
      if (err) {
        Serial.print("[JSON] Deserialization failed: ");
        Serial.println(err.f_str());
      } else {
        Serial.println("[JSON] Successfully parsed JSON data via HTTP");
        https.end();
        return doc;
      }
    } else {
      Serial.printf("[HTTP] GET failed, error: %s\n", https.errorToString(httpCode).c_str());
    }
    https.end();
  }

  // If HTTP failed, try HTTPS with certificate fingerprint (more secure than setInsecure)
  Serial.println("[HTTPS] HTTP failed, trying HTTPS with certificate fingerprint...");

  https.end();
  return doc;
}

/**
 * Example JSON structure:
 * {"connections":[{"from":{"departure":"2026-01-02T20:53:00+0100","delay":0}},{"from":{"departure":"2026-01-02T21:13:00+0100","delay":0}},{"from":{"departure":"2026-01-02T21:33:00+0100","delay":0}},{"from":{"departure":"2026-01-02T21:53:00+0100","delay":0}},{"from":{"departure":"2026-01-02T22:13:00+0100","delay":0}}]}
 */

void displayTrainData(const JsonDocument& doc, int pixelShift) {
  std::string l_topLine = "S4 Sihlau -> Zurich";
  int row = 0;

  epd.drawBitmap(8, 8 + pixelShift, gImage_sbb_logo_tiny, 40, 40, GxEPD_RED);

  epd.setFont(&FreeSans18pt7b);
  epd.setTextColor(GxEPD_BLACK);
  epd.setCursor(60, HEADER_OFFSET - (HEADER_OFFSET - 18)/2 + pixelShift);
  epd.print(l_topLine.c_str());
  epd.drawLine(5, HEADER_OFFSET + pixelShift, epd.epd2.WIDTH - 5, HEADER_OFFSET + pixelShift, GxEPD_BLACK);
  row++;
  Serial.println(l_topLine.c_str());

  Serial.println("[DISPLAY] Checking for connections in JSON...");
  if (!doc["connections"]) {
    Serial.println("[DISPLAY] ERROR: No 'connections' key found in JSON");

    return;
  }

  JsonVariantConst connections = doc["connections"];

  if (!connections.is<JsonArrayConst>()) {
    Serial.printf("[DISPLAY] ERROR: 'connections' is not an array, type: %s\n", connections.as<String>().c_str());
    return;
  }

  JsonArrayConst stationboard = connections.as<JsonArrayConst>();
  Serial.printf("[DISPLAY] Found %d connections\n", stationboard.size());

  for (JsonObjectConst station : stationboard) {
    JsonObjectConst from = station["from"];

    const char* departureTimeSrc = from["departure"];
    const int delay = from["delay"];

    // Handle timezone in departure time (truncate at '+' or 'Z')
    std::string timeStr = departureTimeSrc;
    size_t tzPos = timeStr.find('+');
    if (tzPos == std::string::npos) {
      tzPos = timeStr.find('Z');
    }
    if (tzPos != std::string::npos) {
      timeStr = timeStr.substr(0, tzPos);
    }

    struct tm tm = {0};
    char departureTime[6];

    // Convert to tm struct
    if (strptime(timeStr.c_str(), "%Y-%m-%dT%H:%M:%S", &tm) == nullptr) {
      Serial.printf("[TIME] Failed to parse time: %s\n", timeStr.c_str());
      strcpy(departureTime, "??:??");
    } else {
      strftime(departureTime, 6, "%H:%M", &tm);
    }

    std::string l_bottomLine = "";
    std::string l_delayLine = "";

    l_bottomLine.append(" - ");
    l_bottomLine.append(departureTime);

    epd.setFont(&FreeMonoBold24pt7b);
    epd.setTextColor(GxEPD_BLACK);
    epd.setCursor(10, HEADER_OFFSET + (row * ROW_HEIGHT) + pixelShift);
    epd.print(l_bottomLine.c_str());
    if (delay) {
      l_delayLine.append("+");
      l_delayLine.append(std::to_string(delay));
      l_delayLine.append("m");

      epd.setFont(&FreeMonoBold24pt7b);
      epd.setTextColor(GxEPD_RED);
      epd.setCursor(246, HEADER_OFFSET + (row * ROW_HEIGHT) + pixelShift);
      epd.print(l_delayLine.c_str());
    }

    Serial.print(l_bottomLine.c_str());
    Serial.print(l_delayLine.c_str());
    Serial.print("\n");
    row++;
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();
  while(!Serial);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());

  // Set time via NTP, as required for x.509 validation
  configTime(1 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  while(!time(nullptr))
  {
    Serial.println("*");
    delay(500);
  }

  Serial.println("before init");
  epd.init(115200);
  epd.setRotation(0);
  delay(1000);
}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.println("Starting update cycle");

  JsonDocument doc = fetchTrainData();

  if (currentChecksum != lastChecksum) {
    Serial.println("[LOOP] API response changed, updating display");
    lastChecksum = currentChecksum;
    // Proceed with display update

    epd.fillScreen(GxEPD_WHITE);

    if (!doc.isNull() && doc["connections"].is<JsonArray>()) {
      Serial.println("Displaying train data");
      displayTrainData(doc, pixelShift);
    } else {
      Serial.println("No data to display, showing error message");
      epd.setFont(&FreeSans18pt7b);
      epd.setTextColor(GxEPD_RED);
      epd.setCursor(10, 50);
      epd.print("Connection Failed");
      epd.setCursor(10, 80);
      epd.print("Check WiFi & API");
    }

    epd.display();
    epd.powerOff();

    pixelShift++;
    if (pixelShift > 4) {
      pixelShift = 0;
    }
  } else {
    Serial.println("[LOOP] API response unchanged, skipping display update");
  }

  Serial.println("Waiting 2min before the next round...");
  delay(120000);
}