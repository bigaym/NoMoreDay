# V3 Shadow Pipeline Spec

> **Track ID**: `v3_shadow_pipeline_20260215`  
> **Type**: `feature`  
> **Priority**: P0  
> **Depends On**: `v3_baseline_contracts_20260216`  
> **对应设计文档**: [GPU_Rendering_System_3.md §5](../../设计文档/特效和UI/GPU_Rendering_System_3.md)  
> **实施路线**: Step B（第 2-4 周）

## 1. Goal

交付 V3 阴影能力：2.5D Hybrid Shadow 系统，支持质量分档、确定性回退和完整监控。

### 1.1 档位策略（对齐 §5.1）

| 档位 | 阴影行为 |
|---|---|
| **Medium** | 阴影关闭 |
| **High** | SDF Shadow |
| **Ultra** | Hybrid（关键光 Atlas + 非关键光 SDF）|

## 2. Scope

1. `src/engine/render/passes/ShadowPreparePass.*` (new)
2. `src/engine/render/passes/ShadowBuildPass.*` (new)
3. `src/engine/render/passes/ShadowResolvePass.*` (new)
4. `src/engine/render/GPUData.hpp` — 实例化 Shadow GPU 结构
5. `src/engine/render/shadow/ShadowAtlasAllocator.*` (new)
6. `src/engine/render/shadow/OccluderCollector.*` (new)
7. `assets/shaders/lighting/shadow_sdf.comp` (new)
8. `assets/shaders/lighting/shadow_resolve.frag` (new)
9. `assets/shaders/lighting/light_accumulation.frag` — 集成 shadowFactor
10. tests under `tests/unit`, `tests/integration`, `tests/performance`

## 3. Data Model

### 3.1 GPU 结构（对齐 §21.2）

```cpp
struct GPUShadowCaster {
    glm::vec2 position;
    float radius;
    float occluderHeight;
    uint32_t shapeIndex;
    uint32_t dynamicFlag;
    uint32_t reserved0;
    uint32_t reserved1;
};
// static_assert(sizeof(GPUShadowCaster) == 32)

struct GPUShadowLight {
    uint32_t lightId;
    uint32_t shadowType;    // 0=SDF, 1=Atlas
    glm::vec4 atlasRect;
    glm::vec4 penumbraParams;
};

struct GPUShadowAtlasMeta {
    uint32_t tileIndex;
    uint32_t lastUsedFrame;
    float priorityScore;
    float occupancy;
};
```

### 3.2 ABI 契约

- 上述结构必须通过 ABI V3 生成链路产出，禁止手写 GLSL 重复。
- `static_assert` 对齐检查已在 baseline contracts Track 中预留。

## 4. ECS and Systems

1. **新组件**: `ShadowCasterComponent`（遮挡体形状、高度、dynamic flag）。
2. **复用**: `LightComponent` 作为阴影光源候选。
3. **新服务**: `ShadowAtlasAllocator` — 确定性淘汰的 Atlas tile 分配器。
4. **新服务**: `OccluderCollector` — 遮挡体收集与分阶段上传。
5. **系统职责**:
   - 遮挡体收集（camera 邻域动态更新）
   - 关键光选择（`priority + screen influence` 排序）
   - Shadow Pass 调度与回退

## 5. 关键实现要点（对齐 §5.3，逐条覆盖）

### 5.1 静态遮挡体按 chunk 缓存

- 将世界划分为固定大小 chunk。
- 静态遮挡体仅在 chunk 首次进入视野时上传 GPU。
- chunk 离开视野后标记为冷数据但保留缓存（LRU 淘汰）。
- 避免每帧重复上传不变的遮挡体数据。

### 5.2 动态遮挡体仅更新 camera 邻域

- 定义 camera 邻域半径（常量化到 `RenderConstants.hpp`）。
- 每帧仅收集邻域内的动态遮挡体。
- 邻域外的动态遮挡体不参与阴影计算。

### 5.3 Atlas 仅分配给 Top-N 关键光

- 使用 `priority + screen influence` 复合评分。
- `maxShadowedLights` 控制 Top-N 上限（配置化）。
- 非 Top-N 光源使用 SDF fallback。

### 5.4 Atlas 溢出确定性淘汰

- 淘汰策略：优先级最低 + 最久未使用（LRU）。
- 淘汰需要 **滞回策略**：被淘汰的 tile 需要连续 N 帧低优先级才实际释放，避免抖动。
- 溢出计数器：每帧记录溢出次数到结构化日志。
- 测试覆盖：注入 overflow 场景验证确定性。

### 5.5 失败回退

- 任一 Shadow Pass 失败时，自动回退到 V2 Lighting 路径。
- 回退发生时发出 `spdlog::warn` 日志，包含失败原因和帧号。
- 回退后不影响其他 Pass 的正常执行。

## 6. 光照整合（对齐 §5.4）

`light_accumulation` 主路径升级为：

```
attenuation * shadowFactor * BRDF-lite
```

本 Track 仅负责 `shadowFactor` 的注入，BRDF-lite 由 Material Track 接入。

## 7. Persistence

1. Render settings JSON 必须序列化所有阴影相关字段。
2. 缺失字段加载默认值，不崩溃。
3. 地图/场景遮挡体元数据保持向后兼容。

## 8. Performance Budget（对齐 §15.2, §15.3）

| 指标 | 常规 | 高压 | 极限 |
|---|---:|---:|---:|
| Shadow* 总耗时 | 0.40ms | 0.90ms | 1.30ms |

- `High` 档阴影新增开销 ≤ 0.8ms。
- `Ultra` 档阴影新增开销 ≤ 1.3ms。
- 不得导致现有三档 FPS 目标回归。

## 9. 风险与缓解（对齐 §19）

| 风险 | 影响 | 缓解 |
|---|---|---|
| Shadow Atlas 溢出抖动 | 阴影闪烁 | 确定性淘汰 + 滞回策略 + 日志计数 |
| ABI 偏移错位 | 难排查渲染异常 | 生成链路唯一化 + layout 快照 |

## 10. Non-Goals

1. 不实现 Clustered Lighting（由 `v3_clustered_lighting` 覆盖）。
2. 不升级材质 BRDF 深度（由 `v3_material_lighting_depth` 覆盖）。
3. 不修改 VFX 序列器（由 `v3_vfx_lighting_integration` 覆盖）。

## 11. Acceptance Criteria

1. High/Ultra 档下动态遮挡体产生稳定、可见的柔和阴影。
2. Medium 档阴影完全关闭，无性能开销。
3. Resize、离屏路径、context restore 稳定（无黑屏、无泄漏）。
4. ABI/版本测试和 RenderGraph 合同测试通过。
5. Atlas 溢出的确定性淘汰行为可复现。
6. 回退到 V2 后视觉和性能表现与 V2 一致。
7. `build.bat`、`build.bat analyze`、shadow perf 测试通过。
