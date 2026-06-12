#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <Arduino.h>

uint16_t readAdcAvg(uint8_t n = 8);
int16_t adcToTempCx100(uint16_t adc);
bool adcIndicatesSensorFault(uint16_t adc, bool faultActive = false);

#endif
