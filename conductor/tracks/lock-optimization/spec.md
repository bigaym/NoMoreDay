# 技术规格书：锁与并发优化 (Lock & Concurrency Optimization)

## 1. 背景与目标
基于 2026-01-28 的并发审计报告，本项目目前的锁使用存在高危风险（RNG 竞争）、性能瓶颈（逐粒加锁）以及架构隐患（长时锁持有）。本 Track 旨在通过系统化重构，消除死锁/竞争隐患并提升高负载下的帧率稳定性。

## 2. 技术规格

### 2.1 线程安全 RNG (P0)
- **目标**: 消除 `FragmentDropSystem` 中的静态 `std::mt19937` 竞争。
- **方案**: 
  - 引入 `core/math/ThreadSafeRandom.hpp`。
  - 使用 `ThreadSafeRandom::GetInt(min, max)` 或 `GetFloat()` 替代直接操作 `std::mt19937`。
  - 如果 `ThreadSafeRandom` 未实现特定分布，则在 `FragmentDropSystem` 中使用 `thread_local` 静态 RNG。

### 2.2 最小化临界区与枚举重构 (P0)
- **目标**: 优化 `NemesisDataStore::GetTopAffixes` 并消除字符串依赖。
- **重构方案**:
  - 将 `kill_affix_history` 的存储类型从 `std::string` 更改为 `MonsterAffixType`。
  - **逻辑**:
    ```cpp
    // 优化后数据结构
    std::deque<MonsterAffixType> kill_affix_history;

    // GetTopAffixes 优化逻辑
    std::vector<MonsterAffixType> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        snapshot.assign(kill_affix_history.begin(), kill_affix_history.end());
    }
    // 在锁外使用 MonsterAffixType 进行统计和排序 (使用枚举索引数组代替 Map 性能更佳)
    std::array<int, static_cast<size_t>(MonsterAffixType::Count)> counts{};
    for(auto type : snapshot) counts[static_cast<size_t>(type)]++;
    // ... 排序逻辑 ...
    ```
  - **兼容性**: 在 `Save/Load` 时，使用 `MonsterAffixRegistry::GetAffixNameEn` 和 `GetTypeFromName` 进行字符串转换，确保存档文件的可读性。


### 2.3 高频发射器优化 (P1)
- **目标**: 消除 `GPUParticleSystem::Emit` 的锁竞争。
- **数据模型**:
  - `GPUParticleSystem` 维护一个 `std::vector<std::vector<GPUParticle>> m_threadLocalBuffers` (按线程 ID 索引) 或使用 `thread_local` 静态缓冲。
  - **无锁化方案**:
    ```cpp
    struct ThreadLocalStaging {
        std::vector<components::GPUParticle> buffer;
        void Flush(); // 将数据移动到全局 Buffer
    };
    ```
  - **合并逻辑**: 在 `GPUParticleSystem::Update` 的起始阶段，统一将各线程缓冲合并至 GPU 传输 Buffer。

### 2.4 资源管理读写锁 (P2)
- **目标**: 为 `ResourceManager` 提供基础并发保护。
- **实现**:
  - 使用 `std::shared_mutex m_resourceMutex`。
  - `getTexture`, `getShader` 等只读操作使用 `std::shared_lock`。
  - `loadTexture`, `loadShader` 等写入操作使用 `std::unique_lock`。
  - **断言**: 在修改方法中添加 `assert(IsMainThread())` 以兼容当前的加载约定。

### 2.5 零锁合并模式 (P2)
- **目标**: 优化 `ProjectileSystem` 的 Action 合并。
- **方案**:
  - 利用 Taskflow 的返回值或预分配的 Per-Task 容器。
  - 每一个 Chunk 任务返回一个 `std::vector<DeferredAction>`。
  - 在并行执行完成后，由主线程顺序执行 `insert` 操作（或使用 Taskflow 的 `transform_reduce` 模式）。

## 3. 验收标准 (AC)
- [ ] `FragmentDropSystem` 在多线程高频模拟下不崩溃。
- [ ] `NemesisDataStore` 排序逻辑在锁外执行。
- [ ] `GPUParticleSystem` 的 `m_emitMutex` 被移除或仅在 Batch 刷新时使用。
- [ ] `ResourceManager` 的并发访问受到 `shared_mutex` 保护。
- [ ] 全量测试用例通过，无 ThreadSanitizer 告警。
