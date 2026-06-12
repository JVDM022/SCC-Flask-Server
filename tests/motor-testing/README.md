# Motor Testing

Standalone Arduino motor test project. This does not use or modify anything inside `firmware/`.

The sketch uses the same motor output as `firmware/arduino/SCC-V1.4`:

- Motor PWM pin: `9`
- Motor run PWM: `155`

## Serial Commands

Open Serial Monitor at `115200`.

- `G`: turn motor on at PWM `155`
- `S`: turn motor off

The sketch prints CSV status lines:

```text
state,ms,pwm,enabled
```

## Build And Upload

From `scc-control-platform`:

```bash
pio run -d "tests/motor-testing/Firmwares/Arduino Motor Testing" -t upload
pio device monitor -d "tests/motor-testing/Firmwares/Arduino Motor Testing" -b 115200
```
