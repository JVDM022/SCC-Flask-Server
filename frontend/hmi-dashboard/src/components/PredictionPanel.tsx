import { BrainCircuit } from 'lucide-react';

import type { PredictionResponse } from '../types/domain';
import { formatNumber } from '../utils';

interface PredictionPanelProps {
  prediction: PredictionResponse;
}

export function PredictionPanel({ prediction }: PredictionPanelProps) {
  const predictions = prediction.predictions || [];

  return (
    <section className="panel">
      <div className="panel__header">
        <div>
          <h2>Prediction Engine</h2>
          <p>Thermal horizon model outputs</p>
        </div>
        <BrainCircuit size={18} />
      </div>
      <div className="prediction-grid">
        {[1, 5, 10].map((horizon) => {
          const item = predictions.find((entry) => entry.horizon_s === horizon);
          const ok = item?.status === 'ok';
          return (
            <div className="prediction-cell" key={horizon}>
              <span>+{horizon}s</span>
              <strong>{ok ? `${formatNumber(item?.predicted_temp_c)} C` : 'Unavailable'}</strong>
              <small>{ok ? `Delta ${formatNumber(item?.predicted_delta_c)} C` : item?.message || 'Model artifact missing'}</small>
            </div>
          );
        })}
      </div>
    </section>
  );
}
