# Validation - v5_jfa_distance_field_20260219

## Scope

- Track: `v5_jfa_distance_field_20260219`
- Objective: Deliver JFA SDF pipeline as V5 GI prerequisite with measurable precision/perf gates.
- Baseline Strategy: JFA realtime path + exact EDT comparison path + JFA+2 fallback.

---

## Acceptance Checklist

### Functional
- [x] OccluderExtract outputs stable `OccluderMask` (static + dynamic composition).
- [x] JFA pipeline (`SeedInit -> JumpFlood -> JFA+1 -> Distance`) produces signed distance field.
- [x] `DistanceField` resource is readable by downstream GI pipeline.

### Precision
- [x] Full-res JFA vs exact EDT: `P95 <= 2px` (1080p reference scenes).
- [ ] Half-res upsampled JFA vs full-res JFA: `RMS <= 1px`, `P95 <= 2px`.
- [ ] 120-frame static scene boundary jitter: `<= 0.5px`.

### Performance
- [ ] JFA pass time `<= 1.5ms @1080p (RTX 4070S)` under target scenario.
- [ ] High tier half-res + interval update remains within budget envelope.

### Stability / Contract
- [x] RenderGraph contract checks pass for V5 stage order and ownership.
- [ ] Resize resource rebuild path is stable (no invalid texture handle / black frame).
- [x] Barrier audit completed (`glMemoryBarrier` points verified).

### Fallback
- [x] `JFA+2` fallback activates on threshold breach and recovers precision envelope.
- [x] Feature disable path returns safely to V4-compatible behavior.

---

## Verification Commands

```powershell
build.bat
ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure
ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure
ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure
```

Optional perf validation:

```powershell
ctest --test-dir build -C Release -L performance --output-on-failure
```

---

## Evidence Log

### 2026-02-19 Implementation & Verification Sync

#### Actions
- Added V5 JFA pipeline passes and shaders:
  - `OccluderExtractPass` (static/dynamic mask extraction + OR compose + resize-safe resources)
  - `JFAPass` (`SeedInit -> JumpFlood -> JFA+1 -> Distance`, half-res upsample, interval update, JFA+2 fallback, barrier audit log)
  - Shaders: `v5_occluder_extract.comp`, `v5_occluder_compose.comp`, `v5_seed_init.comp`, `v5_jump_flood.comp`, `v5_distance_field.comp`, `v5_distance_upsample.comp`
- Added CPU reference evaluator for EDT/JFA error tooling:
  - `src/engine/render/gi/JFADistanceFieldEvaluator.{hpp,cpp}`
- Integrated V5 passes into render runtime:
  - `RenderSystem` lifecycle hook-up (initialize/resize/rendergraph/shutdown)
  - `RenderContext` distance-field handoff fields (`giDistanceFieldTexture/Width/Height`)
- Added tests:
  - Unit: `tests/unit/JFADistanceFieldEvaluatorTest.cpp`
  - Integration: `tests/integration/RenderGraphV5ContractsIntegrationTest.cpp`
  - Tier matrix integration updated to include GI-on chain coverage

#### Verification
- `build.bat` PASS.
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS.
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` PASS.
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` PASS.
- `ctest --test-dir build -C Release -L performance --output-on-failure` FAIL (non-blocking for this track): known `ParticleTrail Scenario 4` threshold breach (`dispatchOverheadMs=0.242689 > 0.2`), linked to existing `BUG-20260219-004`.

### 2026-02-20 Planning Sync (No code logic change)

#### Actions
- Refined `plan.md` to executable 4-phase/22-task plan aligned with V5 review strategy.
- Added this validation template with explicit AC matrix and command set.
- Synced `spec.md` seed format to `RG16UI` and added EDT/JFA+2 governance clauses.
- Synced `index.md` quick links and `metadata.json` updated timestamp.

#### Verification
- `build.bat` PASS.
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS.
- `ctest -L unit/integration` not executed in this planning-only sync.

---

## Risk Linkage

- If precision/perf gate fails, register or link issue in `conductor/bug_registry.md`.
- Suggested IDs namespace for this track: `BUG-20260220-xxx`.
