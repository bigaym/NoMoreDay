# Seven Star Slash Full Tree Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Implement the full Sword Saint `七星斩` 26-node specialization tree in runtime code while keeping specialization UI behavior identical to other active skills.

**Architecture:** Keep the UI generic by continuing to drive the tree from `SkillRegistry` and the new `mastery_skill_trees.json` overlay. Implement `七星斩` behavior in three layers: cast-time slash resolution in `SevenStarSlash.cpp`, short-duration post-cast windows via `ActiveEffectsComponent` / a small runtime component, and cross-skill consumers in the affected sword skills (`流云刺` / `裂空斩` / `御剑·回旋` / `绝影绝剑`).

**Tech Stack:** C++20, EnTT ECS, JSON data tables, doctest/CTest, existing `SkillRegistry` + `SkillSystem` specialization runtime.

---

### Task 1: Lock down tree-loading and UI invariants

**Files:**
- Verify: `src/game/data/SkillRegistry.cpp`
- Verify: `src/game/systems/ui/UISkillHub.cpp`
- Verify: `src/game/systems/ui/UISkillTalentTree.cpp`
- Test: `tests/integration/SkillContractRegistryTests.cpp`

**Step 1:** Keep `mastery_skill_trees.json` as the source of the skill-10 tree/contract.

**Step 2:** Add/retain tests proving skill 10 loads a 26-node tree and contract metadata from the overlay.

**Step 3:** Do not add any skill-10-specific UI branch unless a real rendering gap appears; the current tree UI should stay registry-driven.

### Task 2: Define SevenStarSlash runtime state model

**Files:**
- Modify: `src/game/components/PlayerState.hpp`
- Modify: `src/game/systems/skill/behaviors/SevenStarSlash.cpp`
- Possibly modify: `src/game/systems/skill/SkillSystem.cpp`
- Test: `tests/functional/SkillBehaviors.cpp`

**Step 1:** Add a small runtime component for post-cast windows that cannot live only inside buffs, e.g.:
- pending `七曜势` stacks and expiry
- pending next-movement override from `返星入步`
- source-cast bookkeeping / per-window trigger caps

**Step 2:** Use `ActiveEffectsComponent` for simple timed stat buffs:
- `御剑追影` crit / dodge / move speed
- `返星入步` post-movement damage reduction

### Task 3: Rebuild cast-time slash resolution for the 26-node tree

**Files:**
- Modify: `src/game/systems/skill/behaviors/SevenStarSlash.cpp`
- Test: `tests/functional/SkillBehaviors.cpp`

**Step 1:** Replace old node constants/state with the 26-node tree IDs.

**Step 2:** Add deterministic target selection and slash sequencing support for:
- shared/base nodes `1000-1003`
- A branch `1004-1008`
- D branch transmuters `1021/1022` and shape-followups `1023/1024`

**Step 3:** Track per-hit results so final-hit logic can use:
- single-target execute scaling
- same-target streaks
- isolated-target checks
- per-hit crit detection
- sword-scar explosion triggers

### Task 4: Implement B branch loop windows and cross-skill consumers

**Files:**
- Modify: `src/game/systems/skill/behaviors/SevenStarSlash.cpp`
- Modify: `src/game/systems/skill/behaviors/FlowingThrust.cpp`
- Modify: `src/game/systems/skill/behaviors/RendingWave.cpp`
- Modify: `src/game/systems/skill/behaviors/BladeBoomerang.cpp`
- Modify: `src/game/systems/skill/behaviors/PhantomFlash.cpp`
- Test: `tests/functional/SkillBehaviors.cpp`

**Step 1:** In `七星斩`, implement `1009/1011/1012/1013/1014`:
- sword-flow refunds
- low-effect follow-up slash
- movement cooldown recovery
- `七曜势` stack generation
- empowered-skill refund cap

**Step 2:** In the 4 affected sword skills, consume pending `七曜势` / movement overrides on the next valid cast.

### Task 5: Implement C branch landing / protection behavior

**Files:**
- Modify: `src/game/systems/skill/behaviors/SevenStarSlash.cpp`
- Possibly modify: `src/game/systems/skill/SkillSystem.cpp`
- Test: `tests/functional/SkillBehaviors.cpp`

**Step 1:** Implement `1015/1016/1017/1018/1019/1025`:
- invulnerability tolerance / control resistance
- backstab or safe-side landing
- Sword Step synergy buffs
- Dex-based ward with life-percent cap
- missing-life recovery on multi-hit packs
- immediate Sword Step refresh + next movement override + short DR window

### Task 6: Update tests in TDD slices

**Files:**
- Modify: `tests/functional/SkillBehaviors.cpp`
- Modify: `tests/unit/SkillBehaviorGuardTests.cpp`
- Modify: `tests/integration/GameplaySystems.cpp`

**Step 1:** Add small failing tests in slices instead of one giant end-to-end case.

**Step 2:** Recommended first slices:
- A-branch single-target execute path
- D-branch `天枢轮斩` / `星坠` shape rewrite
- B-branch `七曜势` next-skill consumption
- C-branch `返星入步` movement override

### Task 7: Verify

**Files:**
- Verify: `assets/data/mastery_skill_trees.json`
- Verify: `src/game/systems/skill/behaviors/SevenStarSlash.cpp`

**Step 1:** Run targeted tests during each slice.

**Step 2:** Run final verification:
- `./build.bat`
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`

**Step 3:** If behavior tests are split under another label, run those focused tests too.
