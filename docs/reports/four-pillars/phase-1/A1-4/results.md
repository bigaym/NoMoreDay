# Phase 1 / A1-4 Results

## Outcome

- Package status: `bounded-slice-complete`
- Added an offline VFX sequence migration tool that upgrades legacy schema `vfx_schema_version: 1` records to canonical schema `3`.
- Enforced explicit rejection for unsupported legacy header format using `schema_version` without `vfx_schema_version`.

## Migration slice

- Tool: `scripts/migrate_vfx_sequence_schema.py`
- Migrated format (supported):
  - Top-level `vfx_schema_version: 1`
  - `events[*].type` alias normalization for legacy snake_case event names:
    - `shadow_pulse` -> `ShadowPulse`
    - `light_profile_blend` -> `LightProfileBlend`
    - `material_phase_shift` -> `MaterialPhaseShift`
  - Missing `events[*].tierPolicy` is filled with canonical default `skip`
  - Top-level `vfx_schema_version` is rewritten to `3`

## Rejection slice

- Unsupported/dropped format (explicitly rejected):
  - Legacy top-level `schema_version` header without `vfx_schema_version`
- Rejection behavior:
  - Tool exits non-zero and emits: `unsupported legacy format: found 'schema_version' without 'vfx_schema_version'`

## Tests and verification

- Added migration/rejection tests:
  - `tests/python/MigrateVfxSequenceSchemaTest.py`
  - `test_migrates_v1_document_to_v3`
  - `test_rejects_unsupported_legacy_schema_header`
- Commands run:
  - `./build.bat check` -> PASS
  - `./build.bat` -> PASS (`NoMoreDay`, `NoMoreDayTests` built)
  - `python -m unittest tests/python/MigrateVfxSequenceSchemaTest.py` -> PASS (`2` tests)
  - `python scripts/migrate_vfx_sequence_schema.py --input build/tmp_vfx_migration/legacy_v1.json --output build/tmp_vfx_migration/migrated_v3.json` -> PASS (`schema 3`, `ShadowPulse`, `tierPolicy=skip`)
  - `python scripts/migrate_vfx_sequence_schema.py --input build/tmp_vfx_migration/unsupported_legacy.json` -> PASS (expected failure, return code `1`)

## Changed files

- `scripts/migrate_vfx_sequence_schema.py`
- `tests/python/MigrateVfxSequenceSchemaTest.py`
- `docs/reports/four-pillars/phase-1/A1-4/results.md`
- `docs/reports/four-pillars/phase-1/A1-4/residual-risk.md`
