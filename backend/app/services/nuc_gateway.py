from __future__ import annotations

import argparse
import logging
import os
import subprocess
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import requests

from .telemetry_parser import parse_csv_line

LOGGER = logging.getLogger("scc.nuc_gateway")

DEFAULT_API_BASE_URL = "http://localhost:5000"
DEFAULT_SERIAL_PORT = "/dev/ttyACM0"
DEFAULT_SERIAL_BAUD = 115200
DEFAULT_DEVICE_ID = "intel-nuc-gateway"
DEFAULT_SITE_ID = "site-01"
DEFAULT_RIG_ID = "rig-01"


@dataclass(frozen=True)
class GatewayConfig:
    serial_port: str
    serial_baud: int
    api_base_url: str
    api_key: str
    device_id: str
    site_id: str
    rig_id: str
    poll_interval_s: float
    heartbeat_interval_s: float
    request_timeout_s: float
    avrdude_command: str
    arduino_mcu: str
    arduino_programmer: str
    arduino_upload_baud: int


def api_url(base_url: str, path: str) -> str:
    return f"{base_url.rstrip('/')}/{path.lstrip('/')}"


def api_headers(api_key: str) -> dict[str, str]:
    headers = {"Content-Type": "application/json"}
    if api_key:
        headers["X-API-Key"] = api_key
    return headers


def build_gateway_telemetry(row: dict[str, Any], config: GatewayConfig, raw_line: str) -> dict[str, Any]:
    payload = dict(row)
    payload.update(
        {
            "site_id": config.site_id,
            "rig_id": config.rig_id,
            "device_id": config.device_id,
            "raw_payload": {"source": "arduino_usb", "line": raw_line},
        }
    )
    return payload


def serial_command_for_backend_command(command: dict[str, Any]) -> str | None:
    command_type = str(command.get("type") or "").strip().upper()
    if command_type == "KILL":
        return f"KILL {int(command.get('value') or 0)}"
    if command_type == "SET_ON":
        return f"SET_ON {int(command.get('value') or 0)}"
    if command_type == "SETPOINT":
        setpoint_c = float(command.get("setpoint_c"))
        return f"SETPOINT {setpoint_c:.2f}"
    return None


def build_avrdude_command(hex_path: Path, config: GatewayConfig) -> list[str]:
    return [
        config.avrdude_command,
        "-v",
        "-p",
        config.arduino_mcu,
        "-c",
        config.arduino_programmer,
        "-P",
        config.serial_port,
        "-b",
        str(config.arduino_upload_baud),
        "-D",
        "-U",
        f"flash:w:{hex_path}:i",
    ]


class NucGateway:
    def __init__(self, config: GatewayConfig) -> None:
        self.config = config
        self.session = requests.Session()
        self.serial_conn: Any | None = None

    def open_serial(self) -> None:
        try:
            import serial
        except ImportError as exc:
            raise RuntimeError("pyserial is required. Install backend requirements on the NUC.") from exc

        self.serial_conn = serial.Serial(
            self.config.serial_port,
            self.config.serial_baud,
            timeout=0.2,
            write_timeout=1.0,
        )
        LOGGER.info("Opened Arduino USB serial port %s at %d baud", self.config.serial_port, self.config.serial_baud)

    def close_serial(self) -> None:
        if self.serial_conn is not None and self.serial_conn.is_open:
            self.serial_conn.close()
            LOGGER.info("Closed Arduino USB serial port %s", self.config.serial_port)

    def reopen_serial(self) -> None:
        self.close_serial()
        time.sleep(1.0)
        self.open_serial()

    def post_json(self, path: str, payload: dict[str, Any]) -> requests.Response:
        return self.session.post(
            api_url(self.config.api_base_url, path),
            json=payload,
            headers=api_headers(self.config.api_key),
            timeout=self.config.request_timeout_s,
        )

    def post_telemetry(self, row: dict[str, Any], raw_line: str) -> None:
        payload = build_gateway_telemetry(row, self.config, raw_line)
        response = self.post_json("/api/telemetry", payload)
        response.raise_for_status()

    def send_heartbeat(self) -> None:
        payload = {
            "device_id": self.config.device_id,
            "device_name": "Intel NUC USB Gateway",
            "platformio_env": "uno",
            "firmware_source_dir": "firmware/arduino/SCC-V1.4",
            "ota_port": self.config.serial_port,
            "ota_status": "Idle",
            "raw_payload": {"serial_port": self.config.serial_port, "serial_baud": self.config.serial_baud},
        }
        response = self.post_json("/api/firmware/heartbeat", payload)
        response.raise_for_status()

    def ack_command(self, cmd_id: int, status: str, message: str = "") -> None:
        response = self.post_json(
            f"/api/firmware/commands/{cmd_id}/ack",
            {"status": status, "message": message[:255]},
        )
        response.raise_for_status()

    def record_ota_event(self, path: str, status: str, message: str = "") -> None:
        payload = {
            "device_id": self.config.device_id,
            "device_name": "Intel NUC USB Gateway",
            "platformio_env": "uno",
            "ota_port": self.config.serial_port,
            "ota_status": status,
            "message": message[:255],
        }
        response = self.post_json(path, payload)
        response.raise_for_status()

    def poll_command(self) -> dict[str, Any] | None:
        response = self.session.get(
            api_url(self.config.api_base_url, "/api/firmware/commands/next?device=nuc"),
            timeout=self.config.request_timeout_s,
        )
        response.raise_for_status()
        payload = response.json()
        if not isinstance(payload, dict) or payload.get("status") == "none":
            return None
        return payload

    def write_serial_command(self, line: str) -> None:
        if self.serial_conn is None:
            raise RuntimeError("serial port is not open")
        self.serial_conn.write(f"{line}\n".encode("utf-8"))
        self.serial_conn.flush()
        LOGGER.info("-> Arduino: %s", line)

    def download_artifact(self, url: str) -> Path:
        response = self.session.get(url, timeout=self.config.request_timeout_s)
        response.raise_for_status()
        if not response.content:
            raise RuntimeError("firmware artifact download was empty")

        temp = tempfile.NamedTemporaryFile(prefix="scc-arduino-", suffix=".hex", delete=False)
        try:
            temp.write(response.content)
            return Path(temp.name)
        finally:
            temp.close()

    def flash_arduino(self, artifact_url: str) -> None:
        hex_path = self.download_artifact(artifact_url)
        try:
            self.close_serial()
            command = build_avrdude_command(hex_path, self.config)
            LOGGER.info("Starting Arduino flash with %s", " ".join(command))
            result = subprocess.run(command, check=False, capture_output=True, text=True)
            if result.returncode != 0:
                stderr = result.stderr.strip() or result.stdout.strip()
                raise RuntimeError(f"avrdude failed with exit code {result.returncode}: {stderr[-500:]}")
        finally:
            try:
                hex_path.unlink(missing_ok=True)
            finally:
                self.reopen_serial()

    def handle_command(self, command: dict[str, Any]) -> None:
        cmd_id = int(command.get("cmdId") or 0)
        command_type = str(command.get("type") or "").strip().upper()

        if command_type == "ARDUINO_OTA":
            if cmd_id <= 0:
                raise RuntimeError("Arduino OTA command is missing cmdId")
            artifact_url = str(command.get("url") or "").strip()
            if not artifact_url:
                self.ack_command(cmd_id, "failed", "Arduino OTA command did not include a firmware URL")
                return

            try:
                self.ack_command(cmd_id, "started", "Intel NUC started Arduino USB flash")
                self.record_ota_event("/api/firmware/ota-started", "Flashing", "Intel NUC started Arduino USB flash")
                self.flash_arduino(artifact_url)
            except Exception as exc:
                message = str(exc)
                self.record_ota_event("/api/firmware/ota-failed", "Failed", message)
                self.ack_command(cmd_id, "failed", message)
                raise

            self.record_ota_event("/api/firmware/ota-complete", "Version verified", "Arduino flash completed")
            self.ack_command(cmd_id, "success", "Arduino flash completed")
            return

        serial_line = serial_command_for_backend_command(command)
        if serial_line is None:
            if cmd_id > 0:
                self.ack_command(cmd_id, "failed", f"Unsupported command type for NUC gateway: {command_type}")
            LOGGER.warning("Unsupported backend command for NUC gateway: %s", command)
            return
        self.write_serial_command(serial_line)

    def read_once(self) -> None:
        if self.serial_conn is None:
            raise RuntimeError("serial port is not open")
        raw = self.serial_conn.readline()
        if not raw:
            return
        line = raw.decode("utf-8", errors="replace").strip()
        if not line:
            return
        try:
            parsed = parse_csv_line(line)
        except ValueError as exc:
            LOGGER.debug("Ignoring non-telemetry serial line %r: %s", line, exc)
            return
        if parsed is None:
            return
        self.post_telemetry(parsed, line)
        LOGGER.info("<- Arduino telemetry posted: ms=%s temp_c=%s", parsed.get("ms"), parsed.get("temp_c"))

    def run_forever(self) -> None:
        self.open_serial()
        last_poll = 0.0
        last_heartbeat = 0.0
        while True:
            now = time.monotonic()
            try:
                self.read_once()
                if now - last_heartbeat >= self.config.heartbeat_interval_s:
                    self.send_heartbeat()
                    last_heartbeat = now
                if now - last_poll >= self.config.poll_interval_s:
                    command = self.poll_command()
                    if command is not None:
                        self.handle_command(command)
                    last_poll = now
            except KeyboardInterrupt:
                raise
            except Exception as exc:
                LOGGER.exception("NUC gateway loop error: %s", exc)
                time.sleep(2.0)


def config_from_args(argv: list[str] | None = None) -> GatewayConfig:
    parser = argparse.ArgumentParser(description="Bridge Arduino USB telemetry and firmware commands through the Intel NUC.")
    parser.add_argument("--serial-port", default=os.getenv("NUC_SERIAL_PORT", DEFAULT_SERIAL_PORT))
    parser.add_argument("--serial-baud", type=int, default=int(os.getenv("NUC_SERIAL_BAUD", str(DEFAULT_SERIAL_BAUD))))
    parser.add_argument("--api-base-url", default=os.getenv("SCC_API_BASE_URL", DEFAULT_API_BASE_URL))
    parser.add_argument("--api-key", default=os.getenv("API_WRITE_KEY", ""))
    parser.add_argument("--device-id", default=os.getenv("NUC_DEVICE_ID", DEFAULT_DEVICE_ID))
    parser.add_argument("--site-id", default=os.getenv("MQTT_SITE_ID", DEFAULT_SITE_ID))
    parser.add_argument("--rig-id", default=os.getenv("MQTT_RIG_ID", DEFAULT_RIG_ID))
    parser.add_argument("--poll-interval-s", type=float, default=float(os.getenv("NUC_COMMAND_POLL_SECONDS", "1.0")))
    parser.add_argument("--heartbeat-interval-s", type=float, default=float(os.getenv("NUC_HEARTBEAT_SECONDS", "10.0")))
    parser.add_argument("--request-timeout-s", type=float, default=float(os.getenv("NUC_REQUEST_TIMEOUT_SECONDS", "10.0")))
    parser.add_argument("--avrdude-command", default=os.getenv("NUC_AVRDUDE_COMMAND", "avrdude"))
    parser.add_argument("--arduino-mcu", default=os.getenv("NUC_ARDUINO_MCU", "atmega328p"))
    parser.add_argument("--arduino-programmer", default=os.getenv("NUC_ARDUINO_PROGRAMMER", "arduino"))
    parser.add_argument("--arduino-upload-baud", type=int, default=int(os.getenv("NUC_ARDUINO_UPLOAD_BAUD", "115200")))
    args = parser.parse_args(argv)
    return GatewayConfig(**vars(args))


def main(argv: list[str] | None = None) -> None:
    logging.basicConfig(level=os.getenv("LOG_LEVEL", "INFO"), format="%(asctime)s %(levelname)s %(name)s %(message)s")
    gateway = NucGateway(config_from_args(argv))
    try:
        gateway.run_forever()
    finally:
        gateway.close_serial()


if __name__ == "__main__":
    main()
