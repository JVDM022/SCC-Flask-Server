import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { AlertTriangle, CheckCircle2, Clipboard, Cpu, FileUp, FolderGit2, RadioTower, Send, TerminalSquare, UploadCloud } from 'lucide-react';
import { useState } from 'react';

import { createFirmwareCommand, fetchFirmwareStatus, getFirmwareArtifacts, getFirmwareCommands, uploadFirmwareArtifact } from '../api/client';
import { MetricCard } from '../components/MetricCard';
import { Skeleton } from '../components/Skeleton';
import { StatusIndicator, type StatusTone } from '../components/StatusIndicator';
import type { FirmwareArtifact, FirmwareCommand, FirmwareDevice, FirmwareTarget, FirmwareUpdateEvent } from '../types/domain';
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

function artifactSize(artifact: FirmwareArtifact): string {
  const size = artifact.sizeBytes ?? artifact.size_bytes ?? 0;
  if (size < 1024) return `${size} B`;
  if (size < 1024 * 1024) return `${(size / 1024).toFixed(1)} KB`;
  return `${(size / (1024 * 1024)).toFixed(2)} MB`;
}

function shortHash(hash?: string): string {
  return hash ? `${hash.slice(0, 12)}...` : '--';
}

function UploadPanel({
  uploadedArtifact,
  onUploaded,
}: {
  uploadedArtifact: FirmwareArtifact | null;
  onUploaded: (artifact: FirmwareArtifact) => void;
}) {
  const queryClient = useQueryClient();
  const [target, setTarget] = useState<FirmwareTarget>('ESP32');
  const [selectedFile, setSelectedFile] = useState<File | null>(null);
  const [message, setMessage] = useState('');
  const accept = target === 'ESP32' ? '.bin' : '.hex';

  const uploadMutation = useMutation({
    mutationFn: () => {
      if (!selectedFile) throw new Error('Choose a firmware artifact first.');
      return uploadFirmwareArtifact(target, selectedFile);
    },
    onSuccess: (artifact) => {
      onUploaded(artifact);
      setMessage(`Uploaded ${artifact.filename}`);
      queryClient.invalidateQueries({ queryKey: ['firmware-artifacts'] });
    },
    onError: (error) => setMessage(error instanceof Error ? error.message : 'Upload failed'),
  });

  const commandMutation = useMutation({
    mutationFn: () => {
      const artifactId = uploadedArtifact?.artifactId ?? uploadedArtifact?.id;
      if (!artifactId || !uploadedArtifact) throw new Error('Upload an artifact before queueing OTA.');
      return createFirmwareCommand(uploadedArtifact.target, artifactId);
    },
    onSuccess: () => {
      setMessage('OTA command queued');
      queryClient.invalidateQueries({ queryKey: ['firmware-commands'] });
    },
    onError: (error) => setMessage(error instanceof Error ? error.message : 'Command queue failed'),
  });

  return (
    <section className="panel firmware-upload-panel">
      <div className="panel__header">
        <div>
          <h2>Firmware Upload</h2>
          <p>Store a firmware artifact, then queue an OTA command for the ESP32 relay to poll.</p>
        </div>
        <FileUp size={20} />
      </div>

      <div className={`firmware-banner ${target === 'ARDUINO' ? 'warning' : 'info'}`}>
        {target === 'ARDUINO' ? <AlertTriangle size={18} /> : <CheckCircle2 size={18} />}
        <span>
          {target === 'ARDUINO'
            ? 'Arduino .hex upload is supported, but Arduino flashing is scaffolding only unless the ESP32 STK500/Optiboot flasher has been implemented.'
            : 'ESP32 .bin OTA can update the ESP32 when ESP32 firmware supports OTA download from URL.'}
        </span>
      </div>

      <div className="firmware-upload-grid">
        <label className="firmware-field">
          <span>Target</span>
          <select
            value={target}
            onChange={(event) => {
              setTarget(event.target.value as FirmwareTarget);
              setSelectedFile(null);
              setMessage('');
            }}
          >
            <option value="ESP32">ESP32</option>
            <option value="ARDUINO">Arduino</option>
          </select>
        </label>

        <label className="firmware-field">
          <span>Artifact</span>
          <input
            type="file"
            accept={accept}
            onChange={(event) => {
              setSelectedFile(event.target.files?.[0] || null);
              setMessage('');
            }}
          />
        </label>

        <div className="firmware-upload-actions">
          <button className="command-button" onClick={() => uploadMutation.mutate()} disabled={uploadMutation.isPending}>
            <UploadCloud size={16} />
            {uploadMutation.isPending ? 'Uploading' : 'Upload'}
          </button>
          <button className="command-button" onClick={() => commandMutation.mutate()} disabled={!uploadedArtifact || commandMutation.isPending}>
            <Send size={16} />
            {commandMutation.isPending ? 'Queueing' : 'Queue OTA'}
          </button>
        </div>
      </div>

      {message ? <div className="firmware-result">{message}</div> : null}

      {uploadedArtifact ? (
        <div className="command-card">
          <div className="command-card__header">
            <div>
              <span>Latest upload</span>
              <strong>{uploadedArtifact.filename}</strong>
            </div>
            <StatusIndicator label={uploadedArtifact.target} tone={uploadedArtifact.target === 'ESP32' ? 'online' : 'warning'} />
          </div>
          <div className="command-meta">
            <div><span>URL</span><strong>{uploadedArtifact.url}</strong></div>
            <div><span>SHA-256</span><strong>{uploadedArtifact.sha256}</strong></div>
            <div><span>Size</span><strong>{artifactSize(uploadedArtifact)}</strong></div>
            <div><span>Target</span><strong>{uploadedArtifact.target === 'ARDUINO' ? 'Arduino' : 'ESP32'}</strong></div>
          </div>
        </div>
      ) : null}
    </section>
  );
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

function ArtifactTable({ artifacts }: { artifacts: FirmwareArtifact[] }) {
  return (
    <div className="event-table firmware-artifacts">
      <table>
        <thead>
          <tr>
            <th>Time</th>
            <th>Target</th>
            <th>Filename</th>
            <th>Size</th>
            <th>SHA-256</th>
            <th>URL</th>
          </tr>
        </thead>
        <tbody>
          {artifacts.length ? (
            artifacts.map((artifact) => (
              <tr key={artifact.id}>
                <td>{formatDateTime(artifact.created_at)}</td>
                <td>{artifact.target}</td>
                <td>{valueOrDash(artifact.filename)}</td>
                <td>{artifactSize(artifact)}</td>
                <td>{shortHash(artifact.sha256)}</td>
                <td>{valueOrDash(artifact.url)}</td>
              </tr>
            ))
          ) : (
            <tr><td colSpan={6}>No firmware artifacts have been uploaded.</td></tr>
          )}
        </tbody>
      </table>
    </div>
  );
}

function CommandTable({ commands }: { commands: FirmwareCommand[] }) {
  return (
    <div className="event-table firmware-commands">
      <table>
        <thead>
          <tr>
            <th>Time</th>
            <th>Command</th>
            <th>Target</th>
            <th>Status</th>
            <th>Ack</th>
            <th>Message</th>
          </tr>
        </thead>
        <tbody>
          {commands.length ? (
            commands.map((command) => (
              <tr key={command.id}>
                <td>{formatDateTime(command.created_at)}</td>
                <td>{command.command_type} #{command.cmdId ?? command.id}</td>
                <td>{command.target}</td>
                <td><StatusIndicator label={command.status} tone={otaTone(command.status)} /></td>
                <td>{valueOrDash(command.ack_status)}</td>
                <td>{valueOrDash(command.ack_message)}</td>
              </tr>
            ))
          ) : (
            <tr><td colSpan={6}>No firmware commands have been queued.</td></tr>
          )}
        </tbody>
      </table>
    </div>
  );
}

export function FirmwareOtaDashboard() {
  const [uploadedArtifact, setUploadedArtifact] = useState<FirmwareArtifact | null>(null);
  const query = useQuery({
    queryKey: ['firmware-status'],
    queryFn: fetchFirmwareStatus,
    refetchInterval: 3000,
    staleTime: 1500,
    retry: 1,
  });
  const artifactQuery = useQuery({
    queryKey: ['firmware-artifacts'],
    queryFn: getFirmwareArtifacts,
    refetchInterval: 5000,
  });
  const commandQuery = useQuery({
    queryKey: ['firmware-commands'],
    queryFn: getFirmwareCommands,
    refetchInterval: 5000,
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

      <UploadPanel uploadedArtifact={uploadedArtifact} onUploaded={setUploadedArtifact} />

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
            <h2>Artifacts</h2>
            <p>Recent uploaded binaries and hex files served from the backend firmware storage path.</p>
          </div>
        </div>
        <ArtifactTable artifacts={artifactQuery.data || []} />
      </section>

      <section className="panel">
        <div className="panel__header">
          <div>
            <h2>Queued Commands</h2>
            <p>Pending, sent, and acknowledged firmware commands polled by the ESP32 relay.</p>
          </div>
        </div>
        <CommandTable commands={commandQuery.data || []} />
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
