import type { DashboardData } from '../types/domain';

interface SettingsDashboardProps {
  data: DashboardData;
}

export function SettingsDashboard({ data }: SettingsDashboardProps) {
  return (
    <main className="workspace">
      <section className="settings-grid">
        <section className="panel">
          <div className="panel__header">
            <div>
              <h2>Data Connectivity</h2>
              <p>Current frontend data plane</p>
            </div>
          </div>
          <div className="metadata-list">
            <div><span>Polling interval</span><strong>3 seconds</strong></div>
            <div><span>Transport</span><strong>HTTP polling, WebSocket-ready</strong></div>
            <div><span>Historian rows loaded</span><strong>{data.history.length}</strong></div>
          </div>
        </section>
        <section className="panel">
          <div className="panel__header">
            <div>
              <h2>Controller Boundary</h2>
              <p>Operational safety contract</p>
            </div>
          </div>
          <div className="insight-list">
            <p>Arduino firmware owns thermal SCC rig control behavior.</p>
            <p>ESP32 relay firmware only bridges telemetry and commands.</p>
            <p>MPC remains advisory; hard kill, manual kill, and heater lockout dominate all recommendations.</p>
          </div>
        </section>
      </section>
    </main>
  );
}
