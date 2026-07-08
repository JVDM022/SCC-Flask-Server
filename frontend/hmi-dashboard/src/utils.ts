export function formatNumber(value: number | null | undefined, digits = 2): string {
  const numberValue = Number(value);
  if (value === null || value === undefined || !Number.isFinite(numberValue)) {
    return 'N/A';
  }
  return numberValue.toFixed(digits);
}

export function formatDateTime(value?: string | null): string {
  if (!value) {
    return 'N/A';
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
  const finite = values.filter((value) => value !== null && value !== undefined).map(Number).filter(Number.isFinite);
  if (!finite.length) {
    return null;
  }
  return finite.reduce((sum, value) => sum + value, 0) / finite.length;
}

export function percentile(values: Array<number | null | undefined>, pct: number): number | null {
  const finite = values.filter((value) => value !== null && value !== undefined).map(Number).filter(Number.isFinite).sort((a, b) => a - b);
  if (!finite.length) {
    return null;
  }
  const index = Math.min(finite.length - 1, Math.max(0, Math.ceil((pct / 100) * finite.length) - 1));
  return finite[index];
}

export function modeLabel(mode?: number | null): string {
  if (mode === undefined || mode === null) {
    return 'N/A';
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
