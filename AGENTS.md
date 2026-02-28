# AGENTS.md
Operational guide for coding agents working in `F:\NoMoreDay`.

## 1) Priority and scope
- Rule priority:
  1. Direct user instruction
  2. `AGENTS.md`
  3. Other repo docs (`README.md`, `conductor/*`, etc.)
- Assume Windows + PowerShell unless user says otherwise.
- Keep edits minimal and task-focused; do not refactor unrelated areas.
- Never revert unrelated user changes.

## 2) Repository facts
- Toolchain: C++20 + CMake, MSVC-only in this repository.
- Build entrypoint: `./build.bat` from repo root.
- Test binary: `bin/NoMoreDayTests.exe`.
- Test framework: doctest, registered into CTest in `tests/CMakeLists.txt`.
- CTest labels: `ci`, `unit`, `integration`, `performance`.

## 3) Agent workflow
- Start with context from relevant repo docs/files before editing.
- Implement the smallest correct change.
- Verify with the narrowest command that proves correctness.
- If C++/build-system files changed, run build + relevant tests before claiming done.
- If only docs/non-build assets changed, explicitly state why build/test was skipped.

## 4) Build/lint/test commands
Run commands from `F:\NoMoreDay`.

### 4.1 Build
- Default build: `./build.bat`
- Clean CMake cache: `./build.bat clean`
- Full rebuild: `./build.bat clean-all`
- Skip test target during build: `./build.bat notest`
- Release build: `./build.bat release`
- Debug build: `./build.bat debug`

### 4.2 Lint/static checks
- MSVC static analysis: `./build.bat analyze`
- Pre-check scripts only (no compile): `./build.bat check`
- Include/dependency trace: `./build.bat includes`
- Pre-check scripts run by `build.bat` (unless `novalidate`):
  - `python tools/render_abi/generate_gpu_abi.py`
  - `python tools/render_abi/check_no_manual_abi_structs.py`
  - `python scripts/validate_json.py`
  - `python scripts/gen_skill_contracts.py --check`

### 4.3 Test suites (CTest)
- CI suite: `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`
- Unit suite: `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- Integration suite: `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
- Performance suite: `ctest --test-dir build -C Release -L performance --output-on-failure`

### 4.4 Running a single test (important)
CTest here registers grouped suites, not individual doctest cases.
Use the doctest binary for a true single test case.

- List all doctest cases:
  - `./bin/NoMoreDayTests.exe --list-test-cases`
- Run one specific case/filter:
  - `./bin/NoMoreDayTests.exe --test-case="[Unit] JFADistanceFieldEvaluator - Full-res JFA accuracy envelope"`
  - `./bin/NoMoreDayTests.exe --test-case="*RenderGraph V5 Contracts*"`
- Run one whole CTest group by exact name:
  - `ctest --test-dir build -C RelWithDebInfo -R "^nmd.tests.unit$" --output-on-failure`

Notes:
- `build.bat perf` is deprecated; use CTest directly.
- Keep `-C` explicit with MSVC multi-config generators.

## 5) C++ style guidelines
Source of truth: `conductor/code_standard.md` and `.clang-format`.

### 5.1 Naming
- Files: `PascalCase.cpp` / `PascalCase.hpp`
- Directories: `snake_case`
- Types: `PascalCase`
- Functions/methods: `PascalCase`
- Local vars/params: `camelCase`
- Members: `m_camelCase`
- Constants: `UPPER_SNAKE_CASE` or `kPascalCase`

### 5.2 Formatting and imports/includes
- Format C++ with repo `.clang-format` (Google-based, 4 spaces, 120 columns).
- Use `#pragma once` in headers.
- Prefer forward declarations in headers to reduce include fan-out.
- Include ordering is tool-managed (`SortIncludes: true`); do not hand-micro-manage order.
- Keep layout simple and consistent with nearby code.

### 5.3 Types, memory, and modern C++
- Prefer RAII and smart pointers; avoid raw owning `new`/`delete`.
- Use `const` by default and `[[nodiscard]]` for must-check returns.
- Avoid C-style casts; use explicit C++ casts with justification.
- Prefer `std::bit_cast` when doing bit reinterpretation.
- Use C++20 features where they improve safety/clarity (`concepts`, `constexpr`, ranges).

### 5.4 ECS/data-oriented constraints
- Components should remain POD/standard-layout and data-only.
- Keep behavior in systems, not components.
- Avoid string comparisons in hot paths; map string ids to enum/int ids during load.
- Do not hold EnTT component pointers across registry mutations.

### 5.5 Error handling and tests
- In doctest, use `REQUIRE` for preconditions and `CHECK` for value checks.
- Validate assumptions and log actionable context in runtime code.
- Keep fallback behavior explicit (especially rendering tier/degrade paths).
- Never silently swallow failures that affect correctness.

## 6) Python/script style guidelines
- Follow `conductor/code_styleguides/python.md` for scripts.
- Imports grouped as: stdlib, third-party, local; one import per line.
- Avoid bare `except:`; catch specific exceptions.
- Naming: `snake_case` for functions/vars, `PascalCase` for classes.
- Add type annotations for public functions when practical.

## 7) Rendering/platform guardrails
- Respect RenderGraph ownership rules; only composite/final pass writes to FBO 0.
- Preserve frame-stage ordering assumptions used by gameplay/render systems.
- Ensure resize/recreate paths keep framebuffer resources valid.
- Preserve Windows macro constraints (`WIN32_LEAN_AND_MEAN`, `NOMINMAX`).

## 8) Git hygiene
- Do not commit unless user explicitly requests it.
- Avoid destructive git commands unless explicitly requested.
- Keep diffs small and include verification evidence in completion notes.

## 9) Cursor/Copilot instructions status
Checked for:
- `.cursor/rules/`
- `.cursorrules`
- `.github/copilot-instructions.md`

Current repository status:
- No Cursor rule files found.
- No Copilot instruction file found.

If these files are added later, treat them as additional constraints and update this file.
