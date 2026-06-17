# Architecture

The platform is a small SCADA-like research stack for a thermal SCC/corrosion test rig.

```text
Arduino SCC controller firmware
  -> USB serial
  -> Intel NUC gateway
  -> Flask telemetry and command APIs
  -> PostgreSQL storage
  -> alarm rules, pump-cycle extraction, ML prediction, MPC advisory logic
  -> React HMI dashboard
```

The Arduino firmware owns rig control behavior. The Intel NUC gateway is used only as a communications bridge to move telemetry, commands, and Arduino firmware update requests between the hardware-side controller and backend services. The ESP32 MQTT relay path remains available for legacy deployments.

The backend stores raw telemetry first, then derives alarms, events, pump-cycle records, ML predictions, and MPC recommendations. Live device telemetry enters through the JSON telemetry API when using the NUC gateway, or through MQTT when using the legacy ESP32 relay. The HMI reads backend endpoints through polling so it remains simple to run during early research.
