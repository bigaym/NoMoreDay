---
name: project-navigator
description: Guides the agent to clarify requirements and align with the NoMoreDay technical stack before implementation. Use this skill when the user provides high-level or ambiguous C++ development tasks, or when starting a new feature that involves EnTT ECS components, Taskflow, or C++20 standards. It prevents the agent from making assumptions and ensures all code follows the RAII and memory safety rules defined in conductor/code_standard.md.
---

# Project Navigator

This skill acts as a mandatory guardrail for all development tasks. It ensures the Agent is fully synchronized with the project's technical DNA before modification.

## When to use this skill

- Use this at the start of any development task (feature implementation, bug fix, or refactor).
- Use this when user requirements are high-level, ambiguous, or lack technical specs.
- Use this to ensure every code change adheres to the EnTT memory safety and C++20 standards.

## How to use it

### 1. Context Initialization
Before generating any code, you must:
- Execute `st context {operation: 'gather_project'}` to absorb the latest architectural state.
- Query `st memory {operation: 'find', keywords: ['standard', 'component', 'pod']}` to recall specific constraints.

### 2. Requirement Clarification (The Guard)
NEVER execute a task based on fuzzy descriptions. You must pause and ask the user to clarify:
- **Data Structures**: Which POD components are being modified or created?
- **Logic Flow**: What are the specific state transitions and Taskflow dependencies?
- **Safety Boundaries**: Are there concurrency risks or pointer invalidation scenarios?
- **Plan Approval**: Provide a short implementation plan and wait for an "ACK" or "Proceed" from the user.

### 3. Engineering Standards
- **EnTT Safety**: NEVER hold pointers/references to components across registry mutations. Copy PODs to the stack if needed.
- **C++20**: Constrain all templates with `requires` clauses. Use `std::ranges` and `std::span` for data handling.
- **POD Components**: Ensure all ECS components are Plain Old Data (no complex logic/destructors).
- **Hardcoded Values**: Move all magic numbers to `src/game/components/Common.hpp`.

### 4. Build & Test Verification
- **Build Standard**: Use `build.bat` for all compilations. Do not use raw `cmake`.
- **Test-Driven**: Every implementation must have a corresponding test in `tests/`.
- **Assertions**: Use `REQUIRE()` for safety-critical checks and `CHECK()` for value verification.