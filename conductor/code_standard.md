# Coding Standards & Best Practices

To ensure the long-term maintainability, performance, and stability of **NoMoreDay**, all contributors must adhere to the following standards.

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

## 3. Data-Oriented Design (DOD) & ECS

*   **Components**: Must be Plain Old Data (POD) / Standard Layout types. No complex logic or destructors in components.
*   **Systems**: Logic lives in Systems. Systems iterate over entities with specific component signatures.
*   **Cache Locality**: Access components linearly. Avoid random memory hopping.

## 4. Project Specifics

*   **Environment**: Windows, PowerShell.
*   **Constants**: 
    *   **Hardcoded Values**: Magic numbers (speed, damage multipliers, radiuses) should be extracted to `src/game/components/Common.hpp` if they are global constants.
    *   **Skill Parameters**: Skill-specific values (damage ratios, cooldowns) should be loaded from JSON via `SkillRegistry` with sensible fallbacks.
*   **Logging**: Use the project's `Logger` (`LOG_INFO`, `LOG_ERROR`). Do not use `std::cout`.

## 5. Testing

*   **Unit Tests**: New features must have corresponding tests in the `tests/` directory.
*   **Integration Tests**: Verify systems interact correctly without crashes.
*   **No Logic Deadlocks**: Ensure state machines (like `TurnState`) have guaranteed exit conditions in all branches.

## 6. Code Review Checklist (Pre-Commit)

Before committing, run `git diff` and verify:
1.  **UB Check**: Are you dereferencing pointers without null checks?
2.  **UAF Check**: Are you holding component pointers while modifying the registry?
3.  **Memory**: Any raw `malloc` or `new`?
4.  **Logic**: Can this loop run forever? Is this division safe from zero?
5.  **Style**: Are naming conventions consistent? (PascalCase for Types/Functions, snake_case for variables).

