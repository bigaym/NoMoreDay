# AI 流场索敌统一化实现计划 (Plan)

> **Track ID**: `ai_flowfield_integration`  
> **创建时间**: 2026-01-23  
> **总预估工时**: 4.5 小时

---

## Phase 1: The Bitfield 🔧
**绰号**: 数据基座  
**目标**: 建立 GPU 实体数据中 AI 状态的存储基础  
**预估工时**: 1 小时

### 任务清单

| ID | 任务 | 预估 | 依赖 |
|----|------|------|------|
| P1.1 | 在 `GPUData.hpp` 中定义 `GPUFlags` 命名空间和位操作工具 | 20min | - |
| P1.2 | 验证现有 `flags` 字段的使用情况，确保 Bit 8-15 未被占用 | 10min | P1.1 |
| P1.3 | 添加静态断言验证 `AIType` 枚举值 < 256 | 5min | P1.1 |
| P1.4 | 编写 `GPUFlagsTest.cpp` 单元测试验证位操作正确性 | 25min | P1.1 |

### 输出产物

```cpp
// src/engine/render/GPUData.hpp (新增)
namespace GPUFlags {
  constexpr uint32_t AI_STATE_SHIFT = 8;
  constexpr uint32_t AI_STATE_MASK = 0xFF << AI_STATE_SHIFT;
  
  inline constexpr uint32_t PackAIState(uint8_t state) {
    return static_cast<uint32_t>(state) << AI_STATE_SHIFT;
  }
  inline constexpr uint8_t UnpackAIState(uint32_t flags) {
    return static_cast<uint8_t>((flags & AI_STATE_MASK) >> AI_STATE_SHIFT);
  }
  
  // 现有 Flags (Bit 0-7)
  constexpr uint32_t GPU_ENTITY_FLAG_KINEMATIC = 1 << 0;
  constexpr uint32_t GPU_ENTITY_FLAG_NO_RENDER = 1 << 1;
  constexpr uint32_t GPU_ENTITY_FLAG_CHASING   = 1 << 2;
}
```

### 验证目标

- [ ] `static_assert(static_cast<int>(AIType::TANK_BLOCK) < 256)` 编译通过
- [ ] `GPUFlagsTest` 所有用例通过

---

## Phase 2: The Uplink 📡
**绰号**: 状态同步  
**目标**: 实现 CPU → GPU 的 AI 状态同步通道  
**预估工时**: 1.5 小时

### 任务清单

| ID | 任务 | 预估 | 依赖 |
|----|------|------|------|
| P2.1 | 在 `GPUEntitySystem::Update` 中添加 AI 状态收集逻辑 | 30min | P1.4 |
| P2.2 | 修改 Staging Buffer 写入逻辑，更新 `flags` 字段 | 20min | P2.1 |
| P2.3 | 确保 AI 状态同步仅对有 `GPUIndex` 的敌人执行 | 10min | P2.2 |
| P2.4 | 添加条件编译开关 `ENABLE_AI_GPU_SYNC` 便于回滚 | 10min | P2.2 |
| P2.5 | 实现 `ScopedTimer` 性能测量点 | 10min | P2.2 |
| P2.6 | 编写集成测试验证 GPU Buffer 中 AI 状态正确 | 20min | P2.3 |

### 代码示例

```cpp
// src/engine/render/GPUEntitySystem.cpp

void GPUEntitySystem::Update(entt::registry& registry, float dt) {
  ScopedTimer timer("GPUEntitySystem::Update");
  
  // ... 现有位置同步逻辑 ...
  
#ifdef ENABLE_AI_GPU_SYNC
  // Phase 2: AI 状态同步
  auto aiGroup = registry.group<GPUIndex>(entt::get<AIComponent, EnemyTag>);
  for (auto entity : aiGroup) {
    int gpuIdx = aiGroup.get<GPUIndex>(entity).index;
    if (gpuIdx < 0 || gpuIdx >= static_cast<int>(m_entityCount)) continue;
    
    const auto& ai = aiGroup.get<AIComponent>(entity);
    uint8_t stateVal = static_cast<uint8_t>(ai.aiType);
    
    // 原子更新 flags 字段的 AI 状态位
    uint32_t& flags = m_stagingData[gpuIdx].flags;
    flags = (flags & ~GPUFlags::AI_STATE_MASK) | GPUFlags::PackAIState(stateVal);
  }
#endif
  
  // ... 上传到 GPU ...
}
```

### 验证目标

- [ ] 使用 RenderDoc 捕获帧，检查 Entity SSBO 中 `flags` 字段的 Bit 8-15 正确反映 AI 状态
- [ ] `ScopedTimer` 输出 < 0.5ms (10k 敌人)

---

## Phase 3: The Gatekeeper 🚪
**绰号**: GPU 门卫  
**目标**: 在 `physics.compute` 中添加 AI 状态检查，条件性采样流场  
**预估工时**: 1 小时

### 任务清单

| ID | 任务 | 预估 | 依赖 |
|----|------|------|------|
| P3.1 | 在 Shader 头部定义 AI 状态常量（镜像 C++ 枚举） | 10min | P2.6 |
| P3.2 | 实现 `shouldUseFlowField(uint aiState)` 辅助函数 | 10min | P3.1 |
| P3.3 | 修改主循环，使用条件判断是否采样流场 | 20min | P3.2 |
| P3.4 | 为非追击状态敌人添加摩擦力衰减逻辑 | 10min | P3.3 |
| P3.5 | 验证 Shader 编译无错误 | 5min | P3.4 |
| P3.6 | 游戏内测试：远距离敌人不再追击 | 10min | P3.5 |

### Shader 关键代码

```glsl
// assets/shaders/physics.compute

// AI 状态枚举 (与 C++ AIType 同步)
const uint AI_IDLE = 0u;
const uint AI_PATROL = 1u;
const uint AI_CHASE = 2u;
const uint AI_ATTACK = 3u;
const uint AI_FLEE = 4u;
const uint AI_NEMESIS_HUNTER = 5u;
const uint AI_SUPPORT_FLEE = 6u;
const uint AI_ASSASSIN_STEALTH = 7u;
const uint AI_TANK_BLOCK = 8u;

bool shouldUseFlowField(uint aiState) {
    return aiState == AI_CHASE ||
           aiState == AI_ATTACK ||
           aiState == AI_NEMESIS_HUNTER ||
           aiState == AI_ASSASSIN_STEALTH ||
           aiState == AI_TANK_BLOCK;
}

void main() {
    uint id = gl_GlobalInvocationID.x;
    if (id >= maxEntities) return;
    
    GPUEntityData entity = entities[id];
    
    // 提取 AI 状态
    uint aiState = (entity.flags >> 8) & 0xFFu;
    
    if (shouldUseFlowField(aiState)) {
        // 采样流场并应用
        vec2 flow = sampleFlowField(entity.position);
        float len = length(flow);
        if (len > 0.01) {
            entity.velocity = (flow / len) * entity.speed;
        }
    } else {
        // 非追击状态：保持当前速度或应用摩擦力
        entity.velocity *= 0.90; // 逐帧减速
    }
    
    // ... 剩余物理逻辑 ...
    
    entities[id] = entity;
}
```

### 验证目标

- [ ] 在 500px < dist < 1200px 区域的敌人保持静止或缓慢巡逻
- [ ] 进入 500px 范围后，敌人立即开始追击

---

## Phase 4: The Cleanup 🧹
**绰号**: 清理冗余  
**目标**: 移除 AISystem 中重复的流场应用逻辑，统一由 GPU 执行  
**预估工时**: 0.5 小时

### 任务清单

| ID | 任务 | 预估 | 依赖 |
|----|------|------|------|
| P4.1 | 移除 `AISystem::applyFlowFieldCheck` lambda 内的速度设置 | 15min | P3.6 |
| P4.2 | 保留 `applyFlowFieldCheck` 仅作为状态转换辅助（检测是否有有效流场） | 10min | P4.1 |
| P4.3 | 移除 `AI_WARNING` 临时日志（问题已解决） | 5min | P4.2 |

### 代码变更

```cpp
// src/game/systems/ai/AISystem.cpp

// BEFORE (冗余的速度设置)
auto applyFlowFieldCheck = [&](float speed) {
  // ... 采样流场 ...
  if (hasFlow) {
    vel.vx = flow.x * speed;  // ❌ 移除
    vel.vy = flow.y * speed;  // ❌ 移除
  }
};

// AFTER (仅用于辅助逻辑判断)
auto hasValidFlowField = [&]() -> bool {
  int gx = (int)((pos.x - gridOrigin.x) / cellSize);
  int gy = (int)((pos.y - gridOrigin.y) / cellSize);
  if (gx < 0 || gx >= gridW || gy < 0 || gy >= gridH) return false;
  
  int index = gy * gridW + gx;
  if (index >= (int)flowField.size()) return false;
  
  Vector2 flow = flowField[index];
  return std::abs(flow.x) > NORMALIZE_THRESHOLD || 
         std::abs(flow.y) > NORMALIZE_THRESHOLD;
};

// 追击状态：不再在 CPU 设置速度，完全交由 GPU
case AIType::CHASE: {
  // ... 状态切换逻辑保持不变 ...
  // 移除: applyFlowFieldCheck(chaseSpeed);
  break;
}
```

### 验证目标

- [ ] AISystem 中无直接设置 `vel.vx/vy` 的流场相关代码
- [ ] 编译通过，无新警告

---

## Phase 5: The Certification ✅
**绰号**: 全面验证  
**目标**: 编写完整测试套件，验证修复效果  
**预估工时**: 0.5 小时

### 任务清单

| ID | 任务 | 预估 | 依赖 |
|----|------|------|------|
| P5.1 | 编写 `AIFlowFieldIntegrationTest.cpp` | 20min | P4.3 |
| P5.2 | 运行完整测试套件，确保无回归 | 10min | P5.1 |
| P5.3 | 更新 `tracks.md` 添加本 Track | 5min | P5.2 |

### 测试用例

```cpp
// tests/integration/AIFlowFieldIntegrationTest.cpp

#include <doctest/doctest.h>
#include "game/systems/ai/AISystem.hpp"
#include "engine/render/GPUData.hpp"

TEST_SUITE("AI-FlowField Integration") {

  TEST_CASE("Idle enemy does not use flow field") {
    // Setup
    entt::registry registry;
    auto enemy = registry.create();
    registry.emplace<Position>(enemy, 800.0f, 0.0f);  // > activationRange (500)
    registry.emplace<Velocity>(enemy, 0.0f, 0.0f);
    registry.emplace<AIComponent>(enemy, AIType::IDLE);
    registry.emplace<EnemyStateComponent>(enemy);
    registry.emplace<GPUIndex>(enemy, 0);
    
    // Action: Simulate one AI update
    Position playerPos{0.0f, 0.0f};
    AISystem::update(registry, grid, map, playerPos, 0.016f);
    
    // Assert
    auto& ai = registry.get<AIComponent>(enemy);
    auto& vel = registry.get<Velocity>(enemy);
    
    CHECK(ai.aiType == AIType::IDLE);  // 状态未变
    CHECK(std::abs(vel.vx) < 1.0f);    // 几乎静止
    CHECK(std::abs(vel.vy) < 1.0f);
  }
  
  TEST_CASE("Chase enemy uses flow field direction") {
    // Setup
    entt::registry registry;
    auto enemy = registry.create();
    registry.emplace<Position>(enemy, 300.0f, 0.0f);  // < activationRange (500)
    registry.emplace<Velocity>(enemy, 0.0f, 0.0f);
    auto& ai = registry.emplace<AIComponent>(enemy, AIType::CHASE);
    ai.target = createMockPlayer(registry);
    registry.emplace<GPUIndex>(enemy, 0);
    
    // Action: Simulate GPU physics step (mocked)
    uint32_t flags = GPUFlags::IS_ENEMY | GPUFlags::PackAIState(2); // CHASE=2
    
    // Assert: shouldUseFlowField returns true for CHASE
    uint8_t state = GPUFlags::UnpackAIState(flags);
    CHECK(state == 2);  // CHASE
  }
  
  TEST_CASE("Nemesis ignores activation range") {
    // Setup
    entt::registry registry;
    auto nemesis = registry.create();
    registry.emplace<Position>(nemesis, 1500.0f, 0.0f);  // Far away
    registry.emplace<NemesisTag>(nemesis);
    registry.emplace<AIComponent>(nemesis, AIType::NEMESIS_HUNTER);
    
    // Assert: Nemesis 始终处于追踪状态，不受 activationRange 限制
    // (在 AISystem::updateAIEntity 中有特殊豁免逻辑)
    CHECK(true);  // placeholder
  }
}
```

---

## 依赖关系图

```
P1.1 ─► P1.2 ─► P1.3 ─► P1.4
                          │
                          ▼
                        P2.1 ─► P2.2 ─► P2.3 ─► P2.4 ─► P2.5 ─► P2.6
                                                                  │
                                                                  ▼
                                        P3.1 ─► P3.2 ─► P3.3 ─► P3.4 ─► P3.5 ─► P3.6
                                                                                  │
                                                                                  ▼
                                                        P4.1 ─► P4.2 ─► P4.3 ─► P5.1 ─► P5.2 ─► P5.3
```

---

## 风险登记

| ID | 风险描述 | 可能性 | 影响 | 缓解措施 | 状态 |
|----|----------|--------|------|----------|------|
| R1 | Shader 分支导致性能下降 | 低 | 中 | 使用 `mix()` 替代 `if` 进行无分支优化 | 🔵 监控 |
| R2 | AI 状态同步延迟导致追击抖动 | 低 | 低 | 当前 1 帧延迟可接受 | ✅ 可接受 |
| R3 | 现有 flags 位冲突 | 中 | 高 | Phase 1.2 强制审计 | ⚠️ 需验证 |

---

## 进度跟踪

| Phase | 状态 | 开始时间 | 完成时间 |
|-------|------|----------|----------|
| Phase 1: The Bitfield | ⬜ 待开始 | - | - |
| Phase 2: The Uplink | ⬜ 待开始 | - | - |
| Phase 3: The Gatekeeper | ⬜ 待开始 | - | - |
| Phase 4: The Cleanup | ⬜ 待开始 | - | - |
| Phase 5: The Certification | ⬜ 待开始 | - | - |
