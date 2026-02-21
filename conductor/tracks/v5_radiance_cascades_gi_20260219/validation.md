# Validation - v5_radiance_cascades_gi_20260219

## Scope

- Track: `v5_radiance_cascades_gi_20260219`
- Sync Date: 2026-02-21
- Goal: 落地 Radiance Cascades GI 主链路（Emissive -> Cascades -> Composite）并完成首轮验证。

---

## Implementation Evidence

### Code

- Added GI passes:
  - `src/engine/render/passes/RadianceCascadesPass.hpp`
  - `src/engine/render/passes/RadianceCascadesPass.cpp`
  - `src/engine/render/passes/GICompositePass.hpp`
  - `src/engine/render/passes/GICompositePass.cpp`
- Added shaders:
  - `assets/shaders/lighting/v5_emissive_build.comp`
  - `assets/shaders/lighting/v5_emissive_particle.comp`
  - `assets/shaders/lighting/v5_emissive_merge.comp`
  - `assets/shaders/lighting/v5_radiance_cascade.comp`
  - `assets/shaders/lighting/v5_gi_composite.comp`
- RenderGraph / runtime integration:
  - `src/engine/render/RenderSystem.cpp`
  - `src/engine/render/graph/RenderContext.hpp`
- Contract test update:
  - `tests/integration/RenderGraphV5ContractsIntegrationTest.cpp`

### Delivered in this sync

- Emissive buffer pipeline（含 LightManager 投影 + 粒子子缓冲原子计数 + 合并）
- Radiance cascades trace + merge（4/6 级联，half/full-res，barrier 同步）
- GI composite（`giIntensity`、temporal blend、相机运动自适应、光源变更 reset）
- `render.gi.enabled` 运行时门控链路打通
- Holographic exploratory mode（`giHolographicEnabled`）原型入口

---

## Verification Commands

```powershell
build.bat
ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure
ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure
ctest --test-dir build -C Release -L performance --output-on-failure
```

## Verification Result (2026-02-21)

- `build.bat`: PASS
- `ctest -C RelWithDebInfo -L ci`: PASS
- `ctest -C RelWithDebInfo -L integration`: PASS
- `ctest -C Release -L performance`: FAIL（non-blocking, 非本 track 阻塞）
  - Failing case: `[Performance] ParticleTrail - Scenario 4 SubEmitter 1k/frame`
  - Observed: `dispatchOverheadMs=0.259455 > 0.2`
  - Linked existing issue: `BUG-20260219-004`

---

## Remaining Work

- Task 1.3：接入真实 `GPUMaterialDataV3 Mask.A` 材质 emission 链路（当前为 scene HDR 高亮近似）
- Phase 4 验证：`4.4/4.5/4.6`
- Phase 5 验证：`5.2/5.4/5.5/5.6`

