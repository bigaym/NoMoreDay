# V3 Validation and Release Gate Validation

## 1. Gate Execution Validation

1. Gate runner executes end-to-end from clean workspace.
2. Each gate emits expected artifact outputs.
3. Failures include actionable reason and failing category.

## 2. Performance Gate Validation

1. Profiles:
   - `baseline_270`
   - `combat_180`
   - `stress_144`
2. Performance comparator correctly detects:
   - pass budget breach,
   - frame regression > 10%.

## 3. Stability Gate Validation

1. 30-minute stress test reports no sustained VRAM growth trend.
2. Resize and context restore scenarios complete without black screen.

## 4. Contract Gate Validation

1. ABI version/layout mismatch is detected and blocks merge.
2. Binding collision is detected and blocks merge.
3. RenderGraph contract violation is detected and blocks merge.
4. Schema mismatch is detected and blocks merge.

## 5. Rollout Validation

1. `render.v3.enabled=true` path works when gates pass.
2. Failure path triggers V2 fallback and blocks merge.
3. Rollout log includes gate summaries and fallback reason.

## 6. Evidence Checklist

- [ ] Functional matrix report attached.
- [ ] Performance report attached.
- [ ] Stability report attached.
- [ ] Contract checks attached.
- [ ] Fallback drill report attached.

