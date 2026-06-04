export type StatusTone = 'online' | 'warning' | 'critical' | 'offline' | 'neutral';

interface StatusIndicatorProps {
  label: string;
  tone?: StatusTone;
  pulse?: boolean;
}

export function StatusIndicator({ label, tone = 'neutral', pulse = false }: StatusIndicatorProps) {
  return (
    <span className={`status-indicator status-${tone}`}>
      <span className={pulse ? 'status-dot pulse' : 'status-dot'} />
      {label}
    </span>
  );
}
