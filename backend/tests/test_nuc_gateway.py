from __future__ import annotations

from pathlib import Path

from app.services.nuc_gateway import (
    GatewayConfig,
    NucGateway,
    build_avrdude_command,
    build_gateway_telemetry,
    serial_command_for_backend_command,
)


def config() -> GatewayConfig:
    return GatewayConfig(
        serial_port="/dev/ttyACM0",
        serial_baud=115200,
        api_base_url="http://localhost:5000",
        api_key="test",
        device_id="intel-nuc-gateway",
        site_id="site-01",
        rig_id="rig-01",
        poll_interval_s=1.0,
        heartbeat_interval_s=10.0,
        request_timeout_s=10.0,
        avrdude_command="avrdude",
        arduino_mcu="atmega328p",
        arduino_programmer="arduino",
        arduino_upload_baud=115200,
    )


def test_gateway_telemetry_adds_nuc_metadata():
    payload = build_gateway_telemetry({"event": 0, "ms": 123, "temp_c": 125.0}, config(), "0,123,...")

    assert payload["device_id"] == "intel-nuc-gateway"
    assert payload["site_id"] == "site-01"
    assert payload["rig_id"] == "rig-01"
    assert payload["raw_payload"]["source"] == "arduino_usb"


def test_serial_command_mapping():
    assert serial_command_for_backend_command({"type": "KILL", "value": 1}) == "KILL 1"
    assert serial_command_for_backend_command({"type": "SET_ON", "value": 0}) == "SET_ON 0"
    assert serial_command_for_backend_command({"type": "SETPOINT", "setpoint_c": 124.5}) == "SETPOINT 124.50"
    assert serial_command_for_backend_command({"type": "OTA"}) is None


def test_gateway_acknowledges_control_command_after_uart_write(monkeypatch):
    class FakeSerial:
        is_open = True

        def __init__(self):
            self.writes = []

        def write(self, data: bytes):
            self.writes.append(data)

        def flush(self):
            pass

    gateway = NucGateway(config())
    fake_serial = FakeSerial()
    acknowledgements = []
    gateway.serial_conn = fake_serial
    monkeypatch.setattr(
        gateway,
        "ack_command",
        lambda cmd_id, status, message="": acknowledgements.append((cmd_id, status, message)),
    )

    gateway.handle_command({"cmdId": 100000001, "type": "SET_ON", "value": 1})

    assert fake_serial.writes == [b"SET_ON 1\n"]
    assert acknowledgements == [(100000001, "success", "Delivered SET_ON to Arduino UART")]


def test_avrdude_command_uses_uploaded_hex_and_usb_port():
    command = build_avrdude_command(Path("/tmp/controller.hex"), config())

    assert command[:7] == ["avrdude", "-v", "-p", "atmega328p", "-c", "arduino", "-P"]
    assert "/dev/ttyACM0" in command
    assert "flash:w:/tmp/controller.hex:i" in command
