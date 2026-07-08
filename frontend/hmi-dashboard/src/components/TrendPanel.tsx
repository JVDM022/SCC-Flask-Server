import { useEffect, useMemo, useRef, useState } from 'react';

interface PlotlyApi {
  react: (element: HTMLElement, data: unknown[], layout: Record<string, unknown>, config: Record<string, unknown>) => Promise<unknown>;
  purge: (element: HTMLElement) => void;
}

interface TrendSeries {
  name: string;
  x: Array<string | number | null>;
  y: Array<number | null>;
  color: string;
  dash?: 'solid' | 'dot' | 'dash';
  fill?: 'none' | 'tozeroy';
  type?: 'scatter' | 'bar' | 'histogram';
}

interface TrendPanelProps {
  title: string;
  subtitle?: string;
  series: TrendSeries[];
  height?: number;
  yTitle?: string;
}

export function TrendPanel({ title, subtitle, series, height = 320, yTitle }: TrendPanelProps) {
  const plotRef = useRef<HTMLDivElement | null>(null);
  const [plotly, setPlotly] = useState<PlotlyApi | null>(null);
  const [plotlyFailed, setPlotlyFailed] = useState(false);
  const data = useMemo(
    () =>
      series.map((item) => ({
        type: item.type || 'scatter',
        mode: item.type === 'bar' || item.type === 'histogram' ? undefined : 'lines',
        name: item.name,
        x: item.x,
        y: item.y,
        marker: { color: item.color },
        line: { color: item.color, width: 2, dash: item.dash || 'solid' },
        fill: item.fill || 'none',
        hovertemplate: '%{y}<extra>%{fullData.name}</extra>',
      })),
    [series],
  );

  useEffect(() => {
    let mounted = true;

    async function loadPlotly() {
      try {
        const module = await import('plotly.js-dist-min');
        const Plotly = ((module as { default?: unknown }).default || module) as PlotlyApi;
        if (mounted) {
          setPlotly(Plotly);
        }
      } catch (error) {
        console.error('Plotly failed to load', error);
        if (mounted) {
          setPlotlyFailed(true);
        }
      }
    }

    loadPlotly();
    return () => {
      mounted = false;
    };
  }, []);

  useEffect(() => {
    if (!plotly || !plotRef.current) {
      return undefined;
    }

    const element = plotRef.current;
    plotly
      .react(
        element,
        data,
        {
          autosize: true,
          height,
          paper_bgcolor: '#121821',
          plot_bgcolor: '#0B0F14',
          font: { color: '#94A3B8', family: 'Inter, IBM Plex Sans, system-ui, sans-serif', size: 11 },
          margin: { l: 46, r: 18, t: 20, b: 38 },
          xaxis: { gridcolor: '#1F2937', zerolinecolor: '#1F2937', color: '#94A3B8' },
          yaxis: { title: yTitle, gridcolor: '#1F2937', zerolinecolor: '#1F2937', color: '#94A3B8' },
          legend: { orientation: 'h', y: -0.2, x: 0, font: { color: '#94A3B8' } },
          showlegend: true,
        },
        { displayModeBar: false, responsive: true },
      )
      .catch((error) => {
        console.error('Plotly render failed', error);
        setPlotlyFailed(true);
      });

    return () => {
      plotly.purge(element);
    };
  }, [data, height, plotly, yTitle]);

  return (
    <section className="panel trend-panel">
      <div className="panel__header">
        <div>
          <h2>{title}</h2>
          {subtitle ? <p>{subtitle}</p> : null}
        </div>
      </div>
      {plotly && !plotlyFailed ? (
        <div className="plotly-host" ref={plotRef} style={{ height }} />
      ) : (
        <SvgFallbackChart series={series} height={height} failed={plotlyFailed} />
      )}
    </section>
  );
}

function SvgFallbackChart({ series, height, failed }: { series: TrendSeries[]; height: number; failed: boolean }) {
  const width = 900;
  const padding = 32;
  const numericSeries = series.filter((item) => item.type !== 'histogram' && item.type !== 'bar');
  const values = numericSeries.flatMap((item) => item.y).filter((value): value is number => typeof value === 'number' && Number.isFinite(value));
  const min = values.length ? Math.min(...values) : 0;
  const max = values.length ? Math.max(...values) : 1;
  const span = Math.max(1, max - min);

  function point(value: number | null, index: number, count: number) {
    if (value === null || !Number.isFinite(value)) {
      return '';
    }
    const x = padding + (index / Math.max(1, count - 1)) * (width - padding * 2);
    const y = height - padding - ((value - min) / span) * (height - padding * 2);
    return `${x},${y}`;
  }

  return (
    <div className="fallback-chart" style={{ height }}>
      {failed ? <span className="chart-warning">Plotly unavailable, showing fallback view</span> : <span className="chart-warning">Loading chart engine</span>}
      <svg viewBox={`0 0 ${width} ${height}`} role="img">
        {[0, 0.25, 0.5, 0.75, 1].map((tick) => (
          <line key={tick} x1={padding} x2={width - padding} y1={padding + tick * (height - padding * 2)} y2={padding + tick * (height - padding * 2)} />
        ))}
        {numericSeries.map((item) => (
          <polyline key={item.name} points={item.y.map((value, index) => point(value, index, item.y.length)).join(' ')} style={{ stroke: item.color }} />
        ))}
      </svg>
    </div>
  );
}
