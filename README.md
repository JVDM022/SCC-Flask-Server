# scc-control-platform

A small SCADA-like research control platform for a thermal SCC/corrosion test rig. Arduino SCC firmware owns rig control behavior, while the ESP32 relay firmware handles communications to the Flask backend. PostgreSQL stores the time series, and the backend derives alarms, pump-cycle statistics, ML temperature predictions, and advisory MPC heater PWM recommendations. The React HMI dashboard provides live monitoring, trends, alarms, events, ML predictions, MPC output, and equipment health views.

## Architecture

```text
Arduino SCC controller -> ESP32 relay -> Mosquitto MQTT -> Flask subscriber -> PostgreSQL
                                                              |-> alarms and events
                                                              |-> pump-cycle extraction
                                                              |-> ML prediction
                                                              |-> advisory MPC
                                                              |-> React HMI dashboard
```

## Linux/macOS Setup

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r backend/requirements.txt
```

Run the backend:

```bash
cd backend
flask --app run:app init-db
python run.py
```

## Windows Setup

```bat
python -m venv .venv
.venv\Scripts\activate
pip install -r backend/requirements.txt
```

## Docker Setup

```bash
docker compose up --build
```

Backend: `http://localhost:5050`  
Frontend: `http://localhost:5173`

## Frontend HMI

The HMI is a React + Vite + TypeScript app in `frontend/hmi-dashboard`. It uses React Query for API polling, Plotly for dark engineering charts, and modular dashboard pages for operations, analytics, ML models, MPC, digital twin simulation, equipment health, event intelligence, and settings.

```bash
cd frontend/hmi-dashboard
npm install
npm run dev
```

## Live Telemetry

Live ESP32 telemetry is MQTT-only. Devices publish JSON to:

```text
scc/site/<site_id>/rig/<rig_id>/device/<device_id>/telemetry
```

The Flask backend subscribes to Mosquitto, parses MQTT payloads, stores telemetry, and derives alarms, pump-cycle records, ML predictions, and MPC recommendations.

## API Endpoints

- `GET /api/latest`: latest telemetry row.
- `GET /api/history?minutes=60`: telemetry history for plotting.
- `GET /api/events?limit=100`: pump and hard-kill events.
- `GET /api/alarms/active`: active alarms.
- `GET /api/pump-cycles`: extracted pump cycle statistics.
- `GET /api/ml/prediction`: predicted temperatures for +1s, +5s, and +10s when models exist.
- `GET /api/mpc/recommendation`: advisory heater PWM recommendation.
- `POST /api/control/setpoint`: store a desired setpoint command.
- `POST /api/telemetry`: development/manual JSON telemetry ingest, not the live ESP32 path.
- `POST /api/telemetry/csv`: development/manual CSV import, not the live ESP32 path.

## Telemetry Format

Live MQTT telemetry is JSON. The development CSV import endpoint accepts CSV with or without a header. Headerless rows are parsed using this exact order:

```text
event,ms,temp_c,adc,dtemp_c_per_s,setpoint_c,mode,heater_pwm,heating,heater_lockout,pump_enabled,pump_allowed,pump_on,motor_pwm,motor_on_ms,motor_period_ms,temp_before_pump_c,min_temp_after_pump_c,last_pump_drop_c,recovery_time_s,manual_kill,hard_kill,uptime_s
```

Example:

```text
0,123456,126.42,222,-0.1300,125.00,3,184,1,0,1,1,0,155,150,30000,127.20,124.80,2.40,18.50,0,0,300
```

Event codes: `0` normal sample, `1` pump start, `2` pump end, `3` pump recovered, `4` hard kill, `5` sensor fault.

## Safety Philosophy

ESP32 firmware and hardware own hard safety. Flask, ML, and MPC are monitoring and advisory layers, not the only protection layer. Heater lockout, manual kill, and hard kill override all recommendations; MPC returns PWM 0 when any of those states is active.

## Email Alerts

The backend can send SMTP email for critical safety alarms such as hard kill, manual kill, and sensor fault. Configure these environment variables in `.env` or Azure App Settings:

```text
ALERT_EMAIL_ENABLED=1
ALERT_EMAIL_TO=operator@example.com
ALERT_EMAIL_FROM=scc-alerts@example.com
ALERT_EMAIL_COOLDOWN_SECONDS=900
SMTP_HOST=smtp.example.com
SMTP_PORT=587
SMTP_USERNAME=scc-alerts@example.com
SMTP_PASSWORD=your-app-password
SMTP_STARTTLS=1
```

Use an app password or service-specific SMTP credential rather than a personal account password. The backend rate-limits repeated messages for the same device and alarm set using `ALERT_EMAIL_COOLDOWN_SECONDS`.

## Manual Emergency Control

The Operations HMI can queue `KILL 1` and `KILL 0` commands for the ESP32 relay to forward to the Arduino controller. Set `API_WRITE_KEY` on the backend and `VITE_API_WRITE_KEY` on the frontend to the same value when command authentication is enabled. The ESP32 can keep polling:

```text
/api/firmware/commands/next?device=esp32
```

Pending manual kill commands are delivered through that endpoint before OTA commands, so existing relay firmware command polling remains compatible.

## Dependency Explanation

- Flask: API server
- SQLAlchemy: ORM
- PostgreSQL: database
- pandas/numpy: data processing
- scikit-learn: ML models
- joblib: model persistence
- matplotlib/plotly: visualization
- APScheduler: background jobs
- pytest: testing
- xgboost/lightgbm: advanced ML models
