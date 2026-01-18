# Monster Affix Implementation Plan

## P0: Core Infrastructure
- [ ] **Define Components**:
    - Add `MonsterAffixComponent` and `AffixStateComponent` to `EnemyComponent.hpp`.
    - Create `AffixRegistry` class/namespace to hold affix definitions (ID to Name/Type mapping).
- [ ] **Data Loading**:
    - Create `assets/data/affixes.json` to define simple stat-based affixes.
    - Implement loading logic in `ItemFactory` or new `AffixManager`.

## P1: Stat-Based Affixes (The "Easy" Ones)
- [ ] **Spawn Logic in `EnemySpawnSystem`**:
    - Add logic to roll Rarity (Magic/Rare).
    - Roll random affixes based on rarity.
    - Apply `StatModifier`s from the affix to the enemy's `CombatStats`.
- [ ] **Verify**:
    - Spawn an enemy with "Fast", check if it actually moves/attacks faster.

## P2: Mechanic Affixes (Logic Intesive)
- [ ] **Create `MonsterAffixSystem`**:
    - Implement `Update` loop for time-based effects.
- [ ] **Implement Molten**:
    - Add `Molten` logic in `MonsterAffixSystem::Update`.
    - Create `HazardComponent` for the fire patch.
- [ ] **Implement Teleporter**:
    - Add teleport logic (check distance -> move).
- [ ] **Implement Nullifier**:
    - Hook into `DamagePipeline` or `CombatSystem` to trigger "OnHit" affix effects.

## P3: Visuals & Polish
- [ ] **Shader Integration**:
    - Update `SpriteRenderer` or generic shader uniform to support "Outline Color".
    - Pass rarity color to shader.
- [ ] **UI Updates**:
    - Show affix list in the monster's health bar UI (Nameplate).

## P4: Advanced AI (Optional/Later)
- [ ] **Shielding AI**:
    - Requires logic to find allies.
    - Implement `SupportBehavior` in generic AI if not present.
