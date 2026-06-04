export type Severity = 'info' | 'warning' | 'critical' | string;

export interface Telemetry {
  id?: number;
  created_at?: string;
  event: number;
  ms: number;
  temp_c: number;
  adc: number;
  dtemp_c_per_s: number;
  setpoint_c: number;
  mode: number;
  heater_pwm: number;
  heating: number;
  heater_lockout: number;
  pump_enabled: number;
  pump_allowed: number;
  pump_on: number;
  motor_pwm: number;
  motor_on_ms: number;
  motor_period_ms: number;
  temp_before_pump_c: number;
  min_temp_after_pump_c: number;
  last_pump_drop_c: number;
  recovery_time_s: number;
  manual_kill: number;
  hard_kill: number;
  uptime_s: number;
  status?: string;
}

export interface Alarm {
  id?: number;
  created_at?: string;
  cleared_at?: string | null;
  active?: boolean;
  severity: Severity;
  alarm_code: string;
  message: string;
  telemetry_id?: number;
}

export interface RigEvent {
  id?: number;
  created_at?: string;
  event_code: number;
  event_name: string;
  severity: Severity;
  message: string;
  telemetry_id?: number;
}

export interface PumpCycle {
  id?: number;
  pump_start_time?: string | null;
  pump_end_time?: string | null;
  recovery_time?: string | null;
  temp_before_pump_c?: number | null;
  min_temp_after_pump_c?: number | null;
  last_pump_drop_c?: number | null;
  recovery_time_s?: number | null;
  avg_heater_pwm?: number | null;
  avg_motor_pwm?: number | null;
}

export interface Prediction {
  status: 'ok' | 'unavailable' | string;
  horizon_s: number;
  current_temp_c?: number;
  predicted_delta_c?: number;
  predicted_temp_c?: number;
  message?: string;
}

export interface PredictionResponse {
  status: 'ok' | 'unavailable' | string;
  available_horizons?: number[];
  predictions?: Prediction[];
  message?: string;
}

export interface MpcRecommendation {
  status: 'ok' | 'unavailable' | 'safety_override' | string;
  current_pwm?: number;
  recommended_pwm?: number;
  predicted_temp_c?: number | null;
  setpoint_c?: number;
  cost?: number | null;
  model_status?: string;
  advisory_only?: boolean;
  message?: string;
}

export interface FirmwareDevice {
  device_id: string;
  device_name?: string | null;
  ip_address?: string | null;
  mac_address?: string | null;
  firmware_version?: string | null;
  build_time?: string | null;
  platformio_env?: string | null;
  firmware_source_dir?: string | null;
  ota_port?: string | null;
  ota_status?: string | null;
  uptime?: number | null;
  online?: boolean;
  online_status?: string;
  last_heartbeat?: string | null;
  last_ota_update_time?: string | null;
  last_known_ip?: string | null;
  ota_command?: string;
}

export interface FirmwareUpdateEvent {
  id?: number;
  created_at?: string;
  device_id?: string | null;
  event_type: string;
  ota_status: string;
  firmware_version?: string | null;
  build_time?: string | null;
  ip_address?: string | null;
  message?: string | null;
}

export interface FirmwareStatus {
  status: 'ok' | 'unavailable' | string;
  platformio_env?: string;
  firmware_source_dir?: string;
  ota_port?: string;
  device_count: number;
  online_count: number;
  devices: FirmwareDevice[];
  events: FirmwareUpdateEvent[];
}

export interface Device {
  device_id: string;
  site_id: string;
  rig_id: string;
  device_name?: string | null;
  firmware_version?: string | null;
  ip_address?: string | null;
  mac_address?: string | null;
  rssi_dbm?: number | null;
  online: boolean;
  state?: string | null;
  last_topic?: string | null;
  last_payload_at?: string | null;
  last_heartbeat?: string | null;
  message_count: number;
  message_rate_hz: number;
  alarm_status: string;
  updated_at?: string;
}

export interface MqttMessage {
  id?: number;
  created_at?: string;
  topic: string;
  site_id?: string | null;
  rig_id?: string | null;
  device_id?: string | null;
  message_type: string;
  qos?: number | null;
  retained: boolean;
  valid: boolean;
  error?: string | null;
  payload?: Record<string, unknown> | null;
}

export interface MqttStatus {
  connected: boolean;
  started: boolean;
  host: string;
  port: number;
  client_id: string;
  base_topic: string;
  last_message_at?: string | null;
  last_error?: string;
  subscriptions: string[];
}

export interface DashboardData {
  latest: Telemetry | null;
  history: Telemetry[];
  alarms: Alarm[];
  events: RigEvent[];
  cycles: PumpCycle[];
  prediction: PredictionResponse;
  mpc: MpcRecommendation;
}

export interface ModelCardData {
  horizon: 1 | 5 | 10;
  status: string;
  rmse?: number | null;
  mae?: number | null;
  lastTrained?: string | null;
  datasetSize?: number | null;
}

export type PageId =
  | 'operations'
  | 'analytics'
  | 'models'
  | 'mpc'
  | 'digital-twin'
  | 'equipment-health'
  | 'firmware-ota'
  | 'mqtt-fleet'
  | 'events'
  | 'settings';
