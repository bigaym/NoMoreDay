# Implementation Plan: Fix Dimensional Mosaic System

## Phase 1: Affix Registry & UI Polish
**Goal**: Ensure affixes are accurately described and rendered in the UI without glitches.

- [ ] **Task 1: Affix Registry Refactor**
  - Add `descriptionZh` to `MapAffixDefinition` in `MapAffixRegistry.hpp`.
  - Update `MapAffixRegistry::Initialize` to provide templates for all affix types.
  - Implement `MapAffixRegistry::GetDescriptionZh(type, value)`.
- [ ] **Task 2: Font & Character Fixes**
  - Add `0x2022` (•) and potentially `0x2605` (★) to `UISystem.cpp` font loading.
- [ ] **Task 3: Mosaic UI Upgrade**
  - Update `MosaicEditorState::RenderTooltip` and `RenderResonancePreview` to show detailed effects.
  - Implement dynamic tooltip height.
  - Fix ESC key behavior in `HandleInput`.

## Phase 2: Flow & Progression Logic
**Goal**: Smooth out the transition between Rift layers.

- [ ] **Task 4: Portal Progression Logic**
  - Modify `PortalSystem.cpp` to handle Rift progression.
  - Check `ActiveDimensionalState` depth and determine if editor needs to be opened or if we just move to the next layer.
- [ ] **Task 5: Depth Management**
  - Ensure `currentDepth` is correctly incremented and displayed on the Minimap.
  - Sync Minimap zone naming to "异界 - X层".

## Phase 3: Persistence & Scene Recovery
**Goal**: Enable returning to Rifts and safe saving.

- [ ] **Task 6: Serialization Expansion**
  - Include `MosaicGrid` (or a serializable representation of it) in `ActiveDimensionalState` persistence.
- [ ] **Task 7: Scene Restoration**
  - Fix `SceneManager` to correctly identify and restore a Dimensional Rift when returning from Town.
  - Verify that kills and modifiers persist after returning.

## Verification
- Run `tests/unit/MapAffixSystemTest.cpp` (if relevant or updated).
- Manual playtest:
  1. Open Mosaic Editor, check Tooltips.
  2. Generate Map, enter Rift.
  3. Clear Rift Layer 1, enter portal, check if it goes to Layer 2 directly.
  4. At Layer 2, teleport to Town, then return. Verify all stats are preserved.
