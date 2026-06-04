import { EquipmentHealthPanel } from '../components/EquipmentHealthPanel';
import { TimelinePanel } from '../components/TimelinePanel';
import { TrendPanel } from '../components/TrendPanel';
import type { DashboardData } from '../types/domain';

interface EquipmentHealthDashboardProps {
  data: DashboardData;
}

export function EquipmentHealthDashboard({ data }: EquipmentHealthDashboardProps) {
  return (
    <main className="workspace">
      <EquipmentHealthPanel history={data.history} cycles={data.cycles} alarms={data.alarms} />
      <section className="workspace-grid workspace-grid--main">
        <TrendPanel
          title="Cycle Statistics"
          subtitle="Pump drop and recovery behavior"
          height={320}
          series={[
            { name: 'Pump drop C', x: data.cycles.map((cycle) => cycle.recovery_time || cycle.id || ''), y: data.cycles.map((cycle) => cycle.last_pump_drop_c ?? null), color: '#F59E0B' },
            { name: 'Recovery s', x: data.cycles.map((cycle) => cycle.recovery_time || cycle.id || ''), y: data.cycles.map((cycle) => cycle.recovery_time_s ?? null), color: '#38BDF8' },
          ]}
        />
        <TimelinePanel title="Health-Relevant Events" events={data.events.filter((event) => event.severity !== 'info')} />
      </section>
    </main>
  );
}
