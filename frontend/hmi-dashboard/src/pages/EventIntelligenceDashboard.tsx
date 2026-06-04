import { Search } from 'lucide-react';
import { useMemo, useState } from 'react';

import type { DashboardData, RigEvent } from '../types/domain';
import { formatDateTime } from '../utils';

interface EventIntelligenceDashboardProps {
  data: DashboardData;
}

const filters = ['All', 'Warnings', 'Critical', 'Pump Events', 'Controller Events', 'ML Events'];

function matchesFilter(event: RigEvent, filter: string): boolean {
  if (filter === 'Warnings') return event.severity === 'warning';
  if (filter === 'Critical') return event.severity === 'critical';
  if (filter === 'Pump Events') return [1, 2, 3].includes(event.event_code);
  if (filter === 'Controller Events') return event.event_name.toLowerCase().includes('controller') || event.event_code === 4;
  if (filter === 'ML Events') return event.event_name.toLowerCase().includes('model') || event.message.toLowerCase().includes('model');
  return true;
}

export function EventIntelligenceDashboard({ data }: EventIntelligenceDashboardProps) {
  const [activeFilter, setActiveFilter] = useState('All');
  const [search, setSearch] = useState('');

  const feed = useMemo(() => {
    const query = search.trim().toLowerCase();
    return data.events
      .filter((event) => matchesFilter(event, activeFilter))
      .filter((event) => !query || `${event.event_name} ${event.message} ${event.severity}`.toLowerCase().includes(query));
  }, [activeFilter, data.events, search]);

  return (
    <main className="workspace">
      <section className="panel event-intel">
        <div className="panel__header">
          <div>
            <h2>Unified Event Feed</h2>
            <p>Search and triage process, controller, pump, and model activity</p>
          </div>
        </div>
        <div className="event-toolbar">
          <div className="search-box">
            <Search size={16} />
            <input value={search} onChange={(event) => setSearch(event.target.value)} placeholder="Search event intelligence" />
          </div>
          <div className="segmented">
            {filters.map((filter) => (
              <button className={activeFilter === filter ? 'active' : ''} key={filter} onClick={() => setActiveFilter(filter)}>
                {filter}
              </button>
            ))}
          </div>
        </div>
        <div className="event-table">
          <table>
            <thead>
              <tr>
                <th>Time</th>
                <th>Severity</th>
                <th>Event</th>
                <th>Message</th>
              </tr>
            </thead>
            <tbody>
              {feed.length ? (
                feed.map((event, index) => (
                  <tr key={event.id ?? index}>
                    <td>{formatDateTime(event.created_at)}</td>
                    <td><span className={`severity-pill severity-${event.severity}`}>{event.severity}</span></td>
                    <td>{event.event_name}</td>
                    <td>{event.message}</td>
                  </tr>
                ))
              ) : (
                <tr><td colSpan={4}>No matching events</td></tr>
              )}
            </tbody>
          </table>
        </div>
      </section>
    </main>
  );
}
