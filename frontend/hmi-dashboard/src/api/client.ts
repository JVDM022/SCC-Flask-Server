import type {
  Alarm,
  DashboardData,
  Device,
  FirmwareArtifact,
  FirmwareCommand,
  FirmwareStatus,
  FirmwareTarget,
  MqttMessage,
  MqttStatus,
  MpcRecommendation,
  PredictionResponse,
  PumpCycle,
  RigEvent,
  Telemetry,
} from '../types/domain';

const API_BASE = import.meta.env.VITE_API_BASE_URL || 'http://localhost:5000/api';
const API_WRITE_KEY = import.meta.env.VITE_API_WRITE_KEY || '';
const TELEMETRY_FIELDS = [
  'event',
  'ms',
  'temp_c',
  'adc',
  'dtemp_c_per_s',
  'setpoint_c',
  'mode',
  'heater_pwm',
  'heating',
  'heater_lockout',
  'pump_enabled',
  'pump_allowed',
  'pump_on',
  'motor_pwm',
  'motor_on_ms',
  'motor_period_ms',
  'temp_before_pump_c',
  'min_temp_after_pump_c',
  'last_pump_drop_c',
  'recovery_time_s',
  'manual_kill',
  'hard_kill',
  'uptime_s',
] as const;

async function getJson<T>(path: string): Promise<T> {
  const response = await fetch(`${API_BASE}${path}`);
  if (!response.ok) {
    throw new Error(`${path} returned ${response.status}`);
  }
  return response.json() as Promise<T>;
}

async function postJson<T>(path: string, body: unknown): Promise<T> {
  const headers: Record<string, string> = { 'Content-Type': 'application/json' };
  if (API_WRITE_KEY) {
    headers['X-API-Key'] = API_WRITE_KEY;
  }
  const response = await fetch(`${API_BASE}${path}`, {
    method: 'POST',
    headers,
    body: JSON.stringify(body),
  });
  if (!response.ok) {
    const message = await response.text();
    throw new Error(message || `${path} returned ${response.status}`);
  }
  return response.json() as Promise<T>;
}

function settledValue<T>(result: PromiseSettledResult<T>, fallback: T): T {
  return result.status === 'fulfilled' ? result.value : fallback;
}

function normalizeTelemetry(value: Partial<Telemetry> | { status?: string } | null | undefined): Telemetry | null {
  if (!value || value.status === 'unavailable') {
    return null;
  }
  const normalized = { ...value } as Record<string, unknown>;
  for (const field of TELEMETRY_FIELDS) {
    if (normalized[field] === undefined) {
      normalized[field] = null;
    }
  }
  return normalized as unknown as Telemetry;
}

function normalizeTelemetryList(value: unknown): Telemetry[] {
  if (!Array.isArray(value)) {
    return [];
  }
  return value.map((item) => normalizeTelemetry(item)).filter((item): item is Telemetry => item !== null);
}

export async function fetchDashboardData(): Promise<DashboardData> {
  const [latestResult, historyResult, alarmsResult, eventsResult, cyclesResult, predictionResult, mpcResult] = await Promise.allSettled([
    getJson<Telemetry | { status?: string }>('/latest'),
    getJson<Telemetry[]>('/history?minutes=180'),
    getJson<Alarm[]>('/alarms/active'),
    getJson<RigEvent[]>('/events?limit=250'),
    getJson<PumpCycle[]>('/pump-cycles'),
    getJson<PredictionResponse>('/ml/prediction'),
    getJson<MpcRecommendation>('/mpc/recommendation'),
  ]);
  const latest = settledValue<Telemetry | { status?: string }>(latestResult, { status: 'unavailable' });
  const history = settledValue<Telemetry[]>(historyResult, []);
  const alarms = settledValue<Alarm[]>(alarmsResult, []);
  const events = settledValue<RigEvent[]>(eventsResult, []);
  const cycles = settledValue<PumpCycle[]>(cyclesResult, []);
  const prediction = settledValue<PredictionResponse>(predictionResult, { status: 'unavailable', predictions: [] });
  const mpc = settledValue<MpcRecommendation>(mpcResult, { status: 'unavailable' });

  return {
    latest: normalizeTelemetry(latest),
    history: normalizeTelemetryList(history),
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

export async function uploadFirmwareArtifact(target: FirmwareTarget, file: File): Promise<FirmwareArtifact> {
  const form = new FormData();
  form.append('target', target);
  form.append('file', file);
  const response = await fetch(`${API_BASE}/firmware/artifacts`, {
    method: 'POST',
    body: form,
  });
  if (!response.ok) {
    const message = await response.text();
    throw new Error(message || `/firmware/artifacts returned ${response.status}`);
  }
  return response.json() as Promise<FirmwareArtifact>;
}

export async function createFirmwareCommand(target: FirmwareTarget, artifactId: number): Promise<{ status: string; command: Record<string, unknown> }> {
  return postJson('/firmware/commands', { target, artifactId });
}

export async function getFirmwareArtifacts(): Promise<FirmwareArtifact[]> {
  const result = await getJson<{ artifacts: FirmwareArtifact[] }>('/firmware/artifacts');
  return Array.isArray(result.artifacts) ? result.artifacts : [];
}

export async function getFirmwareCommands(): Promise<FirmwareCommand[]> {
  const result = await getJson<{ commands: FirmwareCommand[] }>('/firmware/commands');
  return Array.isArray(result.commands) ? result.commands : [];
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

export async function queueManualKill(enabled: boolean): Promise<{ status: string; id: number; manual_kill: boolean }> {
  return postJson('/control/manual-kill', { enabled });
}
