#ifndef TELEMETRY_SCHEMA_H
#define TELEMETRY_SCHEMA_H

#define TELEMETRY_CSV_COLUMNS 23
#define TELEMETRY_CSV_HEADER \
  "event,ms,temp_c,adc,dtemp_c_per_s,setpoint_c,mode,heater_pwm,heating," \
  "heater_lockout,pump_enabled,pump_allowed,pump_on,motor_pwm,motor_on_ms," \
  "motor_period_ms,temp_before_pump_c,min_temp_after_pump_c,last_pump_drop_c," \
  "recovery_time_s,manual_kill,hard_kill,uptime_s"

#endif
