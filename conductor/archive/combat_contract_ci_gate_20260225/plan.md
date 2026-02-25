# Combat Contract CI Gate — Implementation Plan

> Track ID: `combat_contract_ci_gate_20260225`  
> Series: CS-M1-05  
> Depends on: None (independent, can run in parallel with CS-M1-01)  
> Workflow: Diagnose → Fix → Gate

---

## Phase 1: Diagnose & Fix Drift (Day 1)

- [x] Run `python scripts/gen_skill_contracts.py --check` and capture full diff output.
- [x] Identify root cause of drift:
  - [x] Compare `skills.json` node count/IDs with `skill_contracts_compact.json`.
  - [x] Diff generated output vs current `skills.json` `skill_contract` block.
- [x] Fix the identified drift:
  - [x] If data-side: regenerate `skill_contracts_compact.json` from `skills.json` (`min_nodes/max_nodes` aligned to real node counts).
  - [x] If code-side: regenerate contract block by running `gen_skill_contracts.py`.
  - [x] If script-side: fix generation logic bug. (N/A for drift root cause, script hardened in Phase 2)
- [x] Verify fix: `python scripts/gen_skill_contracts.py --check` → `[OK]`.

Verification:

- [x] `--check` passes.
- [x] `build.bat` compiles clean after regeneration.
- [x] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS.

---

## Phase 2: Script Hardening (Day 2)

- [x] Add idempotency test: run generation twice, second `--check` must still pass.
- [x] Add determinism test: same input → identical output (byte-exact).
- [x] Add input validation: script should fail clearly on malformed `skills.json` or missing fields.
- [x] Add `--verbose` mode for debugging drift sources.

Verification:

- [x] Idempotency: two consecutive `--check` runs both return `[OK]`.
- [x] Determinism: `--check-determinism` returns `[OK]`.

---

## Phase 3: CI Integration & Gate (Day 2-3)

- [x] Add `--check` step to `build.bat` pipeline (or create `build.bat contracts` sub-command).
- [x] Document CI gate in this track's validation.md.
- [x] Run first nightly validation manually to confirm gate works. (`build.bat check` with gate enabled)

Verification:

- [x] Full CI suite passes with contract check integrated.
- [x] `build.bat` includes contract check.

---

## Final Verification Gate

- [x] `python scripts/gen_skill_contracts.py --check` → `[OK]`
- [x] `build.bat` PASS
- [x] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` PASS
- [x] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS

---

## Deliverables

- Fixed contract drift (regenerated blocks).
- Hardened `gen_skill_contracts.py` script.
- CI gate integration.
- `validation.md` evidence in track folder.
