import type {
  Alarm,
  DashboardData,
  Device,
  FirmwareStatus,
  MqttMessage,
  MqttStatus,
  MpcRecommendation,
  PredictionResponse,
  PumpCycle,
  RigEvent,
  Telemetry,
} from '../types/domain';

const API_BASE = import.meta.env.VITE_API_BASE_URL || 'http://localhost:5000/api';

async function getJson<T>(path: string): Promise<T> {
  const response = await fetch(`${API_BASE}${path}`);
  if (!response.ok) {
    throw new Error(`${path} returned ${response.status}`);
  }
  return response.json() as Promise<T>;
}

function normalizeLatest(value: Telemetry | { status?: string }): Telemetry | null {
  if (!value || value.status === 'unavailable') {
    return null;
  }
  return value as Telemetry;
}

export async function fetchDashboardData(): Promise<DashboardData> {
  const [latest, history, alarms, events, cycles, prediction, mpc] = await Promise.all([
    getJson<Telemetry | { status?: string }>('/latest'),
    getJson<Telemetry[]>('/history?minutes=180'),
    getJson<Alarm[]>('/alarms/active'),
    getJson<RigEvent[]>('/events?limit=250'),
    getJson<PumpCycle[]>('/pump-cycles'),
    getJson<PredictionResponse>('/ml/prediction'),
    getJson<MpcRecommendation>('/mpc/recommendation'),
  ]);

  return {
    latest: normalizeLatest(latest),
    history: Array.isArray(history) ? history : [],
    alarms: Array.isArray(alarms) ? alarms : [],
    events: Array.isArray(events) ? events : [],
    cycles: Array.isArray(cycles) ? cycles : [],
    prediction: prediction || { status: 'unavailable', predictions: [] },
    mpc: mpc || { status: 'unavailable' },
  };
}

export async function fetchFirmwareStatus(): Promise<FirmwareStatus> {
  const status = await getJson<FirmwareStatus>('/firmware/status');
  return {
    status: status?.status || 'unavailable',
    platformio_env: status?.platformio_env || '',
    firmware_source_dir: status?.firmware_source_dir || '',
    ota_port: status?.ota_port || '',
    device_count: Number(status?.device_count || 0),
    online_count: Number(status?.online_count || 0),
    devices: Array.isArray(status?.devices) ? status.devices : [],
    events: Array.isArray(status?.events) ? status.events : [],
  };
}

export async function fetchMqttStatus(): Promise<MqttStatus> {
  return getJson<MqttStatus>('/mqtt/status');
}

export async function fetchMqttMessages(limit = 100): Promise<MqttMessage[]> {
  const messages = await getJson<MqttMessage[]>(`/mqtt/messages?limit=${limit}`);
  return Array.isArray(messages) ? messages : [];
}

export async function fetchDevices(): Promise<Device[]> {
  const devices = await getJson<Device[]>('/devices');
  return Array.isArray(devices) ? devices : [];
}

export async function fetchLatestTelemetry(): Promise<Telemetry | { status?: string }> {
  return getJson<Telemetry | { status?: string }>('/telemetry/latest');
}
