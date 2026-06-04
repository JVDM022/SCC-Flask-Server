import { useMemo, useState } from 'react';

import type { Prediction, PredictionResponse, Telemetry } from '../types/domain';
import { formatNumber } from '../utils';
import { TrendPanel } from './TrendPanel';

interface DigitalTwinPanelProps {
  latest: Telemetry | null;
  prediction: PredictionResponse;
}

function validPrediction(item: Prediction | undefined): item is Prediction {
  return item?.status === 'ok' && typeof item.predicted_temp_c === 'number' && Number.isFinite(item.predicted_temp_c);
}

function interpolateForecast(points: Array<{ second: number; temp: number }>): { seconds: number[]; temps: Array<number | null> } {
  const seconds = Array.from({ length: 11 }, (_, index) => index);
  if (!points.length) {
    return { seconds, temps: seconds.map(() => null) };
  }

  const sorted = [...points].sort((a, b) => a.second - b.second);
  const temps = seconds.map((second) => {
    if (second <= sorted[0].second) return sorted[0].temp;
    for (let index = 1; index < sorted.length; index++) {
      const prev = sorted[index - 1];
      const next = sorted[index];
      if (second <= next.second) {
        const ratio = (second - prev.second) / Math.max(1, next.second - prev.second);
        return prev.temp + ratio * (next.temp - prev.temp);
      }
    }
    return sorted[sorted.length - 1].temp;
  });

  return { seconds, temps };
}

export function DigitalTwinPanel({ latest, prediction }: DigitalTwinPanelProps) {
  const [targetSetpoint, setTargetSetpoint] = useState(latest?.setpoint_c ?? 125);
  const [lastRun, setLastRun] = useState(() => new Date());

  const twin = useMemo(() => {
    const currentTemp = latest?.temp_c ?? null;
    const mlPoints = (prediction.predictions || [])
      .filter(validPrediction)
      .map((item) => ({ second: item.horizon_s, temp: item.predicted_temp_c as number }));
    const forecastPoints = currentTemp === null ? mlPoints : [{ second: 0, temp: currentTemp }, ...mlPoints];
    const forecast = interpolateForecast(forecastPoints);
    const terminalPoint = mlPoints.find((point) => point.second === 10) || mlPoints[mlPoints.length - 1];
    const terminalTemp = terminalPoint?.temp ?? currentTemp;
    const finiteTemps = forecast.temps.filter((value): value is number => typeof value === 'number' && Number.isFinite(value));
    const terminalError = terminalTemp === null || terminalTemp === undefined ? null : terminalTemp - targetSetpoint;
    const expectedDrop = currentTemp === null || !finiteTemps.length ? null : Math.max(0, currentTemp - Math.min(...finiteTemps));

    return {
      ...forecast,
      mlPoints,
      terminalTemp,
      terminalError,
      expectedDrop,
      available: mlPoints.length > 0,
    };
  }, [latest?.temp_c, prediction.predictions, targetSetpoint]);

  const modelStatus = twin.available ? 'ML forecast active' : prediction.status === 'unavailable' ? 'ML unavailable' : 'Waiting for prediction';
  const nextPrediction = twin.mlPoints[0];

  return (
    <section className="digital-twin-grid">
      <section className="panel twin-controls">
        <div className="panel__header">
          <div>
            <h2>ML Digital Twin</h2>
            <p>Forecast sandbox built from model output</p>
          </div>
        </div>
        <label>
          <span>Target setpoint C</span>
          <input type="number" value={targetSetpoint} onChange={(event) => setTargetSetpoint(Number(event.target.value))} />
        </label>
        <button className="command-button" type="button" onClick={() => setLastRun(new Date())}>
          Refresh scenario marker
        </button>
        <div className="twin-results">
          <div>
            <span>Model state</span>
            <strong>{modelStatus}</strong>
          </div>
          <div>
            <span>Current temp</span>
            <strong>{formatNumber(latest?.temp_c)} C</strong>
          </div>
          <div>
            <span>Next ML horizon</span>
            <strong>{nextPrediction ? `+${nextPrediction.second}s` : '--'}</strong>
          </div>
          <div>
            <span>Forecast pump drop</span>
            <strong>{formatNumber(twin.expectedDrop)} C</strong>
          </div>
          <div>
            <span>10s setpoint error</span>
            <strong>{formatNumber(twin.terminalError)} C</strong>
          </div>
          <div>
            <span>Last run</span>
            <strong>{lastRun.toLocaleTimeString()}</strong>
          </div>
        </div>
      </section>
      <TrendPanel
        title="ML Forecast Twin"
        subtitle="Interpolated from current temperature and ML predictions at +1s, +5s, and +10s"
        yTitle="Temperature C"
        height={360}
        series={[
          { name: 'ML forecast', x: twin.seconds, y: twin.available ? twin.temps : twin.seconds.map(() => null), color: '#38BDF8' },
          { name: 'ML horizon points', type: 'scatter', x: twin.mlPoints.map((point) => point.second), y: twin.mlPoints.map((point) => point.temp), color: '#22C55E' },
          { name: 'Target setpoint', x: twin.seconds, y: twin.seconds.map(() => targetSetpoint), color: '#F59E0B', dash: 'dot' },
        ]}
      />
    </section>
  );
}
