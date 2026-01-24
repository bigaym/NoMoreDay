# Physics & Rendering Critical Fix - Technical Spec

## 1. 背景与问题陈述 (Context & Problem Statement)

在 `2026-01-24` 的综合代码审计中，发现了三个关键的技术隐患。本规格书旨在提供一个独立、完整的修复方案，无需查阅原始审计报告即可理解和执行。

### 1.1 核心问题 (The Issues)

1.  **逻辑回归 (Force Fields Logic Lost)**
    *   **现象**: 游戏中所有的“力场”效果（如黑洞技能吸附敌人、旋涡减速）目前完全失效。
    *   **原因**: 在最近将 `UpdatePhysics` 重构为多线程 `Taskflow` 版本的过程中，原本串行调用的 `PhysicsSystem::applyForceFields` 被意外遗漏，未被添加到任何 Task 中。
    *   **影响**: 严重影响战斗体验，控制类技能变为纯视觉效果。

2.  **架构隐患 (SSBO Struct Mismatch)**
    *   **现象**: CPU 端定义的两个用于 GPU 交互的结构体大小不一致。
        *   `GPUData.hpp` 中的 `GPUEntity` 大小为 **64 字节**。
        *   `MDIRenderer.hpp` 中的 `GPUInstanceData` 大小为 **48 字节**。
    *   **风险**: 尽管目前 MDI 渲染器可能通过某些“巧合”借用了 `GPUEntitySystem` 的 Buffer，但这种定义上的分歧是致命的。如果任何代码直接使用 `GPUInstanceData` 上传数据，会导致 GPU 读取偏移（Strided Access Misalignment），引发花屏或驱动崩溃。
    *   **标准**: `std430` 布局要求结构体对齐通常为 16 字节，且为了性能最好对齐到 64 字节（常见 Cache Line 大小）。

3.  **代码异味 (Magic Numbers)**
    *   **现象**: `PhysicsSystem.cpp` 中散落着直接硬编码的参数（如排斥力系数 `20.0f`，阻尼 `0.92f`），难以调整且意图不明。

## 2. 解决方案 (The Solution)

### 2.1 修复数据结构 (Fixing MDIRenderer)
将 `MDIRenderer::GPUInstanceData` 强制扩容至 64 字节，使其与 `GPUEntity` 保持二进制兼容。这确保了无论使用哪个结构体，显存布局都是一致的。

**Current Layout (48 Bytes - Dangerous):**
```cpp
Vector2 position;     // 8
Vector2 prevPosition; // 8
Vector2 velocity;     // 8
float radius;         // 4
int32_t type;         // 4
uint32_t flags;       // 4
float padding[3];     // 12 (Total: 48)
```

**New Layout (64 Bytes - Safe):**
```cpp
// Explicit alignment to 16 bytes for std430 compatibility
struct alignas(16) GPUInstanceData {
    Vector2 position;       // 8
    Vector2 prevPosition;   // 8
    Vector2 velocity;       // 8
    float radius;           // 4
    int32_t type;           // 4
    uint32_t flags;         // 4
    float padding[7];       // 28 (Total: 64)
    // 8+8+8+4+4+4 + 28 = 64 bytes
};
static_assert(sizeof(GPUInstanceData) == 64, "Must match GPUEntity size");
```

### 2.2 恢复力场逻辑 (Restoring Force Fields)
在 `GameplayState::UpdatePhysics` 的执行流中，将 `applyForceFields` 重新加入。
由于力场计算会修改实体的 `Velocity`，而后续的碰撞解决（Resolve Collisions）也会读取/修改 `Velocity`，因此必须保证顺序。

**Execution Flow:**
1.  **Phase 0 (Serial or Pre-Task)**: `applyForceFields`
    *   *Input*: Position (Read-Only)
    *   *Output*: Velocity (Accumulate)
    *   *Thread Safety*: Safe if run before parallel tasks, or as a parallel task with exclusive write access (but current `applyForceFields` is not designed for granular parallelism yet, so Serial is safest and simplest).
2.  **Phase 1 (Parallel)**: `resolveCollisions`
    *   *Input*: Position (Read-Only), SpatialGrid (Read-Only)
    *   *Output*: Velocity (Modify)
3.  **Phase 2 (Parallel)**: `updatePosition`
    *   *Input*: Velocity (Read-Only)
    *   *Output*: Position (Write)

**Implementation:**
在 `m_taskflow.run` 之前，直接调用 `PhysicsSystem::applyForceFields(registry, dt, m_spatialGrid);`。

### 2.3 提取常量 (Constants Extraction)
建立专门的常量命名空间，统一管理物理参数。

**New Namespace (`Common.hpp` or `PhysicsSystem.hpp`):**
```cpp
namespace NoMoreDay::Constants::Physics {
    // Wall Repulsion (墙壁排斥力系数)
    constexpr float WALL_REPULSION_FACTOR = 20.0f;
    
    // Entity Damping (实体阻尼 - 空气阻力)
    constexpr float ENTITY_DAMPING_FACTOR = 0.92f;
    
    // CCD Step Size (连续碰撞检测步长)
    constexpr float CCD_STEP_SIZE = 10.0f;
}
```

## 3. 详细修改清单 (Step-by-Step)

| 文件路径 | 所在函数/结构 | 修改动作 |
|---------|--------------|---------|
| `src/engine/render/MDIRenderer.hpp` | `struct GPUInstanceData` | 修改 `padding` 数组大小从 3 改为 7，验证总大小为 64 字节。 |
| `src/game/states/GameplayState.cpp` | `UpdatePhysics` | 在构建 `taskflow` 前或 `executor->run` 前，插入 `PhysicsSystem::applyForceFields(...)`。 |
| `src/engine/physics/PhysicsSystem.hpp` | (Namespace Area) | 添加 `WALL_REPULSION_FACTOR`, `ENTITY_DAMPING_FACTOR`, `CCD_STEP_SIZE` 常量定义。 |
| `src/engine/physics/PhysicsSystem.cpp` | `resolveCollisions` | 替换 `20.0f` 为 `WALL_REPULSION_FACTOR`。 |
| `src/engine/physics/PhysicsSystem.cpp` | `updatePosition` | 替换 `0.92f` 为 `ENTITY_DAMPING_FACTOR`。 |
| `src/engine/physics/PhysicsSystem.cpp` | `performDashStep` | 替换 `10.0f` 为 `CCD_STEP_SIZE`。 |

## 4. 验证计划 (Verification)

### 4.1 静态检查
- [ ] 编译无报错。
- [ ] `static_assert` 确保 `GPUInstanceData` 大小为 64。

### 4.2 运行时测试 (Runtime)
- [ ] **渲染测试**: 启动游戏，观察控制台是否有 GPU 报错，观察画面中大量敌人渲染是否正常（无拉伸、闪烁）。
- [ ] **力场测试**: 
    - 使用技能 "Singularity" (或任意包含 ForceField 组件的技能)。
    - 观察周围的 Dummy 或 Enemy 是否被吸向中心。
    - 如果敌人纹丝不动，则修复失败。

## 5. 交付物
- 更新后的 `MDIRenderer.hpp`
- 修复逻辑后的 `GameplayState.cpp`
- 规范化常量的 `PhysicsSystem.cpp` / `.hpp`
