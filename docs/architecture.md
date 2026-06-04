# Architecture

The platform is a small SCADA-like research stack for a thermal SCC/corrosion test rig.

```text
Arduino SCC controller firmware
  -> ESP32 relay/communications bridge
  -> CSV/JSON telemetry over HTTP
  -> Flask API
  -> PostgreSQL storage
  -> alarm rules, pump-cycle extraction, ML prediction, MPC advisory logic
  -> React HMI dashboard
```

The Arduino firmware owns rig control behavior. The ESP32 firmware is used only as a relay/communications bridge to move telemetry and commands between the hardware-side controller and backend services.

The backend stores raw telemetry first, then derives alarms, events, pump-cycle records, ML predictions, and MPC recommendations. The HMI reads backend endpoints through polling so it remains simple to run during early research.
