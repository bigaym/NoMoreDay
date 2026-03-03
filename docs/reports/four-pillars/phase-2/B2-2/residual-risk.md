# B2-2 Residual Risk

## Low-Risk Areas
- The extraction is a bounded stage split: mitigation math moved to a dedicated service while pipeline ordering remains unchanged.
- Existing combat contract tests and new service-level tests validate core physical and elemental mitigation invariants.

## Remaining Risks
- `CalculateBatch` still contains a separate mitigation path and does not yet route through `DamageMitigationService`, so single-target and batch logic can drift over time.
- The service contract currently depends on a broad parameter list, which increases future misuse risk if new call sites are added without helper wrappers.
- This slice validates combat-focused filters, not the full non-combat test matrix.

## Follow-Up Suggestions
- Converge batch mitigation onto the same service contract to reduce duplication and drift risk.
- Introduce a small context struct for service inputs to reduce argument-order footguns.
