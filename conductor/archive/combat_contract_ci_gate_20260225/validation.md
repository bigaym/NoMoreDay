# Validation — combat_contract_ci_gate_20260225

## Scope

- Fix `gen_skill_contracts.py --check` failure (contract drift).
- Harden script for idempotency and determinism.
- Integrate contract check into CI as blocking gate.

## Test Coverage Added/Updated

- Script-level: idempotency and determinism manual tests.
- CI gate: contract check integrated into build pipeline.

## Verification Evidence

- Baseline drift reproduction:
  - `python scripts/gen_skill_contracts.py --check` → `[FAIL] skill_contract blocks are out of date.`
- Drift root-cause diagnosis:
  - `skills.json` / `skill_contracts_compact.json` skill IDs are aligned (`9/9`), but compact node bounds were stale (`25/25`) vs actual talent tree sizes (`28/28/28/29/28/28/28/29/26`).
- Drift fix and sync:
  - Updated `assets/data/skill_contracts_compact.json` `min_nodes/max_nodes` to current skill tree node counts.
  - Regenerated `assets/data/skills.json` contracts via `python scripts/gen_skill_contracts.py`.
  - `python scripts/gen_skill_contracts.py --check` → `[OK] skill_contract blocks are up to date.`
- Script hardening evidence:
  - `python scripts/gen_skill_contracts.py --check --verbose --check-idempotency --check-determinism` → all `[OK]`.
  - Malformed input validation test:
    - `python scripts/gen_skill_contracts.py --skills <temp_invalid.json> --check` → `[FAIL] ... talent_tree must be a list`.
- CI gate integration evidence:
  - `build.bat check` includes `python scripts/gen_skill_contracts.py --check` and blocks on failure.
  - Manual gate run: `build.bat check` PASS.
- Build/Test verification (track gate):
  - `build.bat` PASS.
  - `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` PASS.
  - `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS.
- Extended metric:
  - 7-day nightly pass streak → PENDING (requires post-merge continuous run in CI/nightly environment).
