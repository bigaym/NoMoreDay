# Validation — combat_contract_ci_gate_20260225

## Scope

- Fix `gen_skill_contracts.py --check` failure (contract drift).
- Harden script for idempotency and determinism.
- Integrate contract check into CI as blocking gate.

## Test Coverage Added/Updated

- Script-level: idempotency and determinism manual tests.
- CI gate: contract check integrated into build pipeline.

## Verification Evidence

_(To be filled during implementation)_

- `python scripts/gen_skill_contracts.py --check` → PENDING
- Idempotency test (run twice, both OK) → PENDING
- `build.bat` → PENDING
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` → PENDING
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` → PENDING
- 7-day nightly pass streak → PENDING (M1 extended metric)
