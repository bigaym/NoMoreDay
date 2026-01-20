# Spec: Monster Affix Timer Refactoring

## 1. Problem Statement
The current `MonsterAffixComponent` uses shared timers (`timer1`, `timer2`) for all affixes attached to an entity. When an entity has multiple active affixes with different tick intervals (e.g., `Molten` at 0.5s and `Shielding` at 3.0s), the high-frequency affix (Molten) constantly resets the shared timer, preventing lower-frequency affixes (Shielding) from ever triggering.

## 2. Proposed Solution
Decouple timers by providing a dedicated timer for each affix slot. Since an entity can have a maximum of 4 affixes, we will use a fixed-size array of timers.

### 2.1 Component Changes (`MonsterAffixComponent`)
- **Remove**: `float timer1`, `float timer2`.
- **Add**: `std::array<float, 4> timers = {0.0f, 0.0f, 0.0f, 0.0f}`.
- **Maintain**: `voidZoneNextSpawnTime`, `isBerserk` (These are specific states, not general tick timers, though they could eventually be refactored too).

### 2.2 System Changes (`MonsterAffixSystem`)
- **Update Loop**:
  ```cpp
  for (size_t i = 0; i < affix.affixes.size(); ++i) {
      auto affixType = affix.affixes[i];
      // Pass 'i' to process functions
      switch (affixType) {
          case MonsterAffixType::Molten:
              ProcessMolten(registry, entity, pos, affix, i, dt, tier);
              break;
          // ...
      }
  }
  ```
- **Process Functions**: Update signatures to accept `size_t affixIdx` and use `affix.timers[affixIdx]`.

## 3. Impact Analysis
- **Performance**: Negligible. Array access is extremely fast.
- **Memory**: `MonsterAffixComponent` size increases slightly (8 bytes -> 16 bytes for timers), but still fits well within cache lines.
- **Persistence**: No changes needed to JSON serialization if we are only persisting the `affixes` vector (runtime timers are transient).

## 4. Verification Plan
- **Unit Test**: Enhance `NemesisScalingTest` or create a new test where a Nemesis has both `Molten` and `Shielding`.
- **Requirement**: Verify that both the Molten trail spawns AND the Shielding invulnerability/link triggers.
