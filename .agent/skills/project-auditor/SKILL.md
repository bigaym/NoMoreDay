---
name: project-auditor
description: Audits completed code changes for technical compliance and synchronizes project documentation. Use this skill when a development task is finished and you need to perform a final review, update conductor/tracks files, and prepare a git commit using the Conventional Commits format. It ensures that no Use-After-Free (UAF) issues exist and that build.bat has been used for final verification.
---

# Project Auditor

This skill provides a final quality gate to ensure that delivered code is stable, correctly named, and fully tracked in the project's documentation.

## When to use this skill

- Use this after implementation is complete and all tests pass.
- Use this strictly for the review and commit phase (not during active coding).
- Use this to update the `conductor/` tracking files after a task.

## How to use it

### 1. Technical Audit Checklist
Review the `git diff` line-by-line and verify:
- **Memory Safety**: No raw `new`/`delete` and no Use-After-Free (UAF) in EnTT logic.
- **Naming**: `PascalCase` for Types/Functions and `kPascalCase` for constants.
- **Safety**: All critical pointer dereferences must be guarded by `REQUIRE()` or null checks.
- **Hygiene**: Use forward declarations instead of unnecessary `#include` in headers.

### 2. Documentation Synchronization
Ensure the project's state is updated:
- Update the completion status in the relevant `conductor/tracks/<track_id>/plan.md`.
- Record new technical decisions or patterns in `conductor/tech-stack.md`.
- Ensure new files are registered in the `conductor/index.md` if necessary.

### 3. Git Submission
- **Commit Format**: Generate a message using the Conventional Commits standard (e.g., `feat: ...`, `fix: ...`, `test: ...`).
- **Cleanliness**: Verify that no temporary logs or build artifacts are staged for commit.

### 4. Final Build Verification
- Execute `build.bat` to confirm zero compilation warnings.
- Run the relevant test binaries in `build/bin/tests/` and confirm a 100% pass rate before finishing the audit.