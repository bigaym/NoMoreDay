cmake_minimum_required(VERSION 3.20)
project(NoMoreDay VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- External Dependencies ---

# 1. Spdlog (FetchContent - Shared Library)
# Switched from local header-only to FetchContent to enable DLL build
include(FetchContent)
set(SPDLOG_BUILD_SHARED ON CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    spdlog
    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.12.0.tar.gz
)
FetchContent_MakeAvailable(spdlog)

# 2. Raylib (FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE) # Build as Dynamic Library (DLL)

FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.0.tar.gz
)
FetchContent_MakeAvailable(raylib)

# 3. EnTT (FetchContent)
# Header-only, no dynamic lib needed.
FetchContent_Declare(
    entt
    URL https://github.com/skypjack/entt/archive/refs/tags/v3.13.0.tar.gz
)
FetchContent_MakeAvailable(entt)

# 4. Taskflow (FetchContent)
# Header-only, no dynamic lib needed.
set(TF_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(TF_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    taskflow
    URL https://github.com/taskflow/taskflow/archive/refs/tags/v3.6.0.tar.gz
)
FetchContent_MakeAvailable(taskflow)

# --- Main Executable ---
add_executable(NoMoreDay 
    src/main.cpp
    src/core/Game.cpp
    src/core/ResourceManager.cpp
    src/systems/PhysicsSystem.cpp
    src/systems/RenderSystem.cpp
    src/systems/InputSystem.cpp
    src/systems/CombatSystem.cpp
    src/tools/Logger.cpp
)

# Enable Precompiled Headers
target_precompile_headers(NoMoreDay PRIVATE src/pch.hpp)

# Link dependencies
target_link_libraries(NoMoreDay 
    PRIVATE 
    raylib 
    spdlog::spdlog 
    EnTT::EnTT 
    Taskflow
)

if(WIN32)
    # Add windows specific libs
    target_link_libraries(NoMoreDay PRIVATE winmm kernel32 opengl32 gdi32 user32 shell32)
    
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        target_compile_options(NoMoreDay PRIVATE "-Wa,-mbig-obj")
        # STRIP symbols in Release mode to drastically reduce size
        target_link_options(NoMoreDay PRIVATE $<$<CONFIG:Release>:-s>)
    endif()
    
    target_compile_definitions(NoMoreDay PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER)
endif()

# --- Output & Assets ---

# 1. Set Output Directory to build/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(NoMoreDay PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# 2. Copy Assets Folder (Robust Cross-Platform)
# Symlinks on Windows via CMake can be flaky due to permissions or shell differences.
# copy_directory is safe and effective for this scale.
add_custom_command(TARGET NoMoreDay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/assets"
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets"
    COMMENT "Copying assets to output directory..."
)

# 3. Copy Dependency DLLs to Output Directory
# Use generator expressions to locate the DLL files automatically.
add_custom_command(TARGET NoMoreDay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:raylib>
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:spdlog>
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/"
    COMMENT "Copying dependency DLLs..."
)
