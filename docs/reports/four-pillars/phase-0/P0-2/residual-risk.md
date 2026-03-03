# Phase 0 / P0-2 Residual Risks

## Open risks

1. Deterministic runner currently uses `P99` as a proxy (`p95_proxy_p99_ms`) because the selected test output does not expose an explicit numeric P95 value.
2. Captured GPU identity is environment-dependent (`GameViewer Virtual Display Adapter`), so cross-host comparisons require matching host profile evidence.
3. Module mapping uses keyword heuristics and may undercount/overcount true coverage depth.

## Mitigation in next package slices

- Add an explicit `RELEASE_GATE_METRIC` emission for P95 in benchmark cases so the runner can switch from proxy to direct metric parsing.
- Require host-profile hash match for candidate-vs-baseline comparisons, or run baseline + candidate on the same pinned host.
- Evolve test map from heuristic keyword match to label + case metadata extraction where practical.
