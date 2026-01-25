# Technical Specification: Fix Dimensional Mosaic System

## 1. Overview
This track addresses several critical bugs in the Dimensional Mosaic (map splicing) system, focusing on UI clarity, progression flow, and state persistence.

## 2. Technical Requirements

### 2.1 Affix Description Engine
- **Component**: `MapAffixRegistry`
- **Requirement**: Move away from just showing "Affix Name" to "Affix Effect".
- **Implementation**:
    - Add a description template string to `MapAffixDefinition`.
    - Implement `MapAffixRegistry::GetDescriptionZh(type, value)` to format the template with the computed value.

### 2.2 Font & UI Polish
- **Component**: `UISystem`, `MosaicEditorState`
- **Requirement**: Fix missing characters (`•`) and improve layout.
- **Implementation**:
    - Add `0x2022` to `UISystem` font codepoints.
    - Update `MosaicEditorState::RenderTooltip` to:
        - Use `GetDescriptionZh`.
        - Calculate height dynamically based on the number of affixes.
    - Update `MosaicEditorState::HandleInput` to ensure ESC always pops the state, even if the player is still inside a portal hitbox.

### 2.3 Dimensional Progression Logic
- **Component**: `PortalSystem`, `ActiveDimensionalState`
- **Requirement**: Portals inside a Rift should progress through layers without re-opening the editor.
- **Implementation**:
    - Modify `PortalSystem::UpdatePortalCollision`:
        - If `PortalType::NextLevel` is triggered:
            - Check `ActiveDimensionalState.isActive`.
            - If `currentDepth < maxDepth`:
                - Increment `currentDepth`.
                - Directly call `SceneManager::RequestMosaicTransition` using the existing `sourceGrid`.
            - Else:
                - Show "Rift Completed" or handle appropriately.

### 2.4 Persistence & Restoration
- **Component**: `ActiveDimensionalState`, `SceneManager`, `SaveManager`
- **Requirement**: Returning from town should not lose the current Rift state.
- **Implementation**:
    - Update `ActiveDimensionalState` JSON macro to include `sourceGrid`.
    - Modify `SceneManager::ApplyLoadedLevel`:
        - Ensure that when transitioning back to a Rift, the state is correctly restored from the `ActiveDimensionalState`.

## 3. Architecture Context
- Follows DOD (Data-Oriented Design) via EnTT.
- UI remains distinct from logic but reads from `ActiveDimensionalState`.
- Persistence uses `nlohmann/json`.

## 4. Risks
- **Persistence Bloat**: Serializing a 3x3 grid of entities is fine as long as we only store the *data* of the fragments if needed, but currently `ActiveDimensionalState` stores the `MosaicGrid`. We need to ensure `MosaicGrid` describes what was there (e.g., item types/rarities) rather than raw pointers/runtime IDs.
