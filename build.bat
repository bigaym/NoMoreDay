@echo off
REM ============================================================================
REM NoMoreDay Build Script
REM ============================================================================
REM Usage: build.bat [options]
REM
REM Options:
REM   clean       - Clean CMake cache (preserves object files)
REM   clean-all   - Clean entire build directory
REM   notest      - Skip building tests
REM   release     - Build in Release mode (with LTO)
REM   debug       - Build in Debug mode
REM   ninja       - Use Ninja generator instead of MinGW Makefiles
REM   j=N         - Set parallel jobs (default: 16)
REM
REM Examples:
REM   build.bat                    - Default RelWithDebInfo build
REM   build.bat release            - Optimized Release build with LTO
REM   build.bat ninja notest       - Fast build with Ninja, no tests
REM   build.bat clean release j=8  - Clean and rebuild Release with 8 jobs
REM ============================================================================

setlocal enabledelayedexpansion
cd /d "%~dp0"

set "BUILD_DIR=build"
set "BUILD_TYPE=release"
set "BUILD_TESTS=ON"
set "ENABLE_LTO=OFF"
set "GENERATOR="
set "PARALLEL_JOBS=16"
set "NEED_CONFIG=0"

:ARGS_LOOP
if "%~1"=="" goto :ARGS_DONE

if /i "%~1"=="clean" (
    echo [Build] Cleaning CMake cache ^(preserving objects^)...
    if exist "%BUILD_DIR%\CMakeCache.txt" del /f /q "%BUILD_DIR%\CMakeCache.txt"
    set "NEED_CONFIG=1"
)
if /i "%~1"=="clean-all" (
    echo [Build] Cleaning full build environment...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    set "NEED_CONFIG=1"
)
if /i "%~1"=="notest" (
    set "BUILD_TESTS=OFF"
    set "NEED_CONFIG=1"
)
if /i "%~1"=="rwb" (
    set "BUILD_TYPE=RelWithDebInfo"
    set "ENABLE_LTO=OFF"
    set "NEED_CONFIG=1"
)
if /i "%~1"=="release" (
    set "BUILD_TYPE=Release"
    set "ENABLE_LTO=ON"
    set "NEED_CONFIG=1"
)
if /i "%~1"=="debug" (
    set "BUILD_TYPE=Debug"
    set "NEED_CONFIG=1"
)
if /i "%~1"=="ninja" (
    set "GENERATOR=-G Ninja"
    set "NEED_CONFIG=1"
)

REM Parse j=N parameter for parallel jobs
echo %~1 | findstr /i /r "^j=[0-9]*$" >nul
if not errorlevel 1 (
    for /f "tokens=2 delims==" %%a in ("%~1") do set "PARALLEL_JOBS=%%a"
)

shift
goto :ARGS_LOOP

:ARGS_DONE

REM Create build directory if needed
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

REM Check if configuration is needed
if not exist CMakeCache.txt set "NEED_CONFIG=1"

REM Configure if needed
if "!NEED_CONFIG!"=="1" (
    echo.
    echo ============================================================
    echo [Build] Configuring project...
    echo   Build Type:    !BUILD_TYPE!
    echo   Tests:         !BUILD_TESTS!
    echo   LTO:           !ENABLE_LTO!
    echo   Parallel Jobs: !PARALLEL_JOBS!
    if defined GENERATOR echo   Generator:     Ninja
    echo ============================================================
    echo.
    
    cmake !GENERATOR! ^
        -DCMAKE_BUILD_TYPE=!BUILD_TYPE! ^
        -DCMAKE_UNITY_BUILD=OFF ^
        -DBUILD_TESTING=!BUILD_TESTS! ^
        -DENABLE_LTO=!ENABLE_LTO! ^
        ..
    
    if errorlevel 1 (
        echo [Build] Configuration failed!
        exit /b 1
    )
)

REM Build
echo.
echo [Build] Building with !PARALLEL_JOBS! parallel jobs...
cmake --build . --config !BUILD_TYPE! -j !PARALLEL_JOBS!

if errorlevel 1 (
    echo [Build] Build failed!
    exit /b 1
)

echo.
echo [Build] Build completed successfully!
echo   Output: %CD%\bin\

cd ..
