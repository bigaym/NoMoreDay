# Plan: Attribute Pipeline Cleanup

## Phase 1: Test Refactor (The Green Light)
**目标**: 先重构测试，确保它们在不依赖 `PhaseX` 函数的情况下能正确验证 `Calculate` 逻辑。
**预估工时**: 1h

### Tasks
1.  **Refactor Test**: 修改 `tests/unit/AttributePipelineTest.cpp`。
    -   移除对 `Phase2`, `Phase3`, `Phase4` 的调用。
    -   使用 `entt::registry` 和 `Calculate` 重新实现测试用例。
    -   验证测试通过。

## Phase 2: Code Cleanup (The Purge)
**目标**: 移除废弃的接口和实现。
**预估工时**: 0.5h

### Tasks
1.  **Remove Interfaces**: 在 `AttributePipeline.hpp` 中删除 `PhaseX` 声明。
2.  **Remove Implementation**: 在 `AttributePipeline.cpp` 中删除 `PhaseX` 定义。
3.  **Final Verify**: 再次运行测试，确保一切正常。

## Phase 3: Integration Verify
**目标**: 确保对其他系统无副作用。
**预估工时**: 0.5h

### Tasks
1.  **Full Test Suite**: 运行所有单元测试。
