# Specification: Persistence System (存档与持久化系统)

## 1. Overview
The Persistence System ensures player progress is securely saved and restored. To guarantee **long-term compatibility** and **data integrity** in a Roguelite environment where balance changes frequently, we adopt a **Snapshot-Based** strategy. Instead of saving random seeds and re-rolling items (which is fragile to version updates), we serialize the **final computed attributes** of all items and characters.

## 2. Technical Context & Constraints
*   **Thread Safety**: File I/O must not block the Render/Game thread. We use **Taskflow** for asynchronous background writing.
*   **ECS Safety**: `entt::entity` is a runtime ID and cannot be saved. All entity references (Inventory, Equipment) must be converted to **Data Transfer Objects (DTOs)**.
*   **Snapshot Strategy**:
    *   **Write**: Main Thread gathers state into a `CharacterSaveData` DTO (Deep Copy). Background thread writes DTO to JSON.
    *   **Read**: Background thread reads JSON to DTO. Main Thread reconstructs ECS entities from DTO.

## 3. Data Structures (DTO Layers)

### 3.1 SerializedItem (The Snapshot)
Captures the "frozen" state of an item, decoupling it from `ItemFactory`'s generation logic.

```cpp
struct SerializedItem {
    // Identity
    uint32_t itemId;           // Config ID from ItemFactory
    uint64_t instanceId;       // Unique ID for tracking (optional future proofing)
    int quantity;

    // Stat Snapshot (The "Real" values)
    struct StatsSnapshot {
        int level;
        Rarity rarity;
        float attack;          // Final Base Attack
        float defense;         // Final Base Defense
        EquipmentSlot slot;
        int forgingPotential;
        float value;           // Gold Value
    } stats;

    // Affix Snapshot (Explicit values)
    struct SavedAffix {
        AffixType type;
        int tier;
        float value;           // The exact rolled value
        bool isPrefix;
        bool isLegendary;      // For merged items
    };
    std::vector<SavedAffix> affixes;
    std::vector<SavedAffix> implicits;

    // Recursive Sockets
    std::vector<SerializedItem> socketedItems; 
};
```

### 3.2 CharacterSaveData (The Container)
```cpp
struct CharacterSaveData {
    // Header
    std::string name;
    std::string characterClass;
    int level;
    double playtime;
    int64_t timestamp;

    // Components
    PlayerStats stats;         // Base attributes (Str, Dex, etc.)
    Transform transform;       // X, Y, MapID
    
    // Inventory Containers
    std::vector<SerializedItem> inventory;
    std::vector<SerializedItem> equipment; // Mapped by slot index

    // Progression
    ActiveSkillsComponent skills; // Skill tree state
    AstrolabeComponent astrolabe; // Passive tree state
    QuestState quests;
};
```

## 4. Functional Requirements

### 4.1 Save Process (Town Only)
1.  **Trigger**: Player enters a Portal to Town, or uses "Save & Exit" in Town.
2.  **Snapshot (Main Thread)**:
    - `SaveManager::createSnapshot(registry)` is called.
    - Iterates `InventoryComponent` and `EquipmentComponent`.
    - Converts every Item Entity -> `SerializedItem` DTO.
    - Copies POD components (Stats, Skills).
    - Returns a `CharacterSaveData` object.
3.  **Serialize (Taskflow)**:
    - Background task converts `CharacterSaveData` -> JSON string.
    - Writes to `saves/temp/slot_0.tmp`.
    - Renames `.tmp` -> `slot_0.json` (Atomic Commit).
    - Updates `global.json` (Last Played Slot).

### 4.2 Load Process
1.  **Deserialize (Taskflow/Loading Screen)**:
    - Reads JSON -> `CharacterSaveData` DTO.
2.  **Restore (Main Thread)**:
    - `registry.clear()`.
    - `SaveManager::restoreFromSnapshot(registry, data)` is called.
    - Recreates Player Entity.
    - **Item Restoration**:
        - For each `SerializedItem` in inventory/equipment:
        - Call `ItemFactory::restoreItem(DTO)`.
        - This creates a new entity and **directly sets** `ItemComponent` fields (Attack, Defense, Affixes) using the DTO values, **skipping** any random generation logic.
    - **Recalculate**: Call `StatsSystem::update(0)` to re-apply all modifiers from the restored gear and skills.

## 5. Corner Cases & Safety
*   **Version Mismatch**: If `SerializedItem` lacks a new field added in V1.1, `from_json` should use a sensible default.
*   **Missing Config**: If `itemId` no longer exists in `ItemFactory` (item removed from game), the loader should:
    - Log a warning.
    - Skip the item (delete it) OR replace it with a generic "Corrupted Item" (to avoid crashing).
*   **Crash During Save**: Atomic Rename ensures the save file is never half-written. The old save remains valid until the new one is fully flushed.
