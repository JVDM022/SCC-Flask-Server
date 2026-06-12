# Heater Testing

Standalone Arduino heater test project. This does not use or modify anything inside `firmware/`.

The sketch uses the same Arduino hardware assumptions as `firmware/arduino/SCC-V1.4`:

- Thermistor input: `A0`
- Heater PWM pin: `6`
- ADC-to-temperature fit: `tempC = -0.1871 * adc + 167.97`
- Heater PWM limit: `200`
- Hard safety cutoff: `130.00 C`

## Serial Commands

Open Serial Monitor at `115200`.

- `G`: start heater control and hold `120.00 C`
- `S`: stop heater immediately

The sketch prints CSV status lines:

```text
state,ms,temp_c,adc,setpoint_c,pwm,enabled,hard_kill
```

## Build And Upload

From `scc-control-platform`:

```bash
pio run -d "tests/heater-testing/Firmwares/Arduino Heater Testing" -t upload
pio device monitor -d "tests/heater-testing/Firmwares/Arduino Heater Testing" -b 115200
```
