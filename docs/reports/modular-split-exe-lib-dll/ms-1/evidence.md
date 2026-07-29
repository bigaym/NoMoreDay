# MS-1 Evidence

## Implemented contract

- The project owner's explicit MS-1 decision sets `cmake_minimum_required` to
  4.2. The actual local verification version is CMake 4.2.3; this evidence does
  not claim CMake 3.20 support.
- `NoMoreDayTypes` is an empty `INTERFACE` target with only the canonical
  `${CMAKE_CURRENT_SOURCE_DIR}/src` public include root. It has no sources,
  link dependencies, or PCH.
- CMake defers a uniquely named root-scope final guard with
  `cmake_language(DEFER CALL ...)`, so it runs after child directories and
  included CMake code whether `BUILD_TESTING` is on or off. It verifies that
  Types remains an `INTERFACE_LIBRARY`; rejects `SOURCES`, `INTERFACE_SOURCES`,
  direct and interface link libraries, direct and interface PCH, any ordinary
  public include-directory value other than canonical `src`, all
  `INTERFACE_SYSTEM_INCLUDE_DIRECTORIES` values, and root deferred calls still
  queued after the guard.
- `core-candidate-contract.json` records the future Core candidates, deferred
  items, Types admission rules, and the aggregate-PCH inventory. The current
  `NoMoreDayCore` remains explicitly identified as a legacy aggregate, not the
  future Core layer.
- `scripts/check_core_candidate_contract.py` validates the contract schema,
  a complete and mutually exclusive direct `src/core` file manifest, PCH policy
  and inventory, direct forbidden candidate includes, and the static CMake Types
  target contract. It conservatively scans all first-party `CMakeLists.txt` and
  `.cmake` files (excluding `third_party`, `build`, and `.git`) for literal
   `NoMoreDayTypes` property mutation through target/property APIs. Every
   first-party `function()`/`macro()` declaration must have a statically
   verifiable literal identifier; safe bracket-argument names are normalized
   before validation, while variable/dynamic names are rejected. It requires
   exactly one root `function` definition of the final guard, using
   case-insensitive CMake command/declaration-name comparison. Any exact or
   case-variant `function()`/`macro()` declaration of that guard is rejected.
   It rejects first-party redefinitions of its required commands:
   `cmake_language`,
   `get_target_property`, `get_filename_component`, `set`, `list`, and
   `message`, with case-insensitive normalized declaration names including
   bracket literals. It permits only the root
   final `cmake_language(DEFER CALL ...)` and the guard's fixed read-only
   `DEFER ... GET_CALL_IDS` query; it rejects every other first-party
   `cmake_language` command, including `EVAL`, `CANCEL_CALL`, and variable or
   bracket-argument forms. This rejects guard redefinition, command shadowing,
   and deferred cancellation/mutation before configure. It is a fail-closed
   static policy, not a claim to evaluate arbitrary CMake expressions or
   provide a CMake sandbox. Its `--repo-root` and `--contract` arguments
   support deterministic repository and temporary-fixture execution.
- `build.bat` runs this verifier after the module-boundary check and before
  CMake configuration in both check and normal validated-build modes.

## Verification

- `python scripts/check_module_boundaries.py`: **PASS**; observed/ledger
  direct reverse edges `129/129` across 37 files.
- `python -m unittest tests/python/ModuleBoundaryCheckerTest.py`: **PASS**;
  6 tests.
- `python scripts/check_core_candidate_contract.py`: **PASS**; Core manifest
  and Types CMake boundary match, including the unique final-guard definition
  and fail-closed `cmake_language` policy.
- `python -m unittest tests/python/CoreCandidateContractCheckerTest.py`:
   **PASS**; 19 tests in 6.704s. The suite includes temporary real CMake projects
  configured by `cmake -S ... -B ...` that prove normal configuration plus
  deferred-guard rejection of
  variable-target, bracket-argument, included-module, system-include,
  interface-link, interface-PCH, root deferred mutation, and final-guard
  redefinition through the pre-configure contract gate, plus `EVAL CODE` guard
  redefinition and `EVAL CODE` deferred-cancellation/mutation attempts. The
   command-shadowing fixtures combine `get_target_property` overrides that spoof
   clean reads, using a bracket name and a variable name, with variable-target
   `target_sources` mutation; the pre-configure contract gate rejects both real
   CMake configures. Exact macro and case-variant function/macro final-guard
   redefinition fixtures each combine variable-target `target_sources` mutation
   and must fail at the pre-configure contract gate. Static fixtures also cover
   literal property mutations in first-party `.cmake` files, include-root drift,
   omitted Core files, and invalid aggregate contract type.
- CMake 4.2.3 is the available fixture and verification version, matching the
  project minimum version of 4.2. CMake 3.20 support is not claimed.
- `python -m py_compile scripts/check_core_candidate_contract.py
   tests/python/CoreCandidateContractCheckerTest.py`: **PASS**.
- `cmd.exe /c build.bat check`: **PASS**; module-boundary and Core-candidate
   contract checks both ran before CMake, then check mode skipped compilation.
- `git diff --check`: **PASS**; no whitespace errors. Git emitted only
   LF-to-CRLF working-copy warnings for `CMakeLists.txt`, `build.bat`, the
   protected design document, and this plan.
- `cmd.exe /c build.bat`: **FAIL**; RelWithDebInfo configuration completed, but
  the unchanged aggregate build stopped at
  `src/game/systems/skill/behaviors/BloodSea.cpp:243` because
  `CombatEventDispatcher::Dispatch` could not be resolved. `BloodSea.cpp` has
  no diff from `HEAD`, and this MS-1 package changes no C++ source, aggregate
  target source list, or runtime target input. Per the project owner's
  compile-boundary focus, this existing skill-module failure is recorded as an
  MS-1-only out-of-scope residual risk, not repaired or hidden, and does not
  establish a passing aggregate-build claim.
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`:
  **FAIL**; latest rerun: 601 passed, 2 failed, and 70 skipped out of 603
  doctest cases (10,129 assertions: 8,448 passed and 1,681 failed). The failing
  CTest suite is `nmd.tests.ci.nonperf`; GI stability and SkillUI failed.
  Heavenly Sword passed in this rerun, but its prior failure remains an
  unclassified, unstable CI signal rather than a resolved issue. Full output:
   `C:\Users\yuminao\AppData\Local\Temp\opencode\ms1-followup2-ctest-ci.log`.

## Existing CI signals and MS-1 acceptance waiver

The project owner explicitly directed that MS-1 is for split compilation and
that GI stability, SkillUI source-guard, and Heavenly Sword historical runtime
signals are not to be pursued in this node. The following failures and
instability remain recorded rather than hidden. They are accepted only as
  existing, out-of-scope, non-MS-1 compile-boundary-contract regressions for this
  MS-1 acceptance only; the waiver neither fixes them nor establishes a clean CI
  baseline, is not a global CI waiver, and does not apply to their owning work or
  later milestones. Each later milestone must independently disposition the
  relevant signal.

- `[Integration] GI - Long-run Stability Proxy (Resize + Tier Switch)` fails
  at `tests/integration/GIStabilityIntegrationTest.cpp:159-160` because both
  GI texture identifiers are zero after repeated `particle emissive pass
  failed` fallback errors. The assertion originated in `1a74ccbf`; no later
  assertion change or clean passing baseline was found. This remains unfixed
  and outside MS-1 scope under the node-specific acceptance waiver.
- `[Tech] SkillUI - mastery hub locks all Blade Ascendant signature skills
  consistently` fails at `tests/tech/UITests.cpp:437`: its source-text guard
  expects `id == 10 || id == 11 || id == 12`, which is absent. History shows
  `6a428d43` added the guard before `b658596f` deliberately replaced that
  literal check with `BladeMasteryRegistry` iteration. This is an established
  test/implementation mismatch, not an MS-1 change. It remains unfixed and is
  accepted only for this MS-1 node under the explicit waiver.
- `[Unit] SkillBehaviorGuard - Heavenly Sword element nodes close remaining
  gaps` fails at `tests/unit/HeavenlySwordClosureTests.cpp:97`: after 30
  updates, `FrozenDominion (1122)` does not report Freeze. The test originated
  in `907d73fa`; its update loop and target health changed in `2df230a2` on
  2026-07-26. It passed in the latest rerun after failing in the prior MS-1
  evidence run. No clean baseline or accepted disposition exists, so this
  unstable signal remains unfixed and outside MS-1 scope under the
  node-specific acceptance waiver.

## Scope and residual risk

- No production source was moved; no `src/types` directory or game type was
  added; no final target graph/source placement was enabled; and no P0
  GPU/render, GI, UI, or gameplay code changed.
- The latest aggregate build remains blocked by the unchanged, out-of-scope
  `BloodSea.cpp` skill-module compile failure recorded above. It is accepted
  only for this MS-1 compile-boundary acceptance; it is not fixed, a global
  build waiver, or a waiver for later milestones.
- MS-1 and MS-1.5 remain `[~]`: independent review and commit gates are pending.
  This round's MS-1 acceptance evidence is limited to the Types target contract,
  configure guard, checker, focused tests, and build verification. The CI waiver
  is limited to MS-1 and does not represent a GI, SkillUI, or Heavenly Sword
  repair, a global CI waiver, or a waiver for later milestones. MS-1 and MS-1.5
  remain `[~]` pending final review. No files were staged or committed.
  `git diff --check` is rerun after this evidence update.
