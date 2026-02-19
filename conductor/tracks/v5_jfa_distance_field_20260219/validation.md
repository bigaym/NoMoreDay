# Validation - v5_jfa_distance_field_20260219

## Scope

- Track: `v5_jfa_distance_field_20260219`
- Objective: Deliver JFA SDF pipeline as V5 GI prerequisite with measurable precision/perf gates.
- Baseline Strategy: JFA realtime path + exact EDT comparison path + JFA+2 fallback.

---

## Acceptance Checklist

### Functional
- [ ] OccluderExtract outputs stable `OccluderMask` (static + dynamic composition).
- [ ] JFA pipeline (`SeedInit -> JumpFlood -> JFA+1 -> Distance`) produces signed distance field.
- [ ] `DistanceField` resource is readable by downstream GI pipeline.

### Precision
- [ ] Full-res JFA vs exact EDT: `P95 <= 2px` (1080p reference scenes).
- [ ] Half-res upsampled JFA vs full-res JFA: `RMS <= 1px`, `P95 <= 2px`.
- [ ] 120-frame static scene boundary jitter: `<= 0.5px`.

### Performance
- [ ] JFA pass time `<= 1.5ms @1080p (RTX 4070S)` under target scenario.
- [ ] High tier half-res + interval update remains within budget envelope.

### Stability / Contract
- [ ] RenderGraph contract checks pass for V5 stage order and ownership.
- [ ] Resize resource rebuild path is stable (no invalid texture handle / black frame).
- [ ] Barrier audit completed (`glMemoryBarrier` points verified).

### Fallback
- [ ] `JFA+2` fallback activates on threshold breach and recovers precision envelope.
- [ ] Feature disable path returns safely to V4-compatible behavior.

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
