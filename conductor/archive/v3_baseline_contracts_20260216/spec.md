# V3 Baseline Contracts Spec

> **Track ID**: `v3_baseline_contracts_20260216`  
> **Type**: `foundation`  
> **Priority**: P0 (所有 V3 feature track 的前置依赖)  
> **对应设计文档**: [GPU_Rendering_System_3.md §3, §4, §10-§14](../../设计文档/特效和UI/GPU_Rendering_System_3.md)  
> **实施路线**: Step A（第 1 周）

## 1. Goal

在不改变任何视觉输出的前提下，落地 V3 的全部架构契约与框架基础设施，使后续 feature track 可以在稳定的 ABI、Binding、Pass 顺序和 Feature Flag 基线上独立并行推进。

## 2. Scope

1. `src/engine/render/core/RenderConstants.hpp` — Pass 预算常量、Cluster/Shadow 硬限常量
2. `src/engine/render/core/RenderConfig.hpp` — V3 配置字段扩展
3. `src/engine/render/GPUData.hpp` — ABI V3 结构预占位
4. `src/engine/render/graph/RenderGraph.*` — Pass 顺序锁定
5. `src/engine/render/core/BindingRegistry.*` — Binding 域治理
6. `tools/render_abi/*` — GPU ABI 生成链路升级
7. `src/engine/render/core/QualityTierManager.*` — V3 降级序列
8. 配置/序列化路径 — Feature Flag `render.v3.enabled`
9. tests under `tests/unit`, `tests/integration`

## 3. 设计决策锁定（对齐 §3）

1. **架构延续**: 保持单主 RenderGraph，不新增并行主渲染管线。
2. **性能红线**: 三档目标不降低（270 / 180 / 144 FPS）。
3. **契约优先**: ABI、Binding、Frame Ownership、Schema 全部强校验。
4. **兼容策略**: 资产允许 N/N-1，关键契约变更必须版本递增。
5. **回退兜底**: V3 全程可 feature-flag 回退到 V2。
6. **内容优先级**: 参数前置到材质/VFX schema，减少 shader 硬编码分叉。

## 4. RenderConfig V3 扩展（对齐 §21.1）

```cpp
enum class ShadowMode : uint8_t { Off = 0, SDF = 1, Hybrid = 2 };

struct RenderConfig {
    // --- Shadow (Step B) ---
    bool shadowEnabled = false;
    ShadowMode shadowMode = ShadowMode::Off;
    uint32_t maxShadowedLights = 4;
    uint32_t shadowAtlasSize = 2048;
    float shadowSoftness = 1.0f;

    // --- Clustered Lighting (Step C) ---
    bool clusteredLightingEnabled = false;
    uint32_t clusterTileSize = 32;
    uint32_t clusterZSliceCount = 4;

    // --- Material 2.0 (Step D) ---
    bool normalLightingEnabled = false;
    bool specularEnabled = false;
    uint32_t materialQualityLevel = 0;

    // --- V3 Feature Flag ---
    bool v3Enabled = false;
};
```

所有字段必须：
- 提供安全默认值（V2 行为不变）。
- JSON 序列化/反序列化往返测试覆盖。
- 缺失字段加载时不崩溃，使用默认值并输出 warning 日志。

## 5. GPU ABI V3 契约（对齐 §10）

1. `GPU_ABI_VERSION` 从 `2` 递增为 `3`。
2. C++ 结构与 GLSL 结构由同一生成链路产出，禁止手写重复 GLSL struct。
3. 所有结构变化必须同步快照与 layout 测试（`static_assert` + CI 断言）。
4. ABI 不匹配必须显式错误并阻断继续运行。
5. GPUData.hpp 中为后续 Track 预留结构占位（`GPUShadowCaster`, `GPUClusterHeader`, `GPUMaterialDataV2` 等），但本 Track 仅定义接口，不实现逻辑。

## 6. Binding Registry 与 Pass 命名域（对齐 §11）

### 6.1 全局域

长期驻留资源的 binding 集中在 `RenderConstants` 管理，使用命名常量。

### 6.2 Pass 局部域

1. 每个 Pass 的局部 binding 独立且不可跨 Pass 假设复用。
2. **禁止字面量 binding**（如硬写 `BindBase(4)`），必须通过 `BindingRegistry` 获取。
3. 冲突检查纳入构建门禁。

## 7. Pass 顺序锁定（对齐 §4.1）

```
Scene -> LightCulling -> Shadow -> Lighting -> Volumetric -> VFX -> UIWorld -> PostProcess -> Distortion -> Composite
```

RenderGraph 合同验证必须断言此顺序，任何偏离导致构建失败。

## 8. Frame Ownership 与生命周期（对齐 §12）

1. `Scene` 写 `HDRScene`
2. `Shadow/Lighting` 读写照明链路中间目标
3. `PostProcess` 写 `LDR` 链路
4. `Composite` 才允许写最终屏幕目标（FBO 0）
5. 持久资源随 resize 安全重建
6. 临时资源走 pool 申请与回收
7. 禁止未声明资源读写

## 9. GL 状态与同步契约（对齐 §13）

1. 每个 Pass 进入/退出时满足基线状态约束。
2. Compute -> Fragment 的依赖路径必须显式插入 `glMemoryBarrier`。
3. rlgl 互操作：进入自定义 GL 阶段前统一 flush，离开时恢复基线状态。

## 10. Quality Tier V3 降级序列（对齐 §9.2）

推荐自动降级顺序常量化：
1. 降低 Bloom 级别
2. 关闭 Distortion
3. 限制动态光数量
4. 关闭 Clustered 高压参数
5. Hybrid 阴影降级为 SDF
6. 关闭高阶材质分支

## 11. 性能预算常量（对齐 §15）

```cpp
// RenderConstants.hpp
constexpr float kBudgetLightCulling_Normal = 0.15f;  // ms
constexpr float kBudgetLightCulling_High   = 0.30f;
constexpr float kBudgetLightCulling_Extreme = 0.45f;

constexpr float kBudgetShadow_Normal  = 0.40f;
constexpr float kBudgetShadow_High    = 0.90f;
constexpr float kBudgetShadow_Extreme = 1.30f;

constexpr float kBudgetLighting_Normal  = 0.60f;
constexpr float kBudgetLighting_High    = 1.00f;
constexpr float kBudgetLighting_Extreme = 1.30f;
```

## 12. Feature Flag 基础设施

1. `render.v3.enabled` 控制 V3 rollout。
2. Flag 为 `false` 时，所有 V3 Pass 被跳过，回退到 V2 路径。
3. Flag 可运行时动态切换，切换时安全释放 V3 资源。

## 13. Acceptance Criteria

1. `RenderConfig` V3 字段序列化往返测试通过。
2. `GPU_ABI_VERSION=3` layout 快照测试通过。
3. Binding Registry 冲突检查通过。
4. RenderGraph 合同验证（Pass 顺序 + Frame Ownership）通过。
5. Feature Flag V3 开/关切换无崩溃。
6. 视觉输出与 V2 完全一致（无功能变化）。
7. `build.bat` 编译通过。

## 14. Completion Record

- Completed on: `2026-02-17`
- Validation summary: `build.bat`, `build.bat analyze`, `ctest(unit/integration/performance)` all passed.
