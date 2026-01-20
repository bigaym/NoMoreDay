# Walkthrough - Affix Timer Fix

## Objective
Resolve the critical bug where multiple active monster affixes (e.g., Molten + Shielding) were conflicting by sharing the same `timer1` and `timer2` variables in `MonsterAffixComponent`.

## Changes

### 1. `MonsterAffixComponent` Refactoring
**File:** `src/game/data/MonsterAffixRegistry.hpp`

- Replaced `float timer1` and `float timer2` with `std::array<float, 4> timers`.
- Added explicit state fields for complex mechanics:
  ```cpp
  bool mirrorTriggered = false;       // MirrorImage HP threshold flag
  float mirrorCooldown = 0.0f;        // MirrorImage independent cooldown
  float voidZoneNextSpawnTime = 0.0f; // Independent void zone timer
  ```

### 2. `MonsterAffixSystem` Update
**File:** `src/game/systems/combat/MonsterAffixSystem.hpp`

- Updated `Update` loop to iterate through affixes with an index:
  ```cpp
  for (size_t i = 0; i < affix.affixes.size(); ++i) {
      affix.timers[i] += dt;
      // ... switch(affixType) passing i to Process functions
  }
  ```
- Updated all `Process...` functions to use `affix.timers[affixIdx]` instead of `timer1`/`timer2`.
- Updated `ProcessTeleporter` to use `timers[affixIdx]`.
- Updated `OnEnemyTakeDamage` (Mirror Image logic) to use `mirrorTriggered` and `mirrorCooldown` instead of shared timers.

### 3. Testing
**Files:**
- `tests/integration/NemesisScalingTest.hpp`: Updated to verify independent timer operation for Molten (index 0) and Shielding (index 1).
- `tests/unit/MonsterAffixTests.hpp`: Updated to use the new `mirrorCooldown` field.

**Verification:**
- `Nemesis Scaling and Phase Shield`: **PASSED**
- `Monster Affix: Mirror Image Logic Test`: **PASSED**
- Full Test Suite: **PASSED** (56/56 tests)

## Verification Results
The shared timer conflict is resolved. Monsters can now support up to 4 concurrent active affixes without timer interference.
