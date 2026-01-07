# Combat System Logic Integrity Review Report

**Date:** 2026-01-06  
**Status:** Completed  
**Reviewer:** AI Assistant (Game Dev Specialist)

## Executive Summary
The combat system's core logic was audited for mathematical correctness and property consistency. Key fixes were applied to the damage pipeline and buff stacking mechanisms. Property snapshotting for projectiles was verified as robust.

## Key Findings & Fixes

### 1. Damage Pipeline & Resistances
- **Issue:** Resistances lacked clamping, potentially allowing >100% mitigation (healing from damage) or extreme vulnerability beyond intended design limits.
- **Fix:** Implemented hard clamping in `DamagePipeline::Calculate`.
    - Maximum Resistance: **75%** (0.75f)
    - Minimum Resistance: **-100%** (-1.0f)
- **Status:** Verified by `DamagePipelineTest.hpp`.

### 2. Buff System Integrity
- **Issue:** `AddOrRefresh` logic in `Buff.hpp` was simplistic. It only incremented stacks by 1 (ignoring the new application's stack count) and failed to update metadata (name, description) or modifiers if a stronger version of the same buff was applied.
- **Fix:** Refactored `AddOrRefresh` to:
    - Update `name`, `description`, and `modifiers`.
    - Correctly add stack counts from the new effect (clamped to `max_stacks`).
    - Reset duration to full.
- **Status:** Verified by `BuffSystemTest.hpp`.

### 3. Projectile Snapshotting
- **Verification:** Audited `ProjectileSystem.cpp` and `SkillSystem.cpp`. Confirmed that `CombatStats` are captured and stored on the projectile entity at the moment of creation.
- **Test Results:** `ProjectileSnapshottingTest.hpp` confirms that mid-flight projectiles maintain their original damage even if the player's attributes change significantly (e.g., buff expiry or equipment swap).

### 4. Tag System & Data Integrity
- **Observation:** Resolved a "Ghost Mapping" issue where stale object files caused incorrect tag bitmask associations (Physical vs Melee). Fixed via forced recompilation and verified with `Tag Registry System` tests.

## Identified Architectural Debt (Future Work)
- **Multiplicative "Increased" Modifiers:** The current `StatsSystem::GetStatWithTags` applies dynamic modifiers (from talents/tags) on top of already-baked global stats. This causes "Increased" modifiers from gear and talents to apply multiplicatively `(1+A)*(1+B)` instead of additively `(1+A+B)`. 
- **Recommendation:** Refactor `StatsSystem` to separate "Increased" and "More" modifiers into discrete accumulators before baking, or implement a "Decomposition" pass in `GetStatWithTags`.

## Final Stability Check
- **Final Integration Test:** Passed (Sword Cultivator Full Flow).
- **Regression Suite:** All core combat tests passing.
