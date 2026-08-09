# M5 Foundation Band (components + registry) Plan

> **Status:** planned 2026-08-09
> **Design:** [Directory Structure Reorganization](../designs/directory-structure-reorganization-design.md), M5
> **Scope:** `git mv` the header-only `game/components/` (36 ECS component headers) and `game/registry/` (`GroupRegistry.hpp`) into `game/foundation/`, and rewrite the 857 include sites that reference the two moved prefixes. Pure header relocation — no CMake change, no target change.

## 1. Implementation Approach

### 1.1 Scope boundary

M5 is a physical-layout and include-path migration only. Both directories are header-only (no `CMakeLists.txt`, no compilation unit, no target — the headers are consumed through `target_include_directories(... ${CMAKE_SOURCE_DIR}/src)` already in place). No CMake file changes at all: no `add_subdirectory`, no link edge, no property. The CMake target graph, PCH, APIs, runtime behavior, and test registration remain unchanged.

### 1.2 Final ownership map for M5

| Ownership | After M5 | Before M5 |
|---|---|---|
| (none — header-only) | `src/game/foundation/components/` (36 headers) | `src/game/components/` |
| (none — header-only) | `src/game/foundation/registry/GroupRegistry.hpp` | `src/game/registry/GroupRegistry.hpp` |

`GroupRegistry.hpp` moves with its directory into `foundation/` as the sole `registry/` member; it is not merged into `components/` (design §4.2 keeps them as separate sub-directories; the band rule is "placement by band first, target second", and neither has a target).

### 1.3 Dependency and call-chain preservation

Two rewrite rules, `SimpleMatch` line counts over `src/` + `tests/` (verified 2026-08-09). Design-doc estimates (538/4) are superseded by the exact counts below:

| # | Old | New | Sites | Design estimate |
|---|---|---|---|---|
| 1 | `game/components/` | `game/foundation/components/` | 852 | 538 |
| 2 | `game/registry/` | `game/foundation/registry/` | 5 | 4 |
| | **Total** | | **857** | 542 |

Notes:

- Rule 1's 852 sites include self-band references (components headers include each other via the `game/components/...` prefix) — they move with their files and are rewritten like external sites; no site is exempt.
- The actual header count is 36 (`AdvancedAffixComponents.hpp` … `WorldState.hpp`), not the design-doc estimate of 37 (design §3.2 item 7); the include-site counts above are the authoritative numbers for T0/T2.
- Rule 2's 5 sites: `src/game/systems/combat/StatsSystem.cpp` (via the `GroupRegistry` include chain, design §3.2 item 12 — fixed by M1 for the physical file; the include prefix is rewritten here), the 4 remaining external sites spread across gameplay systems and tests (exact list captured in T0).
- Runtime relationships (ECS component types constructed by systems; `GroupRegistry` consumed by combat stats) are compile-time include relationships — unchanged by the path migration.

### 1.4 CMake convergence

None. No `CMakeLists.txt` exists in either moved directory; no `add_subdirectory` or include-path entry references `game/components` or `game/registry`; the root `TAG_HEADER` is unaffected.

## 2. Pseudocode Guidance

```text
git mv src/game/components src/game/foundation/components
git mv src/game/registry    src/game/foundation/registry

rules = [
    ("game/components/", "game/foundation/components/"),
    ("game/registry/",   "game/foundation/registry/"),
]
for old, new in rules:
    rewrite(old, new, files=src/**/*.hpp,*.cpp + tests/**/*.hpp,*.cpp)  # SimpleMatch per line
# expect rule 1 == 852 hits, rule 2 == 5 hits
```

## 3. Atomic Tasks

- **T0 — Baseline.** Capture the exact 857-site list per rule (Select-String output to a scratch file); confirm counts match §1.3; confirm `git status` clean and no `CMakeLists.txt` inside `components/`/`registry/`.
- **T1 — `git mv`.** Move both directories per §2.
- **T2 — Include rewrite.** Apply the 2 rules to `src/` + `tests/`; rule 1 is a large mechanical batch — process it file-by-file (or chunked by directory: `src/game/` first, then `src/app/`+`src/core/`+`src/engine/`, then `tests/`), verifying the per-chunk hit counts add to 852.
- **T3 — Static verification.** `Select-String` for both old prefixes returns zero hits; tree check: `game/foundation/` gains exactly `components/` (36 files) and `registry/` (1 file); `git status` shows only the 2 moves + rewrites.
- **T4 — Build.** `./build.bat` (RelWithDebInfo, log redirected to file) — headers are header-only, so any missed site surfaces at first compile of its consumer; the full build is the completeness gate.
- **T5 — Test.** `ctest --test-dir build -C RelWithDebInfo -L unit`, `-L integration`, `-L ci` (serial), then `./build.bat check`. Component-heavy suites receive attention: combat (`CombatAntiMetaLayerTests`, `CombatBalanceTest`), skill (`SkillBehaviorGuardTests`, `SkillSystemTests`), item (`DropSystem`/`InventorySystem` via `GameplaySystems`), world (`NemesisEvolutionTests`, `BiomeMapGeneratorTest`), ui (`UITests`, `AstrolabeSystemTests`).
- **T6 — Completion.** Run §5 checks, mark checklist `[x]` with evidence; no commit (hand-off).

## 4. Test Method

### 4.1 Test level and regression coverage

Full CI suite (no label/registration change, design §7.5). The ECS component headers are consumed by 850+ sites, so the practical gate is: full build success (every consumer compiles) + full `ctest -L ci` (every consumer's behavior is exercised). Suites listed in T5 are the highest-fan-in consumers.

### 4.2 Verification commands

```text
./build.bat > build_m5.log 2>&1
ctest --test-dir build -C RelWithDebInfo -L unit          # serial
ctest --test-dir build -C RelWithDebInfo -L integration   # serial
ctest --test-dir build -C RelWithDebInfo -L ci            # serial
./build.bat check
```

## 5. Completion Definition

1. `src/game/foundation/components/` (36 files) and `src/game/foundation/registry/` (`GroupRegistry.hpp`) exist; `src/game/components/` and `src/game/registry/` no longer exist.
2. `Select-String` for `game/components/` and `game/registry/` returns zero hits across `src/` + `tests/`.
3. No CMake file changed (`git status` confirms: 2 directory renames + include rewrites only).
4. `./build.bat` (RelWithDebInfo) builds clean; `ctest -L unit` / `-L integration` / `-L ci` green; `./build.bat check` green.

## 6. Risks And Mitigations

- **R1 (largest mechanical rewrite).** 852 sites is the single biggest batch in the reorganization. Mitigation: chunked processing with per-chunk count verification (T2); the milestone is isolated to these two prefixes so failures are attributable to it (design R3); the full build is the completeness gate.
- **R2 (header-only blind spots).** No compilation unit exists inside the moved directories, so "was it rewritten" has no direct build signal until a consumer compiles. Mitigation: T0 baseline list + T3 zero-residual grep are mandatory, and the T4 full build covers every consumer.
- **R3 (self-band references).** Components include each other by prefix; a partial rewrite would mix old/new prefixes inside `foundation/components/`. Mitigation: rule 1 applies uniformly to all files including the moved ones; T3's zero-residual grep includes the moved directories themselves.

## 7. Handoff To Implementation

Single implementer agent executes T0-T6 in one session (one prefix family, one coherent batch; no parallelizable disjoint subsets). All build/test commands run serially on the shared build directory with logs redirected to files. The agent must not commit; it returns the T0/T2 per-chunk counts, test results, and the exact `git status` file list.
