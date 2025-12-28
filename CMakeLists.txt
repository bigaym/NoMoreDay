cmake_minimum_required(VERSION 3.20)
project(NoMoreDay VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- External Dependencies ---

# 1. Spdlog
set(SPDLOG_BUILD_SHARED ON CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/spdlog)

# 2. Raylib
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE) # Build as Dynamic Library (DLL)
add_subdirectory(third_party/raylib)

# 3. EnTT
add_subdirectory(third_party/entt)

# 4. Taskflow
set(TF_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(TF_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/taskflow)

# 5. nlohmann/json

# --- Main Executable ---
add_executable(NoMoreDay
    src/main.cpp
    src/core/Game.cpp
    src/core/ResourceManager.cpp
    src/core/LevelManager.cpp
    src/core/ItemFactory.cpp
    src/systems/PhysicsSystem.cpp
    src/systems/RenderSystem.cpp
    src/systems/InputSystem.cpp
    src/systems/CombatSystem.cpp
    src/systems/StatsSystem.cpp
    src/systems/CraftingSystem.cpp
    src/systems/AISystem.cpp
    src/systems/MapSystem.cpp
    src/systems/EnemySpawnSystem.cpp
    src/systems/FogOfWarSystem.cpp
    src/systems/EffectSystem.cpp
    src/systems/EnemyBehavior.cpp
    src/systems/UISystem.cpp
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

# Header-only libraries include paths
target_include_directories(NoMoreDay PRIVATE third_party/json/include)

if(WIN32)
    # Add windows specific libs
    target_link_libraries(NoMoreDay PRIVATE winmm kernel32 opengl32 gdi32 user32 shell32)
    
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        # -Wno-stringop-overflow: Suppress false positives from spdlog/fmt in GCC 13/14
        target_compile_options(NoMoreDay PRIVATE "-Wa,-mbig-obj" "-Wno-stringop-overflow")
        # STRIP symbols in Release mode to drastically reduce size
        target_link_options(NoMoreDay PRIVATE $<$<CONFIG:Release>:-s>)
    endif()
    
    target_compile_definitions(NoMoreDay PRIVATE WIN32_LEAN_AND_MEAN NOGDI NOUSER NOMINMAX)
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
