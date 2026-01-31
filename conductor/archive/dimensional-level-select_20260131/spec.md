# Technical Specification: Dimensional Level Selection & Fragment Check

## 1. Overview
Enhances the Dimensional Map system by introducing a **Map Level Selection** interface and a **Default Fragment Failsafe**. This ensures players can customize the difficulty of their run within reasonable bounds and prevents the "stuck" state where a player has no fragments to start a run.

## 2. Technical Stack
-   **Engine**: Raylib (UI Rendering)
-   **Architecture**: State Pattern (`IState`), ECS (`EnTT`)
-   **Systems**: `GameplayState` (Trigger), `PortalSystem` (Tagging), `InventorySystem` (Item Management)

## 3. Data Model & Logic

### 3.1 New State: `DimensionalLevelSelectState`
A new game state inserted between the Town Portal trigger and the Mosaic Editor.

```cpp
class DimensionalLevelSelectState : public IState {
public:
    // ... Constructor/Destructor ...
    void OnEnter() override;
    void OnRender() override;
    bool OnUpdate(float dt) override;

private:
    int m_minLevel = 1;
    int m_maxLevel = 100;
    int m_selectedLevel = 1;
    
    // UI Constants
    const int ITEM_HEIGHT = 40;
    const int VISIBLE_ITEMS = 10;
    float m_scrollOffset = 0.0f;
    
    void RenderList();
    void RenderButtons();
    void ConfirmSelection();
};
```

### 3.2 Logic Flow
1.  **Trigger**: Player interacts with `DimensionalGate` portal.
2.  **Tag**: `PortalSystem` adds `PendingDimensionalGateTag` to player.
3.  **Handler (GameplayState)**:
    *   **Fragment Check**: Iterate player inventory.
        *   IF `count(MapFragmentComponent) == 0`:
            *   **Generate**: Create "Empty Fragment" (Common, No Element, No Affixes).
            *   **Notify**: Log/UI Message "Granted Basic Dimensional Fragment".
    *   **State Push**: Push `DimensionalLevelSelectState`.
4.  **Selection (DimensionalLevelSelectState)**:
    *   **Range**: `[1, min(PlayerLevel + 10, 100)]`.
    *   **Confirm**: 
        *   Get/Create `ActiveDimensionalState` in context.
        *   Set `ActiveDimensionalState.selectedBaseLevel = m_selectedLevel`.
        *   Push `MosaicEditorState`.
        *   Pop `DimensionalLevelSelectState`.

### 3.3 Default Fragment Definition
If generation is required, the fragment shall have:
```cpp
// Component Values
MapFragmentComponent {
    type = FragmentType::Terrain,
    element = FragmentElement::None,
    enemyDensityMod = 1.0f,
    dropRateMod = 1.0f,
    monsterLevelMod = 0,
    rarity = Rarity::Common,
    remainingLayers = 3
}
ItemComponent {
    name = "Empty Dimensional Fragment",
    rarity = Rarity::Common
}
```

### 3.4 Integration
-   **MosaicEditorState**: Must NOT reset `selectedBaseLevel` to 1. It should read the existing value from `ActiveDimensionalState`.

## 4. UI Design
-   **Style**: Dark overlay (similar to `MosaicEditorState`).
-   **Layout**:
    *   **Title**: "Select Rift Level" (Top Center)
    *   **List**: Central scrollable list of integers.
        *   Highlight current selection.
        *   Show "Recommended" or color code based on Player Level.
    *   **Buttons**: "Confirm" (Bottom Right), "Cancel" (Bottom Left / ESC).

## 5. Persistence
-   `ActiveDimensionalState` is already persistent. `selectedBaseLevel` is already a field in it. No schema changes required.

## 6. Acceptance Criteria
-   [ ] Entering portal with 0 fragments grants 1 empty fragment.
-   [ ] UI allows selecting levels from 1 to (PlayerLevel + 10).
-   [ ] Max level is capped at 100.
-   [ ] Scrollbar works for long lists.
-   [ ] Confirming opens Mosaic Editor.
-   [ ] Mosaic Editor generates map with the *selected* level (verified via logs/gameplay).
-   [ ] ESC closes the menu and returns to Town.
