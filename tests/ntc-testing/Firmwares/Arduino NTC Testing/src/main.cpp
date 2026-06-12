#include <Arduino.h>

// Standalone copy of the production Arduino temperature input/calibration.
// This test project does not include or modify files under firmware/.
#define THERM_PIN A0
#define MAX_Cx100 22000
#define MIN_Cx100 -2000
#define TF_M_C_PER_ADC (-0.1871f)
#define TF_B_C 167.97f

#define READ_PERIOD_MS 1000UL

static uint32_t lastReadMs = 0;

static uint16_t readAdcAvg(uint8_t samples = 8) {
  uint32_t total = 0;
  for (uint8_t i = 0; i < samples; i++) {
    total += analogRead(THERM_PIN);
    delay(2);
  }
  return static_cast<uint16_t>(total / samples);
}

static int16_t adcToTempCx100(uint16_t adc) {
  float tempC = (TF_M_C_PER_ADC * static_cast<float>(adc)) + TF_B_C;

  if (tempC > (MAX_Cx100 / 100.0f)) {
    tempC = MAX_Cx100 / 100.0f;
  }
  if (tempC < (MIN_Cx100 / 100.0f)) {
    tempC = MIN_Cx100 / 100.0f;
  }

  return static_cast<int16_t>(tempC * 100.0f + (tempC >= 0.0f ? 0.5f : -0.5f));
}

static void printTempCx100(int16_t tempCx100) {
  Serial.print(tempCx100 * 0.01f, 2);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("NTC_TEST_READY"));
  Serial.println(F("ms,adc,temp_c"));
}

void loop() {
  const uint32_t now = millis();
  if (now - lastReadMs < READ_PERIOD_MS) {
    return;
  }
  lastReadMs = now;

  const uint16_t adc = readAdcAvg();
  const int16_t tempCx100 = adcToTempCx100(adc);

  Serial.print(now);
  Serial.print(',');
  Serial.print(adc);
  Serial.print(',');
  printTempCx100(tempCx100);
  Serial.println();
}
