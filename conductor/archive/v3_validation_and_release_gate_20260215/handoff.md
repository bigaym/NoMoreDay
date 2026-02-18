# V3 Validation and Release Gate Handoff

## What Was Added
- Gate matrix and schemas:
  - `conductor/validation/v3_gate_matrix.json`
  - `conductor/validation/v3_gate_matrix.schema.json`
  - `conductor/validation/v3_gate_artifact.schema.json`
- Performance profile and baseline files:
  - `conductor/validation/v3_perf_profiles.json`
  - `conductor/validation/v3_perf_baseline.json`
- Screenshot regression manifest:
  - `conductor/validation/v3_screenshot_manifest.json`
- Gate execution tooling:
  - `scripts/v3_release_gate.py`
  - `scripts/v3_screenshot_diff.py`
  - `scripts/v3_stability_stress.py`

## Build Integration
- `build.bat` now supports `gate`:
  - `build.bat gate`
  - `build.bat perf gate`
- Runner output defaults to:
  - `bin/release_gate/v3_gate_report.json`
  - `bin/release_gate/v3_gate_report.csv`

## Test Coverage Mapping
- Functional gates map to existing integration tests:
  - RenderGraph framebuffer ownership
  - Tier matrix stability
  - Lighting stability
  - VFX tier matrix and preview hot-reload hook
- Contract gates map to unit/integration tests:
  - GPU ABI governance
  - Binding registry governance
  - RenderGraph V3 contract validation
  - Material/VFX schema checks
- Performance gates map to benchmarks with explicit release metrics:
  - `baseline_270_fps`
  - `combat_180_fps`
  - `stress_144_fps`
  - `clustered_128_improvement_pct`

## New Integration Checks
- `tests/integration/ReleaseGateIntegrationTest.cpp`
  - Runtime `render.v3.enabled` toggle path.
  - Framebuffer tracked-byte stability under resize stress.

## Runtime/Operations Notes
- Gate runner treats critical failures as merge blockers.
- Fallback reasons are encoded in the report (`fallbackTriggered`, `fallbackReasons`).
- Baseline regression policy is configurable in `v3_perf_profiles.json`.
- Baseline can be refreshed with:
  - `python scripts/v3_release_gate.py --update-baseline`

## Open Items
- Screenshot baselines under `conductor/validation/screenshots/` must be populated
  with real captures before strict visual gating in CI.
- Screenshot gate carry-over has been promoted to V4 preflight dependency check:
  `设计文档/特效和UI/GPU_Rendering_System_V4.md` §1.4 (`DEP-V3-F6.2`).
- Clustered 128-light uplift gate (`>=5%`) remains the hard release signal from
  Step F carry-over. Temporary waiver is active:
  - `WVR-20260218-F4.6-001` (`warning`, expires `2026-03-15`)
  - linked bug: `BUG-20260218-001`
  - exit criteria: `clustered_128_improvement_pct >= 5.0` for 3 consecutive release perf runs.
