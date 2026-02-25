# Combat Contract CI Gate — Implementation Plan

> Track ID: `combat_contract_ci_gate_20260225`  
> Series: CS-M1-05  
> Depends on: None (independent, can run in parallel with CS-M1-01)  
> Workflow: Diagnose → Fix → Gate

---

## Phase 1: Diagnose & Fix Drift (Day 1)

- [ ] Run `python scripts/gen_skill_contracts.py --check` and capture full diff output.
- [ ] Identify root cause of drift:
  - [ ] Compare `skills.json` node count/IDs with `skill_contracts_compact.json`.
  - [ ] Diff generated output vs current `SkillRegistry.cpp` contract block.
- [ ] Fix the identified drift:
  - [ ] If data-side: regenerate `skill_contracts_compact.json` from `skills.json`.
  - [ ] If code-side: regenerate contract block by running `gen_skill_contracts.py`.
  - [ ] If script-side: fix generation logic bug.
- [ ] Verify fix: `python scripts/gen_skill_contracts.py --check` → `[OK]`.

Verification:

- [ ] `--check` passes.
- [ ] `build.bat` compiles clean after regeneration.
- [ ] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS.

---

## Phase 2: Script Hardening (Day 2)

- [ ] Add idempotency test: run generation twice, second `--check` must still pass.
- [ ] Add determinism test: same input → identical output (byte-exact).
- [ ] Add input validation: script should fail clearly on malformed `skills.json` or missing fields.
- [ ] Add `--verbose` mode for debugging drift sources.

Verification:

- [ ] Idempotency: two consecutive `--check` runs both return `[OK]`.
- [ ] Determinism: `diff` between two consecutive generation outputs is empty.

---

## Phase 3: CI Integration & Gate (Day 2-3)

- [ ] Add `--check` step to `build.bat` pipeline (or create `build.bat contracts` sub-command).
- [ ] Document CI gate in this track's validation.md.
- [ ] Run first nightly validation manually to confirm gate works.

Verification:

- [ ] Full CI suite passes with contract check integrated.
- [ ] `build.bat` (or `build.bat contracts`) includes contract check.

---

## Final Verification Gate

- [ ] `python scripts/gen_skill_contracts.py --check` → `[OK]`
- [ ] `build.bat` PASS
- [ ] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` PASS
- [ ] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS

---

## Deliverables

- Fixed contract drift (regenerated blocks).
- Hardened `gen_skill_contracts.py` script.
- CI gate integration.
- `validation.md` evidence in track folder.
