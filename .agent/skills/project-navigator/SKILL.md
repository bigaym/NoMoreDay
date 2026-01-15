---
name: project-navigator
description: Guides the agent through the implementation planning phase. Use this skill before writing code to ensure alignment with technical standards and requirements.
---

# Project Navigator

## Goal
To prevent technical debt and build failures by enforcing architectural standards and clarifying requirements *before* a single line of code is written.

## Instructions
1.  **Smart Exploration & Check**:
    -   **Context**: Use `overview {mode:'quick', depth:2}` to visualize the immediate working environment.
    -   **Reference Search**: Use `search {keyword:'<Component/System Name>'}` to find how similar components are defined and used.
    -   **File Location**: Use `find {type:'code', pattern:'*<feature>*'}` to locate relevant source files quickly.
    -   **Pre-Flight**: Run the environment verification script:
        `python .agent/skills/project-navigator/scripts/preflight_check.py`

2.  **Requirement Guard**:
    -   Do not proceed if the task is vague (e.g., "Fix the bug").
    -   Ask the user to clarify:
        -   **Components**: Which POD structs are involved?
        -   **Systems**: Which `update()` loops need changes?
        -   **Safety**: Are there pointer invalidation risks?

3.  **Implementation Planning**:
    -   Propose a brief plan.
    -   Identify necessary `Common.hpp` constants (magic numbers are forbidden).

4.  **Standard Enforcement**:
    -   **EnTT**: No keeping references to components across registry operations.
    -   **C++20**: Use Concepts and Ranges.
    -   **Taskflow**: Design for parallelism.

## Constraints
-   Must execute `preflight_check.py` first.
-   No raw pointers without ownership semantics.
-   No `new`/`delete`.