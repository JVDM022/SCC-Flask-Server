import { useQuery } from '@tanstack/react-query';
import { CheckCircle2, Clipboard, Cpu, FolderGit2, RadioTower, TerminalSquare, UploadCloud } from 'lucide-react';
import { useState } from 'react';

import { fetchFirmwareStatus } from '../api/client';
import { MetricCard } from '../components/MetricCard';
import { Skeleton } from '../components/Skeleton';
import { StatusIndicator, type StatusTone } from '../components/StatusIndicator';
import type { FirmwareDevice, FirmwareUpdateEvent } from '../types/domain';
import { formatDateTime } from '../utils';

function otaTone(status?: string | null): StatusTone {
  const normalized = String(status || '').toLowerCase();
  if (['online', 'idle', 'reconnected', 'version verified'].includes(normalized)) return 'online';
  if (['ota started', 'flashing', 'rebooting'].includes(normalized)) return 'warning';
  if (['failed', 'offline'].includes(normalized)) return 'critical';
  return 'neutral';
}

function valueOrDash(value?: string | number | null): string {
  if (value === null || value === undefined || value === '') return '--';
  return String(value);
}

function copyCommand(command: string, setCopied: (copied: boolean) => void) {
  navigator.clipboard.writeText(command).then(() => {
    setCopied(true);
    window.setTimeout(() => setCopied(false), 1200);
  });
}

function CommandHelper({ device }: { device: FirmwareDevice }) {
  const [copied, setCopied] = useState(false);
  const command = device.ota_command || `pio run -t upload --upload-port ${device.ip_address || '<device_ip>'}`;

  return (
    <section className="command-card">
      <div className="command-card__header">
        <div>
          <span>{valueOrDash(device.device_name)}</span>
          <strong>{valueOrDash(device.device_id)}</strong>
        </div>
        <button className="command-button compact" onClick={() => copyCommand(command, setCopied)}>
          <Clipboard size={15} />
          {copied ? 'Copied' : 'Copy'}
        </button>
      </div>
      <code>{command}</code>
      <div className="command-meta">
        <div><span>PlatformIO env</span><strong>{valueOrDash(device.platformio_env)}</strong></div>
        <div><span>Firmware source</span><strong>{valueOrDash(device.firmware_source_dir)}</strong></div>
        <div><span>Last known IP</span><strong>{valueOrDash(device.last_known_ip || device.ip_address)}</strong></div>
        <div><span>OTA port</span><strong>{valueOrDash(device.ota_port)}</strong></div>
      </div>
    </section>
  );
}

function FirmwareTable({ devices }: { devices: FirmwareDevice[] }) {
  return (
    <div className="event-table firmware-table">
      <table>
        <thead>
          <tr>
            <th>Device name</th>
            <th>Device ID</th>
            <th>IP address</th>
            <th>MAC address</th>
            <th>Firmware</th>
            <th>Build timestamp</th>
            <th>PlatformIO env</th>
            <th>Status</th>
            <th>Last heartbeat</th>
            <th>Last OTA update</th>
          </tr>
        </thead>
        <tbody>
          {devices.length ? (
            devices.map((device) => (
              <tr key={device.device_id}>
                <td>{valueOrDash(device.device_name)}</td>
                <td>{valueOrDash(device.device_id)}</td>
                <td>{valueOrDash(device.ip_address)}</td>
                <td>{valueOrDash(device.mac_address)}</td>
                <td>{valueOrDash(device.firmware_version)}</td>
                <td>{valueOrDash(device.build_time)}</td>
                <td>{valueOrDash(device.platformio_env)}</td>
                <td>
                  <div className="firmware-status-stack">
                    <StatusIndicator label={device.online ? 'Online' : 'Offline'} tone={device.online ? 'online' : 'offline'} pulse={Boolean(device.online)} />
                    <StatusIndicator label={valueOrDash(device.ota_status)} tone={otaTone(device.ota_status)} />
                  </div>
                </td>
                <td>{formatDateTime(device.last_heartbeat)}</td>
                <td>{formatDateTime(device.last_ota_update_time)}</td>
              </tr>
            ))
          ) : (
            <tr><td colSpan={10}>No ESP32 firmware heartbeat has been received.</td></tr>
          )}
        </tbody>
      </table>
    </div>
  );
}

function EventTable({ events }: { events: FirmwareUpdateEvent[] }) {
  return (
    <div className="event-table firmware-events">
      <table>
        <thead>
          <tr>
            <th>Time</th>
            <th>Device ID</th>
            <th>Event</th>
            <th>OTA status</th>
            <th>Firmware</th>
            <th>IP address</th>
            <th>Message</th>
          </tr>
        </thead>
        <tbody>
          {events.length ? (
            events.map((event, index) => (
              <tr key={event.id ?? index}>
                <td>{formatDateTime(event.created_at)}</td>
                <td>{valueOrDash(event.device_id)}</td>
                <td>{event.event_type}</td>
                <td><StatusIndicator label={event.ota_status} tone={otaTone(event.ota_status)} /></td>
                <td>{valueOrDash(event.firmware_version)}</td>
                <td>{valueOrDash(event.ip_address)}</td>
                <td>{valueOrDash(event.message)}</td>
              </tr>
            ))
          ) : (
            <tr><td colSpan={7}>No OTA update events have been recorded.</td></tr>
          )}
        </tbody>
      </table>
    </div>
  );
}

export function FirmwareOtaDashboard() {
  const query = useQuery({
    queryKey: ['firmware-status'],
    queryFn: fetchFirmwareStatus,
    refetchInterval: 3000,
    staleTime: 1500,
    retry: 1,
  });
  const status = query.data;
  const devices = status?.devices || [];

  if (query.isLoading) {
    return (
      <main className="workspace">
        <Skeleton rows={5} />
      </main>
    );
  }

  return (
    <main className="workspace firmware-workspace">
      {query.isError ? (
        <section className="panel error-panel">
          <div className="panel__header">
            <div>
              <h2>Firmware API Unavailable</h2>
              <p>{query.error instanceof Error ? query.error.message : 'Unable to reach firmware status API'}</p>
            </div>
          </div>
        </section>
      ) : null}

      <section className="metric-grid firmware-metrics">
        <MetricCard label="Devices" value={status?.device_count ?? 0} icon={Cpu} detail="ESP32 records tracked" />
        <MetricCard label="Online" value={status?.online_count ?? 0} icon={RadioTower} tone="success" detail="Heartbeat inside live window" />
        <MetricCard label="PlatformIO Env" value={valueOrDash(status?.platformio_env)} icon={TerminalSquare} tone="accent" detail="Build/upload environment" />
        <MetricCard label="Firmware Source" value={valueOrDash(status?.firmware_source_dir)} icon={FolderGit2} detail="Run PlatformIO here" />
      </section>

      <section className="panel">
        <div className="panel__header">
          <div>
            <h2>Device Status</h2>
            <p>Firmware version, build metadata, heartbeat, and reported OTA state from ESP32/backend status messages.</p>
          </div>
          <UploadCloud size={20} />
        </div>
        <FirmwareTable devices={devices} />
      </section>

      <section className="panel">
        <div className="panel__header">
          <div>
            <h2>PlatformIO OTA Command Helper</h2>
            <p>Copy and run from the firmware source directory. The dashboard does not upload firmware binaries.</p>
          </div>
          <TerminalSquare size={20} />
        </div>
        {devices.length ? (
          <div className="command-grid">
            {devices.map((device) => <CommandHelper key={device.device_id} device={device} />)}
          </div>
        ) : (
          <section className="command-card">
            <div className="command-card__header">
              <div>
                <span>Command format</span>
                <strong>Waiting for device IP</strong>
              </div>
              <CheckCircle2 size={18} />
            </div>
            <code>pio run -t upload --upload-port &lt;ESP32_IP_ADDRESS&gt;</code>
            <div className="command-meta">
              <div><span>PlatformIO env</span><strong>{valueOrDash(status?.platformio_env)}</strong></div>
              <div><span>Firmware source</span><strong>{valueOrDash(status?.firmware_source_dir)}</strong></div>
              <div><span>OTA port</span><strong>{valueOrDash(status?.ota_port)}</strong></div>
            </div>
          </section>
        )}
      </section>

      <section className="panel">
        <div className="panel__header">
          <div>
            <h2>OTA History</h2>
            <p>Events from OTA started, flashing, rebooting, failed, reconnected, and version verification reports.</p>
          </div>
        </div>
        <EventTable events={status?.events || []} />
      </section>
    </main>
  );
}
