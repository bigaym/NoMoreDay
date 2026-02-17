# V3 Baseline Contracts Plan

> **Track ID**: `v3_baseline_contracts_20260216`  
> **TDD Policy**: unit -> integration  
> **实施路线**: Step A（第 1 周）  
> **约束**: 本 Track 完成后视觉输出必须与 V2 完全一致

## Phase 1: RenderConfig & Feature Flag

- [x] **A1.1**: 在 `RenderConfig` 中添加所有 V3 字段（shadow/clustered/material/v3Enabled），提供安全默认值。
- [x] **A1.2**: 实现 `render.v3.enabled` Feature Flag 序列化/反序列化路径。
- [x] **A1.3**: 实现 Flag 运行时切换的资源安全释放逻辑骨架。
- [x] **A1.4**: 添加 unit test：RenderConfig V3 字段序列化往返、缺失字段默认值加载、无效值拒绝。

## Phase 2: GPU ABI V3

- [x] **A2.1**: 将 `GPU_ABI_VERSION` 从 `2` 递增为 `3`。
- [x] **A2.2**: 在 `GPUData.hpp` 中为 V3 结构预留接口占位（`GPUShadowCaster`, `GPUShadowLight`, `GPUShadowAtlasMeta`, `GPUClusterHeader`, `GPUClusterLightIndex`, `GPUMaterialDataV2`），添加 `static_assert` 对齐检查。
- [x] **A2.3**: 更新 ABI 生成链路工具（`tools/render_abi/`），确保 C++ 与 GLSL struct 同源。
- [x] **A2.4**: 添加 CI 检查：禁止手写重复 GLSL struct（grep 脚本 + 构建门禁）。
- [x] **A2.5**: 添加 layout 快照测试：ABI V3 所有结构的 size/alignment 断言。

## Phase 3: Pass 顺序 & Binding & Frame Ownership

- [x] **A3.1**: 在 RenderGraph 中注册 V3 Pass 占位节点（`LightCulling`, `ShadowPrepare`, `ShadowBuild`, `ShadowResolve`），启用时为 no-op。
- [x] **A3.2**: 锁定 Pass 顺序合同：`Scene -> LightCulling -> Shadow -> Lighting -> Volumetric -> VFX -> UIWorld -> PostProcess -> Distortion -> Composite`。
- [x] **A3.3**: 在 `BindingRegistry` 中定义 V3 新增 Pass 的局部域，添加冲突检查断言。
- [x] **A3.4**: 文档化并代码化 Frame Ownership 规则（§12）：HDRScene/LDR/FBO0 写权限断言。
- [x] **A3.5**: 文档化并代码化 GL 状态同步契约（§13）：Compute->Fragment barrier 模板，rlgl flush 模板。
- [x] **A3.6**: 添加 integration test：Pass 顺序违反检测、Binding 冲突检测、Frame Ownership 违反检测。

## Phase 4: Quality Tier & 预算常量

- [x] **A4.1**: 在 `RenderConstants.hpp` 中定义 V3 Pass 预算常量（§15.2 表格全部常量化）。
- [x] **A4.2**: 在 `QualityTierManager` 中注册 V3 降级序列常量（§9.2 的 6 级降级顺序）。
- [x] **A4.3**: 在 `QualityTierManager` 中添加 V3 能力矩阵（§9.1 的 Low/Medium/High/Ultra 四档）。
- [x] **A4.4**: 添加 unit test：降级序列正确性、能力矩阵查询正确性。

## Phase 5: 验证与闭环

- [x] **A5.1**: 运行 `build.bat` 确认编译通过。
- [x] **A5.2**: 运行全量 unit + integration test，确认无回归。
- [x] **A5.3**: 确认 Feature Flag `v3Enabled=false` 时视觉输出与 V2 完全一致。
- [x] **A5.4**: 确认 Feature Flag `v3Enabled=true` 时 no-op Pass 正确跳过，无崩溃。

## Acceptance Gates (DoD)

- [x] `GPU_ABI_VERSION=3` layout 快照测试全部通过。
- [x] Binding Registry 冲突检查通过。
- [x] RenderGraph 合同验证（Pass 顺序 + Frame Ownership）通过。
- [x] Feature Flag 开/关切换无崩溃，V2 视觉不变。
- [x] `build.bat` 编译通过，无新增 warning。
