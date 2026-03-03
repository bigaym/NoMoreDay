# D4-1 Residual Risk

## Residual Risks

1. This slice defines canonical schema for one bounded modifier domain (`skill_spec`) only; other modifier domains (`map`, `monster`, `equipment`, `talent`) and skill records are not yet normalized under the same canonical artifact strategy.
2. The validator in `scripts/validate_canonical_schema.py` implements the subset of JSON Schema keywords used by this slice, not full draft-2020-12 semantics.
3. Canonical schema validation is covered by dedicated Python unit tests but is not yet wired as a mandatory gate in `build.bat` pre-check flow.

## Why Acceptable For This Slice

- D4-1 objective is a practical, low-risk canonical-schema kickoff, not full cross-domain migration.
- Bounded scope avoids runtime behavior changes and preserves current modifier runtime generation/validation paths.
- Added fixtures and tests provide immediate executable contracts for required and invalid payload behavior, enabling incremental expansion in later D4 packages.
