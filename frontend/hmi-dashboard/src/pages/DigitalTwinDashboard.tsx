import { DigitalTwinPanel } from '../components/DigitalTwinPanel';
import type { DashboardData } from '../types/domain';

interface DigitalTwinDashboardProps {
  data: DashboardData;
}

export function DigitalTwinDashboard({ data }: DigitalTwinDashboardProps) {
  return (
    <main className="workspace">
      <DigitalTwinPanel latest={data.latest} prediction={data.prediction} />
    </main>
  );
}
