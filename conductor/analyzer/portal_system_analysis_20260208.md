# Portal System & Dimensional Rift Analysis Report

**Date:** 2026-02-08
**Subject:** Technical Audit of Portal System, Dimensional Rift Logic, and Return Mechanisms

## 1. System Overview (Architecture & Flow)

### 1.1 Core Components
*   **`PortalSystem`**: The central engine responsible for portal instantiation, collision detection (triggering), casting visuals (Town Portal), and the critical **Rift Progression (AdvanceRiftLayer)** logic.
*   **`ActiveDimensionalState`**: A global context singleton acting as the session state for the current run. It holds the Map Generation Seed, Active Affixes (modifiers), Current Depth, Resonance Results, and Fragment Snapshots.
*   **`MosaicEditorState`**: The UI state responsible for the "crafting" phase—placing fragments to generate the dungeon. It currently initializes a *new* run upon confirmation.
*   **`SceneManager`**: Handles the low-level scene transitions and stores "Origin Info" (Previous Biome/Level/Pos) to support the "Return Portal" functionality.

### 1.2 The Dimensional Loop Logic
1.  **Initiation**: 
    - Player interacts with `DimensionalGate` in Town.
    - Flows through `DimensionalLevelSelectState` -> `MosaicEditorState`.
    - **Initialization**: `ActiveDimensionalState` is created/reset with `Depth = 1`, `isActive = true`.
2.  **Progression**:
    - Player finds a `NextLevel` portal in the Rift.
    - **Trigger**: `PortalSystem::AdvanceRiftLayer` is called.
    - **Logic**: 
        - `currentDepth` increments.
        - `gridSnapshots` (Fragments) decay (`remainingLayers--`).
        - **Scaling**: Affix values multiply by `1.0 + (depth-1) * 0.1` (+10% per layer).
        - Scene transitions to the next generated level.
3.  **Completion**:
    - If `currentDepth > maxDepth` (Default: 3) OR all fragments decay to 0.
    - `isActive` set to `false`, `isCompleted` set to `true`.
    - Player is returned to Town.

---

## 2. "Depth 3" & Return Logic Analysis

### 2.1 Scenario A: Successful Completion (The Happy Path)
*   **Condition**: Player enters the `NextLevel` portal after clearing Depth 3.
*   **Logic Execution**: `AdvanceRiftLayer` detects completion criteria.
*   **Result**: The Rift session is cleanly closed (`isActive = false`). The player returns to Town. Interacting with the Dimensional Gate starts a fresh run, which is the intended behavior.

### 2.2 Scenario B: Mid-Run Return (The "Overwrite" Risk)
*   **Condition**: Player is at Depth 2 or 3 and uses a **Town Portal** to return to Town (e.g., to sell items), intending to resume.
*   **State**: `ActiveDimensionalState.isActive` remains **`true`**. The session is "Paused".
*   **The Critical Flaw**:
    - If the player ignores their own "Return Portal" and instead clicks the central **Dimensional Gate**:
    - The system pushes `DimensionalLevelSelectState`.
    - The system pushes `MosaicEditorState`, which runs `m_grid.Clear()`.
    - Upon clicking "Generate", `ConfirmAndGenerate` **unconditionally overwrites** the existing `ActiveDimensionalState`.
*   **Consequence**: **Immediate, silent loss of all progress.** The active high-tier Rift is erased, and a new Level 1 run begins.

---

## 3. Key Risks & Issues

### 🔴 Critical Risk: Progress Overwrite (Data Loss)
*   **Location**: `src/game/states/GameplayState.cpp` (Handling `PendingDimensionalGateTag`), `src/game/states/MosaicEditorState.cpp` (`ConfirmAndGenerate`).
*   **Issue**: Lack of checks for an existing, active session before allowing the start of a new one.
*   **Impact**: Severe user frustration due to accidental progress loss.

### 🔴 High Risk: Memory Safety (EnTT Violation)
*   **Location**: `src/game/systems/world/PortalSystem.cpp` -> `SpawnTownPortal`.
*   **Issue**: Modifying the entity registry (destroying entities) while iterating over a view of the same component type.
    ```cpp
    auto view = registry.view<PortalComponent>();
    for (auto entity : view) { // Iterator held here
        if (...) registry.destroy(entity); // INVALIDATES iterator!
    }
    ```
*   **Impact**: High probability of crashes or undefined behavior (UB) when casting Town Portals.

### 🟡 Medium Risk: Fragment Persistence Disconnect
*   **Location**: `MosaicEditorState`.
*   **Issue**: The UI always clears the grid on entry (`m_grid.Clear()`). Even though `PortalSystem` tracks fragment decay (`remainingLayers`), this data is never fed back into the UI if a player were to theoretically "continue" or "edit" an existing run. The "Durability" mechanic exists in the backend but is invisible/broken in the frontend flow.

### 🟡 Medium Risk: Architecture (Tight Coupling)
*   **Issue**: `PortalSystem` manages its own assets and rendering (Direct OpenGL/Shader calls).
*   **Impact**: Bypasses the resource management system, making debugging visual issues harder and complicating future rendering refactors.

### 🔵 Low Risk: Performance (Scalability)
*   **Issue**: `O(N * M)` collision detection every frame.
*   **Impact**: Negligible for current scale, but poor practice given the existence of a `SpatialHashGrid` designed for this exact purpose.

---

## 4. Recommendations

### 4.1 Fix "Progress Overwrite" (Priority: Critical)
**Strategy**: Implement a "Resume" check in `GameplayState`.
1.  Intercept `PendingDimensionalGateTag` processing.
2.  Check `registry.ctx().get<ActiveDimensionalState>().isActive`.
3.  If active:
    - **Option A (Simple)**: Teleport player directly to the active Rift (simulate clicking the Return Portal).
    - **Option B (UI)**: Show a dialog: "Resume current Rift (Depth X) or Start New?".

### 4.2 Fix EnTT Safety (Priority: High)
**Strategy**: Two-pass deletion.
```cpp
std::vector<entt::entity> toDestroy;
for (auto entity : view) {
    if (...) toDestroy.push_back(entity);
}
for (auto entity : toDestroy) registry.destroy(entity);
```

### 4.3 Refactoring
*   **Remove Dead Code**: Delete unused `targetEntranceId` string fields.
*   **Rendering**: Move `PortalSystem::Render` logic to `RenderSystem` or a dedicated `PortalRenderSystem` that respects `RenderContext`.

### 4.4 UI Feedback
*   Visualize "Remaining Layers" on Fragments in the Inventory/Tooltip to give weight to the durability mechanic.
