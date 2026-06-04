import { Cpu, Gauge, Target, Zap } from 'lucide-react';

import { ForecastPanel } from '../components/ForecastPanel';
import { MetricCard } from '../components/MetricCard';
import { PredictionPanel } from '../components/PredictionPanel';
import type { DashboardData } from '../types/domain';
import { formatNumber } from '../utils';

interface MpcDashboardProps {
  data: DashboardData;
}

export function MpcDashboard({ data }: MpcDashboardProps) {
  const latest = data.latest;
  const tempError = latest ? latest.temp_c - latest.setpoint_c : null;

  return (
    <main className="workspace">
      <section className="metric-grid">
        <MetricCard icon={Target} label="Current State" value={formatNumber(latest?.temp_c)} unit="C" detail={`Error ${formatNumber(tempError)} C`} />
        <MetricCard icon={Gauge} label="Current PWM" value={data.mpc.current_pwm ?? latest?.heater_pwm ?? '--'} unit="/255" />
        <MetricCard icon={Zap} label="Recommended PWM" value={data.mpc.recommended_pwm ?? '--'} unit="/255" tone={data.mpc.status === 'safety_override' ? 'critical' : 'accent'} detail={data.mpc.status} />
        <MetricCard icon={Cpu} label="Optimization Cost" value={formatNumber(data.mpc.cost, 3)} detail={data.mpc.model_status || 'model status unavailable'} />
      </section>

      <section className="workspace-grid workspace-grid--main">
        <ForecastPanel latest={latest} mpc={data.mpc} />
        <PredictionPanel prediction={data.prediction} />
      </section>
    </main>
  );
}
