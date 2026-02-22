# Blade Ascendant Skill Validation Gate - Validation

> Track ID: `blade_ascendant_skill_validation_gate_20260221`  
> Date: `2026-02-22`

## 1. Verification Commands

| Command | Result |
|---|---|
| `.\build.bat` | PASS |
| `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` | PASS (first run failed once at `VFX Lighting - Preview HotReload Diff Hook`, immediate replay PASS) |
| `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` | PASS |
| `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` | PASS |
| `python scripts/gen_skill_contracts.py --check` | PASS (`[OK] skill_contract blocks are up to date.`) |
| `ctest --test-dir build -C Release -L performance --output-on-failure` | FAIL (non-blocking for this track, linked to `BUG-20260219-004`) |
| `ctest --test-dir build -C Release -L performance --output-on-failure -V` | FAIL (same non-blocking issue; used for metric capture) |

## 2. Gate Dimensions

| Dimension | Evidence Source | Status |
|---|---|---|
| Functional correctness | `ctest -L unit` PASS; `ctest -L integration` PASS; behavior/registry coverage in `tests/integration/SkillSystemTests.cpp` and `tests/integration/SkillContractRegistryTests.cpp` | PASS |
| Contract compliance | `python scripts/gen_skill_contracts.py --check` PASS; render ownership/contract coverage in `tests/integration/RenderGraphFramebufferOwnershipIntegrationTest.cpp` and `tests/integration/RenderGraphV3ContractsIntegrationTest.cpp` | PASS |
| Performance | `ctest -C Release -L performance` failed only at ParticleTrail Scenario 4 (`dispatchOverheadMs` threshold), linked to existing bug | CONDITIONAL (non-blocking) |
| Stability | perf logs include high-frequency stress and frame variance metrics; integration suite PASS | PASS |
| Fallback behavior | frozen tier matrix + tier integration suites + runtime degrade logs | PASS |

## 3. Task Evidence Mapping

| Plan Task | Evidence | Status |
|---|---|---|
| 1.1 Skill assertion set | `conductor/archive/blade_ascendant_vfx_design_freeze_20260221/evidence/skill_vfx_matrix.md` | DONE |
| 1.2 Contract rules and checker entry | `conductor/archive/blade_ascendant_vfx_design_freeze_20260221/evidence/render_contract_matrix.md`; `python scripts/gen_skill_contracts.py --check` | DONE |
| 1.3 Performance baseline source | prior Blade Ascendant validation tracks + current `ctest -L performance -V` metrics | DONE |
| 1.4 Validation template ready | this file | DONE |
| 2.1 Unit suite | `ctest -C RelWithDebInfo -L unit` PASS | DONE |
| 2.2 Integration suite | `ctest -C RelWithDebInfo -L integration` PASS | DONE |
| 2.3 Contract checks and diffs | contract generator check PASS + render ownership/contract integration tests PASS | DONE |
| 2.4 Blocker fix and replay | CI first run transient fail replayed immediately and passed; no code fix required | DONE |
| 3.1 Performance suite | `ctest -C Release -L performance` run completed with one known failing case | DONE |
| 3.2 High-frequency stress | VFX stress metrics captured from verbose run (Distortion/VFXSequencer/ParticleTrail scenarios) | DONE |
| 3.3 Tier switch/fallback path | `tests/integration/VFXTierMatrixIntegrationTest.cpp`; `tests/integration/RenderGraphTierMatrixIntegrationTest.cpp`; `tests/integration/ShadowPipelineTierFallbackIntegrationTest.cpp`; runtime auto-degrade logs | DONE |
| 3.4 Non-blocking issue linkage | linked to `BUG-20260219-004` with current run metrics | DONE |
| 4.1 Validation evidence export | this file | DONE |
| 4.2 Track status sync | track docs + `conductor/tracks.md` + `conductor/archive/tracks_archive.md` | DONE |
| 4.3 Release posture | section 6 below | DONE |
| 4.4 Final build rerun | closeout build pass recorded in section 1 | DONE |

## 4. Performance and Stability Metrics

From `ctest --test-dir build -C Release -L performance --output-on-failure -V`:

- `DistortionPass::Execute (2K@8)`: Mean `0.007ms`, P99 `0.008ms`
- `VFXSequencer MaterialSwap+Distortion Stress`: Mean `0.006ms`, P99 `0.020ms`
- `VFXLightingIntegration::10ConcurrentFrameVariance`: Mean `0.000ms`, P99 `0.001ms`
- `Scenario G (TierAutoDegrade)`: Mean `0.001ms`, P99 `0.007ms`
- `ParticleTrail Scenario4 (SubEmitter 1k/frame)`: Baseline `0.116ms`, WithSubEmit `0.334ms`, Overhead `0.218223ms` (threshold `<0.2ms`, fail)

From non-verbose performance run:

- `ParticleTrail Scenario4 dispatchOverheadMs=0.256455` (same known failure family)

## 5. Bug Linkage

- Linked bug: `BUG-20260219-004` (`conductor/bug_registry.md`)
- Classification: `non-blocking for this track`
- Reason: failure is in pre-existing ParticleTrail benchmark gate and not introduced by Blade Ascendant validation track changes.
- Recorded metrics in this run:
  - `dispatchOverheadMs=0.256455` (`ctest -L performance`)
  - `dispatchOverheadMs=0.218223` (`ctest -L performance -V`)

## 6. Release Posture

`CONDITIONAL-GO`

Rationale:

- Functional, contract, stability, and fallback dimensions are closed with explicit evidence.
- Performance label still reports one known non-blocking failure already tracked by `BUG-20260219-004`.
