#include <Arduino.h>
#include <OpendataTransportClient.h>
#include <EpaperPrinter.h>

#define DELAY_BETWEEN_RUNS 120000

void setup() {
  Serial.begin(115200);
  while(!Serial);

  setUpOpendataClient();
  setUpEpaperPrinter();

  Serial.println("[SETUP] Set-up complete");
}

void loop() {
  Serial.println("Starting update cycle");

  Result result = fetchTrainData();

  if (!result.skip) {
    Serial.println("[LOOP] API response changed, updating display");
    
    displayTrainData(result);
  } else {
    Serial.println("[LOOP] API response unchanged, skipping display update");
  }

  Serial.printf("Waiting %dmin before the next round...", DELAY_BETWEEN_RUNS/60000);
  delay(DELAY_BETWEEN_RUNS);
}