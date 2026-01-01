# 规格说明书 (spec.md) - 职业与技能系统基础设施 (Track: Class & Skill System Infrastructure)

## 1. 概览 (Overview)
本任务旨在实现《NoMoreDay》职业与技能系统的核心地基。重点在于构建高性能、标签驱动（Tag-Driven）的伤害计算流水线（Damage Pipeline），以及支持多层修饰符（Modifiers）的基础数据结构。

## 2. 需求分析 (Requirements)

### 2.1 标签与映射系统 (Tag Registry)
- **位掩码存储**：使用 `uint64_t` 作为标签的唯一标识，支持最多 64 个独立标签。
- **自动化生成**：
    - 输入：`assets/data/tags.json`（定义标签名称、分组和预留位）。
    - 输出：自动生成的 C++ 头文件（包含 `enum class Tag : uint64_t` 和字符串映射字典）。
- **分类规划**：预先划分 0-15 位（伤害类型）、16-31 位（形态标签）、32-47 位（机制标签）、48-63 位（状态/其他）。

### 2.2 伤害池与计算流水线 (Damage Pool & Pipeline)
- **DamagePool**：使用 `std::array<float, 16>` 存储基础点伤，索引对应伤害类型标签位，追求极致缓存局部性。
- **五步计算流 (The 5-Step Formula)**：
    1. **基础池构建**：整合武器、技能点伤及附加点伤。
    2. **转换与附加 (Conversion & Gain Extra)**：处理标签转换逻辑，支持“技能层”与“全局层”优先级。
    3. **线性增伤 (Increased)**：汇总所有匹配标签的百分比加成。
    4. **独立倍率 (More)**：应用独立的乘法因子。
    5. **暴击与防御结算**：最终数值输出。

### 2.3 修饰符系统 (Modifier System)
- **数据驱动**：修饰符包含 `source_tag`, `target_tag`, `value`, `type` (Convert, GainExtra, Inc, More)。
- **分层处理**：
    - 技能修饰符（挂载于技能实体）。
    - 全局修饰符（由装备、星盘汇总，挂载于玩家实体）。
    - 装备修饰符在冲突时拥有更高优先级。

## 3. 验收标准 (Acceptance Criteria)
- [ ] `gen_tags.py` 脚本能正确解析 JSON 并生成符合 C++20 标准的头文件。
- [ ] `DamagePipeline` 完成实现，且能通过单元测试验证复杂转换（如：物理转火焰并享受两层增伤）。
- [ ] 实现基础 `Skill` 组件，并能存储特定的修饰符列表。
- [ ] 整个流水线在主循环中**零内存分配**。

## 4. 超出范围 (Out of Scope)
- 具体的星盘 UI 渲染。
- 技能的视觉特效（VFX）。
- 技能的冷却、施法前摇等状态机逻辑（这些属于后续的状态系统任务）。
