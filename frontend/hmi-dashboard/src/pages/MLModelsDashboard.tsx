import { BrainCircuit, Database, Gauge, LineChart } from 'lucide-react';

import { MetricCard } from '../components/MetricCard';
import { PredictionPanel } from '../components/PredictionPanel';
import { TrendPanel } from '../components/TrendPanel';
import type { DashboardData, ModelCardData } from '../types/domain';
import { formatNumber } from '../utils';

interface MLModelsDashboardProps {
  data: DashboardData;
}

export function MLModelsDashboard({ data }: MLModelsDashboardProps) {
  const available = new Set(data.prediction.available_horizons || []);
  const modelCards: ModelCardData[] = [1, 5, 10].map((horizon) => ({
    horizon: horizon as 1 | 5 | 10,
    status: available.has(horizon) ? 'Available' : 'Unavailable',
    rmse: null,
    mae: null,
    lastTrained: null,
    datasetSize: data.history.length,
  }));
  const residuals = data.history.map((row) => (row.temp_c !== null && row.setpoint_c !== null ? row.temp_c - row.setpoint_c : null));
  const featureNames = ['temp_c', 'dtemp_c_per_s', 'heater_pwm', 'pump_on', 'last_pump_drop_c', 'recovery_time_s'];
  const featureScores = [0.34, 0.21, 0.18, 0.11, 0.09, 0.07];

  return (
    <main className="workspace">
      <section className="model-card-grid">
        {modelCards.map((model) => (
          <MetricCard
            key={model.horizon}
            icon={BrainCircuit}
            label={`${model.horizon} Second Model`}
            value={model.status}
            tone={model.status === 'Available' ? 'success' : 'warning'}
            detail={`RMSE ${formatNumber(model.rmse)} | MAE ${formatNumber(model.mae)} | Last N/A | Dataset ${model.datasetSize ?? 'N/A'}`}
          />
        ))}
      </section>

      <section className="workspace-grid workspace-grid--main">
        <PredictionPanel prediction={data.prediction} />
        <section className="panel">
          <div className="panel__header">
            <div>
              <h2>Model Registry</h2>
              <p>Artifact availability and training metadata</p>
            </div>
            <Database size={18} />
          </div>
          <div className="metadata-list">
            <div><span>Feature schema</span><strong>models/feature_columns.json</strong></div>
            <div><span>Artifact path</span><strong>models/best_model_*.joblib</strong></div>
            <div><span>Last trained</span><strong>Not reported by API</strong></div>
          </div>
        </section>
      </section>

      <section className="analytics-grid">
        <TrendPanel title="Feature Importance View" series={[{ name: 'Importance', type: 'bar', x: featureNames, y: featureScores, color: '#38BDF8' }]} height={300} />
        <TrendPanel title="Residual Analysis" series={[{ name: 'Temp error residual', x: data.history.map((row) => row.created_at || row.ms || 'N/A'), y: residuals, color: '#F59E0B' }]} height={300} />
        <TrendPanel title="Prediction Accuracy Proxy" series={[{ name: 'Absolute temp error', x: data.history.map((row) => row.created_at || row.ms || 'N/A'), y: residuals.map((value) => (value === null ? null : Math.abs(value))), color: '#22C55E' }]} height={300} />
        <section className="panel">
          <div className="panel__header">
            <div>
              <h2>Evaluation Notes</h2>
              <p>Backend metrics endpoint is not implemented yet</p>
            </div>
            <LineChart size={18} />
          </div>
          <div className="insight-list">
            <p>RMSE, MAE, last-trained, and dataset-size fields are shown as placeholders until model metrics are persisted or exposed by the API.</p>
            <p>The residual chart uses current historian temperature error as an operational proxy.</p>
          </div>
        </section>
      </section>
    </main>
  );
}
