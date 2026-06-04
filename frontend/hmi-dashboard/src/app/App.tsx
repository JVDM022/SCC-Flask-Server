import {
  Activity,
  AlertTriangle,
  BrainCircuit,
  Cpu,
  Database,
  FlaskConical,
  Gauge,
  RadioTower,
  Settings,
  ShieldAlert,
  UploadCloud,
  type LucideIcon,
} from 'lucide-react';
import { useQueryClient } from '@tanstack/react-query';
import { useEffect, useMemo, useState } from 'react';
import { io } from 'socket.io-client';

import { Skeleton } from '../components/Skeleton';
import { StatusIndicator, type StatusTone } from '../components/StatusIndicator';
import { useDashboardData } from '../hooks/useDashboardData';
import { AnalyticsDashboard } from '../pages/AnalyticsDashboard';
import { DigitalTwinDashboard } from '../pages/DigitalTwinDashboard';
import { EquipmentHealthDashboard } from '../pages/EquipmentHealthDashboard';
import { EventIntelligenceDashboard } from '../pages/EventIntelligenceDashboard';
import { FirmwareOtaDashboard } from '../pages/FirmwareOtaDashboard';
import { MLModelsDashboard } from '../pages/MLModelsDashboard';
import { MpcDashboard } from '../pages/MpcDashboard';
import { MqttFleetDashboard } from '../pages/MqttFleetDashboard';
import { OperationsDashboard } from '../pages/OperationsDashboard';
import { SettingsDashboard } from '../pages/SettingsDashboard';
import type { DashboardData, PageId } from '../types/domain';
import { modeLabel } from '../utils';

const emptyData: DashboardData = {
  latest: null,
  history: [],
  alarms: [],
  events: [],
  cycles: [],
  prediction: { status: 'unavailable', predictions: [] },
  mpc: { status: 'unavailable' },
};

const navItems: Array<{ id: PageId; label: string; icon: LucideIcon }> = [
  { id: 'operations', label: 'Operations', icon: Gauge },
  { id: 'analytics', label: 'Analytics', icon: Activity },
  { id: 'models', label: 'ML Models', icon: BrainCircuit },
  { id: 'mpc', label: 'MPC', icon: Cpu },
  { id: 'digital-twin', label: 'Digital Twin', icon: FlaskConical },
  { id: 'equipment-health', label: 'Equipment Health', icon: ShieldAlert },
  { id: 'firmware-ota', label: 'Firmware OTA', icon: UploadCloud },
  { id: 'mqtt-fleet', label: 'MQTT Fleet', icon: RadioTower },
  { id: 'events', label: 'Events', icon: RadioTower },
  { id: 'settings', label: 'Settings', icon: Settings },
];

function connectionTone(data: DashboardData, isError: boolean): StatusTone {
  if (isError) return 'offline';
  if (data.latest?.hard_kill || data.latest?.manual_kill) return 'critical';
  if (data.alarms.some((alarm) => alarm.severity === 'critical')) return 'critical';
  if (data.alarms.length) return 'warning';
  return data.latest ? 'online' : 'offline';
}

function renderPage(page: PageId, data: DashboardData) {
  if (page === 'analytics') return <AnalyticsDashboard data={data} />;
  if (page === 'models') return <MLModelsDashboard data={data} />;
  if (page === 'mpc') return <MpcDashboard data={data} />;
  if (page === 'digital-twin') return <DigitalTwinDashboard data={data} />;
  if (page === 'equipment-health') return <EquipmentHealthDashboard data={data} />;
  if (page === 'firmware-ota') return <FirmwareOtaDashboard />;
  if (page === 'mqtt-fleet') return <MqttFleetDashboard />;
  if (page === 'events') return <EventIntelligenceDashboard data={data} />;
  if (page === 'settings') return <SettingsDashboard data={data} />;
  return <OperationsDashboard data={data} />;
}

export function App() {
  const [activePage, setActivePage] = useState<PageId>('operations');
  const queryClient = useQueryClient();
  const query = useDashboardData();
  const data = query.data || emptyData;
  const activeNav = navItems.find((item) => item.id === activePage) || navItems[0];
  const activeController = useMemo(() => modeLabel(data.latest?.mode), [data.latest?.mode]);
  const tone = connectionTone(data, query.isError);

  useEffect(() => {
    const baseUrl = import.meta.env.VITE_WS_URL || 'http://localhost:5050';
    const socket = io(baseUrl, { transports: ['websocket', 'polling'] });
    const invalidateLiveData = () => {
      queryClient.invalidateQueries({ queryKey: ['dashboard-data'] });
      queryClient.invalidateQueries({ queryKey: ['mqtt-status'] });
      queryClient.invalidateQueries({ queryKey: ['mqtt-messages'] });
      queryClient.invalidateQueries({ queryKey: ['devices'] });
    };
    socket.on('telemetry_update', invalidateLiveData);
    socket.on('mqtt_message', invalidateLiveData);
    socket.on('device_update', invalidateLiveData);
    return () => {
      socket.close();
    };
  }, [queryClient]);

  return (
    <div className="app-shell">
      <aside className="side-nav">
        <div className="platform-mark">
          <Database size={22} />
          <div>
            <strong>SCC Control Platform</strong>
            <span>Thermal corrosion operations</span>
          </div>
        </div>
        <nav aria-label="Primary">
          {navItems.map((item) => {
            const Icon = item.icon;
            return (
              <button className={activePage === item.id ? 'active' : ''} key={item.id} onClick={() => setActivePage(item.id)}>
                <Icon size={17} />
                <span>{item.label}</span>
              </button>
            );
          })}
        </nav>
      </aside>

      <section className="app-main">
        <header className="top-header">
          <div className="header-title">
            <span className="eyebrow">Engineering Operations</span>
            <h1>{activeNav.label}</h1>
          </div>
          <div className="header-status">
            <div>
              <span>Experiment</span>
              <strong>SCC Thermal Rig</strong>
            </div>
            <div>
              <span>Connection</span>
              <StatusIndicator label={query.isError ? 'Disconnected' : data.latest ? 'Live' : 'Waiting'} tone={tone} pulse={!query.isError && Boolean(data.latest)} />
            </div>
            <div>
              <span>Active Controller</span>
              <strong>{activeController}</strong>
            </div>
            <div>
              <span>Alarm Count</span>
              <strong className={data.alarms.length ? 'alarm-count active' : 'alarm-count'}>{data.alarms.length}</strong>
            </div>
          </div>
        </header>

        {query.isLoading ? (
          <main className="workspace">
            <Skeleton rows={6} />
          </main>
        ) : query.isError ? (
          <main className="workspace">
            <section className="panel error-panel">
              <div className="panel__header">
                <div>
                  <h2>Backend Connection Lost</h2>
                  <p>{query.error instanceof Error ? query.error.message : 'Unable to reach telemetry API'}</p>
                </div>
                <AlertTriangle size={20} />
              </div>
            </section>
            {renderPage(activePage, data)}
          </main>
        ) : (
          renderPage(activePage, data)
        )}
      </section>
    </div>
  );
}
