$ErrorActionPreference = 'Stop'

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$EnvPath = Join-Path $Root '.env'
$DataRoot = Join-Path $Root 'data'
$UploadRoot = Join-Path $DataRoot 'uploads'
$IndexPath = Join-Path $DataRoot 'uploads.csv'

if (!(Test-Path $EnvPath)) {
    throw '.env is required'
}

$envMap = @{}
Get-Content $EnvPath | ForEach-Object {
    if ($_ -match '^\s*#' -or $_ -match '^\s*$') { return }
    $pair = $_ -split '=', 2
    if ($pair.Length -eq 2) {
        $envMap[$pair[0].Trim()] = $pair[1].Trim()
    }
}

$Token = $envMap['SYNC_UPLOAD_TOKEN']
if ([string]::IsNullOrWhiteSpace($Token)) {
    throw 'SYNC_UPLOAD_TOKEN is required'
}

$MaxUploadMb = 64
if ($envMap.ContainsKey('SYNC_MAX_UPLOAD_MB')) {
    $MaxUploadMb = [int]$envMap['SYNC_MAX_UPLOAD_MB']
}
$MaxUploadBytes = $MaxUploadMb * 1MB

New-Item -ItemType Directory -Force $UploadRoot | Out-Null
if (!(Test-Path $IndexPath)) {
    'timestamp_utc,client_ip,device_id,event_id,filename,relative_path,bytes,user_agent' | Set-Content $IndexPath -Encoding UTF8
}

function Get-UtcText {
    return [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')
}

function Sanitize-Name([string]$Name) {
    if ([string]::IsNullOrWhiteSpace($Name)) {
        return ('upload_{0}.bin' -f (Get-Date -Format 'yyyyMMdd_HHmmss'))
    }
    $clean = [regex]::Replace($Name, '[^A-Za-z0-9._-]', '_')
    if ($clean.Length -gt 120) { $clean = $clean.Substring(0, 120) }
    if ([string]::IsNullOrWhiteSpace($clean)) {
        return ('upload_{0}.bin' -f (Get-Date -Format 'yyyyMMdd_HHmmss'))
    }
    return $clean
}

function Write-JsonResponse($Context, [int]$StatusCode, $Payload) {
    $json = $Payload | ConvertTo-Json -Compress
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($json)
    $Context.Response.StatusCode = $StatusCode
    $Context.Response.ContentType = 'application/json; charset=utf-8'
    $Context.Response.ContentLength64 = $bytes.Length
    $Context.Response.OutputStream.Write($bytes, 0, $bytes.Length)
    $Context.Response.OutputStream.Close()
}

$listener = [System.Net.HttpListener]::new()
$listener.Prefixes.Add('http://127.0.0.1:9000/')
$listener.Start()

Write-Host ('watcher-sync host mode listening on {0}' -f '127.0.0.1:9000')

while ($listener.IsListening) {
    $context = $listener.GetContext()
    try {
        $request = $context.Request
        $method = $request.HttpMethod.ToUpperInvariant()
        $path = $request.Url.AbsolutePath

        if ($method -eq 'GET' -and $path -eq '/healthz') {
            Write-JsonResponse $context 200 @{ ok = $true; time = (Get-UtcText) }
            continue
        }

        if (($method -ne 'PUT' -and $method -ne 'POST') -or $path -ne '/upload') {
            Write-JsonResponse $context 404 @{ ok = $false; error = 'not-found' }
            continue
        }

        $auth = $request.Headers['Authorization']
        if ($auth -ne ('Bearer ' + $Token)) {
            Write-JsonResponse $context 401 @{ ok = $false; error = 'unauthorized' }
            continue
        }

        $contentLength = $request.ContentLength64
        if ($contentLength -le 0) {
            Write-JsonResponse $context 411 @{ ok = $false; error = 'content-length-required' }
            continue
        }

        if ($contentLength -gt $MaxUploadBytes) {
            Write-JsonResponse $context 413 @{ ok = $false; error = 'invalid-size'; max_bytes = $MaxUploadBytes }
            continue
        }

        $query = [System.Web.HttpUtility]::ParseQueryString($request.Url.Query)
        $filename = Sanitize-Name($query['filename'])
        $deviceId = Sanitize-Name($request.Headers['X-Device-Id'])
        $eventId = Sanitize-Name($request.Headers['X-Event-Id'])
        $dateDir = Join-Path $UploadRoot (Get-Date -Format 'yyyy-MM-dd')
        New-Item -ItemType Directory -Force $dateDir | Out-Null
        $finalPath = Join-Path $dateDir $filename
        $tempPath = "$finalPath.part"

        $fileStream = [System.IO.File]::Open($tempPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
        try {
            $request.InputStream.CopyTo($fileStream)
        }
        finally {
            $fileStream.Dispose()
            $request.InputStream.Dispose()
        }

        Move-Item -Force $tempPath $finalPath

        $relativePath = $finalPath.Substring($DataRoot.Length).TrimStart('\\').Replace('\\', '/')
        $indexLine = ('{0},{1},{2},{3},{4},{5},{6},{7}' -f (Get-UtcText), $context.Request.RemoteEndPoint.Address, $deviceId, $eventId, $filename, $relativePath, $contentLength, ($request.UserAgent -replace ',', ';'))
        Add-Content -Path $IndexPath -Value $indexLine -Encoding UTF8

        Write-JsonResponse $context 200 @{ ok = $true; bytes = $contentLength; filename = $filename; path = $relativePath }
    }
    catch {
        try {
            Write-JsonResponse $context 500 @{ ok = $false; error = $_.Exception.Message }
        }
        catch { }
    }
}
