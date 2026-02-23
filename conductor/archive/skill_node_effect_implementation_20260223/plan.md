# Skill Node Effects Implementation Plan

> Track ID: `skill_node_effect_implementation_20260223`  
> Method: contract-first implementation + per-skill behavior closure

---

## Phase 1: Baseline Matrix & Task Lock

- [x] Build per-skill node-effect matrix from `skills.json` (implemented / partial / missing).
- [x] Extract key-node expectations from design sections `3.1..3.9`.
- [x] Freeze implementation order by risk and dependency (Trigger/Synergy/Transmuter first).
- [x] Define per-skill acceptance checks for behavior + visual signal.

Verification:

- [x] Baseline matrix committed in track validation evidence.

---

## Phase 2: Skill 1..3 Effect Closure

- [x] Implement missing trigger/synergy/transmuter behaviors for skill `1`.
- [x] Implement missing trigger/synergy/transmuter behaviors for skill `2`.
- [x] Implement missing trigger/synergy/transmuter behaviors for skill `3`.
- [x] Add/adjust unit and integration tests for phase coverage.

Verification:

- [x] Phase-specific tests for `1..3` pass.

---

## Phase 3: Skill 4..6 Effect Closure

- [x] Implement missing behaviors for skill `4`, including defense-trigger interactions.
- [x] Implement missing behaviors for skill `5`, including channeling-trigger semantics.
- [x] Implement missing behaviors for skill `6`, including area synergy hooks.
- [x] Add/adjust tests for these skills and cross-skill interactions.

Verification:

- [x] Phase-specific tests for `4..6` pass.

---

## Phase 4: Skill 7..9 Effect Closure + Visual Readability

- [x] Implement missing behaviors for skill `7`.
- [x] Implement missing behaviors for skill `8`.
- [x] Implement missing behaviors for skill `9`.
- [x] Add/adjust visual signal hooks using existing VFX pathways with tier-safe fallback.
- [x] Add/adjust tests for advanced trigger/synergy chains and visual-state guards.

Verification:

- [x] Phase-specific tests for `7..9` pass.
- [x] No UI/render safety regression in skill-tree related paths.

---

## Phase 5: Full Gate, Evidence, and Sync

- [x] Run full build and required CTest labels.
- [x] Produce implemented-node matrix (before/after) and unresolved debt list.
- [x] Sync track docs (`plan/index/metadata/validation`) and global `tracks.md`.
- [x] Link any residual issue into `conductor/bug_registry.md` if needed.

Final verification commands:

- [x] `build.bat`
- [x] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- [x] `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
- [x] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`
