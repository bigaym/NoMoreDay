# Plan: 敌人渲染架构重构 (CPU 权威化)

## Phase 1: The Great Decoupling (解耦阶段)
**目标**: 移除 GPU 对 CPU 状态的写权限。

- **Task 1.1**: 分析并禁用 `GPUEntitySystem::SyncBack`。 [2h]
- **Task 1.2**: 修改 `physics.compute`，将其功能简化为仅处理 GPU 粒子或视觉效果，不再更新实体核心坐标。 [1h]
- **Task 1.3**: 验证 CPU 端物理/AI 逻辑是否足以支撑当前玩法。 [1h]

## Phase 2: Zero Latency MDI (零延迟渲染)
**目标**: 消除三缓冲导致的 N-2 延迟。

- **Task 2.1**: 重构 `PersistentBuffer` 使用逻辑，在 `MDIRenderer` 中实现当帧提交、当帧渲染。 [3h]
- **Task 2.2**: 调整 `cull.compute` 的输入，确保其读取的是当前帧刚写入的数据。 [1h]
- **Task 2.3**: 优化 SSBO 内存布局，对齐 CPU 数据结构与 GPU 数据结构。 [2h]

## Phase 3: Authority Cleanup (清理与加固)
**目标**: 移除陈旧代码，确保系统稳定性。

- **Task 3.1**: 彻底移除 `PersistentBuffer` 中用于“读取回写”的同步栅栏 (Fence) 逻辑。 [1h]
- **Task 3.2**: 修复因移除 GPU 物理而失效的碰撞检测（若有）。 [2h]
- **Task 3.3**: 性能基准测试：验证大规模敌人下的 CPU 压力。 [1h]

## 验证目标 (Validation)
- `ScopedTimer("MDI_Cull") < 0.2ms`
- `ScopedTimer("MDI_Draw") < 0.5ms`
- 视觉观测：敌人死亡或位移时不再有“瞬移回去”的灵异现象。
