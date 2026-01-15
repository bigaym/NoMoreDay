# Plan: Persistence System Implementation

## Objectives
Implement a robust, asynchronous, snapshot-based save system. Ensure `ItemFactory` supports deterministic restoration of items from DTOs without re-triggering RNG.

**Related Documents**:
- [Spec: Persistence System](SPEC.md)
- [Design: 存档与持久化系统](../../../设计文档/存档与持久化系统.md)

## Phase 1: Data Structures & DTOs
Define the data contracts that decouple the ECS from the file system.

- [ ] **Task 1.1: Define SerializedItem DTO**
  - Create `SerializedItem.hpp` in `src/game/data/`.
  - Implement `StatsSnapshot` and `SavedAffix` nested structs.
  - Implement `nlohmann::json` serialization functions (`to_json`, `from_json`).
- [ ] **Task 1.2: Refactor Affix Serialization**
  - Ensure `Affix` struct in `ItemStats.hpp` aligns with `SavedAffix`.
  - Verify existing `to_json` in `ItemStats.hpp` is sufficient or needs update for Snapshot mode (e.g., storing explicit values).
- [ ] **Task 1.3: Define CharacterSaveData DTO**
  - Create `SaveData.hpp` in `src/game/data/`.
  - Include Header info, Stats, Inventory DTOs, Skills, Astrolabe.

## Phase 2: Item Restoration Logic
Enable `ItemFactory` to bypass RNG and load data directly.

- [ ] **Task 2.1: Implement ItemFactory::restoreItem**
  - Add static method `entt::entity restoreItem(entt::registry&, const SerializedItem&);`.
  - Logic: Create entity -> Add `ItemComponent` -> Copy values from DTO -> Add `ItemStats` -> Copy Affixes.
  - **Critical**: Ensure no `rollRarity` or `rollAffixes` is called.
- [ ] **Task 2.2: Unit Test for Restoration**
  - Create `tests/TestPersistence.cpp`.
  - Test: Create random item -> Convert to DTO -> Destroy Entity -> Restore from DTO -> Assert Attributes Match Exactly.

## Phase 3: SaveManager & System Integration
Build the pipeline and hook it into the game loop.

- [ ] **Task 3.1: Implement SaveManager Class**
  - Create `src/engine/persistence/SaveManager.hpp/cpp`.
  - Implement `createSnapshot(registry) -> CharacterSaveData`.
  - Implement `saveToFileAsync(data, slotIndex)` using `tf::Taskflow`.
  - Implement `loadFromFile(slotIndex) -> CharacterSaveData`.
  - Implement `restoreSnapshot(registry, data)`.
- [ ] **Task 3.2: Integrate with PortalSystem**
  - In `PortalSystem.cpp`, trigger `SaveManager::saveCharacterAsync` when entering Town portal.
- [ ] **Task 3.3: Integrate with MainMenu**
  - Add "Continue" / "Load Game" buttons.
  - Call `SaveManager::loadCharacter` and transition to GameplayState.

## Phase 4: Verification & Polish
- [ ] **Task 4.1: Full Save/Load Cycle Test**
  - Play game -> Get loot -> Return to town -> Exit -> Re-launch -> Continue -> Verify loot is identical.
- [ ] **Task 4.2: Performance Profile**
  - Measure time taken for `createSnapshot` (Target: < 1ms).
  - Measure time taken for Async Write (Target: Non-blocking).
