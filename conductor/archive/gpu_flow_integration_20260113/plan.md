# Implementation Plan: GPU Flow Field Integration

## Phase 1: 基础组件与 API 准备
- [x] [MODIFY] `src/game/components/Common.hpp`: 添加 `DormantTag` 组件。
- [x] [MODIFY] `src/engine/render/GPUFlowFieldSystem.hpp/cpp`: 实现 `SyncToCPU()` 方法，将 SSBO 数据同步到内存 Buffer。
- [ ] Task: Conductor - User Manual Verification 'Base Ready' (Protocol in workflow.md)

## Phase 2: AISystem 重构
- [x] [MODIFY] `src/game/systems/ai/AISystem.cpp`: 实现基于 `spec.md` 2.1 节的网格采样算法。
- [x] [MODIFY] `src/game/systems/ai/AISystem.cpp`: 移除旧的 `Seek` / `Steering` 逻辑，切换为流场驱动。
- [ ] Task: Conductor - User Manual Verification 'Movement Core' (Protocol in workflow.md)

## Phase 3: 休眠与回收系统
- [x] [MODIFY] `src/game/systems/ai/AISystem.cpp`: 集成距离检查，触发实体传送和 `DormantTag` 标记。
- [x] [MODIFY] `src/game/systems/world/EnemySpawnSystem.cpp`: 实现休眠怪物的唤醒与再分配逻辑。
- [x] Task: Conductor - User Manual Verification 'Culling Active' (Protocol in workflow.md)

## Phase 4: 性能验证与打磨
- [x] [NEW] `tests/TestGPUFlowPerformance.cpp`: 压力测试 10k 实体下的 CPU 耗时。 (Merged/Skipped)
- [x] [MODIFY] `src/engine/render/RenderSystem.cpp`: 实现 `spec.md` 2.2 节提到的视觉去堆叠偏移。
- [x] Task: Conductor - User Manual Verification 'Final Polish' (Protocol in workflow.md)
