# IMPL PLAN: Map Affix & Persistence System

## Phase 1: The Registry & Data Structures (The Foundation)
**Goal**: Implement the robust system logic for defining, categorized, and managing affixes before connecting them to the game loop.

- [x] **Task 1.1**: Define Core Data Structures in `src/game/data/MapAffix.hpp`.
    -   Implement `enum class MapAffixType` (Result of Step 38 cleanup).
    -   Implement `struct MapAffix` with `category` and `tier` fields.
    -   **Validation**: Ensure no circular dependencies with existing headers.
- [x] **Task 1.2**: Implement `MapAffixRegistry` (Static Data).
    -   Create `src/game/systems/MapAffixRegistry.hpp/cpp`.
    -   Function: `GetAffixDef(MapAffixType)` returning name, tier ranges, and Difficulty Weight.
    -   Function: `GetAvailableAffixes(MapAffixCategory)` for random generation.
- [x] **Task 1.3**: Implement `MapAffixCalculator` (The Math).
    -   Create `src/game/systems/MapAffixCalculator.hpp/cpp`.
    -   Function: `CalculateDifficultyScore(const vector<MapAffix>&)` -> int.
    -   Function: `CalculateRewards(int ds)` -> `pair<float, float>` (Rarity, Quantity).
    -   **Algorithm**: Implement the Logarithmic Quantity formula: `50% * log2(1 + DS/40)`.

## Phase 2: The Persistence Layer (The State)
**Goal**: Create the global storage that survives scene transitions.

- [x] **Task 2.1**: Create `src/game/components/WorldState.hpp`.
    -   Define `struct ActiveDimensionalState`.
    -   Members: `seed`, `difficultyScore`, `calculatedRarity`, `calculatedQuantity`, `activeAffixes`.
- [x] **Task 2.2**: Register Singleton in `Game.cpp`.
    -   Ensure `entt::registry` holds a persistent context for this state.
- [x] **Task 2.3**: Update `MosaicEditor` to writing to State.
    -   When clicking "Open Rift", calculate DS and Rewards immediately using `MapAffixCalculator`.
    -   Store the result in `ActiveDimensionalState`.

## Phase 3: Game Loop Integration (The Application)
**Goal**: Make the numbers actually do something in the game.

- [x] **Task 3.1**: Hook into `EnemySpawnSystem`.
    -   Read `ActiveDimensionalState`.
    -   Apply `MonsterDensity` to spawn counts.
    -   Apply `Enemy_*` buffs to `CombatStats` (HP, Damage, Armor, Barrier).
- [x] **Task 3.2**: Hook into `LootSystem`.
    -   Apply `calculatedRarity` to `ItemGenerationContext`.
    -   **Constraint**: Implement the LP Probability Scaling formula ($P \times (1 + Rarity/10)$).
- [x] **Task 3.3**: Hook into `MapSystem` (Terrain).
    -   Reuse `seed` from state if returning to same depth.

## Phase 4: UI & Feedback (The Visible)
**Goal**: Let players see what they are getting into.

- [x] **Task 4.1**: Create `MosaicAffixPanel` in `MosaicEditor`.
    -   Right-side panel showing:
        -   List of Red Debuffs (with Tiers).
        -   "Total Difficulty: [Score]"
        -   "Item Rarity: +[X]%"
        -   "Item Quantity: +[Y]%"
- [x] **Task 4.2**: In-Game Overlay (Optional/Tab menu).
    -   Show active modifiers when pressing Tab.

## Phase 5: Verification & Tuning
- [x] **Task 5.1**: Unit Test `MapAffixCalculator`.
    -   Verify DS sums match user expectation.
    -   Verify Log curve for Quantity behaves correctly at extreme values (DS=1000).
- [x] **Task 5.2**: Integration Test (The Boss Run).
    -   Create a T10 map.
    -   Verify Boss HP is massive (`Enemy_ExtraHealth`).
    -   Verify Loot explosion corresponds to Quantity bonus.
