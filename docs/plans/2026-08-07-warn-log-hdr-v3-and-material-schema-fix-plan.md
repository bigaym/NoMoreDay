# 2026-08-07 Warn Log Fix Plan: HDR/V3 Gate + Material Schema Upgrade

## Goal

Remove two recurring warnings from `bin/logs/NoMoreDay.log`:

1. `RenderSystem: skip V3 shadow/cluster passes because HDR scene buffer is disabled (compositeFbo=2)` — repeated every 3 s (10 times, rate-limited by `LOG_LIMITED_WARN(3.0f)`).
2. `MaterialManager: schema_compatibility path=assets/data/materials_vfx.json schema=1 action=apply_defaults target_schema=3` — once, deduped by `m_v1WarnedAssets`.

## Architecture

- **Issue 1** is a rendering gate coupling bug in `RenderSystem` (HDR scene buffer request policy vs. the V3 toggle). The V3 shadow/cluster/lighting chain depends on the HDR scene buffer; today `v3Enabled` alone does not request the HDR chain, so enabling V3 without any other HDR-requesting feature (e.g. Low tier: dynamicLighting/bloom/etc. all off) leaves `useHdrSceneBuffer=false` and the passes are skipped with a warning.
- **Issue 2** is data schema staleness: the shipped VFX material file is still at schema 1 while the engine expects schema 3; the loader tolerates it via `apply_defaults` but warns once.

Both are well-scoped defect fixes that do not change product behavior — Issue 1 only routes V3 through the HDR buffer it already depends on (the per-pass runtime guards still no-op on Low tier), and Issue 2 upgrades data to the current schema with values identical to the applied defaults.

## Tech Stack

C++20, CMake MSVC multi-config, doctest, OpenGL renderer, nlohmann::json.

---

## Task 1: Fold `v3Enabled` into `IsHdrScenePipelineRequested`

**Files:**
- Modify: `src/engine/render/RenderSystem.cpp` (line 286-290)

### Implementation rationale

`IsHdrScenePipelineRequested` currently returns true only when dynamic/volumetric lighting or an HDR post-process is requested. The V3 chain (shadowPrepare/Build/Resolve, lightCulling, lighting) is scheduled only under `renderConfig.v3Enabled && useHdrSceneBuffer`, and each V3 pass reads/writes the HDR scene buffer. Therefore enabling V3 must itself request the HDR scene buffer; otherwise the requested feature is silently skipped and the warning fires.

### Pseudocode

```text
bool IsHdrScenePipelineRequested(config):
    return config.dynamicLightingEnabled
        || config.volumetricLightEnabled
        || config.v3Enabled
        || IsHdrPostProcessRequested(config)
```

### Notes

- Keep the `else if (renderConfig.v3Enabled && !useHdrSceneBuffer)` warning branch at line 1367 — it remains the true error path (e.g. HDR buffer allocation failure sets `useHdrSceneBuffer=false` at line 1235).
- Do not touch per-pass runtime guards (`LightCullingPass`, `ShadowBuildPass`, etc.); on Low tier (`dynamicLighting=false`, `shadowEnabled=false`) they still no-op correctly.
- No existing test references this warning string; validation is build + manual smoke.

### Atomic tasks

- [ ] Add `config.v3Enabled` to `IsHdrScenePipelineRequested`.
- [ ] Build and confirm compile (see Test Method).

## Task 2: Upgrade `assets/data/materials_vfx.json` to schema 3

**Files:**
- Modify: `assets/data/materials_vfx.json`
- Modify: `tests/unit/MaterialTest.cpp` (line 69-95)

### Implementation rationale

The file declares `material_schema_version: 1` but the engine's `MATERIAL_SCHEMA_VERSION` is 3. For schema >= 3 the loader is strict (`strictSchema`), and `kSchemaV2RequiredFields` requires `normalMapSlot/roughness/specular/ao/heightBias/detailNormalScale`. Upgrading the data file to schema 3 with explicit values equal to `MaterialPresets::Default()` (the same values the loader currently applies) removes the warning while keeping render output byte-for-byte identical.

### Data change per material entry

Add to all 5 entries (FireExplosion/IceShatter/PoisonCloud/ShadowMist/HolyLight):

```json
"normalMapSlot": -1,
"roughness": 0.6,
"metallic": 0.0,
"specular": 0.2,
"ao": 1.0,
"heightBias": 0.0,
"detailNormalScale": 1.0
```

and bump `material_schema_version` from 1 to 3.

### Test rework

`[Unit] Material - Schema v1 Compatibility Loading` currently loads the real `assets/data/materials_vfx.json` to assert v1 default-fill behavior. After upgrading the data file this test would no longer exercise a v1 file. Convert it to an inline v1 JSON fixture written to a temp file (same pattern as `Schema v3 Strict Validation`), preserving the regression coverage for the v1 `apply_defaults` path.

### Pseudocode

```text
TEST_CASE "[Unit] Material - Schema v1 Compatibility Loading":
    path = MakeTempPath("tmp_materials_schema_v1_compat.json")
    WriteTextFile(path, inline v1 JSON with the 5 VFX materials)
    loaded = manager.LoadFromJson(path)
    CHECK(loaded >= 5)
    ... same default-value assertions on FireExplosion ...
    CleanupPath(path)
```

Keep the existing assertions (shader/blendMode/emissiveIntensity + default normalMapSlot/roughness/metallic/fresnelF0/rimSuppress/roughnessBias/specular/ao/detailNormalScale).

### Atomic tasks

- [ ] Rewrite `assets/data/materials_vfx.json` to schema 3 with explicit default values.
- [ ] Rework `Schema v1 Compatibility Loading` test to use inline v1 fixture.
- [ ] Add/keep a schema-3 load assertion path (other tests at lines 274/310/330 already load the upgraded file; verify they still pass with `loaded >= 5`).

## Test Method

- Build: `.\build.bat` (RelWithDebInfo).
- Unit: `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
- Targeted: `.\bin\NoMoreDayTests.exe --test-case="[Unit] Material*"`
- Integration (touches RenderSystem/material): `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`

## Verification / Definition of Done

1. Both warnings no longer appear in a fresh `bin/logs/NoMoreDay.log`:
   - No `skip V3 shadow/cluster passes` line.
   - No `schema_compatibility ... action=apply_defaults target_schema=3` for `materials_vfx.json`.
2. `Material*` unit tests pass.
3. Full unit + integration ctest buckets pass (no new regressions).
4. Render behavior unchanged on default (High) tier and on Low tier (V3 still no-ops per-pass when disabled).
5. Update `conductor/bug_registry.md` rows for BUG-20260222-004 / BUG-20260222-008 with evidence, and record memory mirrors.
