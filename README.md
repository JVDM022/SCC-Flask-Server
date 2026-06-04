# scc-control-platform

A small SCADA-like research control platform for a thermal SCC/corrosion test rig. Arduino SCC firmware owns rig control behavior, while the ESP32 relay firmware handles communications to the Flask backend. PostgreSQL stores the time series, and the backend derives alarms, pump-cycle statistics, ML temperature predictions, and advisory MPC heater PWM recommendations. The React HMI dashboard provides live monitoring, trends, alarms, events, ML predictions, MPC output, and equipment health views.

## Architecture

```text
Arduino SCC controller -> ESP32 relay -> Flask API -> PostgreSQL
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

## API Endpoints

- `POST /api/telemetry`: accept JSON telemetry, validate, store, and run alarm checks.
- `POST /api/telemetry/csv`: accept one CSV line or CSV batch from ESP32.
- `GET /api/latest`: latest telemetry row.
- `GET /api/history?minutes=60`: telemetry history for plotting.
- `GET /api/events?limit=100`: pump and hard-kill events.
- `GET /api/alarms/active`: active alarms.
- `GET /api/pump-cycles`: extracted pump cycle statistics.
- `GET /api/ml/prediction`: predicted temperatures for +1s, +5s, and +10s when models exist.
- `GET /api/mpc/recommendation`: advisory heater PWM recommendation.
- `POST /api/control/setpoint`: store a desired setpoint command.

## Telemetry Format

The backend accepts ESP32 CSV with or without a header. Headerless rows are parsed using this exact order:

```text
event,ms,temp_c,adc,dtemp_c_per_s,setpoint_c,mode,heater_pwm,heating,heater_lockout,pump_enabled,pump_allowed,pump_on,motor_pwm,motor_on_ms,motor_period_ms,temp_before_pump_c,min_temp_after_pump_c,last_pump_drop_c,recovery_time_s,manual_kill,hard_kill,uptime_s
```

Example:

```text
0,123456,126.42,222,-0.1300,125.00,3,184,1,0,1,1,0,155,150,30000,127.20,124.80,2.40,18.50,0,0,300
```

Event codes: `0` normal sample, `1` pump start, `2` pump end, `3` pump recovered, `4` hard kill.

## Safety Philosophy

ESP32 firmware and hardware own hard safety. Flask, ML, and MPC are monitoring and advisory layers, not the only protection layer. Heater lockout, manual kill, and hard kill override all recommendations; MPC returns PWM 0 when any of those states is active.

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
