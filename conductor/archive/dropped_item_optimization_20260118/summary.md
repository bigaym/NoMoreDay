# Track Archive: Dropped Item Performance Optimization (2026-01-18)

## 1. 核心目标 (Core Objective)
在同屏掉落 10,000+ 物品/金币的极端情况下，将帧率从 120-150 FPS 提升并稳定在 **180 FPS++**，确保 UI 交互无延迟且渲染管线达到极限效率。

## 2. 性能瓶颈分析 (Bottleneck Analysis)
- **CPU Bound**:
    - **MeasureTextEx**: 每帧对数千个标签调用文字测量，导致主线程卡顿。
    - **Spatial Grid Rebuild**: 即使物品不移动，空间哈希表仍每帧全量重建（Sorting + Hashing），开销高达 3ms+。
    - **Iterative Check**: 渲染系统在绘制精灵时，需要线性检查每个实体是否为物品，导致缓存频繁失效。
- **GPU Bound**:
    - **Batching Breaks**: 在绘制物品标签时，交替调用 `DrawRectangle` 和 `DrawTextEx` 导致渲染批次频繁中断，DrawCall 激增。

## 3. 设计方案 (Technical Specs)

### 3.1 Label Cache (文字缓存)
- **组件**: `LabelCacheComponent`
- **内容**: 缓存 `Vector2 cachedSize`、`int lastFontSize`、`char cachedText[32]` 以及 `isValid` 标志。
- **逻辑**: 仅在字体大小或内容发生变化时触发 `MeasureTextEx` 和 `snprintf`。

### 3.2 Multi-Pass Batching (多阶段批次渲染)
将物品渲染拆分为 5 个独立阶段，强制 Raylib 合并绘制命令：
1. **Beams Pass**: 绘制所有背景光柱（Rare+）。
2. **Background Pass**: 绘制所有标签黑框。
3. **Border Pass**: 绘制所有标签边框。
4. **Text Pass (Primary)**: 绘制所有物品名称。
5. **Text Pass (Secondary)**: 绘制所有金币数量。

### 3.3 EnTT 内建过滤 (Data-Oriented Filtering)
- **优化**: 在 `RenderSystem` 渲染主精灵时，使用 `entt::exclude<NoMoreDay::ItemComponent, GoldComponent>`。
- **原理**: EnTT 会在内部跳过包含这些组件的实体索引，CPU 不再需要访问对应实体的 Component 数据进行逻辑判断。

### 3.4 空间哈希优化 (Spatial Grid Separation)
- **决策**: 将掉落物从 `SpatialHashGrid` 中剔除。它们不再参与每帧的 Hashing 与 Sorting。
- **替代方案**: 渲染和 UI 交互使用基于摄像机视野的**高速线性截头体剔除 (Linear Frustum Culling)**。对于 10,000 个实体，线性剔除的时间开销仅约 0.1ms，远低于哈希表重建开销。

## 4. 实施过程 (Implementation)
1. **Data Definition**: 定义 `LabelCacheComponent`。
2. **Rendering Refactor**: 实现了 `RenderSystem::render` 中的 5 段式逻辑。
3. **Logic Hardening**: 更新 `GameplayState` 排除掉落物更新哈希表。
4. **Interactive Fix**: 更新 `UISystem::Draw` 使用高速线性探测替代空间查询进行物品悬停检测。

## 5. 最终效果 (Results)
- **掉落物数量**: 10,000+
- **优化前**: 120 - 150 FPS (波动较大)
- **优化后**: **180 FPS++ (极其稳定)**
- **DrawCalls**: 显著降低（由于文字和矩形分批次绘制）。
