#ifndef TEST_SUPPORT_ARDUINO_H
#define TEST_SUPPORT_ARDUINO_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#ifndef PI
#define PI 3.14159265358979323846
#endif

#define A0 0
#define LOW 0
#define HIGH 1
#define OUTPUT 1

static inline int analogRead(uint8_t) {
  return 0;
}

static inline void analogWrite(uint8_t, int) {
}

static inline void pinMode(uint8_t, uint8_t) {
}

static inline void digitalWrite(uint8_t, uint8_t) {
}

static inline void delay(uint32_t) {
}

#endif
