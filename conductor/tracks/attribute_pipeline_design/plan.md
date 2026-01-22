# Attribute Pipeline Implementation Plan

## Phase 1: Pipeline Core & Data Logic (The "Refinery")
**目标**: 重构并统一属性计算核心逻辑，确保高性能与正确性。
**估时**: 2 Days

- [ ] **Task 1.1: Context-Aware Modifier Resolution**
    - 扩展 `StatsSystem::Calculate` 以支持 `CalculationContext`。
    - 实现基于 `TagRegistry` 的快速过滤逻辑。
    - *Risk*: 标签匹配的性能开销。需使用 Bitset 优化。

- [ ] **Task 1.2: Stat Conversion Logic**
    - 实现多级转换支持 (e.g., Str -> Armor -> Mitigation)。
    - 防止循环转换死锁 (限制转换深度 max_depth=2)。

- [ ] **Task 1.3: Effective Stats Calculation**
    - 在 `CombatStats` 中完善 `EffectiveXXX` 字段的计算公式 (考虑等级衰减、递减收益)。
    - 确保 UI 直接读取这些字段而无需自行计算。

## Phase 2: GPU Integration & Visuals (The "Hologram")
**目标**: 将属性数值实时映射到渲染表现。
**估时**: 1.5 Days

- [ ] **Task 2.1: GPUVisualStats Structure**
    - 在 `GPUData.hpp` 中定义 `GPUVisualStats`。
    - 修改 `SSBO` 布局以包含此数据。

- [ ] **Task 2.2: Attribute-to-Visual Mapping**
    - 在 `StatsSystem` 的 `Bake` 阶段，填充 `GPUVisualStats`。
    - 逻辑: 
        - `GlowColor` = Max(Fire, Cold, Light...) -> RGB.
        - `AnimSpeed` = `AttackSpeed`.

- [ ] **Task 2.3: Shader Update**
    - 修改 `entity_mdi.frag` 和 `entity_mdi.vert` 读取新的 SSBO 数据。
    - 实现动态发光和缩放效果。

## Phase 3: Validation & UI (The "Mirror")
**目标**: 验证数值准确性并展示在 UI 上。
**估时**: 1 Day

- [ ] **Task 3.1: Unit Tests**
    - 测试用例: 基础加法、百分比乘法、属性转换、Tag 过滤。
    - 验证 "Snapshotting" (快照) 机制 (如果涉及)。

- [ ] **Task 3.2: Character Panel Connection**
    - 确保护性面板 UI 代码从 `CombatStats` 的新字段读取数据。
    - 添加 Debug 视图查看 `GPUVisualStats`。

## Total Estimated Time: 4.5 Days
