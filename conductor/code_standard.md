# Coding Standards & Best Practices

To ensure the long-term maintainability, performance, and stability of **NoMoreDay**, all contributors (human and AI) must adhere to the following standards.

## 0. Tooling & Environment (New)

### 0.1 MCP & Agentic Workflow
*   **Prioritize Smart Tree**: When exploring file structures, analyzing project state, or performing broad searches, prefer `smart-tree` tools (e.g., `analyze`, `overview`, `find`) over basic `filesystem` tools (`list_dir`).
    *   *Reason*: `smart-tree` provides context-aware, token-optimized summaries that prevent context flooding.
*   **Filesystem Usage**: Use standard `filesystem` tools for precise, single-file operations (read/write/edit) after locating the target with `smart-tree`.

### 0.2 Git Conventions
*   **Conventional Commits**: Use conventional commit messages (`feat:`, `fix:`, `docs:`, `refactor:`, `test:`, `chore:`).
    *   Example: `feat: add Rending Wave behavior`, `fix: resolve UAF in Blade Boomerang`.

## 1. Safety & Robustness (Critical)

We prioritize stability. A crash or undefined behavior (UB) is a critical failure.

### 1.1 Memory Safety
*   **No Raw Pointers for Ownership**: Never use `new` or `delete` explicitly. Use `std::unique_ptr` or `std::shared_ptr`.
*   **EnTT Component Handling**:
    *   **WARNING - POINTER INVALIDATION**: Be extremely careful when holding pointers to components (`auto* pos = registry.try_get<Position>(e)`) while creating new entities or adding components to other entities in the same scope. EnTT component pools may reallocate, leaving your pointers dangling (Use-After-Free).
    *   **Fix**: Copy small POD components (like `Position`, `Velocity`) to local stack variables *before* iterating or creating new entities.
*   **References**: Prefer references `T&` over pointers `T*` when nullptr is not a valid state.

### 1.2 Resource Management (RAII)
*   Acquire resources (file handles, textures, mutexes) in constructors and release them in destructors.
*   Use `std::scoped_lock` for mutexes.

### 1.3 Concurrency
*   **No Static Mutable State**: Avoid function-local `static` variables that are not `const`/`constexpr`, as they are not thread-safe by default.
*   **Taskflow**: Use the Taskflow graph for parallelism. Avoid spawning raw `std::thread` unless necessary (e.g., IO workers).

## 2. Modern C++20 Standards

We use C++20 features to write expressive and efficient code.

*   **Modules**: (If enabled) Use modules for faster build times. Otherwise, use `#pragma once`.
*   **Concepts**: Use `requires` clauses and Concepts to constrain templates giving better error messages than SFINAE.
*   **Ranges**: Use `std::ranges` for pipeline-like data processing (e.g., filtering, transforming).
*   **auto**: Use `auto` for verbose types, but prefer explicit types when the type is not obvious (e.g., `float` vs `double`).
*   **Constants**: Use `constexpr` or `consteval` for values known at compile time.
*   **Attributes**: Use `[[nodiscard]]` for functions where return values shouldn't be ignored (e.g., resource allocation).

## 3. Data-Oriented Design (DOD) & ECS

*   **Components**: Must be Plain Old Data (POD) / Standard Layout types. No complex logic or destructors in components.
*   **Systems**: Logic lives in Systems. Systems iterate over entities with specific component signatures.
*   **Cache Locality**: Access components linearly. Avoid random memory hopping.

## 4. Engineering Best Practices (New)

### 4.1 Header Hygiene
*   **Forward Declarations**: Minimize `#include` in header files. Use forward declarations (`struct Position;`) whenever possible to reduce compilation dependencies and build times.
*   **Inline Logic**: Keep header implementations minimal unless templated or `constexpr`.

### 4.2 Const Correctness
*   **Const Everything**: Variables and methods should be `const` unless modification is explicitly intended.
*   **Parameters**: Pass heavy objects by `const T&`. Pass small PODs (int, float, small structs) by value.

### 4.3 Naming Conventions
*   **Types/Classes/Structs**: `PascalCase` (e.g., `BladeBoomerang`, `CombatStats`).
*   **Functions**: `PascalCase` (e.g., `DoCast`, `GetParam`).
*   **Variables/Parameters**: `snake_case` or `camelCase` (be consistent within file).
*   **Constants**: `kPascalCase` or `UPPER_SNAKE_CASE`.

### 4.4 Formatting
*   **Clang-Format**: All code must be formatted using the project's `.clang-format` configuration.

## 5. Testing Standards

*   **Unit Tests**: New features must have corresponding tests in the `tests/` directory.
*   **Integration Tests**: Verify systems interact correctly without crashes.
*   **No Logic Deadlocks**: Ensure state machines (like `TurnState`) have guaranteed exit conditions.
*   **Safety Checks in Tests (Critical)**:
    *   **REQUIRE vs CHECK**: When validating pointers, resources, or container sizes that *must* be valid for the test to proceed safely, use `REQUIRE()`.
        *   **Bad**: `CHECK(ptr != nullptr); ptr->DoSomething();` (If check fails, test continues and crashes).
        *   **Good**: `REQUIRE(ptr != nullptr); ptr->DoSomething();` (If check fails, test aborts safely).
    *   Use `CHECK()` for value assertions (`CHECK(health == 100);`).

## 6. Project Specifics

*   **Environment**: Windows, PowerShell.
*   **Constants**: 
    *   **Hardcoded Values**: Magic numbers (speed, damage multipliers, radiuses) should be extracted to `src/game/components/Common.hpp` if they are global constants.
    *   **Skill Parameters**: Skill-specific values (damage ratios, cooldowns) should be loaded from JSON via `SkillRegistry` with sensible fallbacks.
*   **Logging**: Use the project's `Logger` (`LOG_INFO`, `LOG_ERROR`). Do not use `std::cout`.

## 7. Build & Runtime Standards

### 7.1 Build Script (`build.bat`)
We use a standard build script for Windows. Always use `build.bat` instead of running `cmake` manually when possible.

```batch
@echo off
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
```

### 7.2 Runtime Environment

When running executables or debugging:
*   **GCC/MinGW Builds**: Executables are located in `.\build\bin`
*   **MSVC Builds**: Executables are located in `.\build\bin\Release` (or `Debug`)

## 8. Code Review Checklist (Pre-Commit)

Before committing, run `git diff` and verify:
1.  **UB Check**: Are you dereferencing pointers without null checks?
2.  **UAF Check**: Are you holding component pointers while modifying the registry?
3.  **Memory**: Any raw `malloc` or `new`?
4.  **Logic**: Can this loop run forever? Is this division safe from zero?
5.  **Test Safety**: Did you use `REQUIRE` for non-null checks?
6.  **Style**: Are naming conventions consistent?

