# B3-2 Residual Risk

## Residual Risks

1. `UISystem` still owns broader UI hotkey, popup, and panel orchestration logic; only panel drag interaction was extracted in this slice.
2. `UISystem::UpdatePanelDrag` integration tests validate deterministic paths (initialization and stale drag release), while live mouse-press edge timing remains covered at the service unit boundary.
3. Dragging currently uses global UI reference bounds (`UI_REF_WIDTH`, `UI_REF_HEIGHT`) and a fixed visible margin; behavioral changes to those constants would require contract updates in both callers and tests.

## Why Acceptable For This Slice

- B3-2 target is decomposition of one meaningful UI interaction state/boundary unit with behavior preserved.
- Extracted service now has direct unit contracts, reducing regression risk for drag-state and clamping behavior during future UI refactors.
