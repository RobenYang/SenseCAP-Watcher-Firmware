# Watcher Sync Server

Minimal Dockerized upload endpoint for SenseCAP Watcher recording sync.

For Windows hosts where Docker pull is temporarily blocked, use the PowerShell host mode under `infra/sync_server/windows/`.

## Features
- `GET /healthz` for health checks
- `POST /upload` and `PUT /upload` for raw file uploads
- Bearer token authentication
- Caddy reverse proxy for HTTPS on `sync.metasquilla.space`
- Local fallback HTTP listener on port `8080` for LAN testing

## Environment
- `SYNC_UPLOAD_TOKEN`: required bearer token
- `SYNC_STORAGE_DIR`: upload directory inside container, defaults to `/data/uploads`
- `SYNC_MAX_UPLOAD_MB`: max accepted upload size in MB, defaults to `64`

## Local test
```powershell
cd infra/sync_server
docker compose up -d --build
curl http://127.0.0.1:8080/healthz
```

## Windows host mode
```powershell
cd infra/sync_server/windows
powershell -ExecutionPolicy Bypass -File .\start_sync_stack.ps1
```

## Device upload contract
- Method: `PUT` or `POST`
- URL: `/upload?filename=<name>`
- Header: `Authorization: Bearer <token>`
- Body: raw file bytes

Optional headers:
- `X-Device-Id`
- `X-Event-Id`

Response returns a JSON object containing the saved relative path and size.
