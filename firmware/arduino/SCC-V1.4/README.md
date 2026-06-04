# SCC-V1.4 Arduino Firmware

This firmware controls the Arduino Uno side of the SCC controller and emits CSV
telemetry over UART.

## ESP32-Driven Firmware Update Prep

The optional `ENABLE_SOFTWARE_BOOTLOADER_ENTRY` feature adds a small UART command
entry point for an ESP32 to request a safe reset handoff before programming the
Arduino.

This does not flash the Arduino from the Arduino firmware. The ESP32 must act as
the programmer and provide the upload protocol.

When enabled, the Arduino accepts either command line:

```text
ENTER_BOOTLOADER
OTA_PREPARE
```

On a recognized command, the firmware:

- sets heater PWM to `0`
- turns the motor/pump off
- disables normal control outputs
- prints `OTA_READY`
- flushes serial output
- triggers an ATmega328P watchdog reset

The feature is disabled by default so normal builds keep UART output as CSV
telemetry only. Enable it explicitly at compile time, for example:

```ini
build_flags =
  -DENABLE_SOFTWARE_BOOTLOADER_ENTRY=1
```

For a reliable production update path, wire an ESP32 GPIO to the Arduino RESET
line and use that GPIO for bootloader entry/reset timing. Software reset into the
bootloader is bootloader-dependent and can be less reliable than a hardware
reset pulse.
