# 技术规格书 (Spec): 静态变量安全性与热路径内存分配优化

## 1. 目标 (Objectives)
- **线程安全性**: 修复 `StatsSystem` 和 `ItemFactory` 中的多线程竞争风险。
- **性能优化**: 消除 `SkillSystem` 热路径中的每帧分配，减少 `AttributePipeline` 的计算开销。
- **架构合规**: 建立统一的静态资源管理模式，符合 DOD 和线程安全规范。

## 2. 核心改动方案

### 2.1 线程安全随机数 (Thread-Safe RNG)
- **位置**: `src/game/systems/item/ItemFactory.cpp`
- **方案**: 废弃全局静态 `g_rng`，引入 `core/math/ThreadSafeRandom.hpp` 中已有的线程安全随机数机制，或为 `ItemFactory` 实现专门的 `thread_local` RNG 实例。
- **C++ 定义**:
  ```cpp
  // 在 ItemFactory.cpp 中使用 thread_local
  static thread_local std::mt19937 t_rng; 
  ```

### 2.2 StatsSystem 缓存重构与生命周期增强
- **位置**: `src/game/systems/combat/StatsSystem.cpp`
- **方案**:
    1. **存储转移**: 将 `s_tagStatCache` 转移为 `StatsSystem` 成员或 `entt::registry` 上下文。
    2. **并发保护**: 引入 `std::shared_mutex` 实现读写分离锁。
    3. **引用完整性 (NEW)**: `GetStatWithTags` 的缓存 Key 包含 `source_entity`。需确保在 `on_destroy<CombatStats>` 时，不仅清除该实体的缓存，还需考虑清除以该实体作为 `source_entity` 的关联缓存（或通过版本号/时间戳机制使之自然失效）。
    4. **结构扁平化**: 使用 `uint64_t` 复合 Key 减少嵌套 Map 开销。

### 2.3 SkillSystem 迭代与事件分发优化
- **位置**: `src/game/systems/skill/SkillSystem.cpp`, `CombatEventDispatcher.cpp`
- **方案**:
    - **SkillSystem**: 废弃 `s_entities_scratch` 全量拷贝。采用 `entt::view` 结合延迟操作池。
    - **CombatEventDispatcher (NEW)**: 
        - 修复 `GetHandlers` 静态局部变量单例在多线程并发注册时的风险。
        - 方案：在游戏初始化阶段（主线程）完成所有核心处理器的注册，或使用原子操作保护注册表。

### 2.4 渲染与调试资源管理 (NEW)
- **位置**: `src/engine/render/RenderSystem.cpp`, `GPUFlowFieldSystem.cpp`
- **方案**:
    - **FlowField Sync**: 修改 `SyncToCPU` 或相关调试接口，使用预分配的成员变量 `std::vector` 接收数据，避免 `DownloadFlowField` 每帧产生临时容器分配。
    - **Static Cleanup**: 确保所有静态 `vector` 缓冲区在 `Shutdown` 时强制释放内存（`shrink_to_fit`）。

### 2.5 全局随机数规范化 (NEW)
- **方案**: 
    - 废弃 `ItemFactory` 及其它模块中的 `static std::mt19937`。
    - 统一使用 `core/math/ThreadSafeRandom.hpp` 或 `thread_local` 局部实例，消除并行模拟时的种子竞争。

## 3. 验收标准 (Acceptance Criteria)
1.  **并发测试**: 开启 8 个线程同时生成物品，无崩溃或数据异常。
2.  **性能基准**: 在 10,000 个实体在线时，`SkillSystem::Update` 的耗时降低 15% 以上。
3.  **内存检查**: 运行游戏 10 分钟，`StatsSystem` 内存占用保持稳定，切换地图后完全释放。
4.  **架构审计**: `static` 关键字的使用仅限于受保护的单例或 `thread_local` 缓冲区。