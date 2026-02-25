# NoMoreDay 战斗系统现状审查（对标 ARPG 完整战斗标准）

审查日期：2026-02-25  
审查范围：`DamagePipeline`、`CombatSystem`、`EffectSystem`、`SkillSystem`、`SummonSystem`、`SkillRegistry` 与技能合同配置  
证据原则：代码执行路径 > 生效配置 > 设计文档

---

## 1. 结论摘要

当前系统已具备 ARPG 战斗核心骨架，但还不满足 POE / Grim Dawn / Diablo 系列的“完整系统”标准。  
当前评级：

- 功能完整度：`B-`
- 数值一致性：`C+`
- 可解释性：`C`
- 长线可运营性：`C`

阻塞上线的 P0 问题有 6 项，核心集中在：

1. 伤害入口双轨并存（旧链与新链并行）。
2. DoT 实际不扣血，语义与反馈断裂。
3. 触发/附加伤害系数已配置但未参与结算。
4. 批处理事件值与真实承伤不一致。
5. 召唤物路径存在绕过统一结算的分叉。
6. 技能合同配置漂移，自动校验当前失败。

---

## 2. 关键证据（代码与配置）

## 2.1 伤害入口双轨并存

- 新链：`DamagePipeline::Calculate` / `CalculateBatch`
- 旧链：`CombatSystem::CalculateDamage`
- 证据：
  - `src/game/systems/combat/CombatSystem.cpp` 仍有大量 `CalculateDamage` 调用。
  - `src/game/systems/combat/DamagePipeline.cpp` 已形成独立结算链。
- 影响：
  - 同一词缀/同一标签在不同技能路径表现不一致。
  - 平衡调参出现“同改一处、结果多口径”的不可控状态。

## 2.2 DoT 只计算和弹字，不落地扣血

- 证据：
  - `src/game/systems/combat/EffectSystem.cpp` DoT tick 仅调用 `DamagePipeline::Calculate` 与 `EmitDamagePopup`。
  - 当前 tick 路径未调用 `CombatSystem::ApplyDamage`。
- 影响：
  - 玩家看到伤害数字但目标生命值不变。
  - DoT 流派强度评估严重失真，属于功能正确性错误。

## 2.3 DoT 标签合同未强制执行

- 证据：
  - DoT tick 调用 `DamagePipeline::Calculate(..., Tag::None)`。
  - `DamagePipeline` 事件派发中以 `Tag::DamageOverTime` 区分是否进入 `OnSkillHit` 语义。
- 影响：
  - DoT 有机会被当作 Hit 处理，触发不该触发的击中收益。
  - 触发链和异常收益难以解释。

## 2.4 `added_damage_effectiveness` 未消费

- 证据：
  - 字段存在并解析：`SkillRegistry.hpp` / `SkillRegistry.cpp`。
  - 结算未使用：`DamagePipeline.cpp` 中未引用该字段。
- 影响：
  - 高频技能与低频技能无法用有效系数精准平衡。
  - 失去 ARPG 核心调参旋钮。

## 2.5 `trigger.effectiveness` 未消费

- 证据：
  - 字段存在并解析：`SkillContract.hpp` / `SkillRegistry.cpp`。
  - `SkillSystem.cpp` 触发派发只读取 `trigger_skill_id`、ICD、耗蓝，不读取 `trigger.effectiveness`。
- 影响：
  - 触发流派难做“可控强度曲线”。
  - 高频触发技能要靠硬阈值抑制，缺少线性调参手段。

## 2.6 批处理事件值与实际承伤值脱节

- 证据：
  - `DamagePipeline::CalculateBatch` 中先计算 `final_damage`（被反制/拦截后），再 `ApplyDamage`。
  - 事件仍使用 `res.damage` 构造部分事件载荷。
- 影响：
  - 战斗日志、回放、统计和实际血量变化不一致。
  - 平衡分析与 QA 复现失真。

## 2.7 召唤路径存在直接扣血分叉

- 证据：
  - `SummonSystem.cpp`（近战环绕）中存在固定值 `CombatSystem::ApplyDamage(..., 25.0f, summon.owner)`。
  - 灵剑另一路径通过 `SkillSystem::ShadowCast` 间接走技能流程。
- 影响：
  - 召唤流同系统内存在两套结算口径。
  - 归因、词缀继承、Proc 预算无法统一治理。

## 2.8 召唤组件热路径不够 DOD 友好

- 证据：
  - `SummonComponent` 含 `std::string name`（`SkillDefs.hpp`）。
- 影响：
  - 热路径缓存局部性与序列化治理受损。
  - 与“核心逻辑无字符串依赖”标准冲突风险增加。

## 2.9 配置合同漂移已发生且可复现

- 证据：
  - 2026-02-25 执行：`python scripts/gen_skill_contracts.py --check`
  - 结果：`[FAIL] skill_contract blocks are out of date.`
- 影响：
  - 技能树节点、合同上限、触发约束可能长期偏离真实实现。
  - 后续内容扩展时回归风险持续放大。

---

## 3. 对标差距模型

## 3.1 对标 POE（唯一结算入口 + 乘区治理）

- 已具备：标签化伤害池、转换链、Increased/More 思路。
- 缺口：
  - 入口不唯一（旧链并存）。
  - 关键系数（added/trigger effectiveness）未闭环。
  - 预算治理尚未形成系统合同。

## 3.2 对标 Last Epoch（可解释异常系统 + 专精触发）

- 已具备：专精节点、合同化触发框架。
- 缺口：
  - DoT/Hit 语义边界不稳。
  - Ailment 规则合同尚未形成单一权威引擎。

## 3.3 对标 Grim Dawn（构筑深度 + 防膨胀）

- 已具备：职业/星盘/装备多维叠加基础。
- 缺口：
  - 乘区预算与收益递减未制度化。
  - 召唤归因与预算治理不完整。

## 3.4 对标 Diablo 2/3/4（反馈稳定 + 战斗可读）

- 已具备：命中反馈、基础事件体系、技能派发骨架。
- 缺口：
  - 事件值与真实承伤不一致会破坏反馈可信度。
  - 缺少统一战斗遥测指标和发布门禁。

---

## 4. 优先级清单（按阻塞级别）

## 4.1 P0（必须先修）

1. 统一唯一伤害入口到 `DamagePipeline`。
2. DoT tick 强制 `Tag::DamageOverTime`，并落地 `ApplyDamage`。
3. 批处理事件统一使用 `final_damage`。
4. 接入 `added_damage_effectiveness`。
5. 接入 `trigger.effectiveness`。
6. 修复 `skill_contract` 漂移并纳入 CI 强校验。

## 4.2 P1（平衡可控）

1. 建立 ProcBudget（击回、触发、异常按秒预算）。
2. Ailment 合同化（叠层/刷新/覆盖/免疫/上限）。
3. Summon 统一归因与继承模式（Snapshot/Dynamic）。
4. 防御结算顺序合同化（护甲/抗性/减伤/屏障）。

## 4.3 P2（长线深度）

1. 构筑反约束层（互斥 Keystone、代价词缀、递减机制）。
2. Endgame 词缀到战斗合同映射。
3. 战斗拆解面板（来源与乘区可解释性）。

---

## 5. 完整系统验收定义（DOD: Definition of Done）

当且仅当满足以下条件，战斗系统可判定为“完整可运营”：

1. 所有伤害路径统一由 `DamagePipeline` 结算。
2. Hit / DoT / Ailment 语义互斥且可追踪。
3. 触发链受深度与预算双重约束，无无限放大路径。
4. 召唤伤害全部具备 `owner/summon/source_skill` 三元归因。
5. 事件值与真实承伤一致率 >= `99.9%`。
6. 配置合同校验在 CI 中持续通过。
7. 高压战斗场景下 p95/p99 帧时间满足发布阈值。

---

## 6. 本轮审查结语

当前系统不是“重做”，而是“收敛”。  
建议先完成 P0 收敛，再进入 P1 深化。若跳过 P0 直接做玩法扩展，后续会在平衡、性能和 QA 成本上被动放大。
