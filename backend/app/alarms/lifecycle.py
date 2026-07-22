from __future__ import annotations

from datetime import datetime

from ..database.db import db
from ..database.models import Alarm, Telemetry


def sync_active_alarms(telemetry: Telemetry, evaluated_alarms: list[dict]) -> None:
    """Keep one active alarm per code for a telemetry device and clear stale rows."""
    current_by_code = {alarm["alarm_code"]: alarm for alarm in evaluated_alarms}
    active_query = Alarm.query.join(Telemetry, Alarm.telemetry_id == Telemetry.id).filter(Alarm.active.is_(True))
    if telemetry.device_id is None:
        active_query = active_query.filter(Telemetry.device_id.is_(None))
    else:
        active_query = active_query.filter(Telemetry.device_id == telemetry.device_id)

    existing_by_code: dict[str, list[Alarm]] = {}
    for alarm in active_query.order_by(Alarm.created_at.asc(), Alarm.id.asc()).all():
        existing_by_code.setdefault(alarm.alarm_code, []).append(alarm)

    cleared_at = datetime.utcnow()
    for alarm_code, rows in existing_by_code.items():
        if alarm_code not in current_by_code:
            for row in rows:
                row.active = False
                row.cleared_at = cleared_at
            continue

        current = current_by_code[alarm_code]
        keeper, *duplicates = rows
        keeper.severity = current["severity"]
        keeper.message = current["message"]
        for duplicate in duplicates:
            duplicate.active = False
            duplicate.cleared_at = cleared_at

    for alarm_code, alarm in current_by_code.items():
        if alarm_code not in existing_by_code:
            db.session.add(Alarm(telemetry_id=telemetry.id, **alarm))
