# Detailed Design Specification: GPU Flow Field Integration

## 1. 架构目标 (Architecture Goals)
将 AI 寻路驱动从 CPU 端的启发式算法完全迁移至 GPU 驱动的流场。实现 10,000+ 实体的“零开销”寻路，通过对 SSBO 的单次采样决定移动向量。

## 2. 核心逻辑与边界 (Core Logic & Boundaries)

### 2.1 坐标空间映射 (Coordinate Mapping)
- **输入**: 实体世界坐标 `Pos(x, y)`。
- **网格常量**: `TILE_SIZE = 10.0f`, `GRID_SIZE = 256`。
- **映射公式**:
    ```cpp
    int localX = (int)(worldX / TILE_SIZE) - flowFieldOriginX;
    int localY = (int)(worldY / TILE_SIZE) - flowFieldOriginY;
    int ssboIndex = localY * GRID_SIZE + localX;
    ```
- **边界判定**: 若 `localX` 或 `localY` 处于 `[0, 255]` 之外，则判定为**“脱离引导区”**。

### 2.2 移动算法 (Movement Engine)
- **采样模式 (Exclusive)**: 
    - 实体不计算避障，直接读取 `SSBO[ssboIndex]`。
    - 若采样到的 `Vector2` 模长为 0（代表障碍物或无效区），实体保持当前速度衰减。
- **视觉去堆叠 (Visual Offset)**:
    为了缓解 10,000 实体在同一点重合，渲染层需注入偏移：
    ```cpp
    // 渲染位置偏移公式
    float offsetX = (entityID % 11 - 5) * 1.5f;
    float offsetY = (entityID % 7 - 3) * 1.5f;
    ```

### 2.3 性能剔除与休眠 (Culling & Dormancy)
- **活跃边界**: 以玩家为中心 $128 \times 10 = 1280$ 单位半径。
- **休眠逻辑**:
    - **进入休眠**: 距离 > 1300 单位。移除 `Velocity`, `PhysicsComponent`，添加 `DormantTag`，位置传送至 `( -1000, -1000 )` 或出生点池（出生点优先）。
    - **重新调度**: `EnemySpawnSystem` 每隔 60 帧扫描休眠池，将怪物重新投放到玩家视野边缘的有效网格内。

## 3. 关键组件变更 (Component Changes)
- `DormantTag`: 标记休眠实体，跳过 `AISystem` 和 `PhysicsSystem` 更新。
- `EnemyComponent`: 增加 `lastActiveGridPos` 缓存，用于平滑网格切换。

## 4. 性能约束 (Constraints)
- **CPU Time**: 10,000 实体采样耗时必须通过 SIMD (xsimd) 或极简循环控制在 0.4ms 内。
- **GPU Wait**: 严禁在 AI 更新循环内执行 `glGetBufferSubData` 等阻塞式读取。`AISystem` 应当读取 `GPUFlowFieldSystem` 预先同步到 CPU 的 Shadow Buffer。

## 5. 验收标准 (Acceptance Criteria)
- [ ] 10,000 怪物聚集在狭窄路口时，帧率不低于 60 FPS。
- [ ] 怪物能准确绕过 GPU 计算出的静态障碍物。
- [ ] 快速移动时，远端怪物能正确消失并回收至休眠池。
