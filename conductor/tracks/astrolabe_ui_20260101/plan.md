# Plan: Astrolabe UI Implementation

## Phase 1: UI Overlay & Camera Framework [checkpoint: 767c1ac]
- [x] Task: Implement `AstrolabeUI` component/class to manage UI state and visibility.
- [x] Task: Add hotkey logic in `InputSystem` or `GameplayState` to toggle the Astrolabe UI.
- [x] Task: Implement a 2D camera system within the UI to support Panning and Zooming.
- [x] Task: Basic Node Rendering: Draw simple shapes for nodes at their JSON-defined coordinates.
- [x] Task: Conductor - User Manual Verification 'Phase 1: UI Overlay & Camera Framework' (Protocol in workflow.md)

## Phase 2: Node Interaction & Information [checkpoint: d408bf8]
- [x] Task: Implement Mouse Picking: Detect which node is being hovered/clicked in world-space.
- [x] Task: Hover Tooltips: Create a dynamic tooltip that displays node metadata and localized strings.
- [x] Task: Visual Styling: Update node rendering to distinguish between Minor, Major, and Keystone nodes.
- [x] Task: Node Status Colors: Render nodes differently based on status (Activated, Available, Locked).
- [x] Task: Conductor - User Manual Verification 'Phase 2: Node Interaction & Information' (Protocol in workflow.md)

## Phase 3: Connections & Activation Logic [checkpoint: pending]
- [x] Task: Connection Line Rendering: Draw lines between nodes with status-aware coloring.
- [x] Task: Point Counter HUD: Add a talent point display to the UI.
- [x] Task: Activation Integration: Connect UI clicks to `AstrolabeSystem::activate_node` and trigger visual feedback.
- [x] Task: Input Handling: Ensure game world interaction is blocked while the Astrolabe UI is open.
- [x] Task: Conductor - User Manual Verification 'Phase 3: Connections & Activation Logic' (Protocol in workflow.md)
