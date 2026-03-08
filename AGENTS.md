# AGENTS.md
Operational rules for coding agents in `F:\NoMoreDay`.

## 1) Priority Rules
1. Direct user instruction.
2. This `AGENTS.md`.
3. Other repo docs (`README.md`, `conductor/*`, etc.).

## 2) Environment + Scope
1. Default environment: Windows + PowerShell.
2. Keep changes minimal and task-focused.
3. Never revert unrelated user changes.
4. Do not refactor unrelated code.

## 3) Repo Facts
1. Toolchain: C++20 + CMake + MSVC.
2. Build entry: `./build.bat`.
3. Test binary: `bin/NoMoreDayTests.exe`.
4. CTest labels: `ci`, `unit`, `integration`, `performance`.

## 4) Standard Workflow
1. Read relevant code/docs before editing.
2. Implement the smallest correct change.
3. Verify with the narrowest command first.
4. If C++/build files changed: run build + relevant tests.
5. If only docs/assets changed: state why build/tests were skipped.

## 5) Memory Rules (new)
1. Save checkpoint memory at key milestones:
   - after major design decisions,
   - after significant migration steps,
   - after final verification,
   - after commit completion.
2. Each memory must include:
   - what changed,
   - verification outcome,
   - current risk/blockers.
3. Keep memory entries factual, short, and searchable (stable keywords).
4. Do not store secrets, tokens, credentials, or private keys.
5. If direction changes, store a correction checkpoint to supersede stale memory.
6. Load memory when first conversation.

## 6) Build/Test Commands
Run from repo root `F:\NoMoreDay`.

### Build
- `./build.bat`
- `./build.bat clean`
- `./build.bat clean-all`
- `./build.bat release`
- `./build.bat debug`
- `./build.bat check`

### CTest Labels
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
- `ctest --test-dir build -C Release -L performance --output-on-failure`

### Single doctest case
- List: `./bin/NoMoreDayTests.exe --list-test-cases`
- Run one: `./bin/NoMoreDayTests.exe --test-case="[Unit] ..."`

Notes:
- `build.bat perf` is deprecated.
- Keep `-C` explicit with MSVC multi-config generators.

## 7) C++ Rules
1. Follow `conductor/code_standard.md` and `.clang-format`.
2. Use `#pragma once` in headers.
3. Prefer RAII, `const` by default, explicit casts.
4. Keep ECS components data-only; behavior belongs in systems.
5. In doctest: `REQUIRE` for preconditions, `CHECK` for values.

## 8) Python Script Rules
1. Follow `conductor/code_styleguides/python.md`.
2. Group imports: stdlib / third-party / local.
3. Avoid bare `except:`.
4. Add type hints for public functions when practical.

## 9) Rendering/Platform Guardrails
1. Respect RenderGraph ownership rules.
2. Preserve frame-stage ordering assumptions.
3. Keep resize/recreate resource paths valid.
4. Preserve Windows macro constraints (`WIN32_LEAN_AND_MEAN`, `NOMINMAX`).

## 10) Git Rules
1. Do not commit unless user explicitly requests it.
2. Avoid destructive git commands unless explicitly requested.
3. Keep diffs small and report verification evidence.

## 11) Debug Rules
1. Register/update debug bugs in `conductor/bug_registry.md` before fix and after verification.
2. Keep `AGENTS.md` debug guidance concise; detailed root cause/fix/evidence stays in `conductor/bug_registry.md`.
3. For every debug fix, record at least one reproducible verification command or hand-test result in the bug entry.

## 12) Optional External Rule Files
If added later, treat as extra constraints:
- `.cursor/rules/`
- `.cursorrules`
- `.github/copilot-instructions.md`
