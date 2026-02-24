param(
    [string]$RepoRoot = (Get-Location).Path,
    [int]$MinAgeSeconds = 30
)

$ErrorActionPreference = "SilentlyContinue"

$targetNames = @(
    "cl.exe",
    "c1xx.exe",
    "link.exe",
    "mspdbsrv.exe",
    "MSBuild.exe",
    "cmake.exe",
    "ninja.exe",
    "ccache.exe"
)

$normalizedRoot = [System.IO.Path]::GetFullPath($RepoRoot).TrimEnd('\')
$now = Get-Date
$killed = @()

$processes = Get-CimInstance Win32_Process
foreach ($proc in $processes) {
    if ($targetNames -notcontains $proc.Name) {
        continue
    }

    $cmd = [string]$proc.CommandLine
    if ([string]::IsNullOrWhiteSpace($cmd)) {
        continue
    }

    if ($cmd -notlike "*$normalizedRoot*") {
        continue
    }

    $pid = [int]$proc.ProcessId
    if ($pid -eq $PID) {
        continue
    }

    $live = Get-Process -Id $pid
    if (-not $live) {
        continue
    }

    $ageSeconds = ($now - $live.StartTime).TotalSeconds
    if ($ageSeconds -lt $MinAgeSeconds) {
        continue
    }

    Stop-Process -Id $pid -Force
    $killed += [PSCustomObject]@{
        Pid    = $pid
        Name   = $proc.Name
        AgeSec = [Math]::Round($ageSeconds, 1)
    }
}

if ($killed.Count -gt 0) {
    Write-Output ("[Build] Stale process cleanup: killed {0} process(es)." -f $killed.Count)
    $killed | Sort-Object Name, Pid | Format-Table -AutoSize | Out-String | Write-Output
} else {
    Write-Output "[Build] Stale process cleanup: no matching stale processes."
}

exit 0
