# Skill Tree Doc Alignment Plan

> Track ID: `skill_tree_doc_alignment_20260223`  
> Method: content-first alignment + contract/runtime regression verification

---

## Phase 1: Baseline Audit Matrix

- [x] Generate current per-skill matrix from `skills.json` (node count, role counts, trigger mapping, resist/scope).
- [x] Generate expected matrix from design doc sections `3.1..3.9`.
- [x] Produce mismatch table and lock the implementation order.

Verification:

- [x] Mismatch matrix is committed in track validation evidence.

---

## Phase 2: JSON Contract Alignment

- [x] Align skill `1..3` talent trees and contract nodes.
- [x] Align skill `4..6` talent trees and contract nodes.
- [x] Align skill `7..9` talent trees and contract nodes.
- [x] Ensure trigger role and payload align with documented trigger behavior per skill.
- [x] Ensure transmuter and synergy contract semantics remain valid.

Verification:

- [x] `skills.json` parses and passes schema/registry loading.
- [x] Structural checker confirms alignment targets.

---

## Phase 3: Runtime/Tests Synchronization

- [x] Update contract integration tests for new role/trigger mapping.
- [x] Update behavior guard tests for new trigger/transmuter cases.
- [x] Update skill integration tests where node ids or trigger expectations changed.

Verification:

- [x] Unit and integration tests pass for specialization/contract subsets.

---

## Phase 4: Final Gate & Evidence

- [x] Run full build and required ctest labels.
- [x] Write validation evidence including mismatch-before/after matrix.
- [x] Update bug/track references if any known exception remains.

Final verification commands:

- [x] `build.bat`
- [x] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- [x] `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`
- [x] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`
