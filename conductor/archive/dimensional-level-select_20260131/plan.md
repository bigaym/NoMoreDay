# Implementation Plan: Dimensional Level Selection

## Phase 1: Core Logic & Failsafe
- [x] **Task 1.1: Fragment Failsafe Logic**
    -   **File**: `src/game/states/GameplayState.cpp`
    -   **Action**: In the `PendingDimensionalGateTag` handler:
        -   Scan `InventoryComponent` for any entity with `MapFragmentComponent`.
        -   If none found: Use `ItemFactory` (or manual creation) to spawn a default fragment into inventory.
        -   Add log message.

## Phase 2: UI State Implementation
- [x] **Task 2.1: Create DimensionalLevelSelectState**
    -   **Files**: `src/game/states/DimensionalLevelSelectState.hpp`, `.cpp`
    -   **Logic**: Implement `IState` interface.
    -   **OnEnter**: Calculate `m_maxLevel` based on `PlayerStats`.
    -   **Rendering**: Draw background, title, and level list using `Raylib`.
    -   **Input**: Handle scrolling, selection click, and confirm/cancel.

- [x] **Task 2.2: Register State**
    -   **File**: `src/app/Game.cpp` (or wherever states are managed/included if necessary, usually just including headers is enough for use in `GameplayState`).
    -   **Verify**: Ensure build system picks up new files (CMake).

## Phase 3: Integration & Wiring
- [x] **Task 3.1: Wire GameplayState**
    -   **File**: `src/game/states/GameplayState.cpp`
    -   **Action**: Change `PendingDimensionalGateTag` handler to push `DimensionalLevelSelectState` instead of (or before) `MosaicEditorState`.
    -   *Correction*: The flow is `Gameplay` -> `LevelSelect` -> `Mosaic`. So Gameplay pushes `LevelSelect`. `LevelSelect` pushes `Mosaic`.

- [x] **Task 3.2: Update MosaicEditorState**
    -   **File**: `src/game/states/MosaicEditorState.cpp`
    -   **Action**: in `ConfirmAndGenerate`, REMOVE the line that hardcodes `worldState.selectedBaseLevel = 1;`. Ensure it respects the value set by `LevelSelect`.

## Phase 4: Verification
- [x] **Task 4.1: Functional Test**
    -   Empty inventory -> Trigger Portal -> Check Inventory for item.
    -   Select Level 5 -> Generate -> Check Logs for "Base Level: 5".
    -   Select Level > Player+10 (Should be impossible).