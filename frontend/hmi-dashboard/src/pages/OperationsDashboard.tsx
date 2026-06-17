import { useMutation, useQueryClient } from '@tanstack/react-query';
import { Activity, Flame, Gauge, Power, PowerOff, RotateCcw, ShieldAlert, Thermometer } from 'lucide-react';

import { queueManualKill } from '../api/client';
import { AlarmPanel } from '../components/AlarmPanel';
import { MetricCard } from '../components/MetricCard';
import { TimelinePanel } from '../components/TimelinePanel';
import { TrendPanel } from '../components/TrendPanel';
import type { DashboardData } from '../types/domain';
import { formatNumber, modeLabel } from '../utils';

interface OperationsDashboardProps {
  data: DashboardData;
}

export function OperationsDashboard({ data }: OperationsDashboardProps) {
  const queryClient = useQueryClient();
  const latest = data.latest;
  const tempError = latest ? latest.temp_c - latest.setpoint_c : null;
  const manualKillActive = Boolean(latest?.manual_kill);
  const safetyMutation = useMutation({
    mutationFn: queueManualKill,
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['dashboard-data'] });
      queryClient.invalidateQueries({ queryKey: ['mqtt-status'] });
      queryClient.invalidateQueries({ queryKey: ['firmware-commands'] });
    },
  });
  const safetyError = safetyMutation.error instanceof Error ? safetyMutation.error.message : 'Command failed';
  const x = data.history.map((row) => row.created_at || row.ms);
  const horizon5 = data.prediction.predictions?.find((item) => item.horizon_s === 5);
  const predictedLine = data.history.map(() => null as number | null);
  if (latest && horizon5?.predicted_temp_c !== undefined) {
    predictedLine.push(horizon5.predicted_temp_c);
  }

  return (
    <main className="workspace">
      <section className="metric-grid top-metrics">
        <MetricCard icon={Thermometer} label="Current Temperature" value={formatNumber(latest?.temp_c)} unit="C" tone="accent" detail="Live Arduino USB telemetry" />
        <MetricCard icon={Gauge} label="Setpoint" value={formatNumber(latest?.setpoint_c)} unit="C" detail="Controller target" />
        <MetricCard icon={Activity} label="Temperature Error" value={formatNumber(tempError)} unit="C" tone={Math.abs(tempError || 0) > 5 ? 'warning' : 'default'} detail="Measured minus target" />
        <MetricCard icon={Flame} label="Heater PWM" value={latest?.heater_pwm ?? '--'} unit="/255" detail="Current actuator command" />
        <MetricCard icon={RotateCcw} label="Pump Status" value={latest?.pump_on ? 'On' : 'Off'} tone={latest?.pump_on ? 'success' : 'default'} detail={`Motor PWM ${latest?.motor_pwm ?? '--'}`} />
        <MetricCard icon={Gauge} label="System Mode" value={modeLabel(latest?.mode)} detail="Arduino controller state" />
      </section>

      <section className={`panel safety-control-panel ${manualKillActive ? 'safety-control-panel--active' : ''}`}>
        <div className="panel__header">
          <div>
            <h2>Manual Emergency Control</h2>
            <p>Queues a manual kill command for the Intel NUC gateway to forward to the Arduino controller</p>
          </div>
          <ShieldAlert size={20} />
        </div>
        <div className="safety-control-actions">
          <button
            className="safety-button safety-button--kill"
            onClick={() => safetyMutation.mutate(true)}
            disabled={safetyMutation.isPending || manualKillActive}
            type="button"
          >
            <PowerOff size={18} />
            Emergency Off
          </button>
          <button
            className="safety-button safety-button--release"
            onClick={() => safetyMutation.mutate(false)}
            disabled={safetyMutation.isPending || !manualKillActive}
            type="button"
          >
            <Power size={18} />
            Release Manual Kill
          </button>
          <span className={`safety-state ${manualKillActive ? 'critical' : 'normal'}`}>
            {manualKillActive ? 'Manual kill active' : 'Manual kill clear'}
          </span>
          {safetyMutation.isError ? <span className="safety-error">{safetyError}</span> : null}
        </div>
      </section>

      <section className="workspace-grid workspace-grid--main">
        <TrendPanel
          title="Thermal Process"
          subtitle="Temperature, setpoint, and ML projected temperature"
          yTitle="Temperature C"
          height={390}
          series={[
            { name: 'Temperature', x, y: data.history.map((row) => row.temp_c), color: '#38BDF8' },
            { name: 'Setpoint', x, y: data.history.map((row) => row.setpoint_c), color: '#F59E0B', dash: 'dot' },
            { name: 'Predicted', x: [...x, 'forecast'], y: predictedLine, color: '#22C55E', dash: 'dash' },
          ]}
        />
        <AlarmPanel alarms={data.alarms} />
      </section>

      <TimelinePanel events={data.events.filter((event) => [1, 2, 3, 4, 5].includes(event.event_code))} />
    </main>
  );
}
