from __future__ import annotations

import pytest

from app import create_app
from app.database.db import db
from app.database.models import ControlCommand, MpcRecommendation, Telemetry
from app.services import mpc_control


class TestConfig:
    TESTING = True
    SQLALCHEMY_DATABASE_URI = "sqlite:///:memory:"
    SQLALCHEMY_TRACK_MODIFICATIONS = False
    MODEL_DIR = "./models"
    CORS_ORIGINS = "*"
    MQTT_ENABLED = False
    MPC_CLOSED_LOOP_ENABLED = True
    MPC_MAX_PWM = 200
    MPC_MAX_SAMPLE_GAP_SECONDS = 3.0
    MPC_PREHEAT_MAX_C = 128.0
    MPC_PUMP_PROBE_PWM = 155
    MPC_FIRMWARE_WATCHDOG_MS = 3500


class FakePredictor:
    def available_horizons(self):
        return [5]

    def predict(self, row, horizon_s, override_pwm=None, history=None):
        if int(row.get("event") or 0) == 1 and int(row.get("pump_on") or 0) == 1:
            return {"status": "ok", "horizon_s": horizon_s, "predicted_temp_c": 110.0}
        # Make 160 PWM land exactly at the 125 C setpoint.
        predicted_temp = 125.0 + abs(int(override_pwm) - 160) / 20.0
        return {"status": "ok", "horizon_s": horizon_s, "predicted_temp_c": predicted_temp}


@pytest.fixture()
def app():
    app = create_app(TestConfig)
    with app.app_context():
        db.create_all()
        yield app
        db.drop_all()


def add_history(*, mode: int = 4, heater_lockout: int = 0) -> Telemetry:
    latest = None
    for index in range(10):
        latest = Telemetry(
            device_id="intel-nuc-gateway",
            event=0,
            ms=index * 1000,
            temp_c=124.0,
            dtemp_c_per_s=0.0,
            setpoint_c=125.0,
            mode=mode,
            heater_pwm=128,
            heating=1,
            heater_lockout=heater_lockout,
            pump_on=0,
            motor_pwm=0,
            manual_kill=0,
            hard_kill=0,
        )
        db.session.add(latest)
    db.session.flush()
    return latest


def test_closed_loop_queues_latest_guarded_pwm(app, monkeypatch):
    monkeypatch.setattr(mpc_control, "get_predictor", lambda: FakePredictor())
    latest = add_history()

    result = mpc_control.process_closed_loop(latest)
    db.session.commit()

    assert result is not None
    assert result.advisory_only is False
    assert result.recommended_pwm == 200
    assert ControlCommand.query.one().command_type == "MPC_CONTROL"
    assert ControlCommand.query.one().value == 200
    assert ControlCommand.query.one().setpoint_c == 128.0
    assert result.preheat_target_c == 128.0
    assert result.predicted_pump_drop_c is not None
    assert MpcRecommendation.query.count() == 1


def test_closed_loop_does_not_queue_outside_pid_hold(app, monkeypatch):
    monkeypatch.setattr(mpc_control, "get_predictor", lambda: FakePredictor())
    latest = add_history(mode=3)

    assert mpc_control.process_closed_loop(latest) is None
    assert ControlCommand.query.count() == 0


def test_closed_loop_does_not_queue_during_lockout(app, monkeypatch):
    monkeypatch.setattr(mpc_control, "get_predictor", lambda: FakePredictor())
    latest = add_history(heater_lockout=1)

    assert mpc_control.process_closed_loop(latest) is None
    assert ControlCommand.query.count() == 0
