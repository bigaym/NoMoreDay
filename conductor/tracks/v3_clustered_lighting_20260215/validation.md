# V3 Clustered Lighting Validation

## 1. Functional Coverage

1. `clusteredLightingEnabled=true/false` switch behavior.
2. Cluster dimensions edge cases (`tile size`, `z slices`, screen resize).
3. Empty lights, sparse lights, dense lights.
4. Spot + point + ambient light type mixes.

## 2. Determinism Coverage

1. Overflow trimming selects same light subset for same inputs.
2. Cluster light lists are stable frame-to-frame without input changes.
3. Fallback path output is deterministic.

## 3. Integration Coverage

1. Tier matrix (`Low/Medium/High/Ultra`).
2. Default framebuffer + offscreen framebuffer.
3. Resize and context restore.

## 4. Performance Coverage

1. Scenario A: 64 lights baseline.
2. Scenario B: 128 lights stress.
3. Scenario C: 256 lights extreme.

Output:

1. `LightCullingPass` mean/p95/p99.
2. `LightingPass` mean/p95/p99.
3. Delta vs V2 baseline.

## 5. Evidence Checklist

- [ ] Unit test report attached.
- [ ] Integration report attached.
- [ ] Performance JSON+CSV attached.
- [ ] Overflow diagnostics sample attached.

