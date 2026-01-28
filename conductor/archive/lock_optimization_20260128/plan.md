# 实现计划：锁与并发优化 (Lock & Concurrency Optimization)

## 任务列表

### Phase 1: P0 级风险修复 (稳定性)
- [ ] **Task 1.1: 重构 FragmentDropSystem RNG**
  - 修改 `src/game/systems/item/FragmentDropSystem.cpp`。
  - 引入 `core/math/ThreadSafeRandom.hpp`。
  - 验证：在 `OnEnemyKilled` 中移除静态 `std::mt19937`，改用线程安全 API。
- [ ] **Task 1.2: 优化 NemesisDataStore 锁持有时间**
  - 修改 `src/game/data/NemesisDataStore.hpp` 中的 `GetTopAffixes`。
  - 将 `std::sort` 逻辑移出 `m_mutex` 保护范围。
  - 验证：单元测试 `NemesisTests.exe`（如果存在）通过。

### Phase 2: P1 级性能优化 (吞吐量)
- [ ] **Task 2.1: GPUParticleSystem 批量发射重构**
  - 修改 `src/engine/render/GPUParticleSystem.hpp/cpp`。
  - 引入 `thread_local` 暂存缓冲区。
  - 在 `Update` 循环中实现高效合并。
  - 验证：在大量粒子产生场景（如 Blade Swarm）下，Frame Time 的 Lock Wait 显著降低。

### Phase 3: P2 级架构增强 (安全性)
- [ ] **Task 3.1: ResourceManager 读写锁改造**
  - 修改 `src/engine/resource/ResourceManager.hpp/cpp`。
  - 引入 `std::shared_mutex`。
  - 为获取方法添加 `shared_lock`，为加载方法添加 `unique_lock`。
  - 验证：添加多线程并发读取资源的单元测试。

### Phase 4: 并发模式演进 (可伸缩性)
- [ ] **Task 4.1: ProjectileSystem 零锁合并方案**
  - 修改 `src/game/systems/skill/ProjectileSystem.cpp`。
  - 移除 `actionMutex`。
  - 使用 Taskflow 的任务依赖或结果合并机制处理 `DeferredAction`。
  - 验证：确认弹幕数量极多时，系统伸缩性线性提升。

## 依赖关系
1. Phase 1 必须首先完成，以确保基准稳定性。
2. Task 2.1 与 Phase 4 属于高性能重构，可并行规划。

## 风险提示
- 读写锁 `std::shared_mutex` 在某些老旧编译器/环境下性能可能不如 `std::mutex`，需关注 Windows 平台实际表现。
- 零锁合并需小心处理 `DeferredAction` 的生命周期。
