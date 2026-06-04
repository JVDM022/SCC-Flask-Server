export function formatNumber(value: number | null | undefined, digits = 2): string {
  if (value === null || value === undefined || Number.isNaN(Number(value))) {
    return '--';
  }
  return Number(value).toFixed(digits);
}

export function formatDateTime(value?: string | null): string {
  if (!value) {
    return '--';
  }
  return new Date(value).toLocaleString(undefined, {
    month: 'short',
    day: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  });
}

export function average(values: Array<number | null | undefined>): number | null {
  const finite = values.map(Number).filter(Number.isFinite);
  if (!finite.length) {
    return null;
  }
  return finite.reduce((sum, value) => sum + value, 0) / finite.length;
}

export function percentile(values: Array<number | null | undefined>, pct: number): number | null {
  const finite = values.map(Number).filter(Number.isFinite).sort((a, b) => a - b);
  if (!finite.length) {
    return null;
  }
  const index = Math.min(finite.length - 1, Math.max(0, Math.ceil((pct / 100) * finite.length) - 1));
  return finite[index];
}

export function modeLabel(mode?: number): string {
  if (mode === undefined || mode === null) {
    return '--';
  }
  const labels: Record<number, string> = {
    0: 'Idle',
    1: 'Warmup',
    2: 'Autotune',
    3: 'Closed loop',
    4: 'Fault',
  };
  return labels[mode] || `Mode ${mode}`;
}
