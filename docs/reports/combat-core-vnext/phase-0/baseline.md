# Phase 0 Baseline Artifacts

Baseline artifacts present under `docs/reports/combat-core-vnext/baseline/`:

- `hardware-profile.md`
  - Pins capture environment identity for baseline validity: capture time (`2026-03-03T00:00:00Z`), hardware/software facts, and `profile_hash` (`sha256:9d2757e6cbbe3b8ec78795be121a2d8ec8c79a6d89ccf4a4f7bc84f6f6e294f0`).
- `scenario-manifest.json`
  - Pins canonical Phase-0 scenario catalog (`combat_scenario_manifest_v1`) and deterministic scenario seed (`1337`) for reproducible parity/perf runs.
- `perf-baseline.json`
  - Pins performance contract (`combat_perf_baseline_v1`): protocol (`warmup_runs=1`, `measured_runs=5`, metric `p95_ms`, `max_regression_pct=5.0`) and baseline threshold (`baseline_p95_ms=0.5`) for `combat.core.hit.single_target.v1`.
- `parity-harness-baseline.json`
  - Pins parity contract (`combat_parity_baseline_v1`): per-class tolerances (exact/hit_float/dot_aggregate/status_duration), expected deterministic output (`expected_legacy_final=124.0`), and expected trace hash for fixture reproducibility.

Contractual implication:

- Phase-0 validation is anchored to these baseline files; deviations in environment, scenario definitions, parity tolerances, or perf thresholds require an explicit baseline refresh process rather than ad-hoc gate interpretation.
