# Telemetry Pipeline Test Section

This section is isolated from `firmware/`. It is for testing only:

```text
Arduino test source -> ESP32 parser/HTTP relay -> Flask /api/telemetry -> PostgreSQL
```

It does not start or call the HMI, ML prediction, or MPC endpoints. The verifier reads PostgreSQL directly and compares the final stored telemetry rows with the original Arduino CSV rows.

## Test Projects

- `Firmwares/Arduino Software Testing`: Arduino Uno project that emits deterministic SCC telemetry CSV rows.
- `Firmwares/ESP32 Software Testing`: ESP32 DOIT DevKit V1 project that reads Arduino CSV on UART2, parses it with the same `relay_core` library used by the real ESP32 firmware, and posts JSON to Flask.

Board choices mirror the existing firmware projects:

- Arduino: `uno`
- ESP32: `esp32doit-devkit-v1`

## Hardware Wiring

Use common ground.

```text
Arduino TX1 / D1  -> ESP32 RX2 / GPIO16
Arduino GND      -> ESP32 GND
```

The ESP32 test firmware does not send commands back to Arduino, so Arduino RX wiring is optional for this test.

## Configure ESP32 Test Wi-Fi and Flask URL

Copy the example config and fill in your values:

```bash
cp "tests/telemetry-pipeline/Firmwares/ESP32 Software Testing/include/test_config.example.h" \
   "tests/telemetry-pipeline/Firmwares/ESP32 Software Testing/include/test_config.h"
```

Set:

- `TEST_WIFI_SSID`
- `TEST_WIFI_PASSWORD`
- `TEST_FLASK_TELEMETRY_URL`, usually `http://<your-computer-ip>:5050/api/telemetry`
- `TEST_API_WRITE_KEY`, matching backend `API_WRITE_KEY`

## Build and Upload

From `scc-control-platform`:

```bash
pio run -d "tests/telemetry-pipeline/Firmwares/Arduino Software Testing" -t upload
pio run -d "tests/telemetry-pipeline/Firmwares/ESP32 Software Testing" -t upload
```

Open the ESP32 monitor:

```bash
pio device monitor -d "tests/telemetry-pipeline/Firmwares/ESP32 Software Testing" -b 115200
```

You should see `SOURCE_CSV`, `POST_JSON`, and `POST_OK` lines.

## Verify PostgreSQL Rows Match Arduino CSV

After the ESP32 posts the rows, run:

```bash
python3 tests/telemetry-pipeline/scripts/verify_telemetry_pipeline.py \
  --source tests/telemetry-pipeline/fixtures/arduino_telemetry_sample.csv \
  --database-url "$DATABASE_URL" \
  --mode verify-db
```

For a backend-only smoke test without hardware, the same verifier can post the sample rows directly to Flask and then compare PostgreSQL:

```bash
python3 tests/telemetry-pipeline/scripts/verify_telemetry_pipeline.py \
  --source tests/telemetry-pipeline/fixtures/arduino_telemetry_sample.csv \
  --database-url "$DATABASE_URL" \
  --api-url http://localhost:5050/api/telemetry \
  --api-key "$API_WRITE_KEY" \
  --mode post-and-verify
```

`post-and-verify` checks that `ml_predictions` and `mpc_recommendations` counts do not change. It only calls `/api/telemetry`.
