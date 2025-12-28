# Track Plan: Loot, Experience, and Leveling Systems

## Phase 1: Experience & Leveling Core
- [x] Task: Update `PlayerState.hpp` and `Stats.hpp` with new progression components (`PlayerLevel`, `experience_gain_mult`) (294438b)
- [x] Task: Write tests for XP requirement scaling and level-up logic (4ac06fa)
- [x] Task: Implement `ProgressionSystem` to handle XP accumulation and automatic stat growth (1707d86)
- [x] Task: Implement point allocation logic (5 Attribute, 1 Skill) (1707d86)
- [x] Task: Conductor - User Manual Verification 'Experience & Leveling Core' (Protocol in workflow.md) (706a2ae)

## Phase 2: Combat Integration (XP Rewards)
- [x] Task: Write tests for level-difference XP scaling (diminishing returns) (4ac06fa)
- [x] Task: Implement XP awarding logic in `CombatSystem` or a dedicated `XPSystem` triggered by `OnKill` (e52a282)
- [x] Task: Verify XP gain modifiers from `CombatStats` are correctly applied (4ac06fa)
- [x] Task: Conductor - User Manual Verification 'Combat Integration' (Protocol in workflow.md) (706a2ae)

## Phase 3: Loot & Drop Infrastructure
- [x] Task: Define `LootPool` and `DropTable` structures in `src/components/ItemComponent.hpp` (706a2ae)
- [x] Task: Write tests for the "Drop Function" including Magic Find (Quantity & Rarity) (706a2ae)
- [x] Task: Implement the global loot pool and hybrid boss table logic (706a2ae)
- [x] Task: Implement `DropSystem::Calculate` to generate item/gold rewards on entity death (706a2ae)
- [x] Task: Conductor - User Manual Verification 'Loot & Drop Infrastructure' (Protocol in workflow.md) (706a2ae)

## Phase 4: Performance & Persistence
- [x] Task: Benchmark the `DropSystem` with 1,000+ simultaneous entity deaths (e.g., AoE clear) (706a2ae)
- [x] Task: Validate zero allocations in the `DropSystem` hot path (706a2ae)
- [x] Task: Conductor - User Manual Verification 'Performance & Persistence' (Protocol in workflow.md) (706a2ae)
