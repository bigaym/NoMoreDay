# Technical Specification: Astrolabe Audit Fixes

## 1. Overview
This track addresses critical defects identified in the code audit of the Void Astrolabe system (2026-02-05). The focus is on restoring data integrity (topology), fixing logic loopholes (vow safety), and resolving UI/Rendering issues (missing connections, camera offset).

## 2. Technical Changes

### 2.1 Data Model: Topology Restoration
The `AstrolabeTalentNode` struct lost its connection data during the refactor. We must restore it to support the "Constellation" visual metaphor.

**File:** `src/game/data/TalentData.hpp`
```cpp
struct AstrolabeTalentNode {
    // ... existing fields ...
    
    // Topology: List of parent node IDs that must be unlocked first
    // This defines the lines drawn between stars.
    std::vector<uint32_t> prerequisites; 
};
```

**File:** `src/game/data/TalentLoader.cpp`
- Update `from_json` and `LoadProfessionTalents` to parse the `prerequisites` field array.

### 2.2 Logic: Vow Safety
The `AttributePipeline` currently applies stats from all allocated nodes without checking if they are effectively "sealed" by a Vow to another profession.

**File:** `src/game/systems/stats/AttributePipeline.cpp`
- In `AttributePipeline::Calculate`, inside the `AstrolabeComponent` loop:
```cpp
if (const auto *n = AstrolabeRegistry::Get().GetNode(nid)) {
    // FIX: Check Seal Status
    if (n->type == TalentNodeType::Core) {
        if (as->hasVow() && !as->isMainProfession(n->profession)) {
            continue; // Skip stats from sealed nodes
        }
    }
    // ... apply modifiers ...
}
```

### 2.3 Rendering: Star Connections
Implement the missing `DrawConnections` function to visualize the `prerequisites` graph.

**File:** `src/game/systems/ui/AstrolabeRenderer.cpp`
- Implement `DrawConnections(const TalentGraph& graph, const AstrolabeView& view, const AstrolabeComponent* comp)`.
- Logic:
  - Iterate all nodes.
  - For each node, iterate its `prerequisites`.
  - Find parent node position.
  - Draw line from Parent to Child.
  - **Style:**
    - Width: 3.0f
    - Color: `Fade(GOLD, 0.2f)` (dim) if locked.
    - Color: `GOLD` (bright) if both nodes are unlocked/activated.
    - Use `DrawLineEx` or a custom shader batch if needed (Raylib lines are fine for < 500 lines).

### 2.4 UI: Camera Offset Fix
The camera offset is not updated when the window is resized, causing the center to drift.

**File:** `src/game/systems/ui/UIAstrolabe.cpp`
- In `DrawInternal`, update `s_view.camera.offset` to `{ GetScreenWidth()/2.0f, GetScreenHeight()/2.0f }` every frame or upon resolution change detection.

### 2.5 Infrastructure: Path Constants
Remove hardcoded paths.

**File:** `src/game/components/Common.hpp` (or `Constants.hpp`)
- Add `constexpr const char* TALENT_DATA_PATH = "assets/data/profession_talents.json";`

**File:** `src/game/data/AstrolabeRegistry.cpp`
- Use the constant.

## 3. Acceptance Criteria
1.  **Topology**: JSON data with `prerequisites` loads correctly.
2.  **Visuals**: Lines appear between connected nodes.
3.  **Logic**: Stats from "Sealed" core nodes (if any points were somehow allocated) are NOT applied to the player.
4.  **UI**: Resizing the window keeps the galaxy centered.
5.  **Rendering**: No black bars on background when zoomed out/panning.
