from __future__ import annotations

import argparse
import ast
from datetime import datetime
import importlib.util
from io import BytesIO
from pathlib import Path
import pkgutil
import re
from threading import Timer
from typing import IO
import webbrowser

import pandas as pd


if not hasattr(ast, "Str"):
    class _CompatAstStr(ast.Constant):
        def __init__(self, s: str = "", kind: str | None = None, **kwargs: object) -> None:
            super().__init__(value=s, kind=kind, **kwargs)

        @property
        def s(self) -> str:
            return self.value

        @s.setter
        def s(self, new_value: str) -> None:
            self.value = new_value

    ast.Str = _CompatAstStr  # type: ignore[attr-defined]


if not hasattr(pkgutil, "get_loader"):
    def _compat_get_loader(module_name: str):
        try:
            spec = importlib.util.find_spec(module_name)
        except (ImportError, ValueError):
            return None
        return None if spec is None else spec.loader

    pkgutil.get_loader = _compat_get_loader  # type: ignore[attr-defined]

try:
    from flask import Flask, render_template_string, request
except ModuleNotFoundError as exc:
    raise SystemExit(
        "Missing dependency: flask. Install it with "
        "`python3 -m pip install --user flask`."
    ) from exc

try:
    import plotly.graph_objects as go
except ModuleNotFoundError as exc:
    raise SystemExit(
        "Missing dependency: plotly. Install it with "
        "`python3 -m pip install --user plotly`."
    ) from exc


SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_OUTPUT = SCRIPT_DIR / "telemetry_plot.html"
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 8000
AUTO_CSV_GLOB = "telemetry_log_*.csv"
REQUIRED_COLUMNS = {"timestamp", "temperature_c"}
TRAILING_TZ_ABBR_RE = re.compile(r"^(?P<prefix>.+?)\s+(?P<tz>[A-Za-z]{2,5})$")
TIMEZONE_ABBREVIATION_OFFSETS = {
    "UTC": "+0000",
    "GMT": "+0000",
    "PST": "-0800",
    "PDT": "-0700",
    "MST": "-0700",
    "MDT": "-0600",
    "CST": "-0600",
    "CDT": "-0500",
    "EST": "-0500",
    "EDT": "-0400",
}
TIMESTAMP_FALLBACK_FORMATS = (
    "%Y-%m-%d %H:%M:%S %Z",
    "%Y-%m-%d %H:%M:%S.%f %Z",
    "%Y-%m-%dT%H:%M:%S %Z",
    "%Y-%m-%dT%H:%M:%S.%f %Z",
)
DIGITAL_COLUMNS: dict[str, dict[str, tuple[str, ...] | str]] = {
    "heater_on": {
        "label": "Heater ON",
        "aliases": ("heater_on", "heat"),
    },
    "motor": {
        "label": "Motor",
        "aliases": ("motor",),
    },
    "kill_state": {
        "label": "Kill State",
        "aliases": ("kill_state",),
    },
}
PAGE_TEMPLATE = """
<!doctype html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Telemetry Plotter</title>
    <style>
        :root {
            --bg-top: #fff5d8;
            --bg-bottom: #dce7e0;
            --card: rgba(255, 255, 255, 0.92);
            --ink: #1c2831;
            --muted: #52626f;
            --accent: #0f7a5c;
            --accent-soft: #def2e8;
            --border: #b7cfc4;
            --error-bg: #fee9e7;
            --error-ink: #8e2f2a;
            --shadow: 0 20px 45px rgba(28, 40, 49, 0.12);
        }

        * {
            box-sizing: border-box;
        }

        body {
            margin: 0;
            color: var(--ink);
            font-family: "Avenir Next", "Segoe UI", sans-serif;
            background:
                radial-gradient(circle at top, var(--bg-top) 0%, #f3eee1 38%, var(--bg-bottom) 100%);
        }

        main {
            max-width: 1180px;
            margin: 0 auto;
            padding: 32px 20px 48px;
        }

        h1 {
            margin: 0;
            font-size: clamp(2rem, 4vw, 3.2rem);
            line-height: 1;
            letter-spacing: -0.04em;
        }

        .subtitle {
            margin: 12px 0 0;
            color: var(--muted);
            font-size: 1rem;
        }

        .card {
            margin-top: 22px;
            padding: 24px;
            border: 1px solid rgba(183, 207, 196, 0.9);
            border-radius: 24px;
            background: var(--card);
            box-shadow: var(--shadow);
            backdrop-filter: blur(16px);
        }

        .dropzone {
            display: grid;
            gap: 10px;
            min-height: 220px;
            padding: 28px;
            border: 2px dashed var(--border);
            border-radius: 22px;
            place-items: center;
            text-align: center;
            background:
                linear-gradient(135deg, rgba(222, 242, 232, 0.85), rgba(255, 255, 255, 0.8));
            cursor: pointer;
            transition: transform 160ms ease, border-color 160ms ease, background 160ms ease;
        }

        .dropzone:hover,
        .dropzone:focus,
        .dropzone.dragover {
            outline: none;
            transform: translateY(-2px);
            border-color: var(--accent);
            background:
                linear-gradient(135deg, rgba(222, 242, 232, 1), rgba(255, 255, 255, 0.96));
        }

        .dropzone strong {
            font-size: 1.2rem;
        }

        .dropzone span {
            color: var(--muted);
        }

        .caption {
            font-size: 0.92rem;
        }

        .toolbar {
            display: flex;
            flex-wrap: wrap;
            gap: 14px;
            align-items: center;
            margin-top: 18px;
        }

        button {
            border: 0;
            border-radius: 999px;
            padding: 12px 18px;
            color: white;
            font: inherit;
            font-weight: 600;
            background: linear-gradient(135deg, #136f63, var(--accent));
            cursor: pointer;
        }

        #selected-file {
            color: var(--muted);
            word-break: break-word;
        }

        .error {
            margin-top: 16px;
            padding: 14px 16px;
            border-radius: 16px;
            background: var(--error-bg);
            color: var(--error-ink);
        }

        .plot-card h2 {
            margin: 0 0 14px;
            font-size: 1.1rem;
        }

        .plot-container > div {
            min-height: 420px;
        }

        @media (max-width: 720px) {
            .card {
                padding: 18px;
            }

            .dropzone {
                min-height: 180px;
                padding: 20px;
            }
        }
    </style>
</head>
<body>
    <main>
        <header>
            <h1>Telemetry Plotter</h1>
            <p class="subtitle">
                Drop a telemetry CSV onto the page and the chart will be rendered locally.
            </p>
        </header>

        <form id="upload-form" class="card" action="/plot" method="post" enctype="multipart/form-data">
            <input id="file-input" type="file" name="file" accept=".csv,text/csv" hidden>

            <div id="dropzone" class="dropzone" tabindex="0" role="button" aria-label="Upload telemetry CSV">
                <strong>Drag and drop a CSV file here</strong>
                <span>or click to browse</span>
                <span class="caption">The browser submits the file directly to the local app.</span>
            </div>

            <div class="toolbar">
                <button type="button" id="browse-button">Choose CSV</button>
                <div id="selected-file">{{ file_name or "No file selected yet." }}</div>
            </div>

            {% if error %}
            <div class="error">{{ error }}</div>
            {% endif %}
        </form>

        {% if plot_html %}
        <section class="card plot-card">
            <h2>{{ file_name }}</h2>
            <div class="plot-container">{{ plot_html | safe }}</div>
        </section>
        {% endif %}
    </main>

    <script>
        const uploadForm = document.getElementById("upload-form");
        const dropzone = document.getElementById("dropzone");
        const fileInput = document.getElementById("file-input");
        const browseButton = document.getElementById("browse-button");
        const selectedFile = document.getElementById("selected-file");

        function replaceDocument(html) {
            document.open();
            document.write(html);
            document.close();
        }

        function submitFiles(fileList) {
            if (!fileList || !fileList.length) {
                return;
            }

            const file = fileList[0];
            selectedFile.textContent = file.name;

            if (fileList === fileInput.files) {
                uploadForm.submit();
                return;
            }

            try {
                const transfer = new DataTransfer();
                transfer.items.add(file);
                fileInput.files = transfer.files;
                uploadForm.submit();
            } catch (error) {
                const formData = new FormData();
                formData.append("file", file);
                fetch("/plot", {
                    method: "POST",
                    body: formData
                })
                    .then((response) => response.text())
                    .then(replaceDocument);
            }
        }

        browseButton.addEventListener("click", () => fileInput.click());
        dropzone.addEventListener("click", () => fileInput.click());
        dropzone.addEventListener("keydown", (event) => {
            if (event.key === "Enter" || event.key === " ") {
                event.preventDefault();
                fileInput.click();
            }
        });

        fileInput.addEventListener("change", () => submitFiles(fileInput.files));

        ["dragenter", "dragover"].forEach((eventName) => {
            dropzone.addEventListener(eventName, (event) => {
                event.preventDefault();
                dropzone.classList.add("dragover");
            });
        });

        ["dragleave", "dragend"].forEach((eventName) => {
            dropzone.addEventListener(eventName, () => {
                dropzone.classList.remove("dragover");
            });
        });

        dropzone.addEventListener("drop", (event) => {
            event.preventDefault();
            dropzone.classList.remove("dragover");
            submitFiles(event.dataTransfer.files);
        });
    </script>
</body>
</html>
"""


def coerce_digital_series(series: pd.Series) -> pd.Series:
    normalized = series.astype("string").str.strip().str.lower()
    state_map = {
        "0": 0.0,
        "0.0": 0.0,
        "off": 0.0,
        "false": 0.0,
        "no": 0.0,
        "1": 1.0,
        "1.0": 1.0,
        "on": 1.0,
        "true": 1.0,
        "yes": 1.0,
    }
    mapped = normalized.map(state_map)
    numeric = pd.to_numeric(series, errors="coerce")
    return mapped.fillna(numeric)


def parse_timestamp_value(value: object) -> pd.Timestamp:
    if pd.isna(value):
        return pd.NaT

    raw = str(value).strip()
    if not raw:
        return pd.NaT

    try:
        timestamp = pd.Timestamp(raw)
    except Exception:
        timestamp = pd.NaT
        match = TRAILING_TZ_ABBR_RE.match(raw)
        if match:
            offset = TIMEZONE_ABBREVIATION_OFFSETS.get(match.group("tz").upper())
            if offset:
                candidate = f"{match.group('prefix')} {offset}"
                try:
                    timestamp = pd.Timestamp(candidate)
                except Exception:
                    timestamp = pd.NaT

        if pd.isna(timestamp):
            for fmt in TIMESTAMP_FALLBACK_FORMATS:
                try:
                    timestamp = pd.Timestamp(datetime.strptime(raw, fmt))
                    break
                except ValueError:
                    continue

        if pd.isna(timestamp):
            return pd.NaT

    if timestamp.tzinfo is not None:
        return timestamp.tz_localize(None)

    return timestamp


def parse_timestamp_series(series: pd.Series) -> pd.Series:
    parsed = series.apply(parse_timestamp_value)
    return pd.to_datetime(parsed, errors="coerce")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Start a local drag-and-drop upload UI for telemetry CSV files. "
            "If a CSV path is provided, the script renders it directly to HTML."
        )
    )
    parser.add_argument(
        "csv",
        nargs="?",
        help=(
            "Path to a telemetry CSV file to render directly. If omitted, the script "
            "starts the upload UI."
        ),
    )
    parser.add_argument(
        "--direct",
        action="store_true",
        help=(
            "Render directly to HTML instead of starting the upload UI. If no CSV "
            f"path is provided, the newest `{AUTO_CSV_GLOB}` file is used."
        ),
    )
    parser.add_argument(
        "--output",
        default=str(DEFAULT_OUTPUT),
        help="HTML output path for direct rendering mode.",
    )
    parser.add_argument(
        "--host",
        default=DEFAULT_HOST,
        help="Host for the local upload UI.",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=DEFAULT_PORT,
        help="Port for the local upload UI.",
    )
    parser.add_argument(
        "--no-show",
        action="store_true",
        help="Skip opening a browser automatically.",
    )
    return parser.parse_args()


def discover_csv_candidates() -> list[Path]:
    search_dirs = [Path.cwd(), SCRIPT_DIR, Path.home() / "Downloads"]
    candidates: list[Path] = []
    seen: set[Path] = set()

    for directory in search_dirs:
        for candidate in directory.glob(AUTO_CSV_GLOB):
            resolved = candidate.resolve()
            if resolved not in seen:
                seen.add(resolved)
                candidates.append(resolved)

    return sorted(candidates, key=lambda path: path.stat().st_mtime)


def resolve_csv_path(csv_arg: str | None) -> Path:
    if csv_arg:
        csv_path = Path(csv_arg).expanduser()
        if not csv_path.is_absolute():
            csv_path = (Path.cwd() / csv_path).resolve()
        if csv_path.is_file():
            return csv_path
        raise ValueError(f"CSV file not found: {csv_path}")

    candidates = discover_csv_candidates()
    if candidates:
        return candidates[-1]

    raise ValueError(
        "No telemetry CSV found. Pass a CSV path explicitly, for example:\n"
        "  python3 plot.py /path/to/telemetry_log.csv"
    )


def resolve_output_path(output_arg: str) -> Path:
    output_path = Path(output_arg).expanduser()
    if not output_path.is_absolute():
        output_path = (Path.cwd() / output_path).resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    return output_path


def normalize_dataframe(df: pd.DataFrame) -> pd.DataFrame:
    rename_map: dict[str, str] = {}
    for canonical_name, config in DIGITAL_COLUMNS.items():
        aliases = config["aliases"]
        matching_column = next((column for column in aliases if column in df.columns), None)
        if matching_column and matching_column != canonical_name:
            rename_map[matching_column] = canonical_name

    if rename_map:
        df = df.rename(columns=rename_map)

    return df


def load_dataframe(csv_source: Path | IO[bytes], source_label: str) -> pd.DataFrame:
    try:
        df = pd.read_csv(csv_source)
    except Exception as exc:
        raise ValueError(f"Could not read CSV file `{source_label}`: {exc}") from exc

    df = normalize_dataframe(df)

    missing_columns = REQUIRED_COLUMNS.difference(df.columns)
    if missing_columns:
        missing_list = ", ".join(sorted(missing_columns))
        raise ValueError(
            f"CSV is missing required columns: {missing_list}. File: {source_label}"
        )

    df["timestamp"] = parse_timestamp_series(df["timestamp"])
    df["temperature_c"] = pd.to_numeric(df["temperature_c"], errors="coerce")

    for column_name in DIGITAL_COLUMNS:
        if column_name in df.columns:
            df[column_name] = coerce_digital_series(df[column_name])

    df = df.dropna(subset=["timestamp"]).sort_values("timestamp")
    if df.empty:
        raise ValueError(f"No valid rows with timestamps were found in {source_label}")

    return df


def build_figure(df: pd.DataFrame, source_name: str) -> go.Figure:
    fig = go.Figure()
    digital_columns = [
        column_name
        for column_name in DIGITAL_COLUMNS
        if column_name in df.columns and df[column_name].notna().any()
    ]

    fig.add_trace(
        go.Scatter(
            x=df["timestamp"],
            y=df["temperature_c"],
            mode="lines",
            name="Temperature (°C)",
            hovertemplate="Time=%{x}<br>Temp=%{y:.2f} °C<extra></extra>",
        )
    )

    for column_name in digital_columns:
        label = str(DIGITAL_COLUMNS[column_name]["label"])
        fig.add_trace(
            go.Scatter(
                x=df["timestamp"],
                y=df[column_name],
                mode="lines",
                name=label,
                yaxis="y2",
                line=dict(shape="hv"),
                customdata=df[column_name].map(
                    lambda value: "On" if value >= 0.5 else "Off"
                ),
                hovertemplate=f"Time=%{{x}}<br>{label}=%{{customdata}}<extra></extra>",
            )
        )

    layout: dict[str, object] = {
        "xaxis": dict(title="Time", rangeslider=dict(visible=True), type="date"),
        "yaxis": dict(title="Temperature (°C)"),
        "hovermode": "x unified",
        "template": "plotly_white",
        "legend": dict(orientation="h", y=1.12, x=0),
        "margin": dict(t=42, r=42, b=42, l=56),
    }

    if digital_columns:
        layout["yaxis2"] = dict(
            title="Digital States",
            overlaying="y",
            side="right",
            range=[-0.1, 1.1],
            tickmode="array",
            tickvals=[0, 1],
            ticktext=["Off", "On"],
        )

    fig.update_layout(**layout)

    return fig


def render_direct_plot(csv_path: Path, output_path: Path, show_browser: bool) -> None:
    df = load_dataframe(csv_path, str(csv_path))
    fig = build_figure(df, csv_path.name)
    fig.write_html(
        output_path,
        config={"displaylogo": False, "responsive": True},
    )
    print(f"Saved interactive plot to {output_path}")

    if show_browser:
        fig.show(config={"displaylogo": False, "responsive": True})


def render_plot_fragment(fig: go.Figure) -> str:
    return fig.to_html(
        full_html=False,
        include_plotlyjs=True,
        config={"displaylogo": False, "responsive": True},
    )


def create_app() -> Flask:
    instance_path = (SCRIPT_DIR / "instance").resolve()
    instance_path.mkdir(parents=True, exist_ok=True)
    app = Flask(
        "telemetry_plotter",
        root_path=str(SCRIPT_DIR),
        instance_path=str(instance_path),
        static_folder=None,
    )
    app.config["MAX_CONTENT_LENGTH"] = 32 * 1024 * 1024

    @app.get("/")
    def index() -> str:
        return render_template_string(
            PAGE_TEMPLATE,
            error=None,
            file_name=None,
            plot_html=None,
        )

    @app.post("/plot")
    def plot_upload() -> tuple[str, int] | str:
        uploaded_file = request.files.get("file")
        if uploaded_file is None or not uploaded_file.filename:
            return (
                render_template_string(
                    PAGE_TEMPLATE,
                    error="Choose a CSV file to upload.",
                    file_name=None,
                    plot_html=None,
                ),
                400,
            )

        file_name = Path(uploaded_file.filename).name
        try:
            payload = uploaded_file.read()
            if not payload:
                raise ValueError("The uploaded file is empty.")

            df = load_dataframe(BytesIO(payload), file_name)
            fig = build_figure(df, file_name)
            plot_html = render_plot_fragment(fig)
        except ValueError as exc:
            return (
                render_template_string(
                    PAGE_TEMPLATE,
                    error=str(exc),
                    file_name=file_name,
                    plot_html=None,
                ),
                400,
            )

        return render_template_string(
            PAGE_TEMPLATE,
            error=None,
            file_name=file_name,
            plot_html=plot_html,
        )

    @app.errorhandler(413)
    def request_too_large(_: Exception) -> tuple[str, int]:
        return (
            render_template_string(
                PAGE_TEMPLATE,
                error="File is too large. Upload a CSV smaller than 32 MB.",
                file_name=None,
                plot_html=None,
            ),
            413,
        )

    return app


def open_browser_later(url: str) -> None:
    def _open() -> None:
        try:
            webbrowser.open_new_tab(url)
        except Exception:
            print(f"Open the upload UI in a browser: {url}")

    Timer(1.0, _open).start()


def run_upload_ui(host: str, port: int, open_browser: bool) -> None:
    url = f"http://{host}:{port}"
    print(f"Starting telemetry upload UI at {url}")
    print("Press Ctrl+C to stop the server.")

    if open_browser:
        open_browser_later(url)

    app = create_app()
    app.run(host=host, port=port, debug=False, use_reloader=False)


def main() -> None:
    args = parse_args()
    direct_mode = args.direct or args.csv is not None

    if direct_mode:
        try:
            csv_path = resolve_csv_path(args.csv)
            output_path = resolve_output_path(args.output)
            render_direct_plot(
                csv_path=csv_path,
                output_path=output_path,
                show_browser=not args.no_show,
            )
        except ValueError as exc:
            raise SystemExit(str(exc)) from exc
        return

    run_upload_ui(
        host=args.host,
        port=args.port,
        open_browser=not args.no_show,
    )


if __name__ == "__main__":
    main()
