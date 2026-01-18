# Hybrid Barrier Implementation Plan

## Status: P0/P1/P2 COMPLETE ✅

## P0: Core Data Structure ✅
- [x] **Define `BarrierComponent`** in `src/game/components/Common.hpp`.
    - Fields: `last_damage_time` (护盾值存储在 `CombatStats.barrier` 中).
    - Add `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE`.
- [x] **Data Migration**: `CombatStats` 已包含护盾字段 (verified).

## P1: Logic Implementation ✅
- [x] **Update `StatsSystem::Recalculate`**:
    - Implement `INT -> BarrierRetention` formula (1 INT = +1% Retention).
    - Aggregate `MaxBarrier` and `BarrierRegen` from modifiers.
    - Auto-create `BarrierComponent` when entity has barrier capacity.
- [x] **Modify `RegenerationSystem`** (integrated, no separate BarrierSystem):
    - Implement ES Mode: Regen after `barrier_delay` seconds of no damage.
    - Implement Ward Mode: Decay when `barrier > max_barrier`.
    - Formula: `EffectiveDecay = barrier_decay / (1 + barrier_retention)`.
    - System already registered in `GameplayState` via RegenerationSystem.
- [x] **Update `CombatSystem::ApplyDamage`**:
    - Barrier absorbs damage before Health reduction.
    - Update `last_damage_time` on hit to reset regen delay.
    - Note: `DamageType::True/Chaos` bypass needs DamagePipeline Tag support (future work).

## P2: UI & Feedback ✅
- [x] **Update `PlayerHUD`**:
    - Draw cyan Barrier bar overlaying HP bar.
    - Add barrier text display "HP / MaxHP (+BarrierValue)".
    - Pulsing glow effect when barrier exceeds max_barrier (Ward overflow).
- [x] **Update `MonsterHealthBarSystem`**:
    - Draw cyan Barrier overlay on enemy health bars.
- [ ] **Visual Feedback** (Optional, Deferred):
    - Add shield hit sound/effect when barrier takes damage.

## P3: Testing (Not in Scope)
- [ ] **Unit Tests**: Test Regen delay, Decay rate, and Retention math.
- [ ] **Integration**: Verify in-game behavior with console commands or debug items.

---

## Implementation Notes (2026-01-18)

### Files Modified:
1. `src/game/components/Common.hpp` - Added `BarrierComponent`, `INT_TO_BARRIER_RETENTION` constant
2. `src/game/systems/combat/StatsSystem.cpp` - Barrier stats aggregation and INT scaling
3. `src/game/systems/combat/RegenerationSystem.hpp` - Barrier regen/decay logic
4. `src/game/systems/combat/CombatSystem.cpp` - Barrier damage absorption in `ApplyDamage`
5. `src/game/systems/ui/PlayerHUD.cpp` - Player barrier UI overlay
6. `src/game/systems/ui/MonsterHealthBarSystem.cpp` - Enemy barrier UI overlay

### UI Design Details:
- **Barrier Color**: `#66D9E8` (明亮青色) - RGB(102, 217, 232)
- **Glow Color**: `#40A0FF` (溢出发光) - RGB(64, 160, 255)
- **Text Format**: "HP / MaxHP (+Barrier)" when barrier > 0
- **Pulse Animation**: `sin(Time * 4) * 0.4 + 0.3` for overflow glow

### Known Limitations:
- Chaos/True damage bypass not yet implemented (requires DamageType propagation to ApplyDamage)
- Shield hit VFX/SFX deferred
