@echo off
chcp 65001 >nul
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
set "BUILD_TYPE=RelWithDebInfo"
set "BUILD_TESTS=ON"
set "ENABLE_LTO=OFF"
set "GENERATOR="
set "PARALLEL_JOBS=16"
set "NEED_CONFIG=0"

REM Auto-detect Ninja (Disabled in favor of MSVC default, but kept for reference)
REM where ninja >nul 2>nul
REM if %errorlevel%==0 (
REM     set "GENERATOR=-G Ninja"
REM     echo [Build] Auto-detected Ninja generator.
REM )

REM ============================================================================
REM Visual Studio Environment Setup
REM ============================================================================
set "VSWHERE_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE_PATH%" set "VSWHERE_PATH=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"

set "VS_DEV_CMD_ACTIVE=0"
where cl.exe >nul 2>nul
if %errorlevel%==0 set "VS_DEV_CMD_ACTIVE=1"

if "!VS_DEV_CMD_ACTIVE!"=="0" (
    if exist "%VSWHERE_PATH%" (
        echo [Build] Searching for Visual Studio installation...
        for /f "usebackq tokens=*" %%i in (`"%VSWHERE_PATH%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
            set "VS_INSTALL_DIR=%%i"
        )

        if defined VS_INSTALL_DIR (
            echo [Build] Found Visual Studio at: !VS_INSTALL_DIR!
            if exist "!VS_INSTALL_DIR!\VC\Auxiliary\Build\vcvars64.bat" (
                echo [Build] Activating VS x64 environment...
                call "!VS_INSTALL_DIR!\VC\Auxiliary\Build\vcvars64.bat" >nul
                if !errorlevel! equ 0 (
                    echo [Build] Environment OK.
                ) else (
                    echo [Build] Warning: Failed to run vcvars64.bat
                )
            ) else (
                echo [Build] Warning: vcvars64.bat not found.
            )
        ) else (
            echo [Build] Warning: No Visual Studio installation found via vswhere.
        )
    ) else (
        echo [Build] Warning: vswhere.exe not found.
    )
) else (
    echo [Build] Visual Studio environment already active.
)

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
    REM Default to Ninja if available, otherwise NMake Makefiles (for MSVC)
    if not defined GENERATOR (
        where ninja >nul 2>nul
        if !errorlevel! equ 0 (
            set "GENERATOR=-G Ninja"
            echo [Build] Using Ninja generator - detected
        ) else (
            set "GENERATOR=-G "NMake Makefiles""
            echo [Build] Using NMake Makefiles generator - fallback
        )
    )

    echo.
    echo ============================================================
    echo [Build] Configuring project...
    echo   Build Type:    !BUILD_TYPE!
    echo   Tests:         !BUILD_TESTS!
    echo   LTO:           !ENABLE_LTO!
    echo   Parallel Jobs: !PARALLEL_JOBS!
    if defined GENERATOR echo   Generator:     !GENERATOR!
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
