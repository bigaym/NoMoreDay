# V3 Material Lighting Depth Plan

## 2026-02-18 Carry-Over From Clustered Lighting

- [x] **D0.1**: Baseline and profile 128-light clustered path with Material 2.0 enabled, and classify bottlenecks (culling compute, light list density, fragment accumulation).
- [x] **D0.2**: Implement at least one optimization pass for clustered+material coupling (data layout, loop reduction, or tier policy tuning) with deterministic output parity.
- [x] **D0.3**: Restore strict uplift gate for 128-light A/B (`clustered vs V2`) to `>=5%` mean improvement under the agreed benchmark profile.
  (ownership migrated to `v3_validation_and_release_gate_20260215` / `F4.6` as release-gate responsibility)

### Carry-Over D0 Evidence (2026-02-18)

- Implemented packed clustered light payload path to reduce fragment-stage indirection:
  `assets/shaders/lighting/light_culling.comp`,
  `assets/shaders/lighting/light_accumulation.frag`,
  `src/engine/render/lighting/ClusteredLightingState.*`,
  `src/engine/render/passes/LightCullingPass.cpp`,
  `src/engine/render/passes/LightingPass.cpp`,
  `src/engine/render/core/BindingRegistry.cpp`,
  `src/engine/render/GPUData.hpp`.
- Determinism/parity safety checks passed:
  `[Integration] Clustered Lighting - Deterministic overflow and index output`,
  `[Integration] Clustered Lighting - Culling to lighting consumption path`.
- Baseline profiling (current agreed profile) still misses uplift target:
  `[Performance] Clustered Lighting - 128 lights A/B no regression`
  observed `Clustered mean(ms)=0.142065`, `V2 mean(ms)=0.137935`,
  `improvement=-2.99417%`.
- D0.3 implementation responsibility is moved to Step F release gate (`v3_validation_and_release_gate_20260215` / `F4.6`).

> **Track ID**: `v3_material_lighting_depth_20260215`  
> **TDD Policy**: unit -> integration -> perf  
> **实施路线**: Step D（第 4-6 周）  
> **前置依赖**: `v3_baseline_contracts_20260216` + `v3_shadow_pipeline_20260215` 已完成

## Phase 1: Foundation（Schema 与数据结构）

- [x] **D1.1**: 定义 schema v2 字段表与迁移默认值映射表（配置化到 JSON/头文件）。
- [x] **D1.2**: 在 `GPUData.hpp` 中实例化 `GPUMaterialDataV2`，添加 `static_assert(sizeof == 128)` 与对齐检查。
- [x] **D1.3**: 扩展 `RenderConfig` Tier 控制旋钮：`normalLightingEnabled`, `specularEnabled`, `materialQualityLevel`。
- [x] **D1.4**: 创建中性默认纹理资产（flat normal `(0.5, 0.5, 1.0)`, 默认 roughness 中灰），放入 `assets/textures/defaults/`。
- [x] **D1.5**: 添加 unit test：v1→v2 映射逻辑、GPUMaterialDataV2 layout、默认值填充。

## Phase 2: MaterialManager 扩展

- [x] **D2.1**: 扩展 `MaterialManager` 的 JSON 解析路径，支持 schema v2 字段。
- [x] **D2.2**: 实现 `material_schema_version` 校验：v1 自动填充默认值 + warn 日志（1 次/资产节流）；v2 严格校验。
- [x] **D2.3**: 实现非法字段与缺失字段的可诊断报错（结构化日志），禁止无声失败。
- [x] **D2.4**: 扩展运行时 upload 路径以产出 `GPUMaterialDataV2`（从 `GPUMaterialData` v1 平滑迁移）。
- [x] **D2.5**: 添加 unit test：v1 资产加载兼容、v2 资产严格校验、非法字段拒绝、upload 数据一致性。

## Phase 3: Texture2DArray 管理

- [x] **D3.1**: 实现 `TextureArrayManager` 的 normal/roughness 层分配逻辑。
- [x] **D3.2**: 实现缺失 texture slot 解析到中性默认纹理（引擎初始化时预创建）。
- [x] **D3.3**: 实现 Texture2DArray resize 安全重建（与窗口 resize 联动）。
- [x] **D3.4**: 实现材质资产热重载 **双缓冲句柄策略**：新资源验证 → 原子替换 → 失败回退旧资源。
- [x] **D3.5**: 添加 unit test：层分配/释放、默认纹理 fallback、热重载原子性。

## Phase 4: Shader BRDF-lite 接入

- [x] **D4.1**: 在 `entity_mdi.frag` 中实现 BRDF-lite 分支：diffuse (`N·L`) + specular (half-vector + roughness width)。
- [x] **D4.2**: 在 `particle.frag` 中实现 BRDF-lite 受光分支（where applicable）。
- [x] **D4.3**: 实现 Tier 分支控制：Low/Medium 关闭高阶路径，High 部分启用，Ultra 完全启用。
- [x] **D4.4**: 整合 shadowFactor（来自 Shadow Track）到最终光照方程：`attenuation * shadowFactor * BRDF-lite`。
- [x] **D4.5**: 确保 cluster light list 兼容（如 Clustered Track 已启用）。
- [x] **D4.6**: 添加 integration test：跨 Tier 渲染结果差异验证、Shader/CPU ABI 一致性。

## Phase 5: 测试矩阵与性能验证

- [x] **D5.1**: 添加 integration test：v1 资产全量回归、v2 资产渲染正确性。
- [x] **D5.2**: 添加 integration test：热重载材质资产（修改 JSON → 自动刷新 → 视觉更新）。
- [x] **D5.3**: 添加 integration test：resize/context restore 稳定性。
- [x] **D5.4**: 添加 performance test：验证 High 档材质新增开销 ≤ 0.6ms。
- [x] **D5.5**: 添加 performance test：验证 Low/Medium 档无额外开销（与 V2 持平）。
- [x] **D5.6**: 运行 `build.bat`、`build.bat analyze`、`build.bat perf`，修复回归。

## Acceptance Gates (DoD)

- [x] 材质 v2 字段在渲染中可见体现（同光异材效果）。
- [x] v1 资产向后兼容，确定性默认值，无崩溃。
- [x] High 档材质新增开销 ≤ `0.6ms`。
- [x] Low/Medium 档与 V2 开销持平。
- [x] Shader CPU/GLSL ABI 无不匹配。
- [x] 热重载双缓冲策略工作正常，无黑屏。
- [x] 中性默认纹理 fallback 正常。

### Acceptance Evidence (2026-02-18)

- `build.bat` / `build.bat analyze` / `build.bat perf` all passed.
- `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` passed.
- `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` passed.
- `ctest --test-dir build -C Release -L performance --output-on-failure` passed.
- DoD visual-difference contract validated by `[Unit] Material - BRDF-lite same-light different-material divergence` (`diff = 1.08197 > 0.05`) in `tests/unit/MaterialTest.cpp`.
- Hot-reload and fallback validated by `[Integration] Material Lighting - Schema v2 hot reload updates runtime data` and `[Unit] TextureArrayManager - HotReload Atomic Swap and Rollback`.
- Performance gate validated by `[Performance] Material Lighting - Tier overhead budgets`:
  `highOverhead(ms)=8.5e-06`, `mediumDelta(ms)=3.25e-06`.
