# V3 Shadow Pipeline Validation

## 1. Contract Checks

1. ABI version is `3` in both CPU and shader generated include.
2. RenderGraph order includes: `LightCulling -> ShadowPrepare -> ShadowBuild -> ShadowResolve -> Lighting`.
3. No pass except final composite writes to `FBO 0`.

## 2. Functional Matrix

1. Tiers: `Low/Medium/High/Ultra`.
2. Frame targets: default framebuffer and offscreen framebuffer.
3. Runtime events: startup, resize, Alt+Tab/context restore, hot reload.

Expected:

1. `Medium` shadows are disabled by policy.
2. `High` uses SDF shadow without atlas dependency.
3. `Ultra` uses hybrid path with stable fallback when atlas overflows.

## 3. Determinism

1. Key-light selection order is deterministic for same input.
2. Atlas eviction is deterministic and logged with counters.
3. Shadow fallback path emits one structured warning category.

## 4. Performance

1. Capture per-pass `mean/p95/p99`.
2. Track overhead relative to V2 baseline scene.
3. Gate:
   - High <= `0.8 ms`
   - Ultra <= `1.3 ms`

## 5. Evidence Checklist

- [ ] Unit tests report attached.
- [ ] Integration tests report attached.
- [ ] Performance JSON+CSV attached.
- [ ] Visual diff captures attached.

