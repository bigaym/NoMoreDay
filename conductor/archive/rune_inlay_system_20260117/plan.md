# Plan: Rune Inlay System

The goal is to implement a robust Rune Inlay system where players can socket runes into equipment (max 3 slots) to trigger Rune Words, upgrading base items into Legendary uniquely named items.

## Tasks

### 1. UI Interaction (Socketing)
- [x] **Drag & Drop Logic**: Update `UIInventory::Draw` to handle dragging a Rune item onto an Equipment item (in inventory or equipment slot).
    - [x] Detect if dragged item is a Rune.
    - [x] Detect if target item has available sockets.
    - [x] On drop, call `CraftingSystem::socketRune`.
    - [x] Prevent swapping if it's a socket action.

### 2. Visual Feedback
- [x] **Socket Indicators**: Update `UIRenderer::DrawSlot` to verify if an item has sockets.
    - [x] Draw small circles (e.g., bottom-right or overlay) to represent sockets.
    - [x] Distinguish between Empty (Dark/Gray) and Filled (Gold/Rune Color) sockets.
- [x] **Tooltip**: Ensure tooltip clearly shows socket status (already partially there, verify styling).

### 3. Data & Logic Verification
- [x] **Max 3 Sockets**: Ensure item generation or `ItemFactory` respects the 3-socket limit for relevant items.
- [x] **Runeword Trigger**: Verify `CraftingSystem::socketRune` correctly calls `RunewordSystem::checkForRuneword` (Already looks like it does).
- [x] **Unsocketing**: Simplified. Currently standard D2-like behavior (runes are permanent unless cleansed/cleared via future expansion).

### 5. Audit & Polish (Completed on 2026-01-17)
- [x] **Attribute Refresh**: Fixed missing `StatsDirty` notification in `UIInventory` and `UICrafting` to ensure stats update in real-time.
- [x] **Data Safety**: Verified `ItemFactory` socket limits and `CraftingSystem` logic safety.

## Results
- **Implementation**: Full drag-and-drop socketing support with stack splitting.
- **Visuals**: Dynamic socket indicators in slots and comprehensive tooltips.
- **Runewords**: Fully functional activation logic that promotes base items to Legendary rarity.
- **Performance**: Integrated with the StatsDirty system for real-time attribute synchronization without performance overhead.
