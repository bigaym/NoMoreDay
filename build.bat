
@REM # 基本用法
@REM .\build.bat           # 默认 RelWithDebInfo，自动检测 Ninja

@REM # 新增选项
@REM .\build.bat release   # Release 构建 (最大性能)
@REM .\build.bat debug     # Debug 构建
@REM .\build.bat noavx     # 禁用 AVX2 (兼容老 CPU)
@REM .\build.bat msbuild   # 强制使用 MSBuild
@REM .\build.bat clean     # 清理 CMake 缓存
@REM .\build.bat clean-all # 完全清理
@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

set "BUILD_DIR=build"
set "BUILD_TYPE=RelWithDebInfo"
set "BUILD_TESTS=ON"
set "NEED_CONFIG=0"
set "USE_NINJA=0"
set "PARALLEL_JOBS=16"

REM Detect Ninja
where ninja >nul 2>&1
if %errorlevel%==0 (
    set "USE_NINJA=1"
    echo [Build] Ninja detected, using Ninja generator for faster builds.
) else (
    echo [Build] Ninja not found, using default generator. Install with: pip install ninja
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
    if exist "%BUILD_DIR%\CMakeCache.txt" del /f /q "%BUILD_DIR%\CMakeCache.txt"
    if exist "%BUILD_DIR%\CMakeFiles" rmdir /s /q "%BUILD_DIR%\CMakeFiles"
    if exist "%BUILD_DIR%\.ninja_deps" del /f /q "%BUILD_DIR%\.ninja_deps"
    if exist "%BUILD_DIR%\.ninja_log" del /f /q "%BUILD_DIR%\.ninja_log"
    set "NEED_CONFIG=1"
)
if /i "%~1"=="notest" (
    set "BUILD_TESTS=OFF"
    set "NEED_CONFIG=1"
)
if /i "%~1"=="release" (
    set "BUILD_TYPE=Release"
    set "NEED_CONFIG=1"
)
if /i "%~1"=="debug" (
    set "BUILD_TYPE=Debug"
    set "NEED_CONFIG=1"
)
if /i "%~1"=="msbuild" (
    set "USE_NINJA=0"
    echo [Build] Forcing MSBuild generator.
)
if /i "%~1"=="noavx" (
    set "CMAKE_OPTS=!CMAKE_OPTS! -DENABLE_AVX2=OFF"
    set "NEED_CONFIG=1"
)
shift
goto :ARGS_LOOP

:ARGS_DONE
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

if not exist CMakeCache.txt set "NEED_CONFIG=1"

REM Build CMAKE_OPTS
set "CMAKE_OPTS=-DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_UNITY_BUILD=OFF -DBUILD_TESTING=!BUILD_TESTS! !CMAKE_OPTS!"

if "!NEED_CONFIG!"=="1" (
    echo [Build] Configuring project ^(%BUILD_TYPE%^)...
    if "!USE_NINJA!"=="1" (
        cmake -G Ninja !CMAKE_OPTS! ..
    ) else (
        cmake !CMAKE_OPTS! ..
    )
    if errorlevel 1 (
        echo [Build] Configuration failed!
        exit /b 1
    )
)

echo [Build] Building with %PARALLEL_JOBS% parallel jobs...
if "!USE_NINJA!"=="1" (
    ninja -j%PARALLEL_JOBS%
) else (
    cmake --build . --config %BUILD_TYPE% -j %PARALLEL_JOBS%
)

if errorlevel 1 (
    echo [Build] Build failed!
    exit /b 1
)

echo [Build] Build completed successfully!
echo [Build] Output: %cd%\bin\NoMoreDay.exe
