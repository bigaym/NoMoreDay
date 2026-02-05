# Implementation Plan: Astrolabe Audit Fixes

**Track ID:** astrolabe-audit-fix_20260205
**Spec:** [spec.md](./spec.md)
**Status:** [ ] Planned

## Phase 1: Data & Logic Integrity
**Goal:** Ensure data structures support topology and logic enforces Vow constraints.

- [ ] **Task 1.1: Restore Topology Data**
  - Modify `AstrolabeTalentNode` in `TalentData.hpp` to include `std::vector<uint32_t> prerequisites`.
  - Update `TalentLoader.cpp` to parse this field from JSON.
  - Verification: `NoMoreDayTests` (compile check) & Debug Log.

- [ ] **Task 1.2: Fix Vow Attribute Leak**
  - Modify `AttributePipeline::Calculate` in `AttributePipeline.cpp`.
  - Add check for `TalentNodeType::Core` vs `AstrolabeComponent::mainProfession`.
  - Verification: Unit Test (or manual verification if test suite is complex to extend quickly).

- [ ] **Task 1.3: Clean Hardcoded Paths**
  - Define `TALENT_DATA_PATH` in `src/game/components/Common.hpp`.
  - Update `AstrolabeRegistry.cpp` to use it.

## Phase 2: Visual & UI Fixes
**Goal:** Restore missing visuals and fix rendering bugs.

- [ ] **Task 2.1: Implement Connection Rendering**
  - Add `DrawConnections` to `AstrolabeRenderer` (header and source).
  - Implement line drawing logic based on `prerequisites`.
  - Call `DrawConnections` in `AstrolabeRenderer::Draw`.
  - Verification: Visual inspection.

- [ ] **Task 2.2: Fix Camera & Background**
  - Update `UIAstrolabe::DrawInternal` to recalc camera offset.
  - Update `AstrolabeRenderer::DrawBackground` to use `DrawRectangleRec` with full view bounds.
  - Verification: Resize window, pan aggressively.

## Phase 3: Verification
- [ ] **Task 3.1: Final Audit**
  - Run all tests.
  - Playtest to ensure lines look good and Vow dialog works.
