# NoMoreDay Project Context

## Project Overview
**NoMoreDay** is a high-performance, data-oriented Action/RPG game project built with **C++20**. The project focuses on handling massive entity counts (e.g., 10,000+ units) in real-time by leveraging an Entity Component System (ECS) architecture and task-based parallelism.

## Technical Stack & Architecture

### Core Technologies
*   **Language:** C++20 (Modules, Concepts, Coroutines).
*   **Engine Framework:** Custom architecture using **Raylib** for rendering/windowing.
*   **ECS:** **EnTT** (Fastest C++ ECS) for data management and cache locality.
*   **Concurrency:** **Taskflow** (Planned for DAG-based task parallelism, currently using native threads).
*   **Logging:** **spdlog** (Header-only integration).
*   **Build System:** **CMake** (v3.20+).

### Architectural Patterns
*   **Data-Oriented Design (DOD):** Maximizing CPU cache efficiency.
*   **Hybrid Architecture:** Object-oriented managers (Resources, Application) + Data-oriented core logic (Systems).
*   **Systems:** Pure functions processing `entt::registry` components (e.g., `RenderSystem`, `PhysicsSystem`).

## Directory Structure
*   `src/`: Source code.
    *   `main.cpp`: Current entry point containing the prototype logic (10k particle simulation).
*   `设计文档/`: Comprehensive design documents (Architecture, Combat, Maps, AI).
*   `assets/`: Game assets (currently `textures`).
*   `scripts/`: Python scripts for asset generation (e.g., `asset_gen.py`).
*   `build/`: CMake build artifacts.

## Building & Running

**Prerequisites:**
*   C++20 compatible compiler (MSVC, GCC, Clang).
*   CMake 3.20+.
*   Internet connection (for `FetchContent` dependencies: Raylib, EnTT, Taskflow).
*   *Note:* `spdlog` is currently configured to look for a local path in `F:/C++/third_party`.

**Build Commands:**
```powershell
# Configure
cmake -B build -S .

# Build
cmake --build build --config Release

# Run
./build/Release/NoMoreDay.exe
```

## Current Status
*   **Prototype Phase:** The `main.cpp` currently implements a stress test spawning 10,000 colored entities with velocity and boundary collision.
*   **Implementation Note:** The current code uses `std::thread` for parallel updates, but the architectural roadmap (`技术架构与实现路线.md`) specifies migrating to **Taskflow** for better task scheduling.

## Key Development Guidelines
1.  **Performance First:** Avoid memory allocations in the main loop. Use memory pools or pre-allocated vectors.
2.  **Component Design:** Components should be POD (Plain Old Data) structs.
3.  **System Design:** Systems should be stateless logic processors.
