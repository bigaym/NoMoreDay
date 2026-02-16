# V3 VFX Lighting Integration Validation

## 1. Schema and Parser Validation

1. `vfx_schema_version=3` accepted.
2. `vfx_schema_version=2` accepted through compatibility path.
3. Unsupported versions are rejected with explicit error text.
4. Invalid event payloads and invalid `tierPolicy` values are rejected.

## 2. Runtime Dispatch Validation

1. `ShadowPulse` applies and recovers over event duration.
2. `LightProfileBlend` transitions profile deterministically.
3. `MaterialPhaseShift` modifies and restores material properties.
4. Failures follow fallback policy and produce structured warnings.

## 3. Tier Policy Validation

1. `strict`: fail and stop event.
2. `degrade`: downgrade to supported behavior.
3. `skip`: skip event while keeping sequence progress.

## 4. Content Validation

1. 12 template sequences available and loadable.
2. Sequence hot reload does not corrupt runtime state.
3. Preview workflow reflects timeline changes correctly.

## 5. Evidence Checklist

- [ ] Unit test report attached.
- [ ] Integration report attached.
- [ ] Template list and validation summary attached.
- [ ] Budget estimator output attached.

