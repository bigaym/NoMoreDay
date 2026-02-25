# NoMoreDay 战斗系统能力路线图（Benchmark-Driven）

文档日期：2026-02-25  
周期：M1-M3（3 个版本里程碑）  
目标：在保留当前可玩基础上，完成对标 POE / Grim Dawn / Diablo 2/3/4 的战斗系统能力闭环

---

## 1. 能力地图（目标态）

## 1.1 顶层能力域

1. 结算一致性（Single Source of Truth）
2. 状态语义（Hit/DoT/Ailment）
3. 防御合同（Damage Mitigation Order）
4. 触发与预算（Proc Governance）
5. 召唤策略（Summon Strategy）
6. 构筑约束（Anti-Meta）
7. Boss 与 Endgame 联动
8. 战斗可观测性（Telemetry）
9. 性能门禁（Perf Gate）
10. 配置治理（Contract CI）

## 1.2 对标映射

1. 对标 POE：
- 强项：乘区与标签化能力基础已具备
- 缺口：唯一结算入口、触发系数闭环、预算治理

2. 对标 Grim Dawn：
- 强项：职业/星盘/装备多维叠加已有地基
- 缺口：防膨胀机制、后期抗性与减免合同稳定性

3. 对标 Diablo 2/3/4：
- 强项：即时反馈链路已存在
- 缺口：事件可信度、Boss 机制分层、发布门禁体系

---

## 2. 当前能力分级（2026-02-25 基线）

1. 结算一致性：`L2`（有新链，但旧链未清退）
2. 状态语义：`L2`（Hit 基本可用，DoT/Ailment 不完整）
3. 防御合同：`L2`（公式存在，顺序合同未统一）
4. 触发与预算：`L1`（有 guard，无完整预算）
5. 召唤策略：`L1`（灵剑可用，系统层缺失）
6. 构筑约束：`L1`（有节点与词缀，无系统化抑制）
7. Boss/Endgame：`L1`（内容驱动弱，合同映射不足）
8. 可观测性：`L2`（有日志，缺统一指标面板）
9. 性能门禁：`L2`（有压测经验，缺战斗专项标准）
10. 配置治理：`L1`（存在 `--check`，当前失败）

能力等级定义：

- `L1`：功能可运行，但不可长期稳定扩展
- `L2`：可维护，但跨模块一致性仍不足
- `L3`：可扩展、可验证、可运营（目标）

---

## 3. 里程碑路线

## M1：口径收敛与正确性闭环（目标：L2 -> L3 基础）

关键交付：

1. `DamagePipeline` 成为唯一数值入口。
2. DoT 闭环修复（必扣血 + 正确标签）。
3. `added_damage_effectiveness` 与 `trigger.effectiveness` 正式生效。
4. 批处理事件值与实际承伤强一致。
5. 技能合同校验恢复通过并接入 CI。

门禁：

1. 关键正确性缺陷数 = 0（不扣血、重复触发、归因丢失）。
2. 事件一致率 >= `99.9%`。
3. 合同校验连续通过 `7` 天 nightly。

## M2：系统深度与构筑治理（目标：核心能力全面 L3）

关键交付：

1. AilmentEngine v1（叠层/刷新/覆盖/免疫）。
2. DefenseContract v1（统一结算顺序）。
3. SummonStrategy v1（命令、继承、预算、归因）。
4. ProcBudgetManager v1（按秒预算 + 帧预算）。
5. Anti-Meta 包（互斥 Keystone、代价词缀、收益递减）。

门禁：

1. 高攻速 + 多召唤场景收益无指数爆炸。
2. 四流派（直伤/DoT/召唤/触发）均可通关目标内容。
3. 版本内平衡热修次数较 M1 下降 >= `30%`。

## M3：运营就绪与内容联动（目标：全域 L3 稳态）

关键交付：

1. Endgame Modifiers 到 Combat Contract 的映射闭环。
2. Boss 机制框架（阶段化、反制窗、失败惩罚）。
3. 战斗遥测仪表板（构筑、技能、事件、性能四维）。
4. 发布门禁套件（CI + nightly + release gate）。

门禁：

1. 目标硬件下战斗帧时间 p95/p99 达标。
2. 新词缀/节点引入后 1 轮回归内可定位异常源。
3. 重大战斗回归率较 M1 下降 >= `50%`。

---

## 4. 能力项与前置依赖

1. Single Damage Entry  
前置：无  
依赖后续：所有系统

2. Damage Semantics v1  
前置：Single Damage Entry  
依赖后续：Ailment、ProcBudget

3. AilmentEngine v1  
前置：Damage Semantics v1  
依赖后续：Boss 机制、Endgame 词缀

4. SummonStrategy v1  
前置：Single Damage Entry、ProcBudget v1  
依赖后续：召唤流派拓展

5. DefenseContract v1  
前置：Single Damage Entry  
依赖后续：构筑平衡、Boss 难度曲线

6. Anti-Meta Layer  
前置：Telemetry + ProcBudget + DefenseContract  
依赖后续：赛季化构筑运营

7. Performance Gate Suite  
前置：Telemetry 基础  
依赖后续：版本发布准入

---

## 5. Track 拆解建议

1. Track A：`combat_semantics_unification`
- M1 主线，清理双入口与 DoT 断层

2. Track B：`combat_effectiveness_contracts`
- M1 主线，接入 added/trigger effectiveness

3. Track C：`summon_contract_foundation`
- M1-M2 桥接，召唤归因与统一结算

4. Track D：`ailment_defense_proc_v1`
- M2 主线，规则引擎与预算治理

5. Track E：`combat_telemetry_release_gate`
- M2-M3 桥接，观测与门禁体系

6. Track F：`boss_endgame_combat_linker`
- M3 主线，内容与战斗合同联动

---

## 6. KPI 面板（建议纳入发布评审）

1. Correctness KPI
- Event-Apply 一致率
- DoT 实际生效率
- Trigger 越界率

2. Balance KPI
- 四流派强度分布（P50/P90）
- 单一构筑占比（Meta 集中度）
- 热修频率与变更幅度

3. Performance KPI
- 战斗帧时间 p95/p99
- 事件队列峰值
- `GetStatWithTags` 锁等待与缓存命中率

4. Governance KPI
- 合同校验通过率
- 新内容合同缺失率
- 回归缺陷复发率

---

## 7. 完整系统判定标准（对标视角）

满足以下条件可判定“达到同类 ARPG 一线标准”：

1. 规则层：结算、状态、防御、触发、召唤均有单一权威合同。
2. 内容层：词缀、技能、Boss、Endgame 均通过合同扩展，不靠临时特判。
3. 运维层：有稳定门禁、遥测、回归策略，支持持续迭代。
4. 体验层：玩家可读、可解释、可构筑，且性能与反馈稳定。
