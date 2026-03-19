$ErrorActionPreference = 'Stop'

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$ServerScript = Join-Path $Root 'watcher_sync_server.ps1'
$Caddyfile = Join-Path $Root 'Caddyfile'

Get-CimInstance Win32_Process |
    Where-Object { $_.CommandLine -like "*watcher_sync_server.ps1*" } |
    ForEach-Object { Stop-Process -Id $_.ProcessId -Force }

try { caddy stop | Out-Null } catch { }

Start-Process powershell.exe -WindowStyle Hidden -ArgumentList @(
    '-NoProfile',
    '-ExecutionPolicy', 'Bypass',
    '-File', $ServerScript
)

Start-Sleep -Seconds 2

Start-Process caddy.exe -WindowStyle Hidden -ArgumentList @(
    'start',
    '--config', $Caddyfile,
    '--adapter', 'caddyfile'
)

Write-Host 'Watcher sync stack started.'
Write-Host 'Local health: http://127.0.0.1:8080/healthz'
Write-Host 'Public health: https://sync.metasquilla.space/healthz'

