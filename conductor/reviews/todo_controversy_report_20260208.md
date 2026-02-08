# NoMoreDay TODO 与争议点检索报告

- 生成日期: 2026-02-08
- 检索范围: `src/`, `tests/`, `conductor/`, `设计文档/`
- 关键词: `TODO`, `TBD`, `FIXME`, `HACK`, `XXX`, `待确认`, `争议`, `忽略`, `暂不`

## 1. 摘要

- 代码层 (`src`) 命中 `TODO`: **13** 条。
- 历史归档 (`conductor/archive`) 命中 `TODO/TBD`: **43** 条（多数为历史计划未勾选项）。
- 文档中“待确认/争议语义”命中: **11** 条。

结论:
- 当前主干最值得优先清理的是 `src` 中影响玩法闭环和存档正确性的 TODO。
- `conductor/archive` 大量 TODO 主要是历史 Track 留痕，不应与当前未完成开发混为一谈，但可作为风险追溯依据。

## 2. 代码层 TODO（按优先级）

### P0（高优先）

1. `src/game/systems/skill/SkillSystem.cpp:223`
- 标记: `// TODO: Calculate damage`
- 风险: 技能伤害路径可能缺失或占位，直接影响战斗结果可信度。

2. `src/engine/persistence/SaveManager.cpp:93`
- 标记: `name = "Hero" // TODO: Implement name selection`
- 风险: 存档头信息硬编码，角色命名流程未闭环。

3. `src/engine/persistence/SaveManager.cpp:95`
- 标记: `playtime = 0 // TODO: Implement playtime tracking`
- 风险: 存档统计不准确，影响进度与展示一致性。

### P1（中优先）

4. `src/game/systems/combat/CombatSystem.cpp:170`
- 标记: DoT 未来应绕过闪避判定。
- 风险: 持续伤害机制接入时容易产生规则错误（DoT 被错误闪避）。

5. `src/game/data/MonsterAffixRegistry.hpp:216`
- 标记: Accurate 词缀提到 `TODO: Homing logic in projectile`
- 风险: 词缀文案/设计与实际行为不一致。

6. `src/game/systems/skill/behaviors/InfiniteBlades.cpp:48`
- 标记: `Implementation TODO or check if logic exists`
- 风险: 天赋 551 相关行为可能未生效或重复实现。

### P2（低优先/体验完善）

7. `src/game/systems/item/InventorySystem.cpp:26`
- 标记: 根据类型补齐 `SpriteComponent`。

8. `src/game/systems/item/ItemFactory.cpp:1281`
- 标记: 添加 `SpriteComponent`。

9. `src/game/systems/item/ItemFactory.cpp:1411`
- 标记: 药水图标 `SpriteComponent`。

10. `src/game/systems/ui/UISystem.cpp:693`
- 标记: `"Quantity Popup (TODO)"` 文案占位。

11. `src/game/systems/item/SalvageSystem.cpp:104`
- 标记: 回收流程缺 `VFX/SFX`。

12. `src/game/components/MapFragmentComponent.hpp:7`
- 标记: 维度碎片词缀待扩展。

13. `src/game/data/BuffRegistry.cpp:4`
- 标记: Buff 对应关系待细化。

## 3. 争议/待确认点（文档与流程）

1. `conductor/workflow.md:47`
- 现状: 明确要求“暂停并等待确认”。
- 争议点: 与自动化连续交付节奏存在天然冲突，适合人工验收关键节点，但会降低流水线连续性。

2. `conductor/archive/equipment-level-system/spec.md:32`
- 现状: 对“已装备后因洗点降级是否强制卸下”采取“暂无机制，忽略”。
- 争议点: 这是显式的规则取舍，可能导致边界状态和玩家预期不一致。

3. `设计文档/符文之语系统详情.md:162`
- 现状: 文档列出“实现风险与待办”，包含词条映射不全、底材类型校验不足、名称覆写不可逆等。
- 争议点: 文档层已识别风险，但是否已全部映射到可执行任务需要二次核对。

## 4. 历史归档 TODO/TBD 解读

- `conductor/archive/core_risk_remediation_20260122/plan.md` 含大量未完成 TODO（转换链路、背刺安全瞬移、GPU 物理双缓冲、测试补齐）。
- 这些项多数属于历史 Track 的计划快照，不等同于“当前分支遗漏”。
- 但其中若仍未在 `src` 落地，仍可能是存量技术债来源。

## 5. 建议动作（最小闭环）

1. 建立“代码 TODO 白名单”
- 仅追踪 `src/` 与 `tests/`，将 `conductor/archive/` 设为历史参考，不纳入日常阻断。

2. 先清理 3 个 P0
- `SkillSystem` 伤害计算、`SaveManager` 名称/时长闭环。

3. 规则争议项建 ADR
- 对“降级后是否强制卸下”等取舍，补一页 ADR，避免后续反复。

4. 将文档风险转任务
- 把 `符文之语系统详情` 的风险条目映射到可跟踪 issue 或 Track，避免停留在文档层。

## 6. 附：快速检索命令

```powershell
rg -n -S "TODO|FIXME|HACK|XXX|TBD|待确认|争议" src tests conductor 设计文档
rg -n -S "TODO" src --glob "*.cpp" --glob "*.hpp"
rg -n -S "TODO|TBD" conductor/archive --glob "*.md"
```
