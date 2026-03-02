$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$pidPath = Join-Path (Join-Path $repoRoot "logs") "clangd_graph_rag_mcp.pid"

if (-not (Test-Path $pidPath)) {
    Write-Host "[MCP] No PID file found. Nothing to stop."
    exit 0
}

$procId = Get-Content $pidPath -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $procId) {
    Remove-Item $pidPath -Force -ErrorAction SilentlyContinue
    Write-Host "[MCP] PID file empty. Cleaned up."
    exit 0
}

$proc = Get-Process -Id $procId -ErrorAction SilentlyContinue
if ($proc) {
    Stop-Process -Id $procId -Force
    Write-Host "[MCP] Stopped process PID=$procId"
} else {
    Write-Host "[MCP] Process PID=$procId already exited."
}

Remove-Item $pidPath -Force -ErrorAction SilentlyContinue

# Clean up orphaned MCP Python/Conda processes that may survive parent PowerShell exit.
$orphans = Get-CimInstance Win32_Process |
    Where-Object {
        ($_.CommandLine -like "*graph_mcp_server.py*") -or
        ($_.CommandLine -like "*scripts\\_RunClangdGraphRag.ps1*")
    }

foreach ($p in $orphans) {
    try {
        Stop-Process -Id $p.ProcessId -Force -ErrorAction Stop
        Write-Host "[MCP] Killed orphan PID=$($p.ProcessId)"
    } catch {
    }
}
