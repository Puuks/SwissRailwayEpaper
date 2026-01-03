#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <sbb_logo.h>
#include <GxEPD2_3C.h>
#include <GxEPD2.h>
#include <Result.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>


#define BLACK         GxEPD_BLACK
#define WHITE         GxEPD_WHITE
#define RED           GxEPD_RED
#define ROW_HEIGHT    44
#define HEADER_OFFSET 60

int pixelShift = 0;

// GxEPD2_3C<GxEPD2_420, GxEPD2_420::HEIGHT> epd(GxEPD2_420(/*CS=*/ 15, /*DC=*/ 4, /*RST=*/ 5, /*BUSY=*/ 16));
GxEPD2_3C<GxEPD2_420c, GxEPD2_420c::HEIGHT> epd(GxEPD2_420c(/*CS=*/ 15, /*DC=*/ 4, /*RST=*/ 5, /*BUSY=*/ 16));


void setUpEpaperPrinter() {
    epd.init(115200);
    epd.setRotation(0);
    delay(1000);
}

/**
 * Example JSON structure:
 * {"connections":[{"from":{"departure":"2026-01-02T20:53:00+0100","delay":0}},{"from":{"departure":"2026-01-02T21:13:00+0100","delay":0}},{"from":{"departure":"2026-01-02T21:33:00+0100","delay":0}},{"from":{"departure":"2026-01-02T21:53:00+0100","delay":0}},{"from":{"departure":"2026-01-02T22:13:00+0100","delay":0}}]}
 */

void displayTrainData(const Result& result) {
    JsonDocument doc = result.doc;
    
    epd.fillScreen(WHITE);

    if (!result.errorString.empty()) {
        epd.setFont(&FreeSans18pt7b);
        epd.setTextColor(RED);
        epd.setCursor(10, 50);
        epd.print("Connection Failed");
        epd.setCursor(10, 80);
        epd.print("Check WiFi & API");
        epd.setCursor(10, 120);
        epd.print(result.errorString.c_str());

        epd.display();
        epd.powerOff();

        return;
    }

    if (doc.isNull() || !doc["connections"] || !doc["connections"].is<JsonArray>()) {
        Serial.println("No data to display");
        epd.setFont(&FreeSans18pt7b);
        epd.setTextColor(RED);
        epd.setCursor(10, 50);
        epd.print("Empty response!");
        
        epd.display();
        epd.powerOff();

        return;
    }

    epd.drawBitmap(8, 8 + pixelShift, gImage_sbb_logo_tiny, 40, 40, RED);

    std::string l_topLine = "S4 Sihlau -> Zurich";

    epd.setFont(&FreeSans18pt7b);
    epd.setTextColor(BLACK);
    epd.setCursor(60, HEADER_OFFSET - (HEADER_OFFSET - 18)/2 + pixelShift);
    epd.print(l_topLine.c_str());
    Serial.println(l_topLine.c_str());
    
    epd.drawLine(5, HEADER_OFFSET + pixelShift, epd.epd2.WIDTH - 5, HEADER_OFFSET + pixelShift, BLACK);

    JsonVariantConst connections = doc["connections"];
    JsonArrayConst stationboard = connections.as<JsonArrayConst>();
    Serial.printf("[DISPLAY] Found %d connections\n", stationboard.size());

    int row = 0;
    for (JsonObjectConst station : stationboard) {
        row++;
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
        epd.setTextColor(BLACK);
        epd.setCursor(10, HEADER_OFFSET + (row * ROW_HEIGHT) + pixelShift);
        epd.print(l_bottomLine.c_str());
        
        if (delay) {
            l_delayLine.append("+");
            l_delayLine.append(std::to_string(delay));
            l_delayLine.append("m");

            epd.setFont(&FreeMonoBold24pt7b);
            epd.setTextColor(RED);
            epd.setCursor(246, HEADER_OFFSET + (row * ROW_HEIGHT) + pixelShift);
            epd.print(l_delayLine.c_str());
        }

        Serial.print(l_bottomLine.c_str());
        Serial.print(l_delayLine.c_str());
        Serial.print("\n");
    }

    epd.display();
    epd.powerOff();

    pixelShift++;
    if (pixelShift > 4) {
        pixelShift = 0;
    }
}
