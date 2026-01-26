# Track 5: Singleton Decoupling 规格说明书 (V1.0)

> **Track ID**: `T5_singleton_decoupling`
> **依赖**: T3, T4
> **状态**: 🔵 待开始
> **预计工时**: 16h
> **风险等级**: 🔴 高

---

## 1. 概述 (Overview)

本 Track 是渲染管线重构的**最终阶段**，旨在消除 `GPUEntitySystem::Get()` 和 `MDIRenderer::Get()` 单例模式，引入依赖注入，提高系统可测试性和模块化程度。

### 1.1 问题陈述

**当前 Singleton 模式的问题**:

1. **测试困难**:
   ```cpp
   // 单元测试无法 Mock MDIRenderer
   void SomeSystem::Update() {
       MDIRenderer::Get().Render(...); // 强耦合单例
   }
   ```

2. **隐式依赖**:
   - 调用者无需显式声明对渲染系统的依赖
   - 代码的依赖关系难以追踪
   - 违反显式依赖原则 (Explicit Dependencies Principle)

3. **生命周期混乱**:
   - 单例在首次访问时创建
   - 销毁顺序不可控
   - 可能导致静态销毁顺序问题 (Static Destruction Order Fiasco)

4. **全局状态**:
   - 单例本质上是全局变量
   - 使并行测试困难
   - 违反 DOD 中的"无隐式状态"原则

### 1.2 设计目标

1. **显式依赖**: 通过构造函数/方法参数注入渲染系统引用。
2. **可测试性**: 支持 Mock/Stub 替换用于单元测试。
3. **生命周期可控**: 渲染系统由 `Game` 或 `GameplayState` 显式管理。
4. **渐进式迁移**: 保留 `Get()` 方法但标记为 `[[deprecated]]`，逐步迁移调用点。

---

## 2. 架构设计

### 2.1 新架构: RenderContext

```
┌─────────────────────────────────────────────────────────────────┐
│                           Game                                   │
│                    (Application Entry)                           │
├─────────────────────────────────────────────────────────────────┤
│  成员:                                                           │
│  - GPUEntitySystem m_gpuEntitySystem;                            │
│  - MDIRenderer m_mdiRenderer;                                    │
│  - RenderContext m_renderContext; // 聚合引用                     │
└───────────────────────────┬─────────────────────────────────────┘
                            │ 传递 RenderContext
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                      GameplayState                               │
├─────────────────────────────────────────────────────────────────┤
│  方法:                                                           │
│  - void Init(RenderContext& ctx);                                │
│  - void Update(RenderContext& ctx, float dt);                    │
│  - void Render(RenderContext& ctx, const Camera2D& camera);      │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 RenderContext 定义

```cpp
// ============================================================
// src/engine/render/RenderContext.hpp (新增)
// ============================================================
#pragma once

namespace NoMoreDay {

// 前向声明
namespace render {
    class MDIRenderer;
    class PersistentBuffer;
}
namespace systems {
    class GPUEntitySystem;
}
class ResourceManager;

/**
 * @brief 渲染上下文聚合器。
 * 
 * 用于传递渲染相关系统引用，避免直接访问单例。
 * 这是一个轻量级结构，仅存储指针/引用。
 */
struct RenderContext {
    systems::GPUEntitySystem* gpuEntitySystem = nullptr;
    render::MDIRenderer* mdiRenderer = nullptr;
    ResourceManager* resources = nullptr;
    
    // 快捷访问
    systems::GPUEntitySystem& GPU() { return *gpuEntitySystem; }
    render::MDIRenderer& MDI() { return *mdiRenderer; }
    ResourceManager& Resources() { return *resources; }
    
    // 验证
    bool IsValid() const {
        return gpuEntitySystem && mdiRenderer && resources;
    }
};

} // namespace NoMoreDay
```

### 2.3 重构后的 GPUEntitySystem

```cpp
// ============================================================
// src/engine/render/GPUEntitySystem.hpp (重构后)
// ============================================================
#pragma once
// ... includes

namespace NoMoreDay::systems {

class GPUEntitySystem {
public:
    // === 新接口: 非单例构造 ===
    GPUEntitySystem() = default;
    ~GPUEntitySystem() = default;
    
    // 禁止拷贝，允许移动
    GPUEntitySystem(const GPUEntitySystem&) = delete;
    GPUEntitySystem& operator=(const GPUEntitySystem&) = delete;
    GPUEntitySystem(GPUEntitySystem&&) = default;
    GPUEntitySystem& operator=(GPUEntitySystem&&) = default;

    void Init(ResourceManager& rm, int maxEntities = 200000, entt::registry* registry = nullptr);
    void Update(entt::registry& registry, float dt);
    void Render(const SharedContext& context, const Camera2D& camera);
    void Shutdown();
    
    // === 废弃接口: 单例 ===
    /**
     * @deprecated 使用 RenderContext 依赖注入替代。
     * 将在 V2.0 移除。
     */
    [[deprecated("Use RenderContext injection instead")]]
    static GPUEntitySystem& Get();
    
private:
    // ... 私有成员
    
    // 单例实例 (仅用于过渡期)
    static GPUEntitySystem* s_instance;
};

} // namespace NoMoreDay::systems
```

### 2.4 单例过渡实现

```cpp
// GPUEntitySystem.cpp

GPUEntitySystem* GPUEntitySystem::s_instance = nullptr;

GPUEntitySystem& GPUEntitySystem::Get() {
    if (!s_instance) {
        LOG_WARN("GPUEntitySystem::Get() called without initialization. "
                 "Consider using RenderContext injection.");
        static GPUEntitySystem fallback;
        return fallback;
    }
    return *s_instance;
}

void GPUEntitySystem::Init(ResourceManager& rm, int maxEntities, entt::registry* registry) {
    s_instance = this;  // 注册为"当前实例"供过渡期使用
    // ... 原初始化逻辑
}

void GPUEntitySystem::Shutdown() {
    // ... 原清理逻辑
    if (s_instance == this) {
        s_instance = nullptr;
    }
}
```

---

## 3. 迁移策略

### 3.1 渐进式迁移阶段

| 阶段 | 行动 | 影响 |
|---|---|---|
| **V1.0** | 引入 `RenderContext`，标记 `Get()` 为 `[[deprecated]]` | 编译警告 |
| **V1.1** | 迁移主要调用点 (`GameplayState`, `Game`) | 减少 50%+ 单例调用 |
| **V1.2** | 迁移所有生产代码 | 仅测试代码使用 Mock |
| **V2.0** | 移除 `Get()` 方法 | 完全解耦 |

### 3.2 调用点迁移示例

**Before (GameplayState):**
```cpp
void GameplayState::Update(entt::registry& registry, float dt) {
    GPUEntitySystem::Get().Update(registry, dt);
}

void GameplayState::Render(const Camera2D& camera) {
    GPUEntitySystem::Get().Render(m_context, camera);
}
```

**After (GameplayState):**
```cpp
// 构造函数接收 RenderContext
GameplayState::GameplayState(RenderContext& renderCtx) 
    : m_renderCtx(renderCtx) {}

void GameplayState::Update(entt::registry& registry, float dt) {
    m_renderCtx.GPU().Update(registry, dt);
}

void GameplayState::Render(const Camera2D& camera) {
    m_renderCtx.GPU().Render(m_context, camera);
}
```

**Before (Game.cpp):**
```cpp
void Game::InitializeRendering() {
    GPUEntitySystem::Get().Init(m_resources, MAX_ENTITIES, &m_registry);
    MDIRenderer::Get().Init(m_resources, MAX_ENTITIES);
}
```

**After (Game.cpp):**
```cpp
class Game {
    GPUEntitySystem m_gpuEntitySystem;  // 值成员，非指针
    MDIRenderer m_mdiRenderer;
    RenderContext m_renderContext;
    
    void InitializeRendering() {
        utils::GPUUtils::Initialize();
        
        m_gpuEntitySystem.Init(m_resources, MAX_ENTITIES, &m_registry);
        m_mdiRenderer.Init(m_resources, MAX_ENTITIES);
        
        // 构建 RenderContext
        m_renderContext.gpuEntitySystem = &m_gpuEntitySystem;
        m_renderContext.mdiRenderer = &m_mdiRenderer;
        m_renderContext.resources = &m_resources;
        
        // 传递给需要的子系统
        m_gameplayState = std::make_unique<GameplayState>(m_renderContext);
    }
};
```

---

## 4. 测试支持

### 4.1 Mock 渲染系统

```cpp
// tests/mocks/MockGPUEntitySystem.hpp
#pragma once
#include "engine/render/GPUEntitySystem.hpp"

namespace NoMoreDay::testing {

class MockGPUEntitySystem {
public:
    void Init(ResourceManager&, int, entt::registry*) { initCalled = true; }
    void Update(entt::registry&, float) { updateCalled = true; }
    void Render(const SharedContext&, const Camera2D&) { renderCalled = true; }
    void Shutdown() { shutdownCalled = true; }
    
    bool initCalled = false;
    bool updateCalled = false;
    bool renderCalled = false;
    bool shutdownCalled = false;
};

/**
 * @brief 测试用 RenderContext，使用 Mock 对象。
 */
struct MockRenderContext {
    MockGPUEntitySystem gpu;
    // ... 其他 Mock
    
    RenderContext AsContext() {
        RenderContext ctx;
        // 类型擦除或模板化策略
        return ctx;
    }
};

} // namespace NoMoreDay::testing
```

### 4.2 单元测试示例

```cpp
// tests/unit/GameplayStateTest.cpp
TEST_CASE("GameplayState - Render calls GPU Render") {
    MockRenderContext mockCtx;
    GameplayState state(mockCtx.AsContext()); // 或使用模板
    
    Camera2D camera = {};
    state.Render(camera);
    
    CHECK(mockCtx.gpu.renderCalled);
}
```

---

## 5. 影响范围 (Affected Files)

| 文件 | 变更类型 | 说明 |
|---|---|---|
| `RenderContext.hpp` | **新增** | 上下文聚合器 |
| `GPUEntitySystem.hpp` | 修改 | 标记 `Get()` 废弃 |
| `GPUEntitySystem.cpp` | 修改 | 实现过渡期支持 |
| `MDIRenderer.hpp` | 修改 | 标记 `Get()` 废弃 |
| `MDIRenderer.cpp` | 修改 | 实现过渡期支持 |
| `Game.hpp/cpp` | 修改 | 管理渲染系统实例，构建 RenderContext |
| `GameplayState.hpp/cpp` | 修改 | 接收 RenderContext |
| `InventoryState.hpp/cpp` | 修改 | (如果使用渲染) |
| `tests/mocks/*` | **新增** | Mock 类 |
| 其他调用 `Get()` 的文件 | 修改 | 逐步迁移 |

---

## 6. 验收标准 (Acceptance Criteria)

- [ ] `RenderContext.hpp` 已创建。
- [ ] `GPUEntitySystem::Get()` 和 `MDIRenderer::Get()` 标记为 `[[deprecated]]`。
- [ ] `Game.cpp` 显式创建和管理渲染系统实例。
- [ ] `GameplayState` 通过 `RenderContext` 访问渲染系统。
- [ ] 编译时对剩余 `Get()` 调用产生警告。
- [ ] 创建 `MockGPUEntitySystem` 用于测试。
- [ ] 至少一个测试用例使用 Mock 验证。
- [ ] 所有现有测试通过。
- [ ] 视觉验收: 游戏运行正常。

---

## 7. 风险与缓解

| 风险 | 影响 | 缓解措施 |
|---|---|---|
| 大量调用点修改 | 引入新 Bug | 渐进式迁移，每批次验证 |
| 生命周期问题 | 悬垂指针 | RenderContext 仅存储指针，确保被引用对象生命周期更长 |
| 编译警告泛滥 | 开发体验下降 | 快速迁移主要调用点，减少警告 |
| 模板/类型擦除复杂性 | 实现困难 | 初期使用简单指针，不引入复杂抽象 |

---

## 8. 设计决策

| 问题 | 决策 | 理由 |
|---|---|---|
| 接口设计 | 使用原始指针而非虚函数 | 避免虚函数开销，保持 DOD |
| Mock 策略 | 手动 Mock 类 | 项目未使用 GMock，保持简单 |
| 废弃策略 | `[[deprecated]]` 属性 | 编译器原生支持，警告明确 |
| 过渡期保留单例 | 保留 `Get()` 但返回注册实例 | 兼容未迁移代码 |

---

*规格版本: 1.0*
*最后更新: 2026-01-26*
