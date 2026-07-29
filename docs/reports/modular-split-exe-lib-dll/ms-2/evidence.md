# MS-2 Evidence

## Ownership correction

- `ApplyKnockback` operates directly on the Game ECS `entt::registry` and the
  Game-owned `Position` and `Velocity` components from `Common.hpp`. It is
  therefore Game policy, not a dependency-free Core or Types utility.
- The helper moved from `src/core/math/PhysicsUtils.hpp` to
  `src/game/systems/physics/PhysicsUtils.hpp`. Its
  `NoMoreDay::Utils::ApplyKnockback` namespace, signature, and behavior are
  unchanged. No compatibility forwarding header remains in Core.
- Live direct consumers now include the Game path:
  `src/game/systems/combat/CombatSystem.cpp`,
  `src/game/systems/skill/ProjectileSystem.cpp`, and
  `tests/tech/EngineTechTests.cpp`.
- `src/game/states/GameplayState.cpp` had no `ApplyKnockback` use and its stale
  include was removed. The existing focused knockback regression in
  `EngineTechTests.cpp` is preserved.

## Scope limits

- This package changes no CMake target topology, `build.bat`, Python boundary
  guards, Types/Core source or target, rendering/GPU code, or Engine
  physics/input ownership. The MS-0/MS-1 inventories are updated only to remove
  the resolved `PhysicsUtils` entries.
- `docs/designs/modular-split-exe-lib-dll-design.md` is protected user-owned
  worktree content and was not edited, staged, or committed.
- MS-2 remains `[~]` pending independent review. No files are staged or
  committed by this implementation package.

## Verification

- Exact direct-include search for
  `^#include "core/math/PhysicsUtils.hpp"` under `src` and `tests`: **PASS**;
  no active old-path include remains. The only prior `src/pch.hpp` mention is a
  commented-out line and is not an include.
- Exact direct-include search for
  `^#include "game/systems/physics/PhysicsUtils.hpp"`: **PASS**; exactly the
  three intended consumers were found: `CombatSystem.cpp`, `ProjectileSystem.cpp`,
  and `EngineTechTests.cpp`.
- `ApplyKnockback` search: **PASS**; one definition and the two Game consumers
  plus the focused regression call were found.
- `python scripts/check_module_boundaries.py`: **PASS**; observed/ledger edges
  are `128/128` across 36 files. The removed Core edge was removed from the
  ledger in the same change.
- `python scripts/check_core_candidate_contract.py`: **PASS**; the resolved
  `PhysicsUtils` deferred candidate was removed from the contract.
- `bin/NoMoreDayTests.exe --test-case="[Tech] PhysicsSystem - Interaction
  Logic"`: **PASS**; 1 test case and 1 assertion passed, with 672 cases
  skipped by the filter.
- `cmd.exe /c build.bat check`: **PASS**; module-boundary and Core-candidate
  contract checks ran before check mode skipped compilation.
- `cmd.exe /c build.bat`: **FAIL (known out-of-scope blocker)**; RelWithDebInfo
  reached `SkillBehaviors` then stopped at the unchanged
  `src/game/systems/skill/behaviors/BloodSea.cpp:243` with C2653/C3861 because
  `CombatEventDispatcher::Dispatch` could not be resolved. Build log:
  `C:\Users\yuminao\AppData\Local\Temp\opencode\ms2-build.log`. This
  package does not modify or repair that skill-module failure.
- `git diff --check`: **PASS**; no whitespace errors. Git printed only
  LF-to-CRLF working-copy warnings for tracked files.
