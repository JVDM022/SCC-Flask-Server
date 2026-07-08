from __future__ import annotations

import pytest

from app.services.telemetry_parser import TELEMETRY_COLUMNS, parse_csv_batch, parse_csv_line


ROW = "0,123456,126.42,222,-0.1300,125.00,3,184,1,0,1,1,0,155,150,30000,127.20,124.80,2.40,18.50,0,0,300"
HEADER = ",".join(TELEMETRY_COLUMNS)


def test_row_only_csv_parsing():
    parsed = parse_csv_line(ROW)

    assert parsed["event"] == 0
    assert parsed["ms"] == 123456
    assert parsed["temp_c"] == 126.42
    assert parsed["heater_pwm"] == 184
    assert parsed["recovery_time_s"] == 18.5


def test_header_plus_row_csv_parsing():
    rows = parse_csv_batch(f"{HEADER}\n{ROW}\n")

    assert len(rows) == 1
    assert rows[0]["adc"] == 222


def test_bad_column_count_raises():
    with pytest.raises(ValueError, match="Expected 23 telemetry values"):
        parse_csv_line("0,123456,126.42")


def test_empty_line_returns_none():
    assert parse_csv_line("   ") is None


def test_numeric_conversion_types():
    parsed = parse_csv_line(ROW)

    assert isinstance(parsed["event"], int)
    assert isinstance(parsed["adc"], int)
    assert isinstance(parsed["temp_c"], float)
    assert isinstance(parsed["dtemp_c_per_s"], float)


def test_blank_csv_value_is_preserved_as_none():
    values = ROW.split(",")
    values[2] = ""

    parsed = parse_csv_line(",".join(values))

    assert parsed["temp_c"] is None
    assert parsed["setpoint_c"] == 125.0


def test_non_finite_csv_float_is_preserved_as_none():
    values = ROW.split(",")
    values[16] = "nan"

    parsed = parse_csv_line(",".join(values))

    assert parsed["temp_before_pump_c"] is None
