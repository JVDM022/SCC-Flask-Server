#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path
from typing import Any
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

REPO_ROOT = Path(__file__).resolve().parents[3]
BACKEND_ROOT = REPO_ROOT / "backend"
sys.path.insert(0, str(BACKEND_ROOT))

from app.services.telemetry_parser import FLOAT_FIELDS, INTEGER_FIELDS, TELEMETRY_COLUMNS, parse_csv_batch  # noqa: E402


WATCHED_ML_TABLES = ("ml_predictions", "mpc_recommendations")
text = None


def read_source_rows(path: Path) -> list[dict[str, Any]]:
    rows = parse_csv_batch(path.read_text(encoding="utf-8"))
    if not rows:
        raise SystemExit(f"No telemetry rows found in {path}")
    return rows


def normalize_database_url(database_url: str) -> str:
    if database_url.startswith("postgres://"):
        return database_url.replace("postgres://", "postgresql://", 1)
    return database_url


def table_count(connection, table_name: str) -> int | None:
    exists = connection.execute(
        text(
            """
            select exists (
              select 1
              from information_schema.tables
              where table_schema = current_schema()
                and table_name = :table_name
            )
            """
        ),
        {"table_name": table_name},
    ).scalar()
    if not exists:
        return None
    return int(connection.execute(text(f"select count(*) from {table_name}")).scalar() or 0)


def fetch_telemetry_by_ids(connection, ids: list[int]) -> list[dict[str, Any]]:
    if not ids:
        return []
    params = {f"id_{index}": telemetry_id for index, telemetry_id in enumerate(ids)}
    placeholders = ", ".join(f":id_{index}" for index in range(len(ids)))
    rows = (
        connection.execute(
            text(
                f"""
                select *
                from telemetry
                where id in ({placeholders})
                order by id asc
                """
            ),
            params,
        )
        .mappings()
        .all()
    )
    return [dict(row) for row in rows]


def fetch_latest_telemetry(connection, count: int) -> list[dict[str, Any]]:
    rows = (
        connection.execute(
            text(
                """
                select *
                from telemetry
                order by id desc
                limit :count
                """
            ),
            {"count": count},
        )
        .mappings()
        .all()
    )
    return [dict(row) for row in reversed(rows)]


def post_row(api_url: str, api_key: str, row: dict[str, Any]) -> int:
    body = json.dumps(row).encode("utf-8")
    request = Request(
        api_url,
        data=body,
        headers={
            "Content-Type": "application/json",
            "X-API-Key": api_key,
        },
        method="POST",
    )
    try:
        with urlopen(request, timeout=15) as response:
            payload = json.loads(response.read().decode("utf-8"))
    except HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"POST {api_url} failed with HTTP {exc.code}: {detail}") from exc
    except URLError as exc:
        raise RuntimeError(f"POST {api_url} failed: {exc}") from exc

    if payload.get("status") != "ok" or "id" not in payload:
        raise RuntimeError(f"Unexpected telemetry response: {payload}")
    return int(payload["id"])


def compare_rows(expected_rows: list[dict[str, Any]], stored_rows: list[dict[str, Any]], float_tolerance: float) -> list[str]:
    errors: list[str] = []
    if len(expected_rows) != len(stored_rows):
        return [f"Expected {len(expected_rows)} stored rows, found {len(stored_rows)}"]

    for row_index, (expected, stored) in enumerate(zip(expected_rows, stored_rows), start=1):
        for column in TELEMETRY_COLUMNS:
            expected_value = expected[column]
            stored_value = stored[column]
            if column in INTEGER_FIELDS and int(stored_value) != int(expected_value):
                errors.append(
                    f"row {row_index} column {column}: expected {expected_value}, stored {stored_value}"
                )
            elif column in FLOAT_FIELDS:
                if not math.isclose(float(stored_value), float(expected_value), abs_tol=float_tolerance):
                    errors.append(
                        f"row {row_index} column {column}: expected {expected_value}, stored {stored_value}"
                    )
    return errors


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Verify Arduino CSV telemetry matches rows stored by Flask in PostgreSQL."
    )
    parser.add_argument("--source", required=True, type=Path, help="Arduino CSV file to compare.")
    parser.add_argument("--database-url", required=True, help="PostgreSQL SQLAlchemy URL.")
    parser.add_argument(
        "--mode",
        choices=("verify-db", "post-and-verify"),
        default="verify-db",
        help="verify-db compares latest rows already posted by ESP32; post-and-verify posts source rows first.",
    )
    parser.add_argument("--api-url", default="http://localhost:5050/api/telemetry", help="Flask telemetry POST URL.")
    parser.add_argument("--api-key", default="", help="Backend API_WRITE_KEY for post-and-verify mode.")
    parser.add_argument("--float-tolerance", type=float, default=0.01, help="Absolute tolerance for float comparisons.")
    return parser.parse_args()


def main() -> int:
    global text

    args = parse_args()

    from sqlalchemy import create_engine, text as sqlalchemy_text

    text = sqlalchemy_text
    source_rows = read_source_rows(args.source)
    engine = create_engine(normalize_database_url(args.database_url), future=True)
    inserted_ids: list[int] = []
    before_counts: dict[str, int | None] = {}

    with engine.begin() as connection:
        if args.mode == "post-and-verify":
            if not args.api_key:
                raise SystemExit("--api-key is required with --mode post-and-verify")
            before_counts = {table: table_count(connection, table) for table in WATCHED_ML_TABLES}

    if args.mode == "post-and-verify":
        for row in source_rows:
            inserted_ids.append(post_row(args.api_url, args.api_key, row))

    with engine.begin() as connection:
        if args.mode == "post-and-verify":
            stored_rows = fetch_telemetry_by_ids(connection, inserted_ids)
            after_counts = {table: table_count(connection, table) for table in WATCHED_ML_TABLES}
        else:
            stored_rows = fetch_latest_telemetry(connection, len(source_rows))
            after_counts = {}

    errors = compare_rows(source_rows, stored_rows, args.float_tolerance)

    if args.mode == "post-and-verify":
        for table in WATCHED_ML_TABLES:
            if before_counts.get(table) != after_counts.get(table):
                errors.append(
                    f"{table} count changed: before {before_counts.get(table)}, after {after_counts.get(table)}"
                )

    if errors:
        print("Telemetry pipeline verification failed:")
        for error in errors:
            print(f"- {error}")
        return 1

    stored_ids = [row["id"] for row in stored_rows]
    print("Telemetry pipeline verification passed.")
    print(f"Compared {len(source_rows)} row(s). Stored telemetry id(s): {stored_ids}")
    if args.mode == "post-and-verify":
        print("ML/MPC table counts were unchanged.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
