import csv
import json
import os
import re
from datetime import datetime, timezone
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse


HOST = "0.0.0.0"
PORT = 8080
TOKEN = os.environ.get("SYNC_UPLOAD_TOKEN", "").strip()
STORAGE_DIR = Path(os.environ.get("SYNC_STORAGE_DIR", "/data/uploads"))
MAX_UPLOAD_MB = int(os.environ.get("SYNC_MAX_UPLOAD_MB", "64"))
MAX_UPLOAD_BYTES = MAX_UPLOAD_MB * 1024 * 1024
INDEX_FILE = STORAGE_DIR.parent / "uploads.csv"


def utc_now_text() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def sanitize_filename(name: str) -> str:
    cleaned = re.sub(r"[^A-Za-z0-9._-]", "_", name or "")
    return cleaned[:120] or f"upload_{datetime.now().strftime('%Y%m%d_%H%M%S')}.bin"


def ensure_storage() -> None:
    STORAGE_DIR.mkdir(parents=True, exist_ok=True)
    INDEX_FILE.parent.mkdir(parents=True, exist_ok=True)
    if not INDEX_FILE.exists():
        with INDEX_FILE.open("w", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            writer.writerow([
                "timestamp_utc",
                "client_ip",
                "device_id",
                "event_id",
                "filename",
                "relative_path",
                "bytes",
                "user_agent",
            ])


def append_index(client_ip: str, device_id: str, event_id: str, filename: str, relative_path: str, size_bytes: int, user_agent: str) -> None:
    with INDEX_FILE.open("a", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow([utc_now_text(), client_ip, device_id, event_id, filename, relative_path, size_bytes, user_agent])


class SyncHandler(BaseHTTPRequestHandler):
    server_version = "WatcherSync/1.0"

    def _write_json(self, status: int, payload: dict) -> None:
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _auth_ok(self) -> bool:
        if not TOKEN:
            return False
        auth = self.headers.get("Authorization", "")
        return auth == f"Bearer {TOKEN}"

    def log_message(self, fmt: str, *args) -> None:
        print(f"[{utc_now_text()}] {self.client_address[0]} {fmt % args}")

    def do_GET(self) -> None:
        if self.path.startswith("/healthz"):
            self._write_json(HTTPStatus.OK, {"ok": True, "time": utc_now_text()})
            return
        self._write_json(HTTPStatus.OK, {"service": "watcher-sync", "ok": True})

    def do_POST(self) -> None:
        self._handle_upload()

    def do_PUT(self) -> None:
        self._handle_upload()

    def _handle_upload(self) -> None:
        if not self._auth_ok():
            self._write_json(HTTPStatus.UNAUTHORIZED, {"ok": False, "error": "unauthorized"})
            return

        content_length = self.headers.get("Content-Length")
        if not content_length:
            self._write_json(HTTPStatus.LENGTH_REQUIRED, {"ok": False, "error": "content-length-required"})
            return

        try:
            size = int(content_length)
        except ValueError:
            self._write_json(HTTPStatus.BAD_REQUEST, {"ok": False, "error": "invalid-content-length"})
            return

        if size <= 0 or size > MAX_UPLOAD_BYTES:
            self._write_json(HTTPStatus.REQUEST_ENTITY_TOO_LARGE, {"ok": False, "error": "invalid-size", "max_bytes": MAX_UPLOAD_BYTES})
            return

        parsed = urlparse(self.path)
        query = parse_qs(parsed.query)
        filename = sanitize_filename(query.get("filename", [""])[0] or self.headers.get("X-File-Name", ""))
        device_id = sanitize_filename(self.headers.get("X-Device-Id", "unknown"))
        event_id = sanitize_filename(self.headers.get("X-Event-Id", ""))
        day_dir = STORAGE_DIR / datetime.now().strftime("%Y-%m-%d")
        day_dir.mkdir(parents=True, exist_ok=True)
        final_path = day_dir / filename
        temp_path = final_path.with_suffix(final_path.suffix + ".part")

        remaining = size
        with temp_path.open("wb") as f:
            while remaining > 0:
                chunk = self.rfile.read(min(65536, remaining))
                if not chunk:
                    temp_path.unlink(missing_ok=True)
                    self._write_json(HTTPStatus.BAD_REQUEST, {"ok": False, "error": "unexpected-eof"})
                    return
                f.write(chunk)
                remaining -= len(chunk)

        temp_path.replace(final_path)
        relative_path = str(final_path.relative_to(STORAGE_DIR.parent)).replace("\\", "/")
        append_index(
            client_ip=self.client_address[0],
            device_id=device_id,
            event_id=event_id,
            filename=filename,
            relative_path=relative_path,
            size_bytes=size,
            user_agent=self.headers.get("User-Agent", ""),
        )
        self._write_json(HTTPStatus.OK, {
            "ok": True,
            "bytes": size,
            "filename": filename,
            "path": relative_path,
        })


if __name__ == "__main__":
    ensure_storage()
    if not TOKEN:
        raise SystemExit("SYNC_UPLOAD_TOKEN is required")
    server = ThreadingHTTPServer((HOST, PORT), SyncHandler)
    print(f"watcher-sync listening on {HOST}:{PORT}")
    server.serve_forever()
