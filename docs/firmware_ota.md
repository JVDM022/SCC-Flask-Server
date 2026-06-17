# Firmware Artifact Upload and OTA Commands

The HMI firmware page can upload firmware artifacts to the backend and queue OTA
commands. Arduino `.hex` updates are now handled by the Intel NUC USB gateway.
The legacy ESP32 relay command path remains available for older deployments.

## Artifact URLs

Artifacts are stored under `FIRMWARE_STORAGE_DIR`, which defaults to
`backend/storage/firmware`. They are served from:

```text
GET /firmware/artifacts/<filename>
```

Set `PUBLIC_BASE_URL` to the backend origin reachable by the NUC so queued OTA
commands contain artifact URLs the gateway can download.

## Command Lifecycle

The dashboard queues commands with:

```text
POST /api/firmware/commands
```

The Intel NUC gateway polls:

```text
GET /api/firmware/commands/next?device=nuc
```

The backend marks a command as `sent` when it is fetched. This avoids repeated
polling re-running the same OTA command. The NUC gateway acknowledges with:

```text
POST /api/firmware/commands/<cmd_id>/ack
```

Valid acknowledgement states are `started`, `success`, and `failed`.

## Intel NUC Arduino Flashing

Run the gateway on the NUC that is connected to the Arduino USB port:

```bash
cd backend
python -m app.services.nuc_gateway --serial-port /dev/ttyACM0 --api-base-url http://localhost:5000
```

Arduino flashing uses `avrdude` by default:

```text
avrdude -v -p atmega328p -c arduino -P <serial-port> -b 115200 -D -U flash:w:<artifact.hex>:i
```

Override the command and board parameters with `NUC_AVRDUDE_COMMAND`,
`NUC_ARDUINO_MCU`, `NUC_ARDUINO_PROGRAMMER`, and
`NUC_ARDUINO_UPLOAD_BAUD` if the NUC has a different Arduino toolchain.

## Safety Notes

- Do not expose firmware upload endpoints publicly without authentication.
- Firmware upload should require login and role permissions before production use.
- HTTPS is strongly recommended for OTA artifact URLs.
- Arduino `.hex` upload queues an `ARDUINO_OTA` command. The NUC gateway
  downloads that artifact, closes the serial monitor, runs `avrdude`, reopens
  serial, records OTA status, and acknowledges success or failure.
- ESP32 `.bin` upload remains available for legacy relay firmware only.
