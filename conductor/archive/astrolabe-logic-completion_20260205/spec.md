# 技术规格书 (Spec) - 天赋系统逻辑完备性 (Astrolabe Logic Completion)

## 1. 目标 (Objectives)
解决天赋系统审计中发现的数值缩水、核心机制失效及点数缩放不匹配问题。确保 JSON 中的 node 效果能 100% 映射到 C++ 战斗表现。

## 2. 技术规格 (Technical Specifications)

### 2.1 数据模型对齐 (Data Model)
*   **AstrolabeComponent (Progression.hpp)**: 
    *   **主干**: `nodePoints (std::unordered_map<uint32_t, uint8_t>)` 成为唯一真理。
    *   **废弃**: `activated_nodes (std::set)` 标记为 DEPRECATED，仅用于旧存档向后兼容。
*   **Trait 系统**:
    *   增加 `TraitRegistry` 或扩展 `AstrolabeSystem` 以处理标记类组件。

### 2.2 核心逻辑组件 (Core Components)

#### A. 效果处理器 (Astrolabe Effect Processor)
在 `AttributePipeline` 中引入第二阶段处理循环，解析 `AstrolabeNodeEffect`：
```cpp
enum class AstrolabeEffectType : uint8_t {
    GrantComponent  = 0, // 授予组件 (如 SwordHeartComponent)
    ModifyIntent    = 1, // 修改机制数值 (如 MaxSwordIntent)
    ModifyStat      = 2, // 复杂属性修改
    SpecialBehavior = 3  // 注入 BehaviorInjectionRegistry
};
```

#### B. 数值缩放与标签 (Stat Scaling)
*   **StatsSystem::GetStatWithTags**: 修复对 `nodePoints` 的读取逻辑。
*   计算公式: `FinalValue = Base + Sum(Mod.Value * AllocatedPoints)`。

### 2.3 核心机制实现清单 (Sword Cultivator Focus)
1.  **剑意觉醒 (1010)**: 自动挂载 `SwordIntentComponent`。
2.  **剑心通明 (1011)**: 自动挂载 `SwordHeartComponent`。
3.  **剑意浩荡 (1030)**: 设置 `SwordIntentComponent::max_stacks += 2`。
4.  **心剑合一 (1031)**: 在属性管道末尾注入 `Intelligence -> CritMultiplier` 的转换。

---

## 3. 验收标准 (Acceptance Criteria)
1.  **AC-1**: 投入 5 点的小节点，在战斗统计面板（`GetStatWithTags`）中显示 5 倍加成。
2.  **AC-2**: 激活“剑意觉醒”节点后，玩家 Entity 自动拥有 `SwordIntentComponent`，UI 显示剑意 UI。
3.  **AC-3**: “剑意浩荡”加点后，剑意上限从 10 变为 12（且 UI 与逻辑同步）。
4.  **AC-4**: 转换类天赋（如智力转暴伤）能在角色属性面板实时更新。
