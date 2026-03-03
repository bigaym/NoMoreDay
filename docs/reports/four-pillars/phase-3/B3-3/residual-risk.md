# B3-3 Residual Risk

## Residual Risks

1. Progression integration coverage currently validates the kill-award pipeline with a bounded synthetic setup; broader gameplay session sequencing (multi-kill bursts, map transitions, save/load boundaries) remains outside this slice.
2. Progression gate mapping now has dedicated CTest entries, but `-L unit`/`-L integration` executes both aggregate and progression-specific targets, so progression cases run twice in those label suites.
3. Unlock constraints are covered through progression point-budget contracts and existing astrolabe/skill tests, but additional class-specific unlock rules may still rely on neighboring module contracts.

## Why Acceptable For This Slice

- B3-3 target is expansion of progression contracts and gate registration, not progression runtime redesign.
- Added contracts directly protect level-up, unlock budget, and rollback invariants while preserving existing runtime behavior.
