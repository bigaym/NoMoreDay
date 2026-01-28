# Static Variable & Global State Risk Audit (2026-01-28)

## 0. 审计概述 (Audit Overview)
本次审计利用 `code-risk-analyzer` 对 NoMoreDay 项目中静态变量（包括全局静态、类静态及局部静态）的使用情况进行了全量扫描。主要评估其在 **内存管理**、**并发安全**、及 **架构耦合** 三个维度的潜在风险。

## 1. 核心风险等级 (Risk Assessment)

| 风险类别 | 风险描述 | 影响组件 | 严重程度 |
| :--- | :--- | :--- | :--- |
| **GPU 资源生命周期错误** | 静态变量持有 GPU 句柄（Shader/Texture），在显卡驱动卸载后依然尝试释放或保留。 | `UIMinimap`, `HoloBladeRenderSystem`, `RenderSystem` | **高 (High)** |
| **多线程竞态 (Data Race)** | 静态 Scratch Buffer (如 `s_entities_scratch`) 在多线程 Update 中被并发修改。 | `SkillSystem`, `PhysicsSystem` | **中 (Medium)** |
| **单例状态污染** | UI 系统大量使用 `static inline` 成员存储交互状态，导致存档切换或场景重置困难。 | `UICrafting`, `UIStash`, `UIInventory` | **中 (Medium)** |
| **并发同步安全性** | 全局 ID 生成器在自增时缺乏原子性保护，存在冲突可能。 | `CombatEventDispatcher` | **中 (Medium)** |

---

## 2. 深度风险点详情 (Detailed Findings)

### 2.1 GPU 资源孤岛 (GPU Resource Isolation)
- **发现**: `UIMinimap.cpp` 维护了跨帧的物理纹理 `s_minimapTexture`。
- **风险**: 若游戏在关闭窗口或重启 OpenGL 上下文时未精确调用 `Cleanup()`，由于静态变量的生存期晚于上下文销毁，会导致驱动层面的非法内存访问或泄露。
- **现状样本**:
  ```cpp
  // UIMinimap.cpp
  static Texture2D s_minimapTexture = {0};
  static std::vector<Color> s_minimapPixels;
  ```

### 2.2 静态 Scratch Buffer 竞态 (Concurrency Risk)
- **发现**: `SkillSystem.cpp` 使用 `static std::vector<entt::entity> s_entities_scratch` 作为临时的实体收集池。
- **风险**: 
  - `SkillSystem::Update` 虽然目前在主线程，但其函数签名已引入 `tf::Executor*`。一旦开启并行 Update，该静态池将成为竞态源头，导致 `vector` 内部崩溃。
  - 内存膨胀：静态池在峰值压力后不会释放 `Capacity`，导致内存持续占用。
- **改进建议**: 学习 `PhysicsSystem` 使用 `thread_local` 装饰符或将其移入实例。

### 2.3 UI 系统全局耦合 (Architectural Coupling)
- **发现**: `UICrafting` 等类几乎完全由静态私有成员构成。
- **风险**: 
  - **存档安全性**: 当玩家退出到主界面或加载新存档时，这些静态变量记录的 Item 引用（如 `m_forgeItem`）若不手动清除，会导致 UAF (Use-After-Free) 或逻辑错乱。
  - **测试困难**: 静态状态难以通过 Mock 进行单元测试。
- **现状样本**:
  ```cpp
  // UICrafting.hpp
  static inline entt::entity m_forgeItem = entt::null;
  static inline bool m_visible = false;
  ```

### 2.4 非原子性 ID 生成 (Sync Safety)
- **发现**: `CombatEventDispatcher` 使用局部静态变量生成唯一 ID。
- **风险**: `s_next_id++` 在 C++ 中不是原子操作。多线程并发注册 Handler 时会产生 ID 碰撞，导致 `Unregister` 失效或逻辑越权。
- **代码片段**:
  ```cpp
  // CombatEventDispatcher.cpp
  uint32_t& CombatEventDispatcher::GetNextId() {
      static uint32_t s_next_id = 1;
      return s_next_id;
  }
  ```

---

## 3. 正面实践 (Positive Examples)
- **`PhysicsSystem`**: 完美应用了 `thread_local` 缓存，在保证性能的同时解决了并发隔离问题。
- **`MapAffixRegistry`**: 使用 `static constexpr` 管理固定属性表，既保证了线程安全又实现了零开销查找。
- **`ResourceManager`**: 资源由实例持有且提供完善的卸载接口，是优于静态全局持有的资源管理范式。

---

## 4. 推荐改进行动 (Action Plan)

1.  **[P0] 并发加固**: 将 `CombatEventDispatcher::s_next_id` 替换为 `std::atomic<uint32_t>`。
2.  **[P1] 资源受控**: 移除 `UIMinimap.cpp` 中的静态 Texture，改为由 `ResourceManager` 管理，或确保 `Shutdown` 序列严格执行。
3.  **[P1] 状态隔离**: 
    - 为 `UIStash`, `UICrafting`, `UIInventory` 增加 `static void ResetState()` 方法。
    - 在 `Game::Restart()` 或返回主菜单时强制调用该方法。
4.  **[P2] 消除 Scratch 池**: 将 `SkillSystem.cpp` 中的 `s_entities_scratch` 重构为 `thread_local` 或传参模式，防止未来的并发展开导致 Crash。

---
**审计员**: Antigravity (Code-Risk-Analyzer Skill)
**日期**: 2026-01-28
