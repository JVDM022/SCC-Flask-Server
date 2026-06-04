#include "temperature.h"

#include "config.h"

uint16_t readAdcAvg(uint8_t n) {
  uint32_t s = 0;
  for (uint8_t i = 0; i < n; i++) {
    s += analogRead(THERM_PIN);
  }
  return (uint16_t)(s / n);
}

int16_t adcToTempCx100(uint16_t adc) {
  float tempC = (TF_M_C_PER_ADC * (float)adc) + TF_B_C;

  if (tempC > (MAX_Cx100 / 100.0f)) tempC = (MAX_Cx100 / 100.0f);
  if (tempC < (MIN_Cx100 / 100.0f)) tempC = (MIN_Cx100 / 100.0f);

  return (int16_t)(tempC * 100.0f + (tempC >= 0 ? 0.5f : -0.5f));
}
