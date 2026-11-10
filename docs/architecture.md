# Architecture

The platform is a small SCADA-like research stack for a thermal SCC/corrosion test rig.

```text
Arduino SCC controller firmware
  -> USB serial
  -> Intel NUC gateway
  -> Flask telemetry and command APIs
  -> PostgreSQL storage
  -> alarm rules, pump-cycle extraction, ML prediction, guarded MPC
  -> React HMI dashboard
```

The Arduino firmware owns hard safety and local fallback control. The Intel NUC gateway moves telemetry, guarded PWM commands, manual commands, and Arduino firmware update requests between the hardware-side controller and backend services. Closed-loop MPC is opt-in; its PWM override expires after 3.5 seconds without a fresh command, returning the Arduino to local PID. The ESP32 MQTT relay path remains available for legacy deployments.

The backend stores raw telemetry first, then derives alarms, events, pump-cycle records, ML predictions, and MPC recommendations. Live device telemetry enters through the JSON telemetry API when using the NUC gateway, or through MQTT when using the legacy ESP32 relay. The HMI reads backend endpoints through polling so it remains simple to run during early research.
