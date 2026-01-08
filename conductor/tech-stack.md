# Tech Stack - NoMoreDay

## Core Language & Standards
- **Standard:** C++20
- **Features:** Modules (for build speed/isolation), Concepts (for template safety), Coroutines (for async logic/AI), `constexpr` (for compile-time calculations).

## Frameworks & Libraries
- **ECS (Entity Component System):** **EnTT** - High-performance entity management and data locality.
- **Rendering:** **Raylib** - Lightweight OpenGL-based framework for 2D graphics, windowing, and input.
- **GPU Computing:** **OpenGL 4.3 Compute Shaders** - Utilized for offloading massive-scale calculations including particles, physics, and flow field pathfinding.
- **Task Scheduling:** **Taskflow** - Managing complex task dependencies (DAG) and parallel execution.
- **SIMD:** **xsimd** - Cross-platform SIMD wrappers for optimizing physics and spatial calculations.
- **Memory Allocation:** **mimalloc** - Performance-oriented allocator to reduce fragmentation and improve multi-threaded efficiency.
- **Logging:** **spdlog** - Fast, header-only C++ logging library.
- **Serialization:** **nlohmann/json** - Industry-standard JSON parsing for configurations and save data.

## Tooling & Automation
- **Build System:** CMake (3.20+)
- **Asset Pipeline:** Python 3.10+ scripts for AI-driven image generation (ComfyUI/SDXL), automated C++ header generation for asset registries, and dynamic Tag Registry generation from JSON definitions.
- **Versioning:** Git

## Performance Constraints
- **Target:** 60+ FPS with 10,000 active entities on mid-range hardware.
- **Optimization:** Data-Oriented Design (DOD), minimal runtime allocations, heavy use of SIMD and multi-threading.
