# Monster System Refinement Plan

This plan addresses the gaps identified in the Monster and AI system audit.

## Phase 1: Registry & Data Refactoring (Foundation)
*   [x] **Task 1.1: Refactor `MonsterAffixRegistry`**
    *   Add `static const std::unordered_map<std::string_view, MonsterAffixType> kNameToType`.
    *   Add `GetTypeFromName` static helper.
    *   Move constants from `MonsterAffixSystem` (MOLTEN_TICK_INTERVAL, etc.) to `MonsterAffixDef` (Implemented as `MonsterAffixRegistry::Params`).
*   [x] **Task 1.2: Cleanup `NemesisGenerator`**
    *   Replace `if-else` chain with `MonsterAffixRegistry::GetTypeFromName`.
    *   Standardize affix naming (e.g., ensure "Mirror Image" vs "MirrorImage" consistency).

## Phase 2: AI Logic & Consistency (Behavior)
*   [x] **Task 2.1: Fix Nemesis persistence**
    *   Modify `AISystem::update` to exclude `NemesisTag` from `Dormancy` checks.
    *   Adjust `Leashing` logic to ensure Nemesis doesn't reset position unless explicitly intended.
*   [x] **Task 2.2: Implement `Homing` mechanism**
    *   Update `ProjectileSystem` to handle `Homing` tag.
    *   Trigger `Homing` if the source entity has the `Accurate` affix.

## Phase 3: Adaptive Evolution & New Mechanics (Expansion)
*   [x] **Task 3.1: Adaptive Evolution System**
    *   Implement `GetScaledValue(Type, Tier)` in `MonsterAffixRegistry`.
    *   Update `MonsterAffixSystem` to use scaled values instead of hardcoded constants.
*   [x] **Task 3.2: Implement `Burst Counter` (Phase Shield)**
    *   Add `PhaseShieldComponent`.
    *   Implement logic in `MonsterAffixSystem` or `CombatSystem` to trigger invulnerability on burst damage.

## Phase 4: Verification & Polish (Quality)
*   [x] **Task 4.1: Automated Stress Test**
    *   Create a test case with a Tier 10 Nemesis to verify scaling stability.
*   [x] **Task 4.2: Audit ECS Safety**
    *   Refactor `ProcessShielding` to use a deferred command buffer or collect-then-process to avoid iterator invalidation risks.

## Phase 5: Documentation & Memory
*   [x] Update `设计文档` with final implementation details.
*   [x] Record key architectural changes to `MEMORY`.