from __future__ import annotations

from ..database.db import db
from ..database.models import PumpCycle, Telemetry


def extract_pump_cycle_from_event(telemetry: Telemetry) -> PumpCycle | None:
    # A complete production extractor would pair start/end/recovery events.
    # This lightweight version records recovery events that already contain the ESP32 cycle metrics.
    if telemetry.event != 3:
        return None

    cycle = PumpCycle(
        recovery_time=telemetry.created_at,
        temp_before_pump_c=telemetry.temp_before_pump_c,
        min_temp_after_pump_c=telemetry.min_temp_after_pump_c,
        last_pump_drop_c=telemetry.last_pump_drop_c,
        recovery_time_s=telemetry.recovery_time_s,
        avg_heater_pwm=float(telemetry.heater_pwm) if telemetry.heater_pwm is not None else None,
        avg_motor_pwm=float(telemetry.motor_pwm) if telemetry.motor_pwm is not None else None,
    )
    db.session.add(cycle)
    return cycle
