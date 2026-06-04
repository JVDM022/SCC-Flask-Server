import type { LucideIcon } from 'lucide-react';

type MetricTone = 'default' | 'accent' | 'success' | 'warning' | 'critical';

interface MetricCardProps {
  label: string;
  value: string | number;
  unit?: string;
  icon: LucideIcon;
  tone?: MetricTone;
  detail?: string;
}

export function MetricCard({ label, value, unit, icon: Icon, tone = 'default', detail }: MetricCardProps) {
  return (
    <section className={`metric-card metric-${tone}`}>
      <div className="metric-card__header">
        <span>{label}</span>
        <Icon size={16} aria-hidden="true" />
      </div>
      <div className="metric-card__value">
        {value}
        {unit ? <small>{unit}</small> : null}
      </div>
      {detail ? <div className="metric-card__detail">{detail}</div> : null}
    </section>
  );
}
