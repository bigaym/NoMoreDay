# Phase 1 Residual Risk

Open risks after current Phase-1 run:

1. No active gate failures in this run
   - Evidence: all required commands in `docs/reports/combat-core-vnext/phase-1/commands.txt` passed.
   - Mitigation: continue running the same gate set at each subsequent package/phase checkpoint.

2. Performance sensitivity can still regress over time
   - Evidence: performance gates passed this run, but benchmark suites are inherently environment-sensitive.
   - Mitigation: keep full `-L performance` and scoped combat perf checks in mandatory phase gates; investigate immediately if variance reappears.

3. Tag-domain coverage is currently filter-scoped
   - Evidence: doctest run targeted `[Unit] TagDomainV2*` and passed 3 cases.
   - Mitigation: keep this targeted filter in the package gate list and pair it with full unit/integration labels already executed.
