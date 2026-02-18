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

- [x] Unit test report attached.
- [x] Integration report attached.
- [x] Template list and validation summary attached.
- [x] Budget estimator output attached.

## 6. Command Evidence

1. `build.bat` (RelWithDebInfo + CTest `ci`) passed.
2. `build.bat analyze` passed (MSVC `/analyze`, project scope only).
3. `build.bat perf` passed (Release + CTest `performance`).
4. `bin/NoMoreDayTests.exe --test-case="[Integration] VFX Lighting*" --no-breaks` passed:
   - 3/3 integration cases passed (`E6.1`, `E6.2`, `E6.3`).
5. `bin/NoMoreDayTests.exe --test-case="[Performance] VFXLightingIntegration*" --no-breaks` passed:
   - 1/1 performance case passed (`E6.4`).
