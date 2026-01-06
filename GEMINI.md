# NoMoreDay - Project Context

**NoMoreDay** is a high-performance **2D Diablo-like Roguelite ARPG** built with **C++20** and an **ECS (Entity Component System)** architecture. It aims to support **10,000+ simultaneous entities** on screen, featuring deep itemization, skill trees, and procedural generation.

## 🛠 Tech Stack

*   **Language:** C++20 (Modules, Concepts, Coroutines)
*   **Architecture:** Data-Oriented Design (DOD), ECS
*   **Core Libraries:**
    *   **EnTT:** High-performance ECS framework.
    *   **Raylib:** Lightweight 2D rendering and windowing.
    *   **Taskflow:** DAG-based task parallelism.
    *   **xsimd:** SIMD acceleration for physics/math.
    *   **mimalloc:** High-performance memory allocator.
    *   **nlohmann/json:** JSON parsing.
    *   **spdlog:** Fast logging.

## 🚀 Build & Run

### Prerequisites
*   Windows (Primary dev environment), Linux, or macOS.
*   C++20 compliant compiler.
*   CMake 3.10+.
*   Python 3.10+ (for asset/script pipelines).

### Build Commands (Windows PowerShell)

**One-step Build (Recommended):**
```powershell
.\build.bat
```
*   Builds in `RelWithDebInfo` configuration.
*   Output binaries are located in `build/bin/`.

**Manual CMake Build:**
```powershell
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
cmake --build . --config RelWithDebInfo -j 16
```

### Running the Game
Executable is typically located at:
`build/bin/NoMoreDay.exe` (Verify exact name in `build/bin/`)

### Running Tests
Test executables are in `build/bin/tests/` (e.g., `FinalIntegrationTest.exe`, `CombatSystemTest.exe`).

## 📂 Project Structure

*   **`src/`**: C++ Source Code.
    *   `components/`: ECS Components (POD structs).
    *   `systems/`: ECS Systems (Logic).
    *   `core/`: Engine infrastructure.
    *   `utils/`: Helper functions.
*   **`assets/`**: Game assets (textures, data JSONs).
*   **`scripts/`**: Python scripts for asset generation and tools.
    *   **Env:** Use `conda activate ai` (or equivalent) for scripts requiring ML libraries.
*   **`conductor/`**: Project management, tracks, and plans.
    *   **Important:** Check `conductor/tracks/` for active development plans.
*   **`设计文档/`**: Design documents (Game mechanics, Architecture).
*   **`tests/`**: Unit and Integration tests.

## 📝 Development Conventions

*   **C++ Style:**
    *   Prioritize `constexpr` and compile-time evaluation.
    *   Avoid undefined behavior and dangling pointers.
    *   Follow `conductor/code_styleguides/general.md`.
*   **Python Style:**
    *   Follow Google Python Style Guide (`conductor/code_styleguides/python.md`).
    *   Type annotations encouraged.
*   **Assets:**
    *   Managed via Python scripts in `scripts/`.
    *   Skill icons and other data-driven assets use JSON registries.

## 🧠 Memory & Context

*   **Memory Bank:** `.gemini/GEMINI.md` (Check for saved facts).
*   **Conductor:** Use the `conductor` directory to understand the current feature roadmap and implementation details.
