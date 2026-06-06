from __future__ import annotations

from functools import wraps
from hmac import compare_digest
from typing import Callable, TypeVar

from flask import current_app, jsonify, request


F = TypeVar("F", bound=Callable)


def _request_api_key() -> str:
    header_key = request.headers.get("X-API-Key", "").strip()
    if header_key:
        return header_key

    auth_header = request.headers.get("Authorization", "").strip()
    scheme, _, token = auth_header.partition(" ")
    if scheme.lower() == "bearer" and token:
        return token.strip()
    return ""


def require_api_key(view: F) -> F:
    @wraps(view)
    def wrapped(*args, **kwargs):
        expected_key = str(current_app.config.get("API_WRITE_KEY") or "").strip()
        if not expected_key:
            return view(*args, **kwargs)

        provided_key = _request_api_key()
        if not provided_key or not compare_digest(provided_key, expected_key):
            return jsonify({"status": "error", "message": "Valid API key is required."}), 401

        return view(*args, **kwargs)

    return wrapped  # type: ignore[return-value]
