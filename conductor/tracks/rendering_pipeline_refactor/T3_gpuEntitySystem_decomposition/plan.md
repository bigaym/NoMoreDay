# Track 3: GPUEntitySystem Decomposition 实施计划 (V1.0)

> **Track ID**: `T3_gpuEntitySystem_decomposition`
> **依赖 Spec**: `spec.md` (V1.0)
> **依赖 Track**: T1, T2
> **预计工时**: 16-24h
> **风险等级**: 🔴 高

---

## 阶段总览

| 阶段 | 名称 | 核心产出 | 状态 |
|---|---|---|---|
| **Phase 0** | 基础设施 | 新建文件，定义接口 | 🔵 待开始 |
| **Phase 1** | SlotManager 抽取 | 槽位管理独立类 | 🔵 待开始 |
| **Phase 2** | PhysicsSync 抽取 | 物理同步独立类 | 🔵 待开始 |
| **Phase 3** | VisualSync 抽取 | 视觉同步独立类 | 🔵 待开始 |
| **Phase 4** | 集成与清理 | 重构 Update()，删除旧代码 | 🔵 待开始 |
| **Phase 5** | 验证 | 测试与性能基准 | 🔵 待开始 |

---

## Phase 0: 基础设施 (Infrastructure)

### Task 0.1: 创建 GPUEntitySync.hpp
- [ ] 在 `src/engine/render/` 创建 `GPUEntitySync.hpp`
- [ ] 定义 `GPUSlotManager` 类骨架
- [ ] 定义 `GPUPhysicsSync` 类骨架
- [ ] 定义 `GPUVisualSync` 类骨架
- [ ] 添加必要的 `#include`

### Task 0.2: 创建 GPUEntitySync.cpp
- [ ] 在 `src/engine/render/` 创建 `GPUEntitySync.cpp`
- [ ] 实现空的 `Init()`, `Execute()` / `Process()` 方法

### Task 0.3: 更新 CMakeLists.txt
- [ ] 将 `GPUEntitySync.cpp` 添加到 engine 库

### Task 0.4: 编译验证
- [ ] 执行 `.\build.bat`
- [ ] 确保新文件编译通过

---

## Phase 1: SlotManager 抽取 (Slot Management Extraction)

### Task 1.1: 实现 GPUSlotManager::Init
- [ ] 初始化 `m_maxEntities`
- [ ] 填充 `m_freeSlots` (逆序)
- [ ] 初始化 `m_slotToEntity`
- [ ] 注册 EnTT 销毁回调 (`on_destroy<GPUIndex>`)

### Task 1.2: 实现 GPUSlotManager::OnEntityDestroyed
- [ ] 从原 `GPUEntitySystem::OnGPUIndexDestroyed` 迁移逻辑
- [ ] 添加边界检查
- [ ] 回收槽位到 `m_freeSlots`

### Task 1.3: 实现 GPUSlotManager::Process
- [ ] 从原 `Update()` 中提取槽位分配/回收逻辑 (Line 179-218)
- [ ] 处理 `KilledTag`, `Projectile` 标记的实体
- [ ] 为新实体分配槽位

### Task 1.4: 集成到 GPUEntitySystem
- [ ] 在 `GPUEntitySystem` 中添加 `m_slotManager` 成员
- [ ] 在 `Init()` 中调用 `m_slotManager.Init()`
- [ ] 在 `Update()` 开头调用 `m_slotManager.Process()`
- [ ] 保留原逻辑作为 Fallback 对比

### Task 1.5: 验证
- [ ] 编译通过
- [ ] 运行游戏，验证实体生成/销毁正常
- [ ] 日志无异常

---

## Phase 2: PhysicsSync 抽取 (Physics Sync Extraction)

### Task 2.1: 实现 GPUPhysicsSync::Init
- [ ] 存储配置 (`maxEntities`, `teleportThreshold`)

### Task 2.2: 实现 GPUPhysicsSync::Execute - 核心逻辑
- [ ] 创建 `view<Position, Radius, GPUIndex>`
- [ ] 遍历并更新 `shadowBuffer`:
  - `position`, `prevPosition` (含传送检测)
  - `velocity` (从 `Velocity` 组件)
  - `radius`
  - `frameId`
- [ ] 返回 `highWaterMark`

### Task 2.3: 实现 Type & Flags 同步
- [ ] 从 `SpriteComponent` 获取 `type`
- [ ] 从 `PlayerTag`, `AIComponent` 打包 `flags`

### Task 2.4: 集成到 GPUEntitySystem
- [ ] 在 `GPUEntitySystem` 中添加 `m_physicsSync` 成员
- [ ] 在 `Init()` 中调用 `m_physicsSync.Init()`
- [ ] 在 `Update()` 中调用 `m_physicsSync.Execute()`
- [ ] 使用返回的 `highWaterMark` 优化 `memcpy` 范围

### Task 2.5: 单元测试
- [ ] 创建 `tests/unit/GPUPhysicsSyncTest.cpp`
- [ ] 测试用例: 线性位置更新
- [ ] 测试用例: 传送检测
- [ ] 测试用例: 空 registry

### Task 2.6: 验证
- [ ] 编译通过
- [ ] 单元测试通过
- [ ] 运行游戏，验证实体移动、插值正常

---

## Phase 3: VisualSync 抽取 (Visual Sync Extraction)

### Task 3.1: 实现 GPUVisualSync::Init
- [ ] 存储配置 (`maxEntities`, `refreshInterval`)

### Task 3.2: 实现 GPUVisualSync::Execute
- [ ] 创建 `view<GPUIndex, CombatStats>`
- [ ] 检查 `StatsDirty` 组件或周期性刷新条件
- [ ] 调用 `AttributePipeline::ToGPU()`
- [ ] 打包 `ActiveEffectsComponent` → `activeStatusMask`
- [ ] 更新 `statusTimer`

### Task 3.3: 集成到 GPUEntitySystem
- [ ] 在 `GPUEntitySystem` 中添加 `m_visualSync` 成员
- [ ] 在 `Init()` 中调用 `m_visualSync.Init()`
- [ ] 在 `Update()` 中条件调用 `m_visualSync.Execute()`

### Task 3.4: 单元测试
- [ ] 创建 `tests/unit/GPUVisualSyncTest.cpp`
- [ ] 测试用例: 脏标记触发同步
- [ ] 测试用例: 周期性刷新
- [ ] 测试用例: 状态效果打包

### Task 3.5: 验证
- [ ] 编译通过
- [ ] 单元测试通过
- [ ] 运行游戏，验证状态效果视觉反馈正常

---

## Phase 4: 集成与清理 (Integration & Cleanup)

### Task 4.1: 重构 GPUEntitySystem::Update
- [ ] 移除所有内联逻辑
- [ ] 实现 Facade 模式:
  ```cpp
  void GPUEntitySystem::Update(entt::registry& registry, float dt) {
      m_frameCounter++;
      float currentTime = (float)GetTime();
      
      // Phase 0: Slot Management
      m_slotManager.Process(registry);
      
      // Phase 1: Physics Sync
      GPUEntity* gpuPtr = (GPUEntity*)m_persistentEntityBuffer.BeginWrite();
      int hwm = m_physicsSync.Execute(registry, m_shadowBuffer, m_frameCounter);
      
      // Phase 2: Visual Sync (条件执行)
      m_visualSync.Execute(registry, m_visualStatsShadowBuffer, m_frameCounter, currentTime);
      
      // Bulk Upload
      size_t copyCount = std::min((size_t)hwm + 128, (size_t)m_maxEntities);
      memcpy(gpuPtr, m_shadowBuffer.data(), copyCount * sizeof(GPUEntity));
      MDIRenderer::Get().UpdateStats(m_visualStatsShadowBuffer, (int)copyCount);
      
      m_persistentEntityBuffer.Flush();
      m_persistentEntityBuffer.Lock();
  }
  ```

### Task 4.2: 删除原 Update 逻辑
- [ ] 删除 Line 166-305 的原始实现
- [ ] 确保无编译错误

### Task 4.3: 删除冗余成员变量
- [ ] 检查 `GPUEntitySystem` 中是否有可移除的成员
- [ ] 清理不再需要的 `#include`

### Task 4.4: 代码审查
- [ ] 确保无遗留逻辑
- [ ] 确保命名规范一致
- [ ] 添加必要注释

---

## Phase 5: 验证 (Verification)

### Task 5.1: 全量测试
- [ ] 运行 `.\build\bin\Release\NoMoreDayTests.exe`
- [ ] 确保所有测试通过

### Task 5.2: 性能基准测试
- [ ] 创建 `tests/benchmark/GPUSyncBenchmark.hpp`
- [ ] 20k 实体 PhysicsSync 基准 (目标 < 1.5ms)
- [ ] 20k 实体 VisualSync 基准 (目标 < 0.5ms)
- [ ] 20k 实体 完整 Update 基准 (目标 < 2ms)

### Task 5.3: 内存检测
- [ ] 使用 ASAN 运行测试
- [ ] 确保无内存泄漏、无 UAF

### Task 5.4: 视觉验收
- [ ] 运行游戏，进入高密度战斗场景
- [ ] 验证实体渲染正常
- [ ] 验证状态效果 (燃烧、冻结等) 视觉反馈正常
- [ ] 验证实体生成/销毁无闪烁或残影

### Task 5.5: 提交与标记
- [ ] Git Commit: `refactor(render): decompose GPUEntitySystem into modular sync jobs`
- [ ] Git Tag: `render_refactor_T3_complete`

---

## 任务依赖图

```
T1, T2 Complete
        │
        ▼
    Task 0.*
        │
        ├─────────────────────────────────┐
        ▼                                 │
    Task 1.* (SlotManager)               │
        │                                 │
        ▼                                 ▼
    Task 2.* (PhysicsSync)          Task 3.* (VisualSync)
        │                                 │
        └────────────┬────────────────────┘
                     │
                     ▼
              Task 4.* (Integration)
                     │
                     ▼
              Task 5.* (Verification)
```

---

## 回滚计划

如果某个 Phase 出现严重问题:

1. **Phase 1-3**: 可通过 `#ifdef USE_NEW_SLOT_MANAGER` 等开关回退到原逻辑。
2. **Phase 4**: Git revert 到 Phase 3 Complete 的 commit。
3. **紧急回滚**: `git checkout render_refactor_T2_complete`

---

*计划版本: 1.0*
*最后更新: 2026-01-26*
