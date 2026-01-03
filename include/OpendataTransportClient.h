#pragma once

#include <Arduino.h>
#include <string>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <FastCRC.h>
#include <Result.h>

#define BASE_URL "http://transport.opendata.ch"
#define FROM_STATION 8503099
#define TO_STATION 8503000

FastCRC32 CRC32;
WiFiClient socket;
HTTPClient https;

uint32_t lastChecksum = 0;
uint32_t currentChecksum = 0;


void setUpOpendataClient() {
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
}

Result fetchTrainData() {
    Result result = Result();
    JsonDocument doc;

    // Check WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
        std::string error;
        error.append("[WiFi] Not connected, status: ");
        error.append(std::to_string(WiFi.status()));
        Serial.println(error.c_str());
        result.errorString = error;

        return result;
    }

    Serial.println("[WiFi] Connected, proceeding with HTTPS request");

    char url[256];

    sprintf(url, "%s/v1/connections?from=%d&to=%d&transportations[]=train&limit=5&&fields[]=connections/from/departure&fields[]=connections/from/delay", BASE_URL, FROM_STATION, TO_STATION);

    WiFiClient httpClient;
    if (https.begin(httpClient, url)) {
        https.addHeader("Content-Type", "application/json");
        https.useHTTP10(true);
        https.setTimeout(5000);
        
        Serial.println("[HTTP] Sending GET request...");
        int httpCode = https.GET();
        Serial.printf("[HTTP] GET response code: %d\n", httpCode);
        
        if (httpCode == HTTP_CODE_OK) {
            String response = https.getString();
            Serial.println("[HTTP] Received response string, parsing JSON...");
            
            currentChecksum = CRC32.crc32((const uint8_t*)response.c_str(), response.length());
            Serial.printf("[CRC32] Response checksum: 0x%08X\n", currentChecksum);

            if (currentChecksum == lastChecksum) {
                Serial.println("[CRC32] checksums matched");
                result.skip = true;

                return result;
            } else {
                lastChecksum = currentChecksum;
            }

            DeserializationError err = deserializeJson(doc, response);
            if (err) {
                lastChecksum = 0;
                std::string error;
                error.append("[JSON] Deserialization failed: ");
                error.append(err.c_str());
                
                Serial.println(error.c_str());

                result.errorString = error;

                return result;
            } else {
                Serial.println("[JSON] Successfully parsed JSON data via HTTP");
                https.end();

                result.doc = doc;

                return result;
            }
        } else {
            https.end();
            lastChecksum = 0;
            std::string error;
            error.append("[HTTP] GET failed, error: ");
            error.append(https.errorToString(httpCode).c_str());
            Serial.println(error.c_str());

            result.errorString = error;

            return result;
        }
        https.end();
    }

    https.end();

    return result;
}