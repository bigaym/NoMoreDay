# Validation - v5_validation_release_gate_20260219

## Scope

- Track: `v5_validation_release_gate_20260219`
- Date: 2026-02-21
- Goal: complete V5 core release gate (功能/性能/契约/稳定性/回退), absorb SPH GO/NO-GO decision, produce final V5 posture.

---

## Verification Commands

```powershell
.\build.bat
ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure
ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure
ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure
ctest --test-dir build -C Release -L performance --output-on-failure
```

Additional targeted evidence:

```powershell
.\bin\NoMoreDayTests.exe --test-case="[Unit] JFADistanceFieldEvaluator - Full-res JFA accuracy envelope"
.\bin\NoMoreDayTests.exe --test-case="[Integration] GI - Long-run Stability Proxy (Resize + Tier Switch)"
.\bin\NoMoreDayTests.exe --test-case="[Integration] RenderGraph V5 Contracts*"
.\bin\NoMoreDayTests.exe --test-case="[Integration] ReleaseGate - Runtime render.v3.enabled toggle path"
.\bin\NoMoreDayTests.exe --test-case="[Integration] ReleaseGate - Framebuffer tracked bytes stable under resize stress"
.\bin\NoMoreDayTests.exe --test-case="[Performance] RadianceCascades - Tier and Holographic Matrix"
.\bin\NoMoreDayTests.exe --test-case="[Performance] MaterialVFX - MaterialSwap+Distortion Stress P95"
.\bin\NoMoreDayTests.exe --test-case="[Performance] MDIRenderer - Scenario Gate (50k)"
.\bin\NoMoreDayTests.exe --test-case="[Performance] bench_rendering_system - RenderGraph Contract Validation Guard"
```

---

## Command Results

- `.\build.bat`: PASS
- `ctest -C RelWithDebInfo -L ci`: PASS
- `ctest -C RelWithDebInfo -L unit`: PASS
- `ctest -C RelWithDebInfo -L integration`: PASS
- `ctest -C Release -L performance`: PASS
- All targeted tests above: PASS

---

## Release Metrics Snapshot

- `RELEASE_GATE_METRIC combat_180_fps=190455`
- `RELEASE_GATE_METRIC baseline_270_fps=941.072`
- `RELEASE_GATE_METRIC stress_144_fps=12593.4`
- `RELEASE_GATE_METRIC gi_high_standard_mean_ms=0.80768`
- `RELEASE_GATE_METRIC gi_ultra_standard_mean_ms=1.88112`
- `RELEASE_GATE_METRIC gi_high_holographic_mean_ms=0.905472`
- `RELEASE_GATE_METRIC gi_ultra_holographic_mean_ms=2.23292`
- `RELEASE_GATE_METRIC gi_ultra_high_work_ratio=6.88681`
- `RELEASE_GATE_METRIC vram_proxy_delta_bytes=0`
- `RELEASE_GATE_METRIC fluid_reference_10k_mean_ms=0.99006`
- `RELEASE_GATE_METRIC fluid_reference_10k_p99_ms=1.042`
- `RELEASE_GATE_METRIC fluid_reference_10k_target_hit=0`

Threshold checks (core gate):

- Ultra gate: `combat_180_fps >= 180` PASS
- High gate: `baseline_270_fps >= 270` PASS
- JFA/GI pass budget: `gi_ultra_standard_mean_ms <= 2.5` PASS
- Stability proxy: `vram_proxy_delta_bytes == 0` PASS

SPH exploration decision input:

- `fluid_reference_10k_target_hit=0` (target not met) → `NO-GO` (non-blocking for V5 core release)

---

## Task Evidence Mapping

### Phase 1 - GI 功能回归

- Task 1.1 PASS: `[Unit] JFADistanceFieldEvaluator - Full-res JFA accuracy envelope` (`p95<=2px`, `max<=4px`)
- Task 1.2 PASS: `conductor/archive/v5_radiance_cascades_gi_20260219/validation.md` (3 场景矩阵已完成)
- Task 1.3 PASS: `conductor/archive/v5_radiance_cascades_gi_20260219/plan.md` Phase 4/5 验证项
- Task 1.4 PASS: `conductor/archive/v5_radiance_cascades_gi_20260219/plan.md` + targeted GI stability test
- Task 1.5 PASS: `conductor/archive/v5_radiance_cascades_gi_20260219/validation.md` + core performance/integration PASS

### Phase 2 - 性能验证

- Task 2.1 PASS: `combat_180_fps=190455`
- Task 2.2 PASS: `baseline_270_fps=941.072`
- Task 2.3 PASS: `QualityTierManager` GI off route + `stress_144_fps=12593.4` + V4 baseline tracks evidence
- Task 2.4 PASS: JFA budget path validated via V5 chain in performance label and upstream Track 6 evidence
- Task 2.5 PASS: `gi_ultra_standard_mean_ms=1.88112 <= 2.5`
- Task 2.6 PASS: GI pass budget checks passed in `RadianceCascadesBenchmark` and full performance label
- Task 2.7 PASS: Auto-degrade/tier routing covered in unit/perf and Track 7 validation

### Phase 3 - 契约与稳定性

- Task 3.1 PASS: ABI checks included in `unit` label (`GPUABIGovernanceTest`) and upstream V5 tracks
- Task 3.2 PASS: binding checks included in integration/unit labels and upstream validation
- Task 3.3 PASS: `[Integration] RenderGraph V5 Contracts*`
- Task 3.4 PASS: `[Integration] GI - Long-run Stability Proxy (Resize + Tier Switch)` + `vram_proxy_delta_bytes=0`
- Task 3.5 PASS: tier switch and feature flag routes verified; no integration regressions
- Task 3.6 PASS: JFA evaluator and fallback helpers in unit tests + Track 6 accumulated evidence

### Phase 4 - SPH 决策

- Task 4.1 PASS: decision source `conductor/archive/v5_sph_fluid_exploration_20260219/validation.md`
- Task 4.2 PASS (N/A): GO branch not selected
- Task 4.3 PASS: NO-GO rollback path confirmed in SPH track validation
- Task 4.4 PASS: decision archived in SPH track docs and this track posture

### Phase 5 - 回退与发布

- Task 5.1 PASS: rollback route validated by integration toggle path and V4 compatibility chain
- Task 5.2 PASS: SPH track confirms `render.fluid.enabled=false` resource release path
- Task 5.3 PASS: post-rollback V4 chain remains green (`build + ctest ci/unit/integration/performance`)
- Task 5.4 PASS: OGL 4.3 ceiling risk recorded in `conductor/rendering_system_progress.md` (`V5-R07`)
- Task 5.5 PASS: Vulkan pre-study retained as V6 note in V5 spec/progress docs
- Task 5.6 PASS: `release_posture.md` produced
- Task 5.7 PASS: `conductor/rendering_system_progress.md` and `conductor/tracks.md` synced
- Task 5.8 PASS: final decision = GO (core) / NO-GO (SPH optional branch)
- Task 5.9 PASS: track archived to `conductor/archive/v5_validation_release_gate_20260219`

---

## Summary

- Completed: `31/31` tasks
- Phases completed: `5/5`
- Core gate result: PASS
- SPH exploration result: NO-GO (non-blocking)
- Final release decision: `GO (V5 core GI released)`
