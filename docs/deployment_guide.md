# 10. Deployment Guide

## 10.1. Server Requirements

**Operating System:** Linux, macOS, or Windows with Docker Desktop. A Linux host is preferred for continuous operation.

**CPU:** Minimum 2 CPU cores. Four cores are recommended if running the backend, database, MQTT broker, and dashboard on the same machine.

**RAM:** Minimum 4 GB. Eight GB is recommended for stable Docker operation, database caching, and frontend development services.

**Storage:** Minimum 10 GB free disk space. Twenty GB or more is recommended to allow PostgreSQL data growth, Docker images, firmware artifacts, logs, and model files.

**Network:** The Intel NUC gateway must be able to reach the backend API. Required local ports are `5050` for the backend API, `5173` for the dashboard, and `5432` for PostgreSQL if direct database access is needed. Port `1883` is only required when using the legacy ESP32 MQTT relay.

**Software:** Docker Engine or Docker Desktop with Docker Compose support. For non-Docker development, Python `3.11`, Node.js `22`, PostgreSQL, Mosquitto, and the backend Python dependencies are required.

## 10.2. Docker Services

| Service | Image or build source | Port | Purpose |
| --- | --- | --- | --- |
| Backend | `backend/Dockerfile` using Python `3.11-slim` | `5050:5000` | Runs the Flask/Gunicorn API, MQTT subscriber, telemetry parser, alarm rules, ML prediction, MPC advisory logic, and firmware command APIs |
| Frontend | `node:22-alpine` | `5173:5173` | Runs the React/Vite HMI dashboard for live monitoring, alarms, trends, MPC, ML, equipment health, settings, and firmware views |
| PostgreSQL | `postgres:16` | `5432:5432` | Stores telemetry, alarms, events, pump-cycle records, predictions, command state, and other backend data |
| MQTT Broker | `eclipse-mosquitto:2` | `1883:1883`, `9001:9001` | Receives legacy ESP32 telemetry messages and exposes MQTT/WebSocket transport for local communication |
| Cloudflare Tunnel | `cloudflare/cloudflared:latest` | outbound tunnel | Optional public access layer for the backend and dashboard when `CLOUDFLARE_TUNNEL_TOKEN` is configured |

## 10.3. Deployment Procedure

1. Clone or copy the repository to the deployment server.

   ```bash
   git clone <repository-url>
   cd scc-control-platform
   ```

2. Create the environment file from the example.

   ```bash
   cp .env.example .env
   ```

3. Configure required environment variables in `.env`.

   ```text
   API_WRITE_KEY=<shared-write-key-for-backend-and-dashboard>
   CORS_ORIGINS=http://localhost:5173
   PUBLIC_BASE_URL=http://localhost:5050
   MQTT_HOST=mosquitto
   MQTT_PORT=1883
   MQTT_BASE_TOPIC=scc
   ```

   If email alerts are required, also configure `ALERT_EMAIL_ENABLED`, `ALERT_EMAIL_TO`, `ALERT_EMAIL_FROM`, `SMTP_HOST`, `SMTP_PORT`, `SMTP_USERNAME`, `SMTP_PASSWORD`, and `SMTP_STARTTLS`.

4. Start the Docker services.

   ```bash
   docker compose up --build
   ```

5. Verify that the containers are running.

   ```bash
   docker compose ps
   ```

6. Open the application endpoints.

   ```text
   Backend API: http://localhost:5050
   Frontend HMI: http://localhost:5173
   PostgreSQL: localhost:5432
   MQTT broker: localhost:1883
   ```

7. Start the Intel NUC USB gateway on the host connected to the Arduino.

   ```bash
   cd backend
   python -m app.services.nuc_gateway --serial-port /dev/ttyACM0 --api-base-url http://localhost:5050
   ```

8. Verify connectivity by confirming that telemetry reaches the backend and appears in the dashboard.

   ```text
   GET http://localhost:5050/api/latest
   ```

9. Run validation checks before operating the SCC rig.

   ```bash
   pytest backend/tests
   pio test -d firmware/arduino/SCC-V1.4 -e native
   ```

10. For production or remote access, configure the Cloudflare tunnel token in `.env` and confirm the public URLs route to the backend and frontend services.

    ```text
    CLOUDFLARE_TUNNEL_TOKEN=<cloudflare-token>
    ```

## 10.4. Post-Deployment Checks

| Check | Expected Result |
| --- | --- |
| Backend health/API access | Backend responds on `http://localhost:5050` |
| Dashboard access | HMI loads on `http://localhost:5173` |
| Database startup | PostgreSQL container reports healthy |
| NUC gateway connectivity | Gateway logs show Arduino serial reads and successful backend telemetry posts |
| MQTT broker availability | Required only for legacy ESP32 relay deployments |
| Latest telemetry | `/api/latest` returns the most recent telemetry row after gateway publishing begins |
| Alarm display | Active safety alarms appear in backend API responses and the dashboard |
| Manual kill command | `KILL 1` command can be queued only when write authentication is configured |
| Data persistence | PostgreSQL volume `postgres-data` persists after container restart |
