---
name: project-auditor
description: Performs the final code review and technical audit. Use this skill after implementation is complete to verify safety, style, and documentation before committing.
---

# Project Auditor

## Goal
To act as the final quality gate, ensuring strictly memory-safe, performant, and well-documented C++ code.

## Instructions
1.  **Smart Review & Scan**:
    -   **Change Detection**: Use `analyze {mode:'git_status'}` to identify all modified, staged, and untracked files.
    -   **Safety Scan**: Run the automated safety scanner on the source code:
        `python .agent/skills/project-auditor/scripts/safety_scan.py src`
    -   If any violations (raw `new`, `printf`, etc.) are found, reject the code immediately.

2.  **Manual Code Audit**:
    -   Review `git diff` for logical errors not caught by the scanner:
        -   **UAF**: Use-After-Free in EnTT views?
        -   **Concurrency**: Race conditions in Taskflow tasks?
        -   **Naming**: `PascalCase` types, `kPascalCase` constants?

3.  **Build Verification**:
    -   Run `build.bat` and ensure 0 warnings.
    -   Run relevant tests in `build/bin/tests/`.

4.  **Documentation & Commit**:
    -   Update `conductor/tracks/` files.
    -   Generate a Conventional Commit message (`feat:`, `fix:`, `refactor:`).

## Constraints
-   Zero tolerance for memory safety violations.
-   Build must be clean (no warnings).
-   Documentation must be synced before commit.