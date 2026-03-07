# Sword Saint Post-MVP Polish Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Complete the next polish pass for Sword Saint after the MVP/mastery rollout, focusing on post-full-flow rhythm restart, clearer threshold telegraphing, and balance-safe regression coverage.

**Architecture:** Keep all new behavior inside the existing Blade mastery runtime and standard skill systems. Reuse `BladeResourceComponent`, `BladeResourceService`, `SkillSystem`, behavior TUs, HUD/VFX systems, and existing doctest suites rather than inventing any parallel Sword Saint subsystem.

**Tech Stack:** C++20, EnTT ECS, doctest, CMake/MSVC, Raylib UI/VFX, existing `SkillRegistry + SkillContract + SkillBehaviorRegistry + SkillSystem` runtime.

## Implementation Update (2026-03-07)

**Status:** Tasks 1-4 are complete.

### Completed outcomes

- Added Sword Flow restart-window state in `BladeResourceComponent` and `BladeResourceService`, with a 3.0s armed window after a full 10-stack spend.
- Updated `FlowingThrust.cpp` so the next hit consumes the armed restart window for a one-time +2 Sword Flow restart bonus.
- Added threshold telegraphing to `SwordIntentWidget` (`5+`, `8+`, `10`) and mirrored those tiers into `SwordIntentVisualSystem` via `components::SwordIntentVisual::thresholdTier`.
- Updated full-flow `Rending Wave` and `Seven Star Slash` release behavior so both immediately reset `Flowing Thrust` cooldown to keep the Sword Saint loop from dead-dropping after a spend.
- Added a visible HUD cue in `PlayerHUD` for the armed restart window via `PlayerHUD::ResolveSwordFlowFeedbackText(...)`, prioritizing `Restart Ready <timer>` over the crit-proc `暴击剑流 +1` pulse.

### Verification completed so far

- `cmake --build build --config RelWithDebInfo --target NoMoreDayTests`
- `./bin/NoMoreDayTests.exe --test-case="[Unit] Sword Flow*restart*"`
- `./bin/NoMoreDayTests.exe --test-case="[Integration] Blade Mastery*restart*"`
- `./bin/NoMoreDayTests.exe --test-case="[Tech] Sword Saint*threshold*"`
- `./bin/NoMoreDayTests.exe --test-case="[Tech] PlayerHUD - Render Logic"`
- `./bin/NoMoreDayTests.exe --test-case="[Tech] SwordIntentVisual - Sword Flow crit proc pulse activates"`
- `./bin/NoMoreDayTests.exe --test-case="[Integration] Blade Mastery - full-flow release resets Flowing Thrust"`
- `./build.bat check`
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`

### Final verification outcome

- Full post-MVP Sword Saint polish regression passed after syncing the updated Sword Flow bridge expectation in `tests/integration/SkillSystemTests.cpp` and moving the `Seven Star Slash` skill id constant out of the `SevenStarSlashNodes` namespace so the gameplay node-audit only sees real node ids.

### Small deviations from the original task file list

- Task 3 rhythm polish landed partly in `src/game/systems/ui/PlayerHUD.cpp` and `tests/tech/UITests.cpp` instead of `SkillSystem.cpp` / `tests/functional/SkillBehaviors.cpp`, because the remaining gap after the cooldown reset work was player-facing restart-window feedback rather than another runtime rule.
- The actual release-polish regression stayed in `tests/integration/SkillSystemTests.cpp`; no extra functional test file changes were required once the loop behavior was covered there.

---

### Task 1: Full-Flow Release Restart Window

**Files:**
- Modify: `src/game/components/SkillDefs.hpp`
- Modify: `src/game/systems/skill/BladeResourceService.hpp`
- Modify: `src/game/systems/skill/BladeResourceService.cpp`
- Modify: `src/game/systems/skill/behaviors/FlowingThrust.cpp`
- Test: `tests/unit/BladeMasteryTests.cpp`
- Test: `tests/integration/SkillSystemTests.cpp`

**Step 1: Write the failing tests**

Add a unit test proving that a full 10-stack Sword Flow spend opens a short restart window on the blade resource runtime, and an integration test proving the next `Flowing Thrust` consumes that window to re-prime Sword Flow faster than a normal restart.

**Step 2: Run the targeted tests to verify RED**

Run:
`cmake --build build --config RelWithDebInfo --target NoMoreDayTests && ./bin/NoMoreDayTests.exe --test-case="[Unit] Sword Flow*restart*" && ./bin/NoMoreDayTests.exe --test-case="[Integration] Blade Mastery*restart*"`

Expected: fail because no restart-window state exists yet.

**Step 3: Write minimal implementation**

- Add restart-window state to `BladeResourceComponent` (timer/ready flag or equivalent).
- In `BladeResourceService::Consume(...)`, when Sword Flow spends all 10 stacks, arm the restart window.
- In `FlowingThrust.cpp`, when `DoHit(...)` sees an armed restart window, grant an additional one-time Sword Flow restart bonus and clear the window.

**Step 4: Run the targeted tests to verify GREEN**

Run the same command from Step 2 and confirm all targeted restart tests pass.

**Step 5: Commit**

`git add src/game/components/SkillDefs.hpp src/game/systems/skill/BladeResourceService.hpp src/game/systems/skill/BladeResourceService.cpp src/game/systems/skill/behaviors/FlowingThrust.cpp tests/unit/BladeMasteryTests.cpp tests/integration/SkillSystemTests.cpp && git commit -m "feat: add Sword Saint full-flow restart window"`

### Task 2: Threshold Telegraphing in HUD / Resource Presentation

**Files:**
- Modify: `src/game/systems/ui/PlayerHUD.cpp`
- Modify: `src/game/systems/ui/SwordIntentWidget.hpp`
- Modify: `src/game/systems/ui/SwordIntentWidget.cpp`
- Modify: `src/game/systems/vfx/SwordIntentVisualSystem.cpp`
- Test: `tests/tech/UITests.cpp`

**Step 1: Write the failing tests**

Add a tech test proving high Sword Flow threshold state (`5+`, `8+`, `10`) can be surfaced without crashing and that the dedicated visual system still reacts correctly when threshold telegraph fields are active.

**Step 2: Run the targeted tests to verify RED**

Run:
`cmake --build build --config RelWithDebInfo --target NoMoreDayTests && ./bin/NoMoreDayTests.exe --test-case="[Tech] Sword Saint*threshold*"`

Expected: fail because threshold-specific HUD/visual telegraphing is not present yet.

**Step 3: Write minimal implementation**

- Extend `SwordIntentWidget` or `PlayerHUD` to show threshold cues for Sword Flow (for example glow tier labels, accent rings, or stage text).
- Keep `SwordIntent` legacy presentation intact.
- If needed, piggyback on `SwordIntentVisualSystem` to expose a stronger full-stack visual marker distinct from the current crit-proc pulse.

**Step 4: Run the targeted tests to verify GREEN**

Run the same command from Step 2 and confirm the new tech tests pass.

**Step 5: Commit**

`git add src/game/systems/ui/PlayerHUD.cpp src/game/systems/ui/SwordIntentWidget.hpp src/game/systems/ui/SwordIntentWidget.cpp src/game/systems/vfx/SwordIntentVisualSystem.cpp tests/tech/UITests.cpp && git commit -m "feat: telegraph Sword Saint flow thresholds"`

### Task 3: Post-Spend Seven Star Slash / Rending Wave Rhythm Polish

**Files:**
- Modify: `src/game/systems/skill/behaviors/RendingWave.cpp`
- Modify: `src/game/systems/skill/behaviors/SevenStarSlash.cpp`
- Modify: `src/game/systems/skill/SkillSystem.cpp`
- Test: `tests/functional/SkillBehaviors.cpp`
- Test: `tests/integration/SkillSystemTests.cpp`

**Step 1: Write the failing tests**

Add one functional or integration test proving that a full-flow `Rending Wave` or `Seven Star Slash` spend leaves the Sword Saint loop in a clearer “release complete, restart ready” state instead of a dead drop.

**Step 2: Run the targeted tests to verify RED**

Run:
`cmake --build build --config RelWithDebInfo --target NoMoreDayTests && ./bin/NoMoreDayTests.exe --test-case="[Functional] Sword Saint*release*" && ./bin/NoMoreDayTests.exe --test-case="[Integration] Blade Mastery*release*"`

Expected: fail because the post-spend rhythm marker/polish does not exist yet.

**Step 3: Write minimal implementation**

- Keep the standard skill path.
- Add only the smallest extra state/feedback needed so a full-flow release naturally hands back into the restart window defined in Task 1.
- Avoid adding separate spend systems or special-case branches outside the existing behavior files and `BladeResourceService`.

**Step 4: Run the targeted tests to verify GREEN**

Run the same command from Step 2 and confirm all release tests pass.

**Step 5: Commit**

`git add src/game/systems/skill/behaviors/RendingWave.cpp src/game/systems/skill/behaviors/SevenStarSlash.cpp src/game/systems/skill/SkillSystem.cpp tests/functional/SkillBehaviors.cpp tests/integration/SkillSystemTests.cpp && git commit -m "feat: smooth Sword Saint post-spend rhythm"`

### Task 4: Full Regression + Plan Sync

**Files:**
- Modify: `conductor/docs/plans/2026-03-07-sword-saint-post-mvp-polish-implementation-plan.md`
- Modify: `conductor/docs/plans/2026-03-07-blade-ascendant-mastery-sword-saint-implementation-plan.md` (only if actual behavior/output changed materially)

**Step 1: Update plan/docs with actual implementation notes**

Record any small deviations, thresholds, timers, or trigger semantics that differ from the initial assumptions.

**Step 2: Run the full regression matrix**

Run:
- `./build.bat check`
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`

Expected: all pass.

**Step 3: Commit**

`git add conductor/docs/plans/2026-03-07-sword-saint-post-mvp-polish-implementation-plan.md conductor/docs/plans/2026-03-07-blade-ascendant-mastery-sword-saint-implementation-plan.md && git commit -m "docs: sync Sword Saint post-MVP polish plan"`

---

## Recommended execution order after this plan

1. Full-flow restart window
2. Threshold telegraphing
3. Post-spend rhythm polish
4. Full regression + doc sync

## Out of scope for this plan

- Heavenly Sword / Demon Blade implementation
- New mastery trees beyond Sword Saint
- Reworking the skill contract schema
- Replacing the current HUD widget architecture
- Large-scale combat feel/audio system overhaul
