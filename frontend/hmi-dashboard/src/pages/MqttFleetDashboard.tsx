import { useQuery } from '@tanstack/react-query';
import { Activity, RadioTower, Server, ShieldAlert, Signal, WifiOff } from 'lucide-react';

import { fetchDevices, fetchMqttMessages, fetchMqttStatus } from '../api/client';
import { MetricCard } from '../components/MetricCard';
import { StatusIndicator } from '../components/StatusIndicator';
import { TrendPanel } from '../components/TrendPanel';
import type { Device, MqttMessage } from '../types/domain';
import { formatDateTime, formatNumber } from '../utils';

function toneForDevice(device: Device) {
  if (!device.online) return 'offline';
  if (device.alarm_status && device.alarm_status !== 'normal') return 'critical';
  if ((device.rssi_dbm ?? 0) < -75) return 'warning';
  return 'online';
}

function messagePreview(message: MqttMessage): string {
  if (!message.payload) return message.error || '--';
  return JSON.stringify(message.payload).slice(0, 180);
}

export function MqttFleetDashboard() {
  const statusQuery = useQuery({ queryKey: ['mqtt-status'], queryFn: fetchMqttStatus, refetchInterval: 3000 });
  const devicesQuery = useQuery({ queryKey: ['devices'], queryFn: fetchDevices, refetchInterval: 3000 });
  const messagesQuery = useQuery({ queryKey: ['mqtt-messages'], queryFn: () => fetchMqttMessages(120), refetchInterval: 3000 });

  const status = statusQuery.data;
  const devices = devicesQuery.data || [];
  const messages = messagesQuery.data || [];
  const onlineCount = devices.filter((device) => device.online).length;
  const alarmCount = devices.filter((device) => device.alarm_status && device.alarm_status !== 'normal').length;
  const totalRate = devices.reduce((sum, device) => sum + Number(device.message_rate_hz || 0), 0);
  const rateSeries = devices.slice(0, 12);

  return (
    <main className="workspace mqtt-workspace">
      <section className="metric-grid firmware-metrics">
        <MetricCard icon={Server} label="Broker" value={status?.connected ? 'Connected' : 'Disconnected'} tone={status?.connected ? 'success' : 'critical'} detail={`${status?.host || '--'}:${status?.port || '--'}`} />
        <MetricCard icon={RadioTower} label="Devices" value={`${onlineCount}/${devices.length}`} tone={onlineCount ? 'success' : 'warning'} detail="Online fleet members" />
        <MetricCard icon={Activity} label="Message Rate" value={formatNumber(totalRate, 2)} unit="Hz" tone="accent" detail="Smoothed per-device total" />
        <MetricCard icon={ShieldAlert} label="Alarms" value={alarmCount} tone={alarmCount ? 'critical' : 'success'} detail="Fleet alarm state" />
      </section>

      <section className="workspace-grid workspace-grid--main">
        <section className="panel">
          <div className="panel__header">
            <div>
              <h2>Broker Status</h2>
              <p>Backend MQTT subscriber connection and topic filters</p>
            </div>
            {status?.connected ? <Signal size={18} /> : <WifiOff size={18} />}
          </div>
          <div className="metadata-list">
            <div><span>Connection</span><strong><StatusIndicator label={status?.connected ? 'Connected' : 'Disconnected'} tone={status?.connected ? 'online' : 'critical'} pulse={Boolean(status?.connected)} /></strong></div>
            <div><span>Client ID</span><strong>{status?.client_id || '--'}</strong></div>
            <div><span>Base topic</span><strong>{status?.base_topic || '--'}</strong></div>
            <div><span>Last message</span><strong>{formatDateTime(status?.last_message_at)}</strong></div>
            <div><span>Error</span><strong>{status?.last_error || '--'}</strong></div>
          </div>
        </section>

        <TrendPanel
          title="Telemetry Rate Chart"
          subtitle="Smoothed MQTT message rate by device"
          height={280}
          yTitle="Hz"
          series={[{ name: 'Message rate', type: 'bar', x: rateSeries.map((device) => device.device_id), y: rateSeries.map((device) => device.message_rate_hz || 0), color: '#38BDF8' }]}
        />
      </section>

      <section className="panel">
        <div className="panel__header">
          <div>
            <h2>Device Fleet Table</h2>
            <p>Heartbeat, message health, firmware, RSSI, and alarm status</p>
          </div>
        </div>
        <div className="event-table firmware-table">
          <table>
            <thead>
              <tr>
                <th>Device</th>
                <th>Site / Rig</th>
                <th>Status</th>
                <th>Last heartbeat</th>
                <th>Topic</th>
                <th>Rate</th>
                <th>Firmware</th>
                <th>RSSI</th>
                <th>Alarm</th>
              </tr>
            </thead>
            <tbody>
              {devices.length ? devices.map((device) => (
                <tr key={device.device_id}>
                  <td>{device.device_name || device.device_id}</td>
                  <td>{device.site_id} / {device.rig_id}</td>
                  <td><StatusIndicator label={device.online ? 'Online' : 'Offline'} tone={toneForDevice(device)} pulse={device.online} /></td>
                  <td>{formatDateTime(device.last_heartbeat)}</td>
                  <td>{device.last_topic || '--'}</td>
                  <td>{formatNumber(device.message_rate_hz, 2)} Hz</td>
                  <td>{device.firmware_version || '--'}</td>
                  <td>{device.rssi_dbm ?? '--'} dBm</td>
                  <td>{device.alarm_status || 'normal'}</td>
                </tr>
              )) : <tr><td colSpan={9}>No MQTT devices have reported yet.</td></tr>}
            </tbody>
          </table>
        </div>
      </section>

      <section className="panel">
        <div className="panel__header">
          <div>
            <h2>Live Topic Stream</h2>
            <p>Recent MQTT messages captured by the backend subscriber</p>
          </div>
        </div>
        <div className="event-table mqtt-message-table">
          <table>
            <thead>
              <tr>
                <th>Time</th>
                <th>Topic</th>
                <th>Device</th>
                <th>Type</th>
                <th>QoS</th>
                <th>Valid</th>
                <th>Payload</th>
              </tr>
            </thead>
            <tbody>
              {messages.length ? messages.map((message) => (
                <tr key={message.id}>
                  <td>{formatDateTime(message.created_at)}</td>
                  <td>{message.topic}</td>
                  <td>{message.device_id || '--'}</td>
                  <td>{message.message_type}</td>
                  <td>{message.qos ?? '--'}</td>
                  <td>{message.valid ? 'Yes' : message.error || 'No'}</td>
                  <td>{messagePreview(message)}</td>
                </tr>
              )) : <tr><td colSpan={7}>No MQTT messages captured yet.</td></tr>}
            </tbody>
          </table>
        </div>
      </section>
    </main>
  );
}
