# Track 3: GPUEntitySystem Decomposition 规格说明书 (V1.0)

> **Track ID**: `T3_gpuEntitySystem_decomposition`
> **依赖**: T1, T2
> **状态**: 🔵 待开始
> **预计工时**: 16-24h
> **风险等级**: 🔴 高

---

## 1. 概述 (Overview)

本 Track 是整个重构的**核心高风险任务**，旨在将 `GPUEntitySystem::Update` 方法从"God Method"拆解为多个职责单一的独立阶段，严格遵循 Data-Oriented Design (DOD) 原则。

### 1.1 问题陈述

**当前 `GPUEntitySystem::Update` 的问题** (Line 166-305):

1. **混合职责** (单一职责违规):
   - 槽位分配与回收 (Slot Management)
   - Position/Radius → GPU Buffer 同步 (Physics Sync)
   - Velocity 获取
   - SpriteComponent → TextureIndex 映射
   - AIComponent → Flags 打包
   - CombatStats → GPUVisualStats 同步
   - ActiveEffects → StatusMask 打包
   - 脏标记检查与周期性刷新

2. **缓存不友好** (DOD 违规):
   ```cpp
   for (auto entity : view) {
       auto* velPtr = registry.try_get<Velocity>(entity);  // 随机访问
       if (auto* sprite = registry.try_get<SpriteComponent>(entity)) { ... } // 随机访问
       if (auto *ai = registry.try_get<AIComponent>(entity)) { ... } // 随机访问
       if (auto *stats = registry.try_get<CombatStats>(entity)) { ... } // 随机访问
       if (auto* effects = registry.try_get<ActiveEffectsComponent>(entity)) { ... } // 随机访问
   }
   ```
   每次 `try_get` 可能导致 CPU 缓存失效，性能随组件数量指数下降。

3. **同步频率耦合**:
   - Physics 数据需每帧同步
   - Visual Stats 可仅在脏标记或周期性刷新时同步
   - 当前实现将两者混在同一循环

### 1.2 设计目标

1. **职责分离**: 将 Update 拆分为 2-3 个独立 Job/Phase。
2. **DOD 合规**: 每个 Phase 线性访问单一类型组件族。
3. **频率解耦**: Physics 每帧同步，Visual 按需/周期同步。
4. **可测试性**: 每个 Job 可独立单元测试。
5. **性能目标**: 20k 实体 Update < 2ms (当前基准约 2.8ms)。

---

## 2. 架构设计 (Architecture)

### 2.1 拆分方案

```
┌─────────────────────────────────────────────────────────────────┐
│                     GPUEntitySystem::Update()                    │
│                         (Facade/Orchestrator)                    │
└───────────────────────────┬─────────────────────────────────────┘
                            │
        ┌───────────────────┼───────────────────┐
        │                   │                   │
        ▼                   ▼                   ▼
┌───────────────┐  ┌───────────────────┐  ┌─────────────────────┐
│ Phase 0:      │  │ Phase 1:          │  │ Phase 2:            │
│ SlotManager   │  │ GPUPhysicsSync    │  │ GPUVisualSync       │
│               │  │                   │  │                     │
│ - 槽位分配    │  │ - Position → GPU  │  │ - Stats → GPU       │
│ - 槽位回收    │  │ - Radius → GPU    │  │ - Effects → Mask    │
│ - 实体映射    │  │ - Velocity → GPU  │  │ - Dirty Flag Check  │
└───────────────┘  │ - Flags → GPU     │  │ - 周期性刷新        │
                   └───────────────────┘  └─────────────────────┘
```

### 2.2 数据流

```
Registry (Position, Radius, Velocity, GPUIndex)
    │
    ▼ [Phase 0: SlotManager]
    分配/回收 GPUIndex.index，更新 m_slotToEntity
    │
    ▼ [Phase 1: GPUPhysicsSync]
    EnTT Group<GPUIndex, Position, Radius> → 线性遍历
    写入 m_shadowBuffer: position, prevPosition, velocity, radius, type, flags
    │
    ▼ [Phase 2: GPUVisualSync] (条件执行)
    EnTT View<GPUIndex, CombatStats, ActiveEffectsComponent>
    写入 m_visualStatsShadowBuffer
    │
    ▼ [Bulk Upload]
    memcpy(gpuPtr, m_shadowBuffer, ...)
    MDIRenderer::UpdateStats(m_visualStatsShadowBuffer, ...)
```

---

## 3. 数据模型 (Data Model)

### 3.1 新增结构

```cpp
// ============================================================
// src/engine/render/GPUEntitySync.hpp (新增)
// ============================================================
#pragma once
#include "engine/render/GPUData.hpp"
#include "engine/render/PersistentBuffer.hpp"
#include <entt/entt.hpp>
#include <vector>

namespace NoMoreDay::render {

/**
 * @brief GPU 物理数据同步 Job。
 * 
 * 职责: 将 Position, Radius, Velocity 同步到 GPUEntity Shadow Buffer。
 * 执行频率: 每帧
 * 复杂度: O(N) 线性，缓存友好
 */
class GPUPhysicsSync {
public:
    struct Config {
        int maxEntities = 200000;
        float teleportThreshold = 100.0f;  // 超过此距离视为传送，不插值
    };
    
    void Init(const Config& config);
    
    /**
     * @brief 执行物理同步。
     * @param registry EnTT Registry
     * @param shadowBuffer 输出: GPUEntity 影子缓冲区
     * @param frameCounter 当前帧号
     * @return 使用的最高槽位索引 (用于优化 memcpy 范围)
     */
    int Execute(entt::registry& registry, 
                std::vector<NoMoreDay::components::GPUEntity>& shadowBuffer,
                uint64_t frameCounter);
    
private:
    Config m_config;
};

/**
 * @brief GPU 视觉数据同步 Job。
 * 
 * 职责: 将 CombatStats, ActiveEffects 同步到 GPUVisualStats Shadow Buffer。
 * 执行频率: 脏标记触发 或 每 N 帧
 */
class GPUVisualSync {
public:
    struct Config {
        int maxEntities = 200000;
        int refreshInterval = 5;  // 无脏标记时，每 N 帧刷新一次
    };
    
    void Init(const Config& config);
    
    /**
     * @brief 执行视觉同步。
     * @param registry EnTT Registry
     * @param visualBuffer 输出: GPUVisualStats 影子缓冲区
     * @param frameCounter 当前帧号
     * @param currentTime 当前游戏时间 (用于状态计时器)
     */
    void Execute(entt::registry& registry,
                 std::vector<NoMoreDay::components::GPUVisualStats>& visualBuffer,
                 uint64_t frameCounter,
                 float currentTime);
    
private:
    Config m_config;
};

/**
 * @brief GPU 槽位管理器。
 * 
 * 职责: 管理 GPUIndex.index 的分配与回收。
 */
class GPUSlotManager {
public:
    void Init(int maxEntities, entt::registry* registry);
    
    /**
     * @brief 为新实体分配槽位，回收已死亡实体槽位。
     */
    void Process(entt::registry& registry);
    
    /**
     * @brief EnTT 销毁回调。
     */
    void OnEntityDestroyed(entt::registry& registry, entt::entity entity);
    
    // 访问器
    int GetMaxEntities() const { return m_maxEntities; }
    const std::vector<entt::entity>& GetSlotToEntity() const { return m_slotToEntity; }
    
private:
    int m_maxEntities = 0;
    std::vector<int> m_freeSlots;
    std::vector<entt::entity> m_slotToEntity;
};

} // namespace NoMoreDay::render
```

### 3.2 重构后的 GPUEntitySystem

```cpp
// ============================================================
// src/engine/render/GPUEntitySystem.hpp (重构后)
// ============================================================
#pragma once
#include "engine/render/GPUEntitySync.hpp"
#include "engine/render/PersistentBuffer.hpp"
#include "engine/render/MDIRenderer.hpp"
// ... 其他 includes

namespace NoMoreDay::systems {

class GPUEntitySystem {
public:
    static GPUEntitySystem& Get();
    
    void Init(ResourceManager& rm, int maxEntities = 200000, entt::registry* registry = nullptr);
    
    /**
     * @brief 主更新入口 (Facade)。
     * 编排 SlotManager → PhysicsSync → VisualSync → Upload
     */
    void Update(entt::registry& registry, float dt);
    
    void Render(const SharedContext& context, const Camera2D& camera);
    void Shutdown();
    
    // 访问器 (用于测试)
    const render::GPUSlotManager& GetSlotManager() const { return m_slotManager; }
    const render::GPUPhysicsSync& GetPhysicsSync() const { return m_physicsSync; }
    const render::GPUVisualSync& GetVisualSync() const { return m_visualSync; }
    
private:
    GPUEntitySystem() = default;
    
    // 新架构: 独立 Job
    render::GPUSlotManager m_slotManager;
    render::GPUPhysicsSync m_physicsSync;
    render::GPUVisualSync m_visualSync;
    
    // Buffers
    render::PersistentBuffer m_persistentEntityBuffer;
    std::vector<components::GPUEntity> m_shadowBuffer;
    std::vector<components::GPUVisualStats> m_visualStatsShadowBuffer;
    
    int m_maxEntities = 0;
    uint64_t m_frameCounter = 0;
    
    // Rendering
    Shader m_renderShader;
    unsigned int m_quadVAO = 0;
    unsigned int m_quadVBO = 0;
    
    void InitRender(ResourceManager& rm);
};

} // namespace NoMoreDay::systems
```

---

## 4. 实现细节

### 4.1 GPUPhysicsSync::Execute 伪代码

```cpp
int GPUPhysicsSync::Execute(entt::registry& registry, 
                            std::vector<GPUEntity>& shadowBuffer,
                            uint64_t frameCounter) {
    int highWaterMark = 0;
    
    // 使用 EnTT Group 或 View 进行线性遍历
    // 注意: 避免与 GroupRegistry 中的 Owned Group 冲突
    auto view = registry.view<Position, Radius, GPUIndex>();
    
    for (auto [entity, pos, radius, gpuIdx] : view.each()) {
        if (gpuIdx.index < 0) continue;
        
        int slot = gpuIdx.index;
        if (slot >= shadowBuffer.size()) continue;
        
        if (slot > highWaterMark) highWaterMark = slot;
        
        auto& gpu = shadowBuffer[slot];
        
        // Teleport Detection
        float dx = pos.x - gpu.position.x;
        float dy = pos.y - gpu.position.y;
        bool teleported = (dx*dx + dy*dy) > (m_config.teleportThreshold * m_config.teleportThreshold);
        
        if (teleported) {
            gpu.position = {pos.x, pos.y};
            gpu.prevPosition = {pos.x, pos.y};
        } else {
            gpu.prevPosition = gpu.position;
            gpu.position = {pos.x, pos.y};
        }
        
        // Velocity (Optional: 可内联 try_get 或分离到另一 Pass)
        if (auto* vel = registry.try_get<Velocity>(entity)) {
            gpu.velocity = {vel->vx, vel->vy};
        } else {
            gpu.velocity = {0, 0};
        }
        
        gpu.radius = radius.value;
        gpu.frameId = static_cast<uint32_t>(frameCounter);
        
        // Type & Flags (可考虑移到 VisualSync 以进一步解耦)
        if (auto* sprite = registry.try_get<SpriteComponent>(entity)) {
            gpu.type = sprite->textureLayerIndex;
        } else {
            gpu.type = Constants::GPU::SDF_CIRCLE_TYPE;
        }
        
        gpu.flags = DetermineFlags(registry, entity);
    }
    
    return highWaterMark;
}
```

### 4.2 GPUVisualSync::Execute 伪代码

```cpp
void GPUVisualSync::Execute(entt::registry& registry,
                            std::vector<GPUVisualStats>& visualBuffer,
                            uint64_t frameCounter,
                            float currentTime) {
    
    bool periodicRefresh = (frameCounter % m_config.refreshInterval == 0);
    
    auto view = registry.view<GPUIndex, CombatStats>();
    
    for (auto [entity, gpuIdx, stats] : view.each()) {
        if (gpuIdx.index < 0) continue;
        
        int slot = gpuIdx.index;
        if (slot >= visualBuffer.size()) continue;
        
        // 检查是否需要同步
        bool hasDirtyFlag = registry.any_of<StatsDirty>(entity);
        if (!hasDirtyFlag && !periodicRefresh) {
            // 仅更新计时器
            visualBuffer[slot].statusTimer = currentTime;
            continue;
        }
        
        // 完整同步
        auto& visualStats = visualBuffer[slot];
        AttributePipeline::ToGPU(stats, visualStats);
        
        // Status Effects
        visualStats.activeStatusMask = 0;
        if (auto* effects = registry.try_get<ActiveEffectsComponent>(entity)) {
            for (const auto& effect : effects->effects) {
                switch (effect.type) {
                    case BuffType::Freeze: visualStats.activeStatusMask |= STATUS_FROZEN; break;
                    case BuffType::Burn:   visualStats.activeStatusMask |= STATUS_BURNING; break;
                    case BuffType::Poison: visualStats.activeStatusMask |= STATUS_POISONED; break;
                    case BuffType::Shock:  visualStats.activeStatusMask |= STATUS_SHOCKED; break;
                    default: break;
                }
            }
        }
        
        visualStats.statusTimer = currentTime;
    }
}
```

---

## 5. 测试策略

### 5.1 单元测试

```cpp
// tests/unit/GPUPhysicsSyncTest.cpp
TEST_CASE("GPUPhysicsSync - Linear Position Update") {
    entt::registry registry;
    std::vector<GPUEntity> shadow(1000);
    
    // 创建 100 个实体
    for (int i = 0; i < 100; i++) {
        auto e = registry.create();
        registry.emplace<Position>(e, float(i * 10), float(i * 10));
        registry.emplace<Radius>(e, 5.0f);
        registry.emplace<GPUIndex>(e, i);
    }
    
    GPUPhysicsSync sync;
    sync.Init({.maxEntities = 1000});
    
    int hwm = sync.Execute(registry, shadow, 1);
    
    REQUIRE(hwm == 99);
    REQUIRE(shadow[0].position.x == 0.0f);
    REQUIRE(shadow[50].position.x == 500.0f);
    REQUIRE(shadow[99].position.x == 990.0f);
}

TEST_CASE("GPUPhysicsSync - Teleport Detection") {
    // ... 验证传送检测逻辑
}
```

### 5.2 性能基准测试

```cpp
// tests/benchmark/GPUSyncBenchmark.hpp
BENCHMARK("GPUPhysicsSync - 20k Entities") {
    // 预置 20000 个实体
    // 执行 Execute()
    // 记录耗时
    // 断言 < 1.5ms
}
```

---

## 6. 验收标准 (Acceptance Criteria)

- [ ] `GPUEntitySync.hpp/cpp` 已创建，包含 `GPUSlotManager`, `GPUPhysicsSync`, `GPUVisualSync`。
- [ ] `GPUEntitySystem::Update()` 重构为 Facade，编排三个 Job。
- [ ] 原 `Update()` 中的逻辑完全迁移，无遗留代码。
- [ ] `GPUPhysicsSyncTest` 测试通过。
- [ ] `GPUVisualSyncTest` 测试通过。
- [ ] 性能基准: 20k 实体 Update < 2ms。
- [ ] 视觉验收: 游戏运行画面与重构前无差异。
- [ ] 无内存泄漏 (ASAN 测试通过)。

---

## 7. 风险与缓解

| 风险 | 影响 | 可能性 | 缓解措施 |
|---|---|---|---|
| EnTT Group 冲突 | 运行时崩溃 | 中 | 使用 `view` 替代 `group`，或确保与 `GroupRegistry` 无冲突 |
| 拆分后逻辑遗漏 | 渲染异常 | 中 | 充分的单元测试 + 视觉验收 |
| 性能不升反降 | 优化失败 | 低 | 基准测试驱动开发，每阶段验证 |
| 多线程竞争 (未来) | 数据损坏 | 低 | 当前单线程，未来引入并行时需加锁 |

---

## 8. 迁移策略

1. **Phase 0**: 创建新文件 `GPUEntitySync.hpp/cpp`，不修改原代码。
2. **Phase 1**: 实现 `GPUPhysicsSync`，在 `Update()` 中调用新实现，保留原逻辑作为 Fallback。
3. **Phase 2**: 验证 `GPUPhysicsSync` 正确后，删除原物理同步代码。
4. **Phase 3**: 实现 `GPUVisualSync`，同样采用 Fallback 策略。
5. **Phase 4**: 验证后完全切换，删除原代码。

可选: 使用 `#define USE_NEW_GPU_SYNC` 编译开关控制切换。

---

*规格版本: 1.0*
*最后更新: 2026-01-26*
