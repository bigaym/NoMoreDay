# 实现计划 (Plan) - 天赋系统逻辑完备性

## Phase 1: 系统协议统一 (System Protocol Unification)
*   **Task 1.1**: 更新 `StatsSystem::GetStatWithTags`。将原有的 `activated_nodes` 迭代修改为对 `nodePoints` 的迭代，实现属性加成的点数缩放支持。
*   **Task 1.2**: 在 `AstrolabeSystem::addPointToNode` 中，确保在修改点数后调用 `AttributePipeline::Calculate` 的同时，清除 `StatsSystem` 的属性缓存。

## Phase 2: 效果引擎实现 (Effect Engine Implementation)
*   **Task 2.1**: 在 `AttributePipeline` 中实现 `ProcessNodeEffects` 私有方法。
    *   实现 `GrantComponent` 逻辑：根据 `TraitID` 将组件（如 `SwordHeartComponent`）添加/移除。
    *   处理 `Conversion`：确保 `conversions` 数组按点数加权应用。
*   **Task 2.2**: 扩展 `BehaviorInjectionRegistry`，注册 `profession_talents.json` 中涉及的所有特殊行为 ID。

## Phase 3: 核心天赋适配 (Core Talent Adaptation)
*   **Task 3.1**: 实现“剑意”上限动态调整。修改 `SkillSystem::UpdateSwordIntent` 或 `AttributePipeline`，使 `max_stacks` 响应天赋效果。
*   **Task 3.2**: 实现“心剑合一”（智力转暴伤）的转换逻辑。

## Phase 4: 开发验证 (Verification)
*   **Task 4.1**: 编写集成测试 `tests/integration/AstrolabeLogicTest.cpp`。验证多级属性缩放和组件挂载的正确性。
*   **Task 4.2**: 跑通全量测试并观察 `AttributePipeline` 性能开销。

---
## 进度监控 (Progress)
- [x] Task 1.1
- [x] Task 1.2
- [x] Task 2.1
- [x] Task 2.2
- [x] Task 3.1
- [x] Task 3.2
- [x] Task 4.1
- [x] Task 4.2
