# Specification: Astrolabe (星盘) UI Implementation

## Overview
This track focuses on creating the interactive user interface for the Astrolabe talent tree. Building on the Phase 1 Foundation, this UI will allow players to visualize the tree, navigate via zooming and panning, view node details, and activate nodes to enhance their character.

## Functional Requirements
- **Hotkey Access**: Open/Close the UI using a configurable hotkey (e.g., 'N').
- **Interactive Tree View**:
    - **Rendering**: Draw nodes and connections based on `AstrolabeRegistry` data.
    - **Navigation**: Support mouse-drag for panning and mouse-wheel for zooming the view.
    - **Node Styling**: 
        - Visual differentiation between Minor (small), Major (medium/colored), and Keystone (large/ornate) nodes.
        - Status-based styling: Activated (bright/glowing), Available (dim), Locked (grayed out).
    - **Connection Lines**: Render lines between nodes that update based on activation status.
- **Information & Interaction**:
    - **Hover Tooltips**: Display node name, localized description, and stat modifiers when the mouse hovers over a node.
    - **Node Activation**: Clicking an "Available" node triggers `AstrolabeSystem::activate_node`.
- **UI HUD Elements**:
    - Display the current "Available Talent Points".
- **State Management**: Implement as a UI overlay/component within `GameplayState`, ensuring input is captured when the UI is open.

## Non-Functional Requirements
- **Visual Polish**: Use Raylib's primitive drawing or textures for a clean, "concentric" aesthetic.
- **Performance**: Efficient rendering of nodes and lines (frustum culling or simple screen-space checks if the tree becomes very large).
- **Responsive Interaction**: Zooming and panning should feel smooth at 60 FPS.

## Acceptance Criteria
- [ ] Pressing the hotkey toggles the Astrolabe UI overlay.
- [ ] The tree correctly displays nodes from `astrolabe.json` at their defined coordinates.
- [ ] Players can pan the view by dragging and zoom in/out with the mouse wheel.
- [ ] Nodes show tooltips with correct information from the registry.
- [ ] Clicking a valid node activates it (stat changes confirmed via the foundation logic) and updates its visual state.
- [ ] Talent point counter updates immediately upon activation.

## Out of Scope
- Detailed animations for "Star Bridges".
- Search or filter functionality for nodes.
- Re-spec (point refund) logic (to be handled in a future track).
