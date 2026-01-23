# AI 流场索敌统一化技术规格书 (Spec)

> **Track ID**: `ai_flowfield_integration`  
> **创建时间**: 2026-01-23  
> **优先级**: HIGH  
> **预估工时**: 4-5 小时

---

## 1. 问题陈述 (Problem Statement)

### 1.1 冲突描述

当前系统中存在两套独立的"敌人感知玩家"机制，它们之间设计不统一导致逻辑冲突：

| 机制 | 数据源 | 触发条件 | 职责 |
|------|--------|----------|------|
| **GPU 流场 (`GPUFlowFieldSystem`)** | 以玩家为中心的窗口化向量场 | 实体处于流场窗口内 | 提供**方向引导** |
| **AI 索敌 (`AISystem`)** | `EnemyStateComponent.activationRange` | 距离 < `activationRange` (500px) | 控制**状态切换** (IDLE→CHASE) |

### 1.2 具体问题

1. **流场覆盖范围 >> activationRange**
   - 流场窗口大小由 `GPUFlowFieldSystem::Init` 决定（通常覆盖 1500-2000px 半径）。
   - 敌人在 500px < dist < 1500px 范围内，能采样到有效流场方向，但 AI 状态仍为 `IDLE/PATROL`。
   - 结果：敌人获得了"朝向玩家"的移动意图，但 **不应该执行追击**。

2. **GPU 物理驱动时的状态不一致**
   - `GPUEntitySystem` 使用 `physics.compute` 对敌人执行物理模拟。
   - 若 GPU 物理直接使用流场驱动敌人移动（未检查 AI 状态），敌人会穿越 `activationRange` 边界直接逼近玩家。
   - 结果：**越权索敌**——敌人在 `IDLE` 状态下也能追击。

3. **性能与一致性矛盾**
   - 如果在 GPU 层强制过滤（检查每个敌人的距离），会增加 GPU 开销和 CPU-GPU 同步复杂度。
   - 如果在 CPU 层处理，会丢失 GPU 并行化优势。

### 1.3 当前日志表现

```log
[AI_WARNING] Entity 12345 triggered AGGRO outside range! Dist: 782.3, Activation: 500.0
[AI_DEBUG] Entity 12345 in CHASE state at dist 782.3. Activation: 500.0
```

---

## 2. 设计目标 (Design Goals)

### 2.1 功能目标

- **G1**: 统一 GPU 流场和 AI 索敌的触发边界，确保 `activationRange` 是唯一的仇恨触发权威。
- **G2**: 使用分层策略：GPU 提供**潜在方向**，CPU 提供**状态决策**。
- **G3**: 屏蔽非仇恨状态敌人对流场的"实际使用"，仅允许 `CHASE/ATTACK/NEMESIS_HUNTER` 等状态下的敌人真正跟随流场移动。

### 2.2 性能目标

| 指标 | 基线 (当前) | 目标 |
|------|------------|------|
| AI 更新耗时 (20k 敌人) | ~2.5ms | ≤ 2.8ms (+12% 可接受) |
| 不引入新的 GPU 回读 | N/A | 禁止新增 `SyncBack` 操作 |
| CPU-GPU 一致性延迟 | 1-2 帧 | 保持不变 |

### 2.3 非目标 (Out of Scope)

- 修改流场计算本身的算法（如 Dijkstra 边界、Density 权重）。
- 调整 `activationRange` 的默认值（可由策划调整）。

---

## 3. 技术方案 (Technical Solution)

### 3.1 架构层次划分

```
┌─────────────────────────────────────────────────────────────────────────┐
│                              GPU 层                                     │
│  ┌─────────────────────┐    ┌─────────────────────────────────────────┐ │
│  │ GPUFlowFieldSystem  │◄───│ physics.compute (移动采样)              │ │
│  │ - Flow Field SSBO   │    │ - 检查 aiState (新增字段)               │ │
│  │ - 不感知 AI 状态    │    │ - 仅 CHASE/ATTACK/HUNTER 时采样流场     │ │
│  └─────────────────────┘    └─────────────────────────────────────────┘ │
│                                       ▲                                 │
│                                       │ (每帧上传)                     │
├───────────────────────────────────────┼─────────────────────────────────┤
│                              CPU 层   │                                 │
│  ┌─────────────────────┐    ┌─────────┴───────────────────────────────┐ │
│  │     AISystem        │───►│ GPUEntitySystem::SyncEntityStates       │ │
│  │ - 距离检测          │    │ - 上传 aiState 枚举到 GPU               │ │
│  │ - 状态切换          │    │ - 使用 Indirect SSBO 避免全量同步       │ │
│  │ - activationRange   │    └─────────────────────────────────────────┘ │
│  └─────────────────────┘                                                │
└─────────────────────────────────────────────────────────────────────────┘
```

### 3.2 核心数据结构变更

#### 3.2.1 GPU 实体数据结构 (GPUEntityData)

**现有结构** (`src/engine/render/GPUData.hpp`):
```cpp
struct GPUEntityData {
  Vector2 position;
  Vector2 prevPosition;  // For interpolation
  Vector2 velocity;
  float radius;
  float speed;
  uint32_t textureIndex;
  uint32_t flags;        // 已存在: IsEnemy, IsPlayer, IsProjectile 等
  // ...
};
```

**拓展方案**:
```cpp
struct GPUEntityData {
  // ... 现有字段 ...
  uint32_t flags;        // 复用 flags 的高位
  // flags 布局变更:
  // Bit 0: KINEMATIC
  // Bit 1: NO_RENDER
  // Bit 2: CHASING
  // Bit 8-15: AIState (IDLE=0, PATROL=1, CHASE=2, ATTACK=3, ...)
  // Bit 16-31: Reserved
};

// 辅助宏/inline
namespace GPUFlags {
  // 现有 Flag 定义 (src/engine/render/GPUData.hpp)
  // Bit 0: KINEMATIC
  // Bit 1: NO_RENDER
  // Bit 2: CHASING implies (AIState == CHASE || ATTACK || NEMESIS)
  
  constexpr uint32_t AI_STATE_SHIFT = 8;
  constexpr uint32_t AI_STATE_MASK = 0xFF << AI_STATE_SHIFT;
  
  inline constexpr uint32_t PackAIState(uint8_t state) {
    return static_cast<uint32_t>(state) << AI_STATE_SHIFT;
  }
  inline constexpr uint8_t UnpackAIState(uint32_t flags) {
    return static_cast<uint8_t>((flags & AI_STATE_MASK) >> AI_STATE_SHIFT);
  }
}
```

#### 3.2.2 physics.compute Shader 修改

```glsl
// 新增: 仅在特定 AI 状态下采样流场
uint aiState = (entity.flags >> 8) & 0xFF;

// AIState 枚举镜像 (与 C++ AIType 对应)
const uint AI_IDLE = 0u;
const uint AI_PATROL = 1u;
const uint AI_CHASE = 2u;
const uint AI_ATTACK = 3u;
// ...

// 也可以继续使用 bit 2 (CHASING) 作为快速判断，或者使用完整 aiState
bool shouldUseFlowField = (aiState == AI_CHASE) || 
                          (aiState == AI_ATTACK) || 
                          (aiState == AI_NEMESIS_HUNTER) ||
                          ((entity.flags & 4u) != 0u); // 兼容旧 flag

if (shouldUseFlowField) {
    // 采样流场并应用速度
    vec2 flow = sampleFlowField(entity.position);
    entity.velocity = flow * entity.speed;
} else {
    // 保持当前CPU设定的速度（巡逻/闲置）
    // 或应用摩擦力减速
    entity.velocity *= 0.95;
}
```

### 3.3 CPU 同步策略

#### 3.3.1 GPUEntitySystem::Update 修改

在现有的 `GPUEntitySystem::Update` 中，增加 AI 状态同步：

```cpp
void GPUEntitySystem::Update(entt::registry& registry, float dt) {
  // ... 现有位置/速度同步逻辑 ...
  
  // 新增: AI 状态同步
  auto aiView = registry.view<GPUIndex, AIComponent>();
  for (auto entity : aiView) {
    int gpuIdx = aiView.get<GPUIndex>(entity).index;
    if (gpuIdx < 0) continue;
    
    const auto& ai = aiView.get<AIComponent>(entity);
    uint8_t aiStateEnum = static_cast<uint8_t>(ai.aiType);
    
    // 使用 Staging Buffer 批量更新 flags 字段
    m_stagingData[gpuIdx].flags = 
      (m_stagingData[gpuIdx].flags & ~GPUFlags::AI_STATE_MASK) |
      GPUFlags::PackAIState(aiStateEnum);
  }
  
  // ... 上传 Staging Buffer 到 GPU ...
}
```

#### 3.3.2 避免回读

关键设计原则：**AI 状态仅从 CPU 向 GPU 单向同步**，无需回读。GPU 不会修改 AI 状态。

---

## 4. 修改文件清单

| 文件路径 | 修改类型 | 主要变更 |
|----------|----------|----------|
| `src/engine/render/GPUData.hpp` | 修改 | 添加 `GPUFlags` 命名空间和 AI 状态位操作 |
| `src/engine/render/GPUEntitySystem.cpp` | 修改 | 添加 AI 状态同步逻辑 |
| `assets/shaders/physics.compute` | 修改 | 添加 AI 状态检查，条件性采样流场 |
| `src/game/systems/ai/AISystem.cpp` | 微调 | 移除流场应用逻辑（已迁移至 GPU 条件执行） |
| `src/game/components/AIComponent.hpp` | 无变更 | 现有枚举可直接复用 |

---

## 5. 验收标准 (Acceptance Criteria)

### 5.1 功能验收

- [ ] **AC1**: 距离 > `activationRange` 的敌人，即使在流场覆盖范围内，也不会朝玩家移动。
- [ ] **AC2**: 敌人从 `IDLE→CHASE` 转换只在距离 < `activationRange` 时发生。
- [ ] **AC3**: 日志中不再出现 `[AI_WARNING] Entity X triggered AGGRO outside range` 警告。
- [ ] **AC4**: Nemesis (宿敌) 仍然能在任意距离追踪玩家（特殊豁免）。

### 5.2 性能验收

- [ ] **PC1**: AI 更新 + GPU 同步总耗时 < 3.5ms (20k 敌人，Intel Iris Xe)。
- [ ] **PC2**: 无新增 `glGetBufferSubData` 或类似 GPU 回读操作。

### 5.3 测试用例

```cpp
// tests/unit/AIFlowFieldIntegrationTest.cpp

TEST(AIFlowFieldIntegration, IdleEnemyDoesNotChase) {
  // Setup: 敌人在 800px 处，activationRange = 500px
  // Assert: 敌人 AIType 保持 IDLE，velocity 接近 0
}

TEST(AIFlowFieldIntegration, ChaseEnemyUsesFlowField) {
  // Setup: 敌人在 300px 处，已触发 CHASE
  // Assert: 敌人 velocity 方向指向玩家
}

TEST(AIFlowFieldIntegration, NemesisAlwaysChases) {
  // Setup: Nemesis 在 1500px 处
  // Assert: Nemesis 仍然追踪玩家
}
```

---

## 6. 风险评估

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|--------|------|----------|
| GPU flags 字段位操作出错 | 中 | 高 | 添加静态断言验证位布局 |
| AI 状态同步延迟导致抖动 | 低 | 中 | 保持每帧同步，1帧延迟可接受 |
| physics.compute 分支降低 GPU 效率 | 低 | 低 | 现代 GPU 对简单分支优化良好 |

---

## 7. 附录

### 7.1 当前相关常量

```cpp
// Common.hpp
namespace NoMoreDay::Constants::Enemy {
  constexpr float DEFAULT_AGGRO_DISTANCE = 500.0f;         // AI 仇恨距离
  constexpr float DEFAULT_ACTIVATION_DISTANCE = 1200.0f;   // 生成/活跃距离
  constexpr float DEFAULT_DEACTIVATION_DISTANCE = 2000.0f; // 休眠距离
}
```

### 7.2 AIType 枚举值映射

| 枚举名 | 值 | GPU 采样流场 |
|--------|---|-------------|
| IDLE | 0 | ❌ |
| PATROL | 1 | ❌ |
| CHASE | 2 | ✅ |
| ATTACK | 3 | ✅ |
| FLEE | 4 | ❌ (反向移动) |
| NEMESIS_HUNTER | 5 | ✅ |
| SUPPORT_FLEE_BUFF | 6 | ❌ |
| ASSASSIN_STEALTH | 7 | ✅ (背刺阶段) |
| TANK_BLOCK | 8 | ✅ |
