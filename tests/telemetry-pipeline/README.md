# Telemetry Pipeline Test Section

This section is isolated from `firmware/`. It is for testing the live NUC path only:

```text
Arduino mock telemetry source -> Intel NUC gateway -> Flask /api/telemetry -> PostgreSQL
```

It does not start or call the HMI, ML prediction, or MPC endpoints. The verifier reads PostgreSQL directly and compares the final stored telemetry rows with the original Arduino CSV fixture.

## Test Project

- `Firmwares/Arduino Software Testing`: Arduino Uno project that emits one repeatable mock telemetry package over USB serial.

The mock package contains:

```text
MOCK_TELEMETRY_PACKAGE_BEGIN <n>
event,ms,temp_c,...
<fixture row 1>
<fixture row 2>
<fixture row 3>
MOCK_TELEMETRY_PACKAGE_END <n>
```

The NUC gateway ignores the package marker and header lines, parses the three CSV rows, and posts JSON telemetry to Flask.

Board choice:

- Arduino: `uno`

## Hardware Wiring

Connect the Arduino Uno directly to the Intel NUC over USB. No ESP32, UART jumper wiring, or Wi-Fi test firmware is used for this pipeline test.

## Backend and NUC Gateway Setup

Start the backend and make sure PostgreSQL is available. If write authentication is enabled, set the same `API_WRITE_KEY` for the backend and the NUC gateway.

In a terminal on the NUC, run the gateway against the Arduino USB port:

```bash
cd backend
python -m app.services.nuc_gateway \
  --serial-port /dev/ttyACM0 \
  --api-base-url http://localhost:5050
```

Use `/dev/ttyUSB0` or the macOS `/dev/cu.usbmodem*` path if that is where the Arduino appears. Use `http://localhost:5000` when running Flask directly instead of through Docker port mapping.

## Build and Upload the Arduino Mock Source

From `scc-control-platform`:

```bash
pio run -d "tests/telemetry-pipeline/Firmwares/Arduino Software Testing" -t upload
```

Once the Arduino resets, it repeatedly sends the mock telemetry package over USB. The NUC gateway should log parsed telemetry posts.

## Verify PostgreSQL Rows Match Arduino CSV

After the NUC gateway posts at least one mock package, run:

```bash
python3 tests/telemetry-pipeline/scripts/verify_telemetry_pipeline.py \
  --source tests/telemetry-pipeline/fixtures/arduino_telemetry_sample.csv \
  --database-url "$DATABASE_URL" \
  --mode verify-db
```

For a backend-only smoke test without Arduino hardware, the same verifier can post the sample rows directly to Flask and then compare PostgreSQL:

```bash
python3 tests/telemetry-pipeline/scripts/verify_telemetry_pipeline.py \
  --source tests/telemetry-pipeline/fixtures/arduino_telemetry_sample.csv \
  --database-url "$DATABASE_URL" \
  --api-url http://localhost:5050/api/telemetry \
  --api-key "$API_WRITE_KEY" \
  --mode post-and-verify
```

`post-and-verify` checks that `ml_predictions` and `mpc_recommendations` counts do not change. It only calls `/api/telemetry`.
