# AGENTS.md

This file provides guidance to agents when working with code in this repository.

## Build System
- Uses CMake with FetchContent for dependencies (raylib, entt, taskflow, spdlog)
- Windows builds require WIN32_LEAN_AND_MEAN and NOMINMAX to avoid Windows API conflicts
- Executable output goes to build/bin directory
- DLLs are automatically copied to output directory

## Project Architecture
- ECS (Entity-Component-System) architecture using EnTT library
- SpatialHashGrid for efficient collision detection and neighbor queries
- Parallel processing with Taskflow for physics updates
- Components are in src/components/, systems in src/systems/, core logic in src/core/

## Key Non-Obvious Patterns
- Game loop order: Input → Player Movement → AI → Combat → Spatial Grid Rebuild → Physics (critical for frame consistency)
- Combat system uses spatial grid from previous frame (1-frame latency acceptable)
- EntityUpdateTask functor used to avoid MinGW template errors
- Position and Velocity components are required for physics processing
- Spatial grid cell size should be ~3x largest entity diameter for optimal performance

## Platform-Specific Considerations
- Windows API DrawText conflicts with raylib DrawText - resolved with NOMINMAX definition
- Windows builds link additional libraries: winmm, kernel32, opengl32, gdi32, user32, shell32
- MinGW requires "-Wa,-mbig-obj" compile option for large objects

## Component Requirements
- Entities need Position + Velocity for physics processing
- Entities need Position + ColorComponent (without SpriteComponent) to render as circles
- Entities need Position + SpriteComponent for texture rendering
- Enemy entities need EnemyTag + AIComponent to be processed by AI system