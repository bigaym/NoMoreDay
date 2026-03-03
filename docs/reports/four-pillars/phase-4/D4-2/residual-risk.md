# D4-2 Residual Risk

## Residual Risks

1. This integration is intentionally bounded to one domain file (`skill_spec_modifiers.json`) and one canonical fixture set; other modifier domains still rely on their existing generation/manual paths.
2. Runtime projection fields are validated by generator-side checks, but they are not yet described by a dedicated JSON Schema artifact.
3. Canonical source for this bounded slice currently lives under test fixtures, not a broader production canonical catalog.

## Why Acceptable For This Slice

- D4-2 target is bounded integration, not full cross-domain migration.
- The new pre-check gate prevents silent drift between canonical input and runtime contract output for the selected slice.
- Existing runtime asset behavior stays stable because generated output is checked against the committed runtime contract file.
