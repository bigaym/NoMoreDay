# NoMoreDay Coding Standard (V2.1)

## 1. Introduction
This document defines the coding standards, architectural principles, and safety requirements for the **NoMoreDay** project. Adherence to these standards is mandatory for all contributors to ensure long-term maintainability, performance, and stability.

## 2. General Principles
1.  **Performance First**: NoMoreDay is a high-performance ARPG. Prioritize Data-Oriented Design (DOD) and cache locality. Avoid heap allocations in hot paths (Update/Render).
2.  **Safety & Robustness**: Zero tolerance for Undefined Behavior (UB), Use-After-Free (UAF), and memory leaks.
3.  **Clarity & Maintainability**: Write expressive code. Use Modern C++ features to reduce boilerplate and improve type safety.

## 3. Naming Conventions

### 3.1 Files & Directories
*   **Source Files**: `PascalCase.cpp`, `PascalCase.hpp`.
*   **Directories**: `snake_case`.

### 3.2 Types & Symbols
*   **Classes/Structs**: `PascalCase` (e.g., `CombatSystem`, `ItemComponent`).
*   **Functions/Methods**: `PascalCase` (e.g., `Initialize()`, `CalculateDamage()`).
*   **Local Variables & Parameters**: `camelCase` (e.g., `deltaSeconds`, `targetEntity`).
*   **Member Variables**: `m_camelCase` (e.g., `m_screenWidth`, `m_registry`).
*   **Constants & Macros**: `UPPER_SNAKE_CASE` or `kPascalCase` (e.g., `MAX_ENTITIES`, `kBaseGravity`).
*   **Enums**: `PascalCase` for both type name and values (e.g., `enum class Rarity { Common, Rare }`).

## 4. Layout & Formatting
1.  **Clang-Format**: All C++ code must be formatted using the project's `.clang-format` file.
2.  **Header Hygiene**:
    *   Use `#pragma once` for all headers.
    *   Minimize `#include` in headers. Use **Forward Declarations** whenever possible to reduce compile times.
3.  **Const Correctness**: Use `const` by default. Every variable and method that does not modify state should be `const`.
4.  **Modern C++ Attributes**: Use `[[nodiscard]]` for functions where ignoring the return value is likely a bug (e.g., factory methods, resource acquisition).

## 5. Memory & Resource Management
1.  **RAII**: All resource ownership must be managed via RAII.
2.  **No Raw Ownership**: Never use `new` or `delete`. Use `std::unique_ptr` for exclusive ownership and `std::shared_ptr` for shared ownership.
3.  **EnTT Safety (CRITICAL)**:
    *   **Pointer Invalidation**: Component pointers obtained via `registry.try_get<T>(e)` are only valid as long as the underlying component pool is not modified.
    *   **Rule**: Never hold a component pointer across operations that might add/remove components or create/destroy entities.
    *   **Fix**: Copy small POD components to the stack before performing complex registry operations.

## 6. Modern C++ & Type Safety
1.  **Casting Restrictions**:
    *   **No `dynamic_cast`**: Strictly avoid `dynamic_cast` as RTTI is expensive and often indicative of poor architectural design in ECS. Use component-based checks instead.
    *   **Avoid C-style Casts**: Use `static_cast` for safe numeric or pointer conversions.
    *   **Reinterpret Cast**: Use `reinterpret_cast` only for low-level memory manipulation (e.g., serialization) and document the reason. Prefer `std::bit_cast` (C++20) for bit-level type punning.
2.  **Concepts**: Use `concepts` and `requires` clauses to constrain templates instead of SFINAE.
3.  **Ranges**: Use `std::ranges` for data processing pipelines.
4.  **Auto**: Use `auto` when the type is obvious, but prefer explicit types for clarity in business logic.
5.  **Constexpr**: Use `constexpr` or `consteval` for values and logic that can be evaluated at compile time.

## 7. ECS & Data-Oriented Design (DOD)
1.  **Components**:
    *   Must be **POD** (Plain Old Data) / Standard Layout types.
    *   No complex logic, virtual functions, or non-trivial destructors.
2.  **String-Free Core Logic (CRITICAL)**:
    *   **No String Comparisons**: Strictly forbid using string comparisons (`if (name == "fire_ball")`) in hot paths or core logic (skills, combat, AI).
    *   **Enums over Strings**: Use `enum class` or unique integer IDs for logic branching and identification.
    *   **Registry Pattern**: For data loaded from JSON (e.g., skill IDs), map strings to Enums/IDs once at load-time using a registry.
    *   **String Views**: Use `std::string_view` for read-only string handling to avoid unnecessary allocations.
3.  **Systems**:
    *   Logic must reside in Systems, not Components.
    *   Systems should iterate over entities linearly to maximize cache efficiency.
4.  **State Management**: Avoid global mutable state. Store global state in the `GameContext` or as EnTT singletons.

## 8. Concurrency & Parallelism
1.  **Taskflow**: Use the `taskflow` graph for parallelizing logic. Do not spawn raw `std::thread`.
2.  **Thread Safety**: Ensure systems accessing the same component types are not executed in parallel if one of them is writing.

## 9. Testing & Quality Assurance
1.  **Test Driven**: New features must include unit or functional tests in the `tests/` directory.
2.  **Assertion Safety**:
    *   Use `REQUIRE()` for preconditions that must hold for the test to continue (e.g., pointer null checks).
    *   Use `CHECK()` for value-based assertions.
3.  **Code Review Checklist**:
    *   Check for potential null pointer dereferences.
    *   Verify RAII compliance.
    *   Ensure no magic numbers (extract to `Common.hpp` or JSON).
    *   Verify test coverage.

## 10. Tooling & Workflow
1.  **Git**: Use Conventional Commits (`feat:`, `fix:`, `refactor:`, etc.).
2.  **Build**: Always use `.\build.bat` for consistent builds.
3.  **Agentic Workflow**:
    *   Prioritize `smart-tree` tools for exploration.
    *   Use the `cpp-analyzer` suite for structural analysis before making changes.
    *   Log significant architectural changes using `save_memory`.