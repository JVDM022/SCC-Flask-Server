import type { RigEvent } from '../types/domain';
import { formatDateTime } from '../utils';

interface TimelinePanelProps {
  events: RigEvent[];
  title?: string;
}

export function TimelinePanel({ events, title = 'Event Timeline' }: TimelinePanelProps) {
  return (
    <section className="panel timeline-panel">
      <div className="panel__header">
        <div>
          <h2>{title}</h2>
          <p>Pump, recovery, hard-kill, and controller events</p>
        </div>
      </div>
      <div className="timeline">
        {events.length ? (
          events.slice(0, 18).map((event, index) => (
            <article className={`timeline__row severity-${event.severity}`} key={event.id ?? index}>
              <span className="timeline__code">{event.event_code}</span>
              <div>
                <strong>{event.event_name}</strong>
                <p>{event.message}</p>
              </div>
              <time>{formatDateTime(event.created_at)}</time>
            </article>
          ))
        ) : (
          <div className="empty-state">No event history</div>
        )}
      </div>
    </section>
  );
}
