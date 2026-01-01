# Plan: Astrolabe System - Phase 1 Foundation

## Phase 1: Data Infrastructure & Registry [checkpoint: b0a76ba]
- [x] Task: Define `AstrolabeNode` data structures in `src/components/Progression.hpp` (or similar). [b0a76ba]
- [x] Task: Create `src/core/AstrolabeRegistry.hpp` and `.cpp` for loading and managing node metadata. [b0a76ba]
- [x] Task: Implement JSON parsing for `assets/data/astrolabe.json` using `nlohmann/json`. [b0a76ba]
- [x] Task: Create a basic `assets/data/astrolabe.json` with a few test nodes (Minor, Major). [b0a76ba]
- [x] Task: Conductor - User Manual Verification 'Phase 1: Data Infrastructure' (Protocol in workflow.md) [b0a76ba]

## Phase 2: ECS Components & Logic
- [ ] Task: Implement `AstrolabeComponent` to store activated node IDs and available talent points.
- [ ] Task: Create `src/systems/AstrolabeSystem.hpp` and `.cpp` to handle activation logic.
- [ ] Task: Implement `AstrolabeSystem::can_activate(entity, node_id)` validation (points, prerequisites).
- [ ] Task: Implement `AstrolabeSystem::activate_node(entity, node_id)` and integrate "Dirty Flag" for stats refresh.
- [ ] Task: Conductor - User Manual Verification 'Phase 2: ECS Components & Logic' (Protocol in workflow.md)

## Phase 3: Stats Integration & Testing
- [ ] Task: Update `StatsSystem` to iterate through `AstrolabeComponent` and apply modifiers from the `AstrolabeRegistry`.
- [ ] Task: Create `tests/AstrolabeSystemTest.hpp` to verify the full flow: load -> validate -> activate -> stat change.
- [ ] Task: Verify persistence (save/load) of `AstrolabeComponent` (if save system is ready, otherwise mock it).
- [ ] Task: Conductor - User Manual Verification 'Phase 3: Stats Integration & Testing' (Protocol in workflow.md)
