# C5-1 Residual Risk

## Residual Risks

1. Module filtering currently depends on doctest name matching (`*UI*`, `*Item*`, `*Progression*`), so future test-case naming drift could reduce intended gate coverage.
2. There is no dedicated `nmd.tests.item.integration` gate yet because no integration tests currently match the item module slice.
3. Label-based gate runs overlap with broad `unit`/`integration` runs, which is intentional for discoverability but can increase duplicate execution in CI if multiple labels are scheduled together.

## Why Acceptable For C5-1

- C5-1 objective is explicit module gate expansion and CI discoverability, which is now met through dedicated CTest entries and labels.
- Existing `unit` and `integration` label behavior is preserved and validated.
- Remaining risk is governance/documentation-oriented, not runtime behavior, and can be tightened in later packages by introducing stricter module tags and integration parity for item.
