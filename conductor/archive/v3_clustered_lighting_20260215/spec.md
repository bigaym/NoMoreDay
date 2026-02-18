# V3 Clustered 2D Lighting Spec

> **Track ID**: `v3_clustered_lighting_20260215`  
> **Type**: `feature`  
> **Priority**: P0  
> **Depends On**: `v3_baseline_contracts_20260216`  
> **对应设计文档**: [GPU_Rendering_System_3.md §6](../../设计文档/特效和UI/GPU_Rendering_System_3.md)  
> **实施路线**: Step C（第 3-5 周）

## 1. Goal

将光照从"逐像素遍历全部光源"升级为"逐 cluster 遍历局部光源列表"，在 ≥128 光源场景下实现 ≥25% 的 Lighting 耗时降低，同时保持视觉一致性和确定性回退。

## 2. Scope

1. `src/engine/render/passes/LightCullingPass.*` (new)
2. `src/engine/render/passes/LightingPass.*` — 消费 cluster list
3. `src/engine/render/GPUData.hpp` — 实例化 Cluster GPU 结构
4. `src/engine/render/core/RenderConstants.hpp` — Cluster 常量
5. `src/engine/render/lighting/ClusteredLightingState.*` (new)
6. `assets/shaders/lighting/light_culling.comp` (new)
7. `assets/shaders/lighting/light_accumulation.frag` — cluster 读取分支
8. `src/engine/render/core/QualityTierManager.*` — Tier 默认值
9. tests under `tests/unit`, `tests/integration`, `tests/performance`

## 3. Data Model

### 3.1 GPU 结构（对齐 §21.3）

```cpp
struct GPUClusterHeader {
    uint32_t offset;         // 在 light index buffer 中的起始偏移
    uint32_t count;          // 当前 cluster 内的光源数量
    uint32_t overflowCount;  // 被裁剪掉的光源计数
    uint32_t reserved;
};

struct GPUClusterLightIndex {
    uint32_t lightIndex;
};

struct GPULightBounds {
    glm::vec2 minXY;
    glm::vec2 maxXY;
    float minLayer;     // 最小渲染层/高度带
    float maxLayer;     // 最大渲染层/高度带
    uint32_t lightIndex;
    uint32_t reserved;
};
```

### 3.2 关键常量（提取到 RenderConstants.hpp）

```cpp
constexpr uint32_t kDefaultClusterTileSize = 32;
constexpr uint32_t kDefaultClusterZSliceCount = 4;
constexpr uint32_t kMaxLightsPerCluster = 64;
constexpr uint32_t kMaxTotalClusteredLights = 4096;
```

## 4. 算法形态（对齐 §6.1）

### 4.1 Cluster 维度

- `tile(x, y)`: 屏幕空间 2D 网格，tile 大小由 `clusterTileSize` 控制。
- `layer(z)`: 来自 **渲染层/高度带**，不依赖真实 3D 深度。
- z-layer 映射方法：将实体的渲染层 ID 映射到 `clusterZSliceCount` 个离散 slice。

### 4.2 两阶段裁剪（对齐 §6.3）

1. **粗筛**: AABB 包围盒检测（光源 bounds vs cluster bounds）。
2. **精筛**: 半径/锥体精确检测。
3. `maxLightsPerCluster` 固定上限，溢出时按优先级裁剪并计数。

## 5. ECS and Systems

1. 复用 `LightComponent`。
2. 复用渲染层/高度元数据（现有组件）。
3. **新单例**: `ClusteredLightingState` — 管理 buffer pool、帧计数、cluster 索引表。
4. **新 Pass**: `LightCullingPass` — compute shader 构建 cluster -> light list。

## 6. Persistence

1. 序列化 `clusteredLightingEnabled`、`clusterTileSize`、`clusterZSliceCount`。
2. 缺失字段映射到安全默认值（`clusteredLightingEnabled=false`）。
3. Tier 策略在运行时控制默认值。

## 7. GL 同步契约（对齐 §13.2）

`LightCullingPass`(compute) 输出 -> `LightingPass`(fragment) 读取：

- 必须插入 `glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT)` 显式屏障。
- barrier 点必须代码化为可审计的显式调用，禁止隐式依赖。

## 8. 回退策略（对齐 §6.4）

当 `clusteredLightingEnabled=false` 或 Pass/资源失败时：

1. 回退到 V2 全光遍历路径。
2. 保持视觉输出确定性。
3. 发出 warning 日志，含节流逻辑（once-per-window）。

## 9. Performance Budget（对齐 §15.2, §15.3）

| Pass | 常规 | 高压 | 极限 |
|---|---:|---:|---:|
| LightCullingPass | 0.15ms | 0.30ms | 0.45ms |
| LightingPass | 0.60ms | 1.00ms | 1.30ms |

**专项目标**: ≥128 lights 场景下，Clustered Lighting 使 Lighting 平均耗时下降 ≥25%。

## 10. 风险与缓解（对齐 §19）

| 风险 | 影响 | 缓解 |
|---|---|---|
| Cluster 溢出导致漏光 | 视觉错误 | 固定裁剪优先级 + 溢出统计 + 回归用例 |
| Tier 降级抖动 | 帧时间波动 | 降级冷却时间 + 恢复阈值滞回 |

## 11. Acceptance Criteria

1. 常规场景视觉结果与 V2 等价或更优。
2. ≥128 lights 场景 Lighting 平均耗时提升 ≥25%。
3. 无跨 Tier 闪烁/漏光回归。
4. 溢出、空场景、边界条件行为确定性且有测试覆盖。
5. Overflow 计数器可通过日志/profiler 读取。
6. 低光源场景无性能回归（等于或快于 baseline）。
7. Build、analyze、performance 套件通过。
