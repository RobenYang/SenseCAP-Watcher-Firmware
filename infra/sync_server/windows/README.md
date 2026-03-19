# Windows Host Mode

Fallback deployment for a Windows laptop when Docker image pulls are blocked.

## Files
- `watcher_sync_server.ps1`: local upload API on `127.0.0.1:9000`
- `Caddyfile`: reverse proxy for `sync.metasquilla.space` and local `:8080`
- `start_sync_stack.ps1`: start or restart both services

## Required `.env`
Create `.env` in this directory with:

```text
SYNC_UPLOAD_TOKEN=replace-with-long-random-token
SYNC_MAX_UPLOAD_MB=64
```

## Start
```powershell
powershell -ExecutionPolicy Bypass -File .\start_sync_stack.ps1
```

## Health checks
- Local LAN fallback: `http://<host>:8080/healthz`
- Public domain: `https://sync.metasquilla.space/healthz`

