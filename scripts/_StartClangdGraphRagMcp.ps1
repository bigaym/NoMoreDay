param(
    [switch]$RebuildGraph,
    [switch]$Foreground
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$runScript = Join-Path $PSScriptRoot "_RunClangdGraphRag.ps1"
$logDir = Join-Path $repoRoot "logs"
$logPath = Join-Path $logDir "clangd_graph_rag_mcp.log"
$errLogPath = Join-Path $logDir "clangd_graph_rag_mcp.err.log"
$pidPath = Join-Path $logDir "clangd_graph_rag_mcp.pid"

if (-not (Test-Path $runScript)) {
    throw "Missing run script: $runScript"
}

New-Item -ItemType Directory -Force -Path $logDir | Out-Null

function Test-DockerReady {
    try {
        cmd /c "docker info >nul 2>nul"
        return ($LASTEXITCODE -eq 0)
    } catch {
        return $false
    }
}

if (-not (Test-DockerReady)) {
    $dockerDesktopCandidates = @(
        "D:\Program Files\Docker\Docker\Docker Desktop.exe",
        "C:\Program Files\Docker\Docker\Docker Desktop.exe"
    )

    $dockerDesktop = $dockerDesktopCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $dockerDesktop) {
        throw "Docker daemon is not ready and Docker Desktop executable was not found."
    }

    Write-Host "[MCP] Docker not ready. Launching Docker Desktop..."
    Start-Process -FilePath $dockerDesktop | Out-Null

    $ready = $false
    for ($i = 0; $i -lt 120; $i++) {
        Start-Sleep -Seconds 1
        if (Test-DockerReady) {
            $ready = $true
            break
        }
    }

    if (-not $ready) {
        throw "Docker daemon did not become ready in time."
    }
}

if (Test-Path $pidPath) {
    $oldPid = Get-Content $pidPath -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($oldPid -and (Get-Process -Id $oldPid -ErrorAction SilentlyContinue)) {
        Write-Host "[MCP] Already running. PID=$oldPid"
        Write-Host "[MCP] Log: $logPath"
        exit 0
    }
    Remove-Item $pidPath -Force -ErrorAction SilentlyContinue
}

$scriptArgs = @()
if (-not $RebuildGraph) {
    $scriptArgs += "-NoBuilder"
}

if ($Foreground) {
    Write-Host "[MCP] Starting in foreground..."
    & powershell -ExecutionPolicy Bypass -File $runScript @scriptArgs
    exit $LASTEXITCODE
}

$argLine = @("-ExecutionPolicy", "Bypass", "-File", ('"' + $runScript + '"')) + $scriptArgs

$proc = Start-Process -FilePath "powershell" `
    -ArgumentList $argLine `
    -WorkingDirectory $repoRoot `
    -WindowStyle Hidden `
    -RedirectStandardOutput $logPath `
    -RedirectStandardError $errLogPath `
    -PassThru

Set-Content -Path $pidPath -Value $proc.Id -Encoding ascii

Write-Host "[MCP] Started. PID=$($proc.Id)"
Write-Host "[MCP] Waiting for health endpoint..."

$ok = $false
for ($i = 0; $i -lt 360; $i++) {
    Start-Sleep -Milliseconds 500

    if (-not (Get-Process -Id $proc.Id -ErrorAction SilentlyContinue)) {
        Write-Host "[MCP] ERROR: MCP process exited early."
        Write-Host "[MCP] Check logs: $logPath and $errLogPath"
        exit 1
    }

    try {
        $resp = Invoke-WebRequest -UseBasicParsing "http://127.0.0.1:8800/health" -TimeoutSec 2
        if ($resp.StatusCode -eq 200) {
            $ok = $true
            break
        }
    } catch {
    }
}

if (-not $ok) {
    Write-Host "[MCP] WARNING: Health check did not pass in time."
    Write-Host "[MCP] Check log: $logPath"
    exit 1
}

Write-Host "[MCP] Ready at http://127.0.0.1:8800/sse"
Write-Host "[MCP] Log: $logPath"
