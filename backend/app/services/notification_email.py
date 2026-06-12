from __future__ import annotations

import logging
import smtplib
import threading
from datetime import datetime, timedelta
from email.message import EmailMessage
from typing import Any

from flask import Flask

LOGGER = logging.getLogger(__name__)
_LAST_SENT_AT: dict[str, datetime] = {}
_LOCK = threading.Lock()


def _split_recipients(value: str) -> list[str]:
    return [item.strip() for item in value.replace(";", ",").split(",") if item.strip()]


def _email_configured(app: Flask) -> bool:
    return bool(app.config.get("ALERT_EMAIL_ENABLED") and app.config.get("SMTP_HOST") and app.config.get("ALERT_EMAIL_TO"))


def _cooldown_allows(key: str, cooldown_seconds: int) -> bool:
    now = datetime.utcnow()
    with _LOCK:
        previous = _LAST_SENT_AT.get(key)
        if previous is not None and now - previous < timedelta(seconds=cooldown_seconds):
            return False
        _LAST_SENT_AT[key] = now
        return True


def notify_critical_alarms(app: Flask, telemetry: Any, alarms: list[dict]) -> None:
    if not _email_configured(app):
        return

    critical = [alarm for alarm in alarms if alarm.get("severity") == "critical"]
    if not critical:
        return

    device_id = str(getattr(telemetry, "device_id", None) or "unknown-device")
    cooldown_seconds = int(app.config.get("ALERT_EMAIL_COOLDOWN_SECONDS") or 900)
    alarm_codes = ",".join(sorted(str(alarm.get("alarm_code") or "UNKNOWN") for alarm in critical))
    cooldown_key = f"{device_id}:{alarm_codes}"
    if not _cooldown_allows(cooldown_key, cooldown_seconds):
        return

    recipients = _split_recipients(str(app.config.get("ALERT_EMAIL_TO") or ""))
    if not recipients:
        return

    from_addr = str(app.config.get("ALERT_EMAIL_FROM") or app.config.get("SMTP_USERNAME") or "scc-alerts@localhost")
    subject = f"SCC critical alert: {alarm_codes}"
    lines = [
        "SCC critical safety alert",
        "",
        f"Device: {device_id}",
        f"Site: {getattr(telemetry, 'site_id', None) or 'unknown'}",
        f"Rig: {getattr(telemetry, 'rig_id', None) or 'unknown'}",
        f"Temperature: {getattr(telemetry, 'temp_c', None)} C",
        f"ADC: {getattr(telemetry, 'adc', None)}",
        f"Heater PWM: {getattr(telemetry, 'heater_pwm', None)}",
        f"Pump on: {getattr(telemetry, 'pump_on', None)}",
        "",
        "Alarms:",
    ]
    lines.extend(f"- {alarm.get('alarm_code')}: {alarm.get('message')}" for alarm in critical)
    lines.extend(
        [
            "",
            "Firmware safety response should force heater and pump outputs off for hard kill or sensor fault conditions.",
        ]
    )

    message = EmailMessage()
    message["From"] = from_addr
    message["To"] = ", ".join(recipients)
    message["Subject"] = subject
    message.set_content("\n".join(lines))

    try:
        with smtplib.SMTP(str(app.config["SMTP_HOST"]), int(app.config.get("SMTP_PORT") or 587), timeout=10) as smtp:
            if app.config.get("SMTP_STARTTLS"):
                smtp.starttls()
            username = str(app.config.get("SMTP_USERNAME") or "")
            password = str(app.config.get("SMTP_PASSWORD") or "")
            if username:
                smtp.login(username, password)
            smtp.send_message(message)
    except Exception:
        LOGGER.exception("Failed to send critical alert email")
