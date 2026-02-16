# V3 Material Lighting Depth Plan

> **Track ID**: `v3_material_lighting_depth_20260215`  
> **TDD Policy**: unit -> integration -> perf  
> **实施路线**: Step D（第 4-6 周）  
> **前置依赖**: `v3_baseline_contracts_20260216` + `v3_shadow_pipeline_20260215` 已完成

## Phase 1: Foundation（Schema 与数据结构）

- [ ] **D1.1**: 定义 schema v2 字段表与迁移默认值映射表（配置化到 JSON/头文件）。
- [ ] **D1.2**: 在 `GPUData.hpp` 中实例化 `GPUMaterialDataV2`，添加 `static_assert(sizeof == 128)` 与对齐检查。
- [ ] **D1.3**: 扩展 `RenderConfig` Tier 控制旋钮：`normalLightingEnabled`, `specularEnabled`, `materialQualityLevel`。
- [ ] **D1.4**: 创建中性默认纹理资产（flat normal `(0.5, 0.5, 1.0)`, 默认 roughness 中灰），放入 `assets/textures/defaults/`。
- [ ] **D1.5**: 添加 unit test：v1→v2 映射逻辑、GPUMaterialDataV2 layout、默认值填充。

## Phase 2: MaterialManager 扩展

- [ ] **D2.1**: 扩展 `MaterialManager` 的 JSON 解析路径，支持 schema v2 字段。
- [ ] **D2.2**: 实现 `material_schema_version` 校验：v1 自动填充默认值 + warn 日志（1 次/资产节流）；v2 严格校验。
- [ ] **D2.3**: 实现非法字段与缺失字段的可诊断报错（结构化日志），禁止无声失败。
- [ ] **D2.4**: 扩展运行时 upload 路径以产出 `GPUMaterialDataV2`（从 `GPUMaterialData` v1 平滑迁移）。
- [ ] **D2.5**: 添加 unit test：v1 资产加载兼容、v2 资产严格校验、非法字段拒绝、upload 数据一致性。

## Phase 3: Texture2DArray 管理

- [ ] **D3.1**: 实现 `TextureArrayManager` 的 normal/roughness 层分配逻辑。
- [ ] **D3.2**: 实现缺失 texture slot 解析到中性默认纹理（引擎初始化时预创建）。
- [ ] **D3.3**: 实现 Texture2DArray resize 安全重建（与窗口 resize 联动）。
- [ ] **D3.4**: 实现材质资产热重载 **双缓冲句柄策略**：新资源验证 → 原子替换 → 失败回退旧资源。
- [ ] **D3.5**: 添加 unit test：层分配/释放、默认纹理 fallback、热重载原子性。

## Phase 4: Shader BRDF-lite 接入

- [ ] **D4.1**: 在 `entity_mdi.frag` 中实现 BRDF-lite 分支：diffuse (`N·L`) + specular (half-vector + roughness width)。
- [ ] **D4.2**: 在 `particle.frag` 中实现 BRDF-lite 受光分支（where applicable）。
- [ ] **D4.3**: 实现 Tier 分支控制：Low/Medium 关闭高阶路径，High 部分启用，Ultra 完全启用。
- [ ] **D4.4**: 整合 shadowFactor（来自 Shadow Track）到最终光照方程：`attenuation * shadowFactor * BRDF-lite`。
- [ ] **D4.5**: 确保 cluster light list 兼容（如 Clustered Track 已启用）。
- [ ] **D4.6**: 添加 integration test：跨 Tier 渲染结果差异验证、Shader/CPU ABI 一致性。

## Phase 5: 测试矩阵与性能验证

- [ ] **D5.1**: 添加 integration test：v1 资产全量回归、v2 资产渲染正确性。
- [ ] **D5.2**: 添加 integration test：热重载材质资产（修改 JSON → 自动刷新 → 视觉更新）。
- [ ] **D5.3**: 添加 integration test：resize/context restore 稳定性。
- [ ] **D5.4**: 添加 performance test：验证 High 档材质新增开销 ≤ 0.6ms。
- [ ] **D5.5**: 添加 performance test：验证 Low/Medium 档无额外开销（与 V2 持平）。
- [ ] **D5.6**: 运行 `build.bat`、`build.bat analyze`、`build.bat perf`，修复回归。

## Acceptance Gates (DoD)

- [ ] 材质 v2 字段在渲染中可见体现（同光异材效果）。
- [ ] v1 资产向后兼容，确定性默认值，无崩溃。
- [ ] High 档材质新增开销 ≤ `0.6ms`。
- [ ] Low/Medium 档与 V2 开销持平。
- [ ] Shader CPU/GLSL ABI 无不匹配。
- [ ] 热重载双缓冲策略工作正常，无黑屏。
- [ ] 中性默认纹理 fallback 正常。
