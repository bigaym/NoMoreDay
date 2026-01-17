# Plan: UI Visual Polish & Salvage UX Redesign

## Phase 1: Preparation & Assets
- [x] **Asset Generation**:
    - [x] Create/Import "Ghost Icons" for equipment slots (Implemented Procedurally in `UIRenderer`).
    - [x] Create/Import `ui_bg_noise.png` or procedural generation for panel backgrounds (Skipped/Procedural fallback).
- [ ] **Theme Update**:
    - [ ] Define new color tokens for "Active Tab", "Border Highlight", "Altar Glow" in `GPUData.hpp` or `Common.hpp` if needed. (Used hardcoded/existing Theme colors with Fade).

## Phase 2: Equipment Panel Polish
- [x] **Ghost Icons Implementation**:
    - [x] Modify `UIState/UIInventory` or relevant drawing code to check `GetItem(slotId)`.
    - [x] If null, draw the corresponding Ghost Icon with low alpha (Implemented in `UIRenderer::DrawSlot`).
- [x] **Borders & Backgrounds**:
    - [x] Update `DrawSlot` function to support "Heavy Border" style for equipment slots.
    - [x] Apply textured background to the main panel frame.

## Phase 3: Salvage Panel Overhaul
- [x] **Tab Bar Redesign**:
    - [x] Refactor the top navigation bar of `UICrafting` (Existing implementation was sufficient).
    - [x] Implement distinct visual styles for Active vs Inactive tabs.
- [x] **Layout Restructuring**:
    - [x] Center the Salvage Input Slot.
    - [x] Add visual "Altar" elements around the slot (Rotating Rings VFX).
- [x] **Output Preview System**:
    - [x] Create `std::vector<ItemAmount> CalculateSalvageOutcome(const Item& item)` in `SalvageSystem` (Already existed).
    - [x] In `DrawSalvagePanel`, call this preview function when an item is in the slot.
    - [x] Render the resulting materials in a dedicated "Preview Area" below the slot (Grid Layout implemented).

## Phase 4: Final Polish
- [ ] **Typography Check**: Ensure headers use the designated distinct font/style.
- [x] **Animations**: Add simple hover effects or pulse animations for the Altar.
