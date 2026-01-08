# General Code Style Principles

This document outlines general coding principles that apply across all languages and frameworks used in this project.

## 1. Safety & Robustness
- **Zero Undefined Behavior (UB):** Code must be written to strictly avoid UB. Use static analysis and sanitizers to verify.
- **RAII:** Use Resource Acquisition Is Initialization for all resources (memory, files, handles).
- **Smart Pointers:** Never use raw `new` or `delete`. Use `std::unique_ptr`, `std::shared_ptr`, or EnTT's internal management.
- **Explicit Intent:** Use `[[nodiscard]]` for functions where ignoring the return value is a bug. Use `explicit` for single-argument constructors.

## 2. Performance & Architecture
- **Data-Oriented Design (DOD):** Prioritize data locality. ECS Components must be POD (Plain Old Data) structs.
- **Avoid Polymorphism in Hot Paths:** Minimize virtual functions and deep inheritance hierarchies in performance-critical loops.
- **Allocation Strategy:** Minimize heap allocations during the frame loop. Pre-allocate memory or use stack-based buffers where possible.
- **Modern Types:** Use `std::string_view` and `std::span` to pass data without copying.

## 3. Modern C++20 Standards
- **Compile-time Evaluation:** Use `constexpr` and `consteval` aggressively to move logic to compile time.
- **Concepts:** Use C++20 Concepts to constrain templates instead of `static_assert` or SFINAE where possible.
- **Ranges:** Utilize the Ranges library for more expressive and efficient data processing.

## 4. Readability & Consistency
- **Readability:** Code should be easy to read and understand by humans. Avoid overly clever or obscure constructs.
- **Consistency:** Follow existing patterns in the codebase. Maintain consistent formatting, naming, and structure.
- **Simplicity:** Prefer simple solutions over complex ones. Break down complex problems into smaller, manageable parts.

## 5. Documentation & Maintenance
- **Maintainability:** Write code that is easy to modify and extend. Minimize dependencies and coupling.
- **Documentation:** Document *why* something is done, not just *what*. Keep documentation up-to-date with code changes.
