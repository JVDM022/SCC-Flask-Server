# Firmware Artifact Upload and OTA Commands

The HMI firmware page can upload firmware artifacts to the backend and queue OTA
commands for the ESP32 relay to poll.

## Artifact URLs

Artifacts are stored under `FIRMWARE_STORAGE_DIR`, which defaults to
`backend/storage/firmware`. They are served from:

```text
GET /firmware/artifacts/<filename>
```

Set `PUBLIC_BASE_URL` to the externally reachable backend origin so queued OTA
commands contain URLs the ESP32 can download.

## Command Lifecycle

The dashboard queues commands with:

```text
POST /api/firmware/commands
```

The ESP32 polls:

```text
GET /api/firmware/commands/next?device=esp32
```

The backend marks a command as `sent` when it is fetched. This avoids repeated
polling re-running the same OTA command. Devices should then acknowledge with:

```text
POST /api/firmware/commands/<cmd_id>/ack
```

Valid acknowledgement states are `started`, `success`, and `failed`.

## Safety Notes

- Do not expose firmware upload endpoints publicly without authentication.
- Firmware upload should require login and role permissions before production use.
- HTTPS is strongly recommended for OTA artifact URLs.
- ESP32 firmware should verify the downloaded artifact hash before applying it if
  hash verification is implemented.
- ESP32 `.bin` upload can perform real OTA only when the ESP32 firmware supports
  downloading and applying the queued URL.
- Arduino `.hex` upload currently stores, logs, and queues an `ARDUINO_OTA`
  command only. Arduino flashing is scaffolding until the ESP32-side
  STK500/Optiboot flashing bridge is implemented and tested.
