import { Activity, Flame, Gauge, RotateCcw } from 'lucide-react';

import type { Alarm, PumpCycle, Telemetry } from '../types/domain';
import { average, formatNumber } from '../utils';
import { MetricCard } from './MetricCard';

interface EquipmentHealthPanelProps {
  history: Telemetry[];
  cycles: PumpCycle[];
  alarms: Alarm[];
}

export function EquipmentHealthPanel({ history, cycles, alarms }: EquipmentHealthPanelProps) {
  const heaterDuty = history.length ? (history.filter((row) => row.heating).length / history.length) * 100 : null;
  const avgDrop = average(cycles.map((cycle) => cycle.last_pump_drop_c));
  const avgRecovery = average(cycles.map((cycle) => cycle.recovery_time_s));
  const sensorFaults = alarms.filter((alarm) => alarm.alarm_code === 'SENSOR_FAULT').length;
  const degradation = (avgRecovery ?? 0) > 60 || (avgDrop ?? 0) > 5;

  return (
    <section className="health-grid">
      <MetricCard icon={Gauge} label="Sensor health" value={sensorFaults ? 'Degraded' : 'Nominal'} tone={sensorFaults ? 'critical' : 'success'} detail={`${sensorFaults} active faults`} />
      <MetricCard icon={RotateCcw} label="Pump health" value={degradation ? 'Watch' : 'Nominal'} tone={degradation ? 'warning' : 'success'} detail={`Avg drop ${formatNumber(avgDrop)} C`} />
      <MetricCard icon={Flame} label="Heater health" value={`${formatNumber(heaterDuty, 1)}%`} unit=" duty" detail="Recent historian window" />
      <MetricCard icon={Activity} label="Recovery trend" value={`${formatNumber(avgRecovery)} s`} tone={(avgRecovery ?? 0) > 60 ? 'warning' : 'default'} detail="Cycle average" />
    </section>
  );
}
