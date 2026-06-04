import { AlertTriangle } from 'lucide-react';

import type { Alarm } from '../types/domain';
import { formatDateTime } from '../utils';

interface AlarmPanelProps {
  alarms: Alarm[];
}

export function AlarmPanel({ alarms }: AlarmPanelProps) {
  return (
    <section className="panel alarm-panel">
      <div className="panel__header">
        <div>
          <h2>Alarm Management</h2>
          <p>Active safety and process alarms</p>
        </div>
        <AlertTriangle size={18} />
      </div>
      <div className="feed-list">
        {alarms.length ? (
          alarms.map((alarm, index) => (
            <article className={`feed-item severity-${alarm.severity}`} key={alarm.id ?? index}>
              <span>{formatDateTime(alarm.created_at)}</span>
              <strong>{alarm.alarm_code}</strong>
              <p>{alarm.message}</p>
            </article>
          ))
        ) : (
          <div className="empty-state">No active alarms</div>
        )}
      </div>
    </section>
  );
}
