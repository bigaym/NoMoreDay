# Phase 0 / P0-2 Residual Risks

## Open risks

1. Deterministic performance baseline is not fully captured yet (no fixed scenario runner output with 1 warmup + 5 measured P95 runs in this package slice).
2. Hardware fingerprint is partial (GPU, driver version, RAM fields not yet auto-collected in baseline snapshot).
3. Module mapping uses keyword heuristics and may undercount/overcount true coverage depth.

## Mitigation in next package slices

- Finalize canonical perf runner command/profile in Phase 0 and append measured P95 run set.
- Add a small hardware capture helper to baseline tooling (GPU/driver/RAM fields).
- Evolve test map from heuristic keyword match to label + case metadata extraction where practical.
