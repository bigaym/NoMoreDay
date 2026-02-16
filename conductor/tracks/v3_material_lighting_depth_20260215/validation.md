# V3 Material Lighting Depth Validation

## 1. Schema Compatibility

1. v2 materials parse and validate correctly.
2. v1 materials auto-map to v2 defaults without crash.
3. Unsupported schema version fails with explicit error.

## 2. Rendering Consistency

1. Same light setup shows intended differences for:
   - smooth vs rough materials,
   - high vs low specular,
   - normal map on/off.
2. No severe temporal shimmer under camera movement.

## 3. Tier Behavior

1. Low/Medium disable expensive branches and remain stable.
2. High/Ultra enable full Material 2.0 path.
3. Tier transitions do not corrupt material state.

## 4. Performance

1. Measure `entity_mdi` and `particle` pass impact.
2. Gate: High-tier increase <= `0.6 ms`.

## 5. Evidence Checklist

- [ ] Unit tests attached.
- [ ] Integration matrix attached.
- [ ] Perf results attached.
- [ ] Visual comparisons attached.

