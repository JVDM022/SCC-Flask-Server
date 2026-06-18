#include <Arduino.h>

static const char *kRows[] = {
    "0,123456,126.42,222,-0.1300,125.00,3,184,1,0,1,1,0,155,150,30000,127.20,124.80,2.40,18.50,0,0,300",
    "1,124456,126.70,223,0.2800,125.00,3,190,1,0,1,1,1,180,150,30000,126.70,126.70,0.00,0.00,0,0,301",
    "2,125456,124.90,220,-1.8000,125.00,3,210,1,0,1,1,0,0,150,30000,126.70,124.90,1.80,0.00,0,0,302",
};

static const size_t kRowCount = sizeof(kRows) / sizeof(kRows[0]);
static uint32_t lastEmitMs = 0;
static uint32_t packageIndex = 0;
static const uint32_t kPackageIntervalMs = 5000U;

static void printHeader() {
  Serial.println(F("event,ms,temp_c,adc,dtemp_c_per_s,setpoint_c,mode,heater_pwm,heating,heater_lockout,pump_enabled,pump_allowed,pump_on,motor_pwm,motor_on_ms,motor_period_ms,temp_before_pump_c,min_temp_after_pump_c,last_pump_drop_c,recovery_time_s,manual_kill,hard_kill,uptime_s"));
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println(F("ARDUINO_NUC_MOCK_TELEMETRY_READY"));
}

static void emitMockTelemetryPackage() {
  Serial.print(F("MOCK_TELEMETRY_PACKAGE_BEGIN "));
  Serial.println(packageIndex);
  printHeader();
  for (size_t i = 0; i < kRowCount; i++) {
    Serial.println(kRows[i]);
  }
  Serial.print(F("MOCK_TELEMETRY_PACKAGE_END "));
  Serial.println(packageIndex);
  packageIndex++;
}

void loop() {
  const uint32_t now = millis();
  if (packageIndex == 0 || now - lastEmitMs >= kPackageIntervalMs) {
    lastEmitMs = now;
    emitMockTelemetryPackage();
  }
}
