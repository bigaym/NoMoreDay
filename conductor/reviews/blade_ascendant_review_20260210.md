# 剑修 (Blade Ascendant) 技能实现审查报告

**审查日期**: 2026-02-10
**审查目标**: 验证 `设计文档\职业设计草案_剑修.md` 中规划的 9 个技能及其专精在代码中的实现情况与风险。
**审查状态**: ⚠️ 部分风险 (Partial Risk)

## 1. 总体概览 (Executive Summary)

当前代码库中已包含所有 9 个核心技能的基础框架与行为文件 (`src/game/systems/skill/behaviors/`). 
然而，**专精系统 (Talent System)** 的实现存在显著的碎片化问题。核心物理机制实现较好，但“元素转化”与“特定机制交互”（如物理碰撞忽略、分身逻辑）在代码中经常缺位或仅作为 TODO 存在。

此外，代码中普遍存在 **ID 硬编码 (Magic Numbers)** 与 JSON 数据文件不一致的现象，这是目前最大的维护风险。

## 2. 技能详细审查 (Skill-by-Skill Review)

### Skill 1: 流云刺 (Flowing Thrust)
*   **状态**: 🟢 良好 (已重构)
*   **实现**: `FlowingThrust.cpp` 使用了命名空间 `FlowingThrustNodes`，代码结构清晰。
*   **已实现特性**:
    *   基础伤害与位移。
    *   [110] 贯日 (穿透)。
    *   [112] 势如破竹 (距离增伤)。
    *   [130] 留影 (生成 Shadow 实体)。
    *   [133] 瞬狱影杀 (标记 ShadowKillArrayReady)。
*   **缺失/问题**:
    *   **[113] 风行者**: 仅实现了移速加成，**未实现**“无视体积碰撞”逻辑。
    *   **[170] 元素幻化**: 仅实现了冰霜 (Frost) 转化，完全缺失 **火焰 (Fire)** 和 **闪电 (Lightning)** 分支逻辑。
    *   **[150] 要害感知**: `DoHit` 中仅有空判断 `if (hp > 99%)`，内部逻辑为空 (TODO)。

### Skill 2: 裂空斩 (Rending Wave)
*   **状态**: 🟡 中等 (ID 硬编码)
*   **实现**: `RendingWave.cpp` 使用原始数字 ID (200, 210...)，可读性较差。
*   **已实现特性**:
    *   [210] 多重剑气 (数量增加)。
    *   [230] 回旋劲 (BoomerangComponent)。
    *   [233] 时空锁定 (Hover at apex)。
    *   [252] 剑意爆发 (消耗剑意倍增投射物)。
*   **缺失/问题**:
    *   **[211] 碎裂之刃**: 设置了 `OnDeathBehavior::Split`，但并未定义分裂出的子投射物数据，可能导致无效分裂。
    *   **[270] 元素形态**: **完全缺失**。代码中仅处理了 [250] 虚空转化，未见火/冰/雷逻辑。

### Skill 3: 灵剑决 (Blade Formation)
*   **状态**: 🔴 风险 (ID 混乱)
*   **实现**: `BladeFormation.cpp`.
*   **已实现特性**:
    *   [300] 剑池 (增加数量)。
    *   [330] 巨剑降临 (单体巨剑)。
    *   [353] 不灭剑魂 (免死标记)。
*   **缺失/问题**:
    *   **ID 不一致**: 代码中使用 `311` 表示 "Shockwave on Crit"，但在 JSON 中 `311` 是 "无尽剑匣 (Infinite Sheath)"。这是一个严重的逻辑错位。
    *   **ID 不一致**: 代码中使用 `321` 回蓝，JSON 中对应的是 `351` (气劲回流)。
    *   **元素附魔**: 缺失。

### Skill 5: 万剑归宗 (Infinite Blades)
*   **状态**: 🟡 依赖外部系统
*   **实现**: `InfiniteBlades.cpp` 仅负责设置 `ChannelingComponent`。
*   **风险**:
    *   实际发射逻辑位于 `SkillSystem` 或 `ChannelingSystem` 中，Behavior 文件本身无法验证具体的弹道逻辑。
    *   存在注释提及 ID 混淆 (551 vs 520)。

### 其他技能 (4, 6, 7, 8, 9)
*   **概况**: 均存在对应的 `.cpp` 文件，但预计共享上述问题：
    *   缺少元素分支实现。
    *   ID 硬编码风险。

## 3. 主要风险与问题 (Key Risks)

### 3.1 数据与逻辑脱节 (ID Mismatch)
这是最严重的隐患。C++ 代码中大量使用硬编码数字 (如 `if (spec.contains(311))`)，而 `skills.json` 的 ID 定义可能已经迭代变更（如 Skill 3 的 311/321 问题）。
*   **后果**: 玩家点了天赋树上的 A 节点，实际生效的可能是 B 效果，或者完全无效。

### 3.2 元素分支缺失 (Missing Elemental Variants)
设计文档承诺了每个技能都有“火/冰/雷”三系转化，但目前代码中绝大多数技能**仅实现了部分（主要是冰或虚空）或完全未实现**元素逻辑。
*   **后果**: 职业玩法的丰富度将大幅缩水，"灵根变转" 分支形同虚设。

### 3.3 复杂机制依赖缺失
部分高级机制（如“无视碰撞”、“分裂弹道的具体配置”、“分身模仿技能”）在 Behavior 层有标记，但在底层系统（Physics, ProjectileSystem）中可能缺乏对应支持。

## 4. 改进建议 (Recommendations)

1.  **全局 ID 重构 (Refactor to Constants)**:
    *   参考 `FlowingThrust.cpp` 的模式，为所有技能创建对应的 `Namespace` 和 `constexpr ID`。
    *   **必须**先根据 `skills.json` 校对一遍所有 ID。

2.  **元素系统补全 (Implement Elemental Factory)**:
    *   不要在每个 `DoCast` 里写 `if (isFire) ... else if (isIce) ...`。
    *   建议引入一个 `ElementalModifierSystem` 或在 `SkillBehaviorBase` 中提供通用的 `ApplyElementalConversion(proj, element)` 辅助函数，统一处理颜色、Tag 转换和特效。

3.  **修复 Skill 3 (灵剑决) 的 ID 错误**:
    *   立即修正 `BladeFormation.cpp` 中的 `311` -> `?` (需核对 JSON) 和 `321` -> `351`。

4.  **自动化测试**:
    *   编写一个简单的单元测试，加载 `skills.json` 并检查 C++ 中引用的 ID 是否存在于 JSON 数据中，防止“幽灵 ID”。

---
**报告生成**: Gemini CLI
**存档位置**: `conductor/reviews/blade_ascendant_review_20260210.md`
