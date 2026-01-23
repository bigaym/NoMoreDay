# Specification: Attribute Pipeline Cleanup

## 1. 问题陈述 (Problem Statement)
目前 `AttributePipeline` 中保留了 `Phase2_ResolvePrimary`, `Phase3_ResolveSecondary`, `Phase4_BakeEffective` 等空实现的函数，仅为了兼容 `AttributePipelineTest.cpp` 中的单元测试。这些测试是基于旧的、分阶段的实现逻辑编写的，而现在的 `Calculate` 函数已经将所有逻辑整合。保留这些空函数会导致代码混淆，且测试并没有真正验证 `Calculate` 的核心逻辑。

## 2. 目标 (Goals)
- **移除废弃接口**: 删除 `AttributePipeline` 中的 `PhaseX` 系列函数。
- **重构测试**: 更新 `AttributePipelineTest.cpp`，使其直接调用 `Calculate` 并验证最终的 `CombatStats` 结果，而不是验证中间步骤。
- **验证完整性**: 确保重构后的测试覆盖了原有的测试场景（属性转化、词缀应用、最终值计算）。

## 3. 技术规格 (Technical Specs)

### 3.1 AttributePipeline.hpp
移除以下声明：
```cpp
static void Phase2_ResolvePrimary(...);
static void Phase3_ResolveSecondary(...);
static void Phase4_BakeEffective(...);
```

### 3.2 AttributePipeline.cpp
移除对应的空实现。

### 3.3 AttributePipelineTest.cpp
将测试逻辑从“分步调用”改为“构建环境 -> 调用 Calculate -> 验证结果”。

**示例测试重构**:
*Old:*
```cpp
AttributePipeline::Phase2_ResolvePrimary(stats, mods, 1);
CHECK(stats.effective_strength == 16.5f);
```

*New:*
```cpp
// Setup Registry & Entity
entt::registry registry;
auto entity = registry.create();
registry.emplace<PrimaryStats>(entity, 10.0f, 0.f, 0.f, 0.f); // Base
// ... Add modifiers via ModifierList ...

AttributePipeline::Calculate(registry, entity);
const auto& stats = registry.get<CombatStats>(entity);
CHECK(stats.effective_strength == 16.5f);
```

## 4. 验收清单 (Checklist)
- [ ] `AttributePipeline.hpp/cpp` 中无废弃的 `PhaseX` 函数。
- [ ] `AttributePipelineTest.cpp` 编译通过并成功运行。
- [ ] 核心测试场景（Tag过滤、属性计算、结构布局）依然有效。
