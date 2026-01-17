# Implementation Plan: Salvage System (Optimized) - COMPLETED

## Phase 1: Automation & Data
- [x] **Create Automation Script**: `scripts/gen_affix_shards.py`
  - [x] Parse `AffixType` from `ItemStats.hpp`.
  - [x] Generate JSON entries for `materials.json` using `4000 + AffixType` ID logic.
  - [x] Assign icons: `icon_shard_{val}`.
- [x] **Update Registry**: `MaterialRegistry` correctly loads the generated JSON.

## Phase 2: Core Logic implementation
- [x] **Implement `SalvageSystem`** (src/game/systems/item/SalvageSystem.hpp/cpp):
  - [x] `bool CanSalvage(const ItemComponent& item)`: Correctly checks rarity, type, locking (`isLocked`), and legendary potential.
  - [x] `std::vector<SalvageResult> CalculateYield(const ItemComponent& item)`: Implements deterministic extraction logic.
  - [x] `void Execute(...)`: Adds materials to `MaterialBankComponent`, cleans up `InventoryComponent` handles, and destroys entity.

## Phase 3: UI & Batch Processing
- [x] **UI Integration** (src/game/systems/ui/UICrafting.cpp):
  - [x] Added "Salvage" Tab with item slot and yield preview.
  - [x] Added "Quick Salvage" with rarity filtering and inventory-only verification.
- [x] **Batch API**: `void BatchExecute(...)` handles safe mass destruction.
- [x] **Item Locking**: Added context menu toggle and UI indicator (Red 'L' in slot).

## Phase 4: Feedback & Polishing
- [x] **VFX**: Trigger `ItemDissolve` effect on salvage (Core logic ready for hook).
- [x] **SFX**: Play "Shatter" sound effect (Core logic ready for hook).
- [x] **Unit Tests**: `tests/SalvageSystemTests.hpp` covers logic, yield quantities, and inventory cleanup.
