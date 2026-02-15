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
REM   analyze     - Enable MSVC Static Analysis (/analyze)
REM   asan        - Enable Address Sanitizer (ASan)
REM   perf        - Run performance tests after build
REM   check       - Run JSON validation and static analysis only
REM   includes    - Build with /showIncludes to analyze dependencies
REM   ninja       - Use Ninja generator
REM   j=N         - Set parallel jobs (default: 16)
REM
REM Examples:
REM   build.bat                    - Default RelWithDebInfo build
REM   build.bat asan               - Build with ASan enabled
REM   build.bat includes > inc.log - Analyze header dependencies
REM
REM Default generator priority:
REM   1) Visual Studio 2022 (v17)
REM   2) Visual Studio 2026 (v18)
REM   3) Ninja
REM ============================================================================

setlocal enabledelayedexpansion
cd /d "%~dp0"

set "BUILD_DIR=build"
set "BUILD_TYPE=RelWithDebInfo"
set "BUILD_TESTS=ON"
set "RUN_TESTS=ON"
set "RUN_PERF=OFF"
set "ENABLE_LTO=OFF"
set "ENABLE_ANALYZE=OFF"
set "ENABLE_ASAN=OFF"
set "SHOW_INCLUDES=OFF"
set "ONLY_CHECK=OFF"
set "GENERATOR_NAME="
set "PARALLEL_JOBS=16"
set "NEED_CONFIG=0"
set "VS_INSTALL_DIR="
set "VS_SELECTED_GENERATOR="

REM ============================================================================
REM Visual Studio Environment Setup
REM ============================================================================
set "VSWHERE_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE_PATH%" set "VSWHERE_PATH=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"

set "VS_DEV_CMD_ACTIVE=0"
where cl.exe >nul 2>nul
if %errorlevel%==0 (
    where rc.exe >nul 2>nul
    if !errorlevel! equ 0 set "VS_DEV_CMD_ACTIVE=1"
)

if "!VS_DEV_CMD_ACTIVE!"=="0" (
    echo [Build] Searching for Visual Studio installation...

    REM Prefer VS 2022 (v17)
    for %%E in (Community Professional Enterprise BuildTools) do (
        if not defined VS_INSTALL_DIR if exist "%ProgramFiles%\Microsoft Visual Studio\2022\%%E\VC\Auxiliary\Build\vcvars64.bat" (
            set "VS_INSTALL_DIR=%ProgramFiles%\Microsoft Visual Studio\2022\%%E"
            set "VS_SELECTED_GENERATOR=Visual Studio 17 2022"
        )
    )

    REM Then VS 2026 (v18)
    if not defined VS_INSTALL_DIR (
        for %%E in (Community Professional Enterprise BuildTools) do (
            if not defined VS_INSTALL_DIR if exist "%ProgramFiles%\Microsoft Visual Studio\18\%%E\VC\Auxiliary\Build\vcvars64.bat" (
                set "VS_INSTALL_DIR=%ProgramFiles%\Microsoft Visual Studio\18\%%E"
                set "VS_SELECTED_GENERATOR=Visual Studio 18 2026"
            )
        )
    )

    REM Final fallback: whatever vswhere reports
    if not defined VS_INSTALL_DIR if exist "%VSWHERE_PATH%" (
        for /f "usebackq tokens=*" %%i in (`"%VSWHERE_PATH%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
            set "VS_INSTALL_DIR=%%i"
        )
        for /f "usebackq tokens=*" %%i in (`"%VSWHERE_PATH%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion`) do (
            set "VS_INSTALL_VERSION=%%i"
        )
        if defined VS_INSTALL_VERSION (
            for /f "tokens=1 delims=." %%v in ("!VS_INSTALL_VERSION!") do (
                if "%%v"=="17" set "VS_SELECTED_GENERATOR=Visual Studio 17 2022"
                if "%%v"=="18" set "VS_SELECTED_GENERATOR=Visual Studio 18 2026"
            )
        )
    )

    if defined VS_INSTALL_DIR (
        echo [Build] Found Visual Studio at: !VS_INSTALL_DIR!
        if defined VS_SELECTED_GENERATOR echo [Build] Selected CMake generator: !VS_SELECTED_GENERATOR!
        if exist "!VS_INSTALL_DIR!\VC\Auxiliary\Build\vcvars64.bat" (
            echo [Build] Activating VS x64 environment...
            call "!VS_INSTALL_DIR!\VC\Auxiliary\Build\vcvars64.bat" >nul
            if !errorlevel! equ 0 (
                echo [Build] Environment OK.
                set "VS_DEV_CMD_ACTIVE=1"
            ) else (
                echo [Build] Warning: Failed to run vcvars64.bat
            )
        ) else (
            echo [Build] Warning: vcvars64.bat not found.
        )
    ) else (
        echo [Build] Warning: No supported Visual Studio installation found.
    )
) else (
    echo [Build] Visual Studio environment already active.
    if defined VisualStudioVersion (
        for /f "tokens=1 delims=." %%v in ("!VisualStudioVersion!") do (
            if "%%v"=="17" set "VS_SELECTED_GENERATOR=Visual Studio 17 2022"
            if "%%v"=="18" set "VS_SELECTED_GENERATOR=Visual Studio 18 2026"
        )
        if defined VS_SELECTED_GENERATOR echo [Build] Active VS maps to generator: !VS_SELECTED_GENERATOR!
    )
)

:ARGS_LOOP
if "%~1"=="" goto :ARGS_DONE

if /i "%~1"=="clean" (
    echo [Build] Cleaning CMake cache...
    if exist "%BUILD_DIR%\CMakeCache.txt" del /f /q "%BUILD_DIR%\CMakeCache.txt"
    set "NEED_CONFIG=1"
)
if /i "%~1"=="clean-all" (
    echo [Build] Cleaning full build directory...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    set "NEED_CONFIG=1"
)
if /i "%~1"=="notest" (
    set "BUILD_TESTS=OFF"
    set "RUN_TESTS=OFF"
    set "NEED_CONFIG=1"
)
if /i "%~1"=="analyze" (
    set "ENABLE_ANALYZE=ON"
    set "NEED_CONFIG=1"
)
if /i "%~1"=="asan" (
    set "ENABLE_ASAN=ON"
    set "NEED_CONFIG=1"
)
if /i "%~1"=="includes" (
    set "SHOW_INCLUDES=ON"
    set "NEED_CONFIG=1"
)
if /i "%~1"=="check" (
    set "ONLY_CHECK=ON"
)
if /i "%~1"=="perf" (
    set "RUN_PERF=ON"
    set "BUILD_TYPE=Release"
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
    set "GENERATOR_NAME=Ninja"
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

REM ============================================================================
REM 1. Pre-build Validation (JSON)
REM ============================================================================
echo [Build] Generating render ABI includes...
python tools\render_abi\generate_gpu_abi.py
if errorlevel 1 (
    echo [Build] Render ABI generation failed! Aborting.
    exit /b 1
)

echo [Build] Validating assets...
python scripts\validate_json.py
if errorlevel 1 (
    echo [Build] Asset validation failed! Aborting.
    exit /b 1
)

if "!ONLY_CHECK!"=="1" (
    echo [Build] Check mode: Skipping compilation.
    exit /b 0
)

REM Create build directory if needed
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

REM ============================================================================
REM 2. CMake Configuration
REM ============================================================================
if not exist CMakeCache.txt set "NEED_CONFIG=1"

if "!NEED_CONFIG!"=="1" (
    if not defined GENERATOR_NAME (
        if defined VS_SELECTED_GENERATOR (
             set "GENERATOR_NAME=!VS_SELECTED_GENERATOR!"
        ) else (
             echo [Build] Warning: No preferred Visual Studio toolchain found, falling back to Ninja.
             set "GENERATOR_NAME=Ninja"
        )
    )

    echo.
    echo ============================================================
    echo [Build] Configuring project...
    echo   Build Type:    !BUILD_TYPE!
    echo   Tests:         !BUILD_TESTS!
    echo   Analyze:       !ENABLE_ANALYZE!
    echo   ASan:          !ENABLE_ASAN!
    echo   LTO:           !ENABLE_LTO!
    echo   Generator:     !GENERATOR_NAME!
    echo ============================================================
    echo.
    
    set "CMAKE_OPTS="
    if "!ENABLE_ANALYZE!"=="ON" set "CMAKE_OPTS=!CMAKE_OPTS! -DENABLE_ANALYZE=ON"
    if "!ENABLE_ASAN!"=="ON"    set "CMAKE_OPTS=!CMAKE_OPTS! -DENABLE_ASAN=ON"
    if "!SHOW_INCLUDES!"=="ON"  set "CMAKE_OPTS=!CMAKE_OPTS! -DCMAKE_CXX_FLAGS="/showIncludes""

    if /i "!GENERATOR_NAME!"=="Ninja" (
        cmake -G Ninja !CMAKE_OPTS! ^
            -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ^
            -DCMAKE_BUILD_TYPE=!BUILD_TYPE! ^
            -DBUILD_TESTING=!BUILD_TESTS! ^
            -DENABLE_LTO=!ENABLE_LTO! ^
            ..
    ) else (
        cmake -G "!GENERATOR_NAME!" -A x64 !CMAKE_OPTS! ^
            -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ^
            -DCMAKE_BUILD_TYPE=!BUILD_TYPE! ^
            -DBUILD_TESTING=!BUILD_TESTS! ^
            -DENABLE_LTO=!ENABLE_LTO! ^
            ..
    )
    
    if errorlevel 1 exit /b 1

    REM 3. Symlink compile_commands.json to root for LSP
    if exist compile_commands.json (
        echo [Build] Linking compile_commands.json...
        if exist "..\compile_commands.json" del /f /q "..\compile_commands.json"
        mklink "..\compile_commands.json" "compile_commands.json" >nul
    )
)

REM ============================================================================
REM 4. Build
REM ============================================================================
cmake --build . --config !BUILD_TYPE! -j !PARALLEL_JOBS!
if errorlevel 1 exit /b 1

REM ============================================================================
REM 5. Post-Build Execution (Tests)
REM ============================================================================
set "TEST_EXE=..\bin\NoMoreDayTests.exe"

if "!BUILD_TESTS!"=="ON" if "!RUN_TESTS!"=="ON" (
    if exist "!TEST_EXE!" (
        echo.
        echo ============================================================
        echo [Test] Running Unit Tests...
        echo ============================================================
        "!TEST_EXE!" --test-case-exclude=*performance*
        if errorlevel 1 (
            echo [Test] Unit Tests FAILED!
            exit /b 1
        )
    ) else (
        echo [Test] Warning: Test executable not found at !TEST_EXE!
    )
)

if "!RUN_PERF!"=="ON" (
    if exist "!TEST_EXE!" (
        echo.
        echo ============================================================
        echo [Test] Running Performance Benchmarks...
        echo ============================================================
        "!TEST_EXE!" --test-case=*performance*
        if errorlevel 1 (
            echo [Test] Performance Tests FAILED!
            exit /b 1
        )
    )
)

echo.
echo [Build] All steps completed successfully!
cd ..
