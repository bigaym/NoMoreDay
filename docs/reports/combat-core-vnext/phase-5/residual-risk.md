# Residual Risk - Phase 5 (P5-C)

## Current Residual Risks

1. Environment-sensitive runtime logging noise
   - Observation: MMKV initialization logs appear during targeted doctest execution.
   - Risk: Low.
   - Impact: No correctness impact observed; logs can add noise during triage.
   - Mitigation: Keep CI log filtering conventions and monitor for anomalous volume changes.

2. Performance baseline drift over time
   - Observation: Performance gates passed in this run.
   - Risk: Low (ongoing).
   - Impact: Regressions can still appear as unrelated modules evolve.
   - Mitigation: Continue running `performance` and `combat.perf.baseline` labels at phase gates.

3. Runtime mode toggle misconfiguration
   - Observation: `NMD_COMBAT_V2_CUTOVER` takes precedence over `NMD_COMBAT_V2_DUAL_RUN`.
   - Risk: Low.
   - Impact: Incorrect environment setup can unexpectedly force candidate-only behavior.
   - Mitigation: Document toggle precedence and verify with focused cutover/parity doctest commands before release.

4. Candidate path is still a parity stub
   - Observation: Current candidate path intentionally diverges from primary in strict parity checks.
   - Risk: Low (known).
   - Impact: Strict mismatch artifacts are expected until full kernel parity lands.
   - Mitigation: Treat strict mismatch reports as expected for this stage; enforce tolerance-class classification in parity evidence.

## Overall Residual Risk Assessment

Residual risk is LOW and acceptable for Phase 5 package P5-C completion.
