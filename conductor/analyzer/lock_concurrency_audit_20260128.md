# 🔒 锁使用与并发风险审查报告 (Lock & Concurrency Risk Audit)

**日期**: 2026-01-28  
**审查范围**: Physics, Render, Skill, Combat, Resource, Persistence  
**工具**: code-risk-analyzer  

本报告对 NoMoreDay 项目中的锁使用合理性、颗粒度及潜在的死锁风险进行了全面审查。

---

## 1. 🚨 高危风险 (Critical Risks)

这些问题可能直接导致数据竞争 (Data Race) 或未定义行为 (UB)，建议列为 P0 级修复任务。

### 1.1 `FragmentDropSystem` 的 RNG 线程安全隐患
*   **位置**: `src/game/systems/item/FragmentDropSystem.cpp` (Line 23)
*   **代码片段**:
    ```cpp
    static std::mt19937 s_fragmentRng(std::random_device{}());
    // ...
    void FragmentDropSystem::OnEnemyKilled(...) {
        // ...
        if (dist(s_fragmentRng) > dropChance) { ... }
    }
    ```
*   **风险分析**: `std::mt19937` 不是线程安全的。目前 `OnEnemyKilled` 作为回调注册在 `CombatEventDispatcher` 中。虽然当前的 `OnKill` 事件可能在串行阶段分发，但一旦战斗系统引入并行分发，或者不同线程触发生物死亡（例如 DoT 系统在并行更新中触发死亡），多个线程同时访问静态 RNG 将导致不可恢复的崩溃。
*   **建议方案**: 将 RNG 改为 `thread_local`，可以使用 `core/math/ThreadSafeRandom.hpp` (如果存在) 或本地静态变量。

### 1.2 `ResourceManager` 完全无锁
*   **位置**: `src/engine/resource/ResourceManager.cpp`
*   **风险分析**: `m_textures`, `m_shaders` 等 `std::unordered_map` 没有任何互斥保护。虽然目前资源加载主要发生在初始化阶段，但在引入 `SaveManager` 异步线程 (`tf::Executor`) 后，如果尝试在后台线程进行资源预加载或访问（例如截图保存、缩略图生成），将发生 Data Race。
*   **建议方案**:
    *   **短期**: 在所有公共方法首部添加 `assert(IsMainThread())` 确保只在主线程调用。
    *   **长期**: 引入 `std::shared_mutex` (读写锁) 保护资源 Map，支持多线程只读访问。

---

## 2. 🐢 性能瓶颈 (Performance Bottlenecks)

锁粒度设置不当导致的高频竞争，严重拖慢高并发场景下的帧率。

### 2.1 `GPUParticleSystem::Emit` 的“逐粒加锁”
*   **位置**: `src/engine/render/GPUParticleSystem.cpp` (Line 207)
*   **代码片段**:
    ```cpp
    void GPUParticleSystem::Emit(...) {
        std::lock_guard<std::mutex> lock(m_emitMutex);
        m_stagedParticles.push_back(particle);
    }
    ```
*   **性能影响**: 这是一个极高频调用的函数。在高负载战斗（如 Infinite Blades 技能爆发、大量爆炸）中，每帧可能触发数千次 `Emit`。为了插入一个微小的结构体而反复 Lock/Unlock，会导致严重的 Cacheline 颠簸和线程上下文切换开销。
*   **优化方案**:
    1.  **Thread-Local Buffer**: 每个工作线程维护一个 `thread_local vector<GPUParticle>`。
    2.  **Batch Commit**: 仅在帧结束或 Buffer 满时，一次性加锁将数据合并到主 Buffer。

### 2.2 `ProjectileSystem` 的合并锁
*   **位置**: `src/game/systems/skill/ProjectileSystem.cpp` (Line 466)
*   **代码片段**:
    ```cpp
    std::lock_guard<std::mutex> lock(actionMutex);
    globalActions.insert(...)
    ```
*   **性能影响**: 并行任务使用 `actionMutex` 将局部结果合并到全局列表。随着线程数增加，这个单一的合并点会成为 Amdahl 定律描述的串行瓶颈。
*   **优化方案**:
    *   **Zero-Lock Merge**: 修改 `run_parallel` 使其返回 `std::vector<DeferredAction>`。主线程在 `executor.run()` 完成后，负责收集并移动所有子任务返回的 Vector，完全消除互斥锁。

---

## 3. ⚖️ 架构设计隐患 (Architectural Risks)

### 3.1 锁内排序 (Sort inside Lock)
*   **位置**: `src/game/data/NemesisDataStore.hpp` -> `GetTopAffixes`
*   **代码问题**:
    ```cpp
    std::lock_guard<std::mutex> lock(m_mutex);
    // ... 构建 vector ...
    std::sort(sorted.begin(), ...); // 排序操作在锁的保护范围内！
    ```
*   **风险**: 随着历史记录数据量增长，排序操作耗时增加，会长时间阻塞其他尝试写入数据的线程（如战斗线程尝试记录新的 Kill Affix），造成瞬间卡顿。
*   **修复**: 遵循“最小化临界区”原则。在锁内只进行数据拷贝 (Snapshot)，释放锁后再对拷贝的数据进行排序。

### 3.2 `SharedContext` Taskflow 生命周期
*   **位置**: `src/app/SharedContext.hpp`
*   **风险**: 持有原始指针 `tf::Executor *executor`。必须确保 `Executor` 的析构严格晚于所有使用它的 Systems。否则在 Game 关闭阶段可能引发 Use-After-Free。

---

## 4. ✅ 优秀实践 (Good Practices)

*   **Taskflow Fork-Join**: `PhysicsSystem` 和 `DamagePipeline` 正确使用了 `executor->run(tf).wait()` 模式。这是一种结构化的并发，比手写 `std::thread` 或 `std::async` 更可控且开销更低。
*   **Persistent Buffers**: `PhysicsSystem` 使用 `thread_local vector` 作为暂存区，有效避免了高频内存分配，同时天然线程安全（只要不跨线程共享）。
*   **SIMD With Local State**: `DamagePipeline` 的 `CalculateBatch` 使用了栈上数组和 SIMD，避免了复杂的共享状态，展示了优秀的数据导向设计。

---

## 5. 🛠️ 行动建议 (Action Plan)

建议按以下优先级进行重构：

**Priority 0 (Fix Now)**:
1.  **FragmentDropSystem**: 将 `s_fragmentRng` 改为 `thread_local`。
2.  **NemesisDataStore**: 将 `GetTopAffixes` 中的排序逻辑移出锁外。

**Priority 1 (Performance)**:
1.  **GPUParticleSystem**: 实现 `Emit` 的无锁化/批处理化。移除 `m_emitMutex`。

**Priority 2 (Architecture)**:
1.  **ResourceManager**: 添加线程检查断言。
2.  **ProjectileSystem**: 优化 Action 合并逻辑，移除 `actionMutex`。
