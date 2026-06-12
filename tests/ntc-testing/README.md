# NTC Testing

Standalone Arduino NTC temperature readout project. This does not use or modify anything inside `firmware/`.

The sketch uses the same temperature input and conversion as `firmware/arduino/SCC-V1.4`:

- Thermistor input: `A0`
- ADC-to-temperature fit: `tempC = -0.1871 * adc + 167.97`
- Clamp range: `-20.00 C` to `220.00 C`

Open Serial Monitor at `115200`. The sketch prints live CSV readings once per second:

```text
ms,adc,temp_c
```

## Build And Upload

From `scc-control-platform`:

```bash
pio run -d "tests/ntc-testing/Firmwares/Arduino NTC Testing" -t upload
pio device monitor -d "tests/ntc-testing/Firmwares/Arduino NTC Testing" -b 115200
```
