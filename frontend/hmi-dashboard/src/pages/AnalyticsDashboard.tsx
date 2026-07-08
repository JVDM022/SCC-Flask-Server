import { Activity, BarChart3, Gauge, Thermometer } from 'lucide-react';

import { MetricCard } from '../components/MetricCard';
import { TrendPanel } from '../components/TrendPanel';
import type { DashboardData } from '../types/domain';
import { average, formatNumber, percentile } from '../utils';

interface AnalyticsDashboardProps {
  data: DashboardData;
}

function statBlock(values: Array<number | null | undefined>) {
  const finite = values.filter((value) => value !== null && value !== undefined).map(Number).filter(Number.isFinite);
  return {
    mean: average(finite),
    min: finite.length ? Math.min(...finite) : null,
    max: finite.length ? Math.max(...finite) : null,
    p95: percentile(finite, 95),
  };
}

export function AnalyticsDashboard({ data }: AnalyticsDashboardProps) {
  const tempStats = statBlock(data.history.map((row) => row.temp_c));
  const dropStats = statBlock(data.cycles.map((cycle) => cycle.last_pump_drop_c));
  const recoveryStats = statBlock(data.cycles.map((cycle) => cycle.recovery_time_s));
  const heaterStats = statBlock(data.history.map((row) => row.heater_pwm));

  return (
    <main className="workspace">
      <section className="metric-grid">
        <MetricCard icon={Thermometer} label="Mean Temperature" value={formatNumber(tempStats.mean)} unit="C" detail={`Max ${formatNumber(tempStats.max)} C`} />
        <MetricCard icon={BarChart3} label="Pump Drop P95" value={formatNumber(dropStats.p95)} unit="C" detail={`Mean ${formatNumber(dropStats.mean)} C`} />
        <MetricCard icon={Activity} label="Recovery P95" value={formatNumber(recoveryStats.p95)} unit="s" detail={`Mean ${formatNumber(recoveryStats.mean)} s`} />
        <MetricCard icon={Gauge} label="Heater PWM P95" value={formatNumber(heaterStats.p95, 0)} detail={`Mean ${formatNumber(heaterStats.mean, 0)}`} />
      </section>

      <section className="analytics-grid">
        <TrendPanel title="Temperature Distribution" series={[{ name: 'Temp C', type: 'histogram', x: data.history.map((row) => row.temp_c), y: [], color: '#38BDF8' }]} height={280} />
        <TrendPanel title="Pump Drop Distribution" series={[{ name: 'Drop C', type: 'histogram', x: data.cycles.map((cycle) => cycle.last_pump_drop_c ?? null), y: [], color: '#F59E0B' }]} height={280} />
        <TrendPanel title="Recovery Time Distribution" series={[{ name: 'Recovery s', type: 'histogram', x: data.cycles.map((cycle) => cycle.recovery_time_s ?? null), y: [], color: '#22C55E' }]} height={280} />
        <TrendPanel title="Heater Usage" series={[{ name: 'PWM', type: 'histogram', x: data.history.map((row) => row.heater_pwm), y: [], color: '#94A3B8' }]} height={280} />
      </section>
    </main>
  );
}
