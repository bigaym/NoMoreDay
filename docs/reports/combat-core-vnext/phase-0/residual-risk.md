# Phase 0 Residual Risk

Open risks after current Phase-0 run:

1. Full performance suite remains red (outside touched package)
   - Evidence: `ctest -L performance` failed in `ParticleTrailBenchmark` and `RenderGraphContractBenchmark`; those files are outside Phase-0 package scope.
   - Mitigation: keep full-suite perf failures tracked as blocker debt and rerun full perf suite at every phase checkpoint.

2. ParticleTrail Scenario 4 load sensitivity
   - Evidence: focused repeats show overhead fluctuating around gate (`0.096..0.214ms`), while full-suite runs consistently exceeded threshold (`0.224..0.245ms`).
   - Mitigation: investigate benchmark methodology and stabilize gate behavior (sampling/rerun policy) without weakening regression detection.

3. RenderGraph contract micro-margin volatility
   - Evidence: full-suite failure was small (`0.0329` vs `<=0.03`) and did not reproduce in focused 30-run sampling.
   - Mitigation: treat as environment-sensitive variance risk; monitor trend and confirm sustained compliance before final cutover sign-off.

4. Provisional completion status
   - Evidence: module-gate tests for this phase pass, but full perf label does not.
   - Mitigation: use exception protocol only for untouched pre-existing failures, with documented root-cause analysis and mandatory rerun in later phases.
