from __future__ import annotations

import csv
from io import StringIO


TELEMETRY_COLUMNS = [
    "event",
    "ms",
    "temp_c",
    "adc",
    "dtemp_c_per_s",
    "setpoint_c",
    "mode",
    "heater_pwm",
    "heating",
    "heater_lockout",
    "pump_enabled",
    "pump_allowed",
    "pump_on",
    "motor_pwm",
    "motor_on_ms",
    "motor_period_ms",
    "temp_before_pump_c",
    "min_temp_after_pump_c",
    "last_pump_drop_c",
    "recovery_time_s",
    "manual_kill",
    "hard_kill",
    "uptime_s",
]

INTEGER_FIELDS = {
    "event",
    "ms",
    "adc",
    "mode",
    "heater_pwm",
    "heating",
    "heater_lockout",
    "pump_enabled",
    "pump_allowed",
    "pump_on",
    "motor_pwm",
    "motor_on_ms",
    "motor_period_ms",
    "manual_kill",
    "hard_kill",
    "uptime_s",
}

FLOAT_FIELDS = {
    "temp_c",
    "dtemp_c_per_s",
    "setpoint_c",
    "temp_before_pump_c",
    "min_temp_after_pump_c",
    "last_pump_drop_c",
    "recovery_time_s",
}


def _read_csv_values(line: str) -> list[str]:
    reader = csv.reader(StringIO(line), skipinitialspace=True)
    try:
        values = next(reader)
    except StopIteration:
        return []
    return [value.strip() for value in values]


def parse_csv_line(line: str) -> dict | None:
    stripped = line.strip()
    if not stripped:
        return None
    if stripped.lower().startswith("event,ms,temp_c"):
        return None

    values = _read_csv_values(stripped)
    if len(values) != len(TELEMETRY_COLUMNS):
        raise ValueError(
            f"Expected {len(TELEMETRY_COLUMNS)} telemetry values, got {len(values)}: {stripped}"
        )

    parsed: dict[str, int | float] = {}
    for column, raw_value in zip(TELEMETRY_COLUMNS, values):
        if raw_value == "":
            raise ValueError(f"Missing value for telemetry field '{column}'")
        try:
            if column in INTEGER_FIELDS:
                parsed[column] = int(raw_value)
            elif column in FLOAT_FIELDS:
                parsed[column] = float(raw_value)
            else:
                raise ValueError(f"Unknown telemetry field '{column}'")
        except ValueError as exc:
            raise ValueError(f"Invalid value for field '{column}': {raw_value!r}") from exc

    return parsed


def parse_csv_batch(text: str) -> list[dict]:
    rows: list[dict] = []
    for line_number, line in enumerate(text.splitlines(), start=1):
        try:
            parsed = parse_csv_line(line)
        except ValueError as exc:
            raise ValueError(f"CSV line {line_number}: {exc}") from exc
        if parsed is not None:
            rows.append(parsed)
    return rows
