# Phase 2: EnTT Group Memory Optimization 实施计划

**Track ID**: `performance_optimization/phase2_entt_group`  
**状态**: ✅ Completed  
**预计工时**: 2 天

---

## 任务分解 (Task Breakdown)

### Task 2.1: 创建 GroupRegistry 模块 ✅
**优先级**: Critical  
**预计时间**: 1h

**操作**:
1. 创建 `src/game/registry/GroupRegistry.hpp`
2. 定义 `RegisterGroups(entt::registry&)` 函数
3. 声明所有需要的 Group 类型别名

**验收条件**:
- [x] 编译通过
- [x] 无 EnTT 静态断言错误

---

### Task 2.2: 在 GameplayState 中集成 GroupRegistry ✅
**优先级**: Critical  
**预计时间**: 0.5h

**操作**:
1. 在 `GameplayState::OnEnter()` 最开始调用 `groups::RegisterGroups(*m_context->registry)`
2. 确保在任何 `emplace` 调用之前

**代码位置**: `src/game/states/GameplayState.cpp`

---

### Task 2.3: 重构 StatsSystem 使用 CombatGroup ✅
**优先级**: High  
**预计时间**: 2h

**操作**:
1. 修改 `StatsSystem::update()` 使用 Group 遍历
2. 引入 `StatsDirty` 组件作为 Group 拥有的组件，确保 dirty 实体被线性访问

---

### Task 2.4: 重构 GPUEntitySystem::Update 使用 RenderGroup ✅
**优先级**: High  
**预计时间**: 1.5h

**操作**:
1. 修改 `GPUEntitySystem::Update()` 和 `SyncBack()` 使用 RenderGroup 遍历
2. 确保物理原始数据（Position, Velocity, Radius, GPUIndex）在内存中连续

---

### Task 2.5: 重构 AISystem 使用 AIGroup ✅
**优先级**: Medium  
**预计时间**: 1.5h

**操作**:
1. 修改 `AISystem::update()` 使用 AIGroup 遍历
2. 优化热点循环中的 AI 决策逻辑访问模式

---

### Task 2.6: 创建 GroupLayoutTest ✅
**优先级**: High  
**预计时间**: 1h

**操作**:
1. 创建 `tests/unit/GroupLayoutTest.hpp`
2. 测试组件内存连续性
3. 验证 Group 遍历顺序

---

### Task 2.7: 运行全量测试并修复 ✅
**优先级**: Critical  
**预计时间**: 2h

**操作**:
1. 运行 `build.bat` 编译
2. 执行 `NoMoreDayTests.exe`
3. 验证 59 个测试通过，无回归

---

## 依赖关系

```
Task 2.1 ──► Task 2.2
                │
    ┌───────────┼───────────┐
    ▼           ▼           ▼
Task 2.3    Task 2.4    Task 2.5
    │           │           │
    └───────────┴───────────┘
                │
                ▼
           Task 2.6 ──► Task 2.7
```

---

## 验收清单

- [x] `GroupRegistry.hpp` 创建完成
- [x] `GameplayState::OnEnter` 正确调用 `RegisterGroups`
- [x] `StatsSystem` 使用 CombatGroup
- [x] `GPUEntitySystem` 使用 RenderGroup
- [x] `AISystem` 使用 AIGroup
- [x] `GroupLayoutTest` 通过
- [x] 所有现有测试通过 (59/59)

---

## 回滚计划

若遇到严重问题:
1. 注释 `RegisterGroups()` 调用
2. 将所有 `group<...>` 改回 `view<...>`
3. Group 代码保留，待问题修复后重新启用

---

*规划者: Gemini (Skill: designer)*
