$ErrorActionPreference = "Stop"
$VsPath = "C:\Program Files\Microsoft Visual Studio\18\Community"
$VcVarsPath = "$VsPath\VC\Auxiliary\Build\vcvars64.bat"

if (-not (Test-Path $VcVarsPath)) {
    Write-Host "Error: vcvars64.bat not found at $VcVarsPath" -ForegroundColor Red
    exit 1
}

$AnalysisDir = "build_msvc_analysis"
Write-Host "Preparing MSVC Static Analysis in $AnalysisDir..." -ForegroundColor Cyan

$BatchContent = @"
@echo off
call "$VcVarsPath" >nul
if errorlevel 1 exit /b 1

if not exist "$AnalysisDir" mkdir "$AnalysisDir"
cd "$AnalysisDir"

echo [Analysis] Configuring with Ninja and /analyze...
cmake -G "Ninja" -DCMAKE_CXX_COMPILER=cl -DCMAKE_C_COMPILER=cl -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="/analyze /W4 /EHsc /std:c++20" ..
if errorlevel 1 exit /b 1

echo [Analysis] Running Build and Analysis...
cmake --build .
"@

$BatchContent | Out-File -Encoding ASCII "run_analysis_impl.bat"

try {
    cmd /c "run_analysis_impl.bat"
} finally {
    if (Test-Path "run_analysis_impl.bat") { Remove-Item "run_analysis_impl.bat" }
}
