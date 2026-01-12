@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

set "BUILD_DIR=build"
set "CMAKE_OPTS=-DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_UNITY_BUILD=OFF"
set "BUILD_TESTS=ON"
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
    if exist "%BUILD_DIR%\CMakeCache.txt" del /f /q "%BUILD_DIR%\CMakeCache.txt"
    if exist "%BUILD_DIR%\CMakeFiles" rmdir /s /q "%BUILD_DIR%\CMakeFiles"
    set "NEED_CONFIG=1"
)
if /i "%~1"=="notest" (
    set "BUILD_TESTS=OFF"
    set "NEED_CONFIG=1"
)
shift
goto :ARGS_LOOP

:ARGS_DONE
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

if not exist CMakeCache.txt set "NEED_CONFIG=1"

if "!NEED_CONFIG!"=="1" (
    echo [Build] Configuring project...
    cmake !CMAKE_OPTS! -DBUILD_TESTING=!BUILD_TESTS! ..
)

cmake --build . --config RelWithDebInfo -j 16

@REM echo [Build] Generating PDBs...
@REM cd bin
@REM call ..\..\scripts\gen_pdb.bat
@REM cd ..

@REM cd ..
