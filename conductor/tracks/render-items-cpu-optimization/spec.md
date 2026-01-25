# Rendering Items CPU Optimization Spec

## 1. 问题陈述 (Problem Statement)
当前物品渲染流程存在严重的 CPU 瓶颈，导致在高负载场景下（10倍怪物掉落）帧率从理想值显著下降。计时器数据显示：
- `RenderSystem::Total`: ~3.8ms (其中 `ItemsLabels` 占 ~2.9ms)
- `UISystem::Draw`: ~1.3ms (其中 `GroundHover` 占 ~0.65ms)
- **Total Overhead**: ~4.2ms / frame

结合 GPU 降频现象（650MHz），推测大量 Immediate Mode 渲染（光柱）和频繁的 CPU-Driver 交互导致了 GPU 饥饿和 CPU 阻塞。

| 指标 | 现状 (Baseline) | 瓶颈描述 |
| :--- | :--- | :--- |
| **FPS (大量掉落)** | ~80 FPS (12.5ms) | 相比纯 MDI (100 FPS) 损失 2.5ms+，主要在 CPU 逻辑 |
| **UISystem::Draw** | $O(N_{total})$ 遍历 | 每帧对所有掉落物进行 `GetWorldToScreen2D` 计算，含昂贵的矩阵乘法 |
| **RenderSystem::render** | 冗余遍历 | 尽管 Label 已经 Instanced，Beams 渲染仍使用 Immediate Mode (GL 1.x style) |
| **Draw Call** | $2 \times N_{beams}$ | 每个稀有物品产生 2 个独立的 Draw Call (Beam + Core) |

## 2. 优化目标 (Optimization Goals)
| 指标 | 目标值 (Target) | 提升幅度 |
| :--- | :--- | :--- |
| **FPS (大量掉落)** | > 110 FPS (< 9ms) | +30% |
| **UI Item Check** | $O(N_{visible})$ | 仅遍历屏幕内物品 |
| **Beams Draw Call** | 1 (Instanced) | 减少 99% (假设 200 个光柱) |
| **String Ops** | 零分配 (Zero-Alloc) | 消除 `DrawText` 带来的字符串拷贝开销 |

## 3. 技术方案 (Technical Solution)

### 3.1 引入 `VisibleItemCache` (单次遍历多处复用)
在 `RenderSystem` 的主循环中，一次性完成视锥剔除（Frustum Culling），并将通过剔除的实体 ID 存入静态缓存。
`UISystem` 将直接复用此缓存，彻底移除自身的 $O(N)$ 全量遍历。

```cpp
// src/engine/render/RenderSystem.hpp
struct VisibleItemCache {
    static std::vector<entt::entity> entities;
    static std::vector<Vector2> screenPositions; // 预计算屏幕坐标，供 UI 使用
    static void Clear() { entities.clear(); screenPositions.clear(); }
};
```

### 3.2 Beam 渲染 Instancing 化 (GPU Instancing)
将光柱渲染迁移至 GPU Instancing 管道。
新增 `GPUBeamInstance` 结构体和对应的 Shader（或者复用 Label Shader，如果支持多 Pass 或 UberShader，但为了简单可能新增一个 `beam_instanced.vert/frag`）。

**Shader Binding 5**:
```cpp
struct GPUBeamInstance {
    vec2 position;
    vec2 size;      // width, height
    vec4 centerColor;
    vec4 edgeColor;
    float time;     // For pulsing animation
    float padding[3]; // Align to 64 bytes if needed, or 16
};
```

### 3.3 字符串与字体优化
- 将 `DrawTextEx` 的使用限制在仅“可见且需要更新”的场景。
- 利用 `LabelCacheComponent` 避免重复测量文本。

## 4. 数据结构变更 (Data Structures)
无核心 ECS 组件变更。
新增 `src/engine/render/GPUBeamData.hpp` (仅渲染层使用)。

## 5. 验收标准 (Acceptance Criteria)
1. **性能**: 在 10 倍掉落场景下，`ScoptedTimer` 显示 `RenderSystem::render` 和 `UISystem::Draw` 的 CPU 耗时总和减少 2ms 以上。
2. **正确性**: 鼠标悬停物品、捡起物品功能无异常。
3. **视觉**: 光柱特效与原有 Immediate Mode 效果一致（包含脉冲动画）。

```
