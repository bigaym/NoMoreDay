@echo off
setlocal EnableExtensions

set "PROJECT_ROOT=%~dp0.."
set "ICON_DIR=%PROJECT_ROOT%\assets\textures\skill_nodes"

if not exist "%ICON_DIR%" (
  echo [Error] Icon directory not found: %ICON_DIR%
  exit /b 1
)

echo [Info] Renaming skill node icons in:
echo        %ICON_DIR%
echo [Info] Rule: skill_nodes_^^<id^^>_*.png ^> skill_nodes_^^<id^^>.png

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference='Stop';" ^
  "$dir=[System.IO.Path]::GetFullPath('%ICON_DIR%');" ^
  "$pattern='^skill_nodes_(\d+)(?:_.*)?\.png$';" ^
  "$renamed=0; $skipped=0;" ^
  "Get-ChildItem -LiteralPath $dir -File -Filter 'skill_nodes_*.png' | ForEach-Object {" ^
  "  if (-not ($_.Name -match $pattern)) { Write-Host ('[Skip] Unmatched: ' + $_.Name); $skipped++; return }" ^
  "  $id=$matches[1]; $newName=('skill_nodes_' + $id + '.png');" ^
  "  if ($_.Name -ieq $newName) { $skipped++; return }" ^
  "  $target=Join-Path $dir $newName;" ^
  "  if (Test-Path -LiteralPath $target) { Write-Host ('[Skip] Target exists: ' + $newName + ' (from ' + $_.Name + ')'); $skipped++; return }" ^
  "  Rename-Item -LiteralPath $_.FullName -NewName $newName;" ^
  "  Write-Host ('[Renamed] ' + $_.Name + ' -> ' + $newName);" ^
  "  $renamed++" ^
  "};" ^
  "Write-Host ('[Done] renamed=' + $renamed + ' skipped=' + $skipped)"

if errorlevel 1 (
  echo [Error] Rename operation failed.
  exit /b 1
)

echo [Next] Run:
echo        python scripts\gen_asset_registries.py
echo        python scripts\sync_skill_node_icon_ids.py
exit /b 0
