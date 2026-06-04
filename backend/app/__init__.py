from __future__ import annotations

from .config import Config


def create_app(config_object: type[Config] = Config) -> Flask:
    from flask import Flask
    from flask_cors import CORS

    from .api.firmware import firmware_api
    from .api.routes import api
    from .database.db import db
    from .realtime import socketio
    from .services.mqtt_subscriber import start_mqtt_subscriber

    app = Flask(__name__)
    app.config.from_object(config_object)

    CORS(app, origins=app.config.get("CORS_ORIGINS", "*"))
    db.init_app(app)
    socketio.init_app(app, cors_allowed_origins=app.config.get("CORS_ORIGINS", "*"))
    app.register_blueprint(api)
    app.register_blueprint(firmware_api)
    start_mqtt_subscriber(app)

    @app.cli.command("init-db")
    def init_db() -> None:
        from .database import models  # noqa: F401

        db.create_all()
        print("Database tables created.")

    return app
