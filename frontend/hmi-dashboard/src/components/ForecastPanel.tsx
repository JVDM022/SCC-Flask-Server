import type { MpcRecommendation, Telemetry } from '../types/domain';
import { TrendPanel } from './TrendPanel';

interface ForecastPanelProps {
  latest: Telemetry | null;
  mpc: MpcRecommendation;
}

export function ForecastPanel({ latest, mpc }: ForecastPanelProps) {
  const current = latest?.temp_c ?? null;
  const setpoint = latest?.setpoint_c ?? mpc.setpoint_c ?? null;
  const predicted = mpc.predicted_temp_c ?? current;
  const x = ['now', '+1s', '+5s', '+10s'];

  return (
    <TrendPanel
      title="Forecast Visualization"
      subtitle="Current, predicted, and setpoint trajectories"
      yTitle="Temperature C"
      height={300}
      series={[
        { name: 'Current trajectory', x, y: [current, current, current, current], color: '#38BDF8' },
        { name: 'Predicted trajectory', x, y: [current, current, predicted, predicted], color: '#22C55E', dash: 'dash' },
        { name: 'Setpoint trajectory', x, y: [setpoint, setpoint, setpoint, setpoint], color: '#F59E0B', dash: 'dot' },
      ]}
    />
  );
}
