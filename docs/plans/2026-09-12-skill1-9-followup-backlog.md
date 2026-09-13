# 技能1~9收尾销项清单与设计裁决清单

- 日期：2026-09-12
- 性质：活文档（销项即勾选并附证据；裁决项完成后在本表标注结论并关联设计文档/审查报告）
- 目的：汇总技能1~9专精周期、模块化技能重构、伤害管线现代化三个工作流中所有「另开任务 / 登记项 / 待确认」，给出 B1 / B2 / A 归属与优先级，并按影响面对设计裁决项排序。
- 后续批次原则（已定）：先做 B1（立即收尾），B2 并入 A（抽象化工作包第一批改动），避免同一批文件改两遍。

## 来源文档

| 来源 | 文件 | 结论 |
|---|---|---|
| 技能1 | `docs/reviews/2026-09-07-skill1-specialization-nodes-review.md` | 第6轮 提交 |
| 技能2 | `docs/reviews/2026-09-08-skill2-rending-wave-specialization-review.md` | 第3轮 提交 |
| 技能3 | `docs/reviews/2026-09-08-skill3-specialization-nodes-review.md` | §16.1 提交（附 §16.4 遗留） |
| 技能4 | `docs/reviews/2026-09-08-skill4-specialization-nodes-review.md` | §16.2 提交（附 §16.7 后续清单） |
| 技能5 | `docs/reviews/2026-09-09-skill5-specialization-nodes-review.md` | 三轮整改验证后合入（N10/N12/N13 列为后续任务） |
| 技能6 | `docs/reviews/2026-09-10-skill6-specialization-nodes-review.md` | §16.3 提交（仅 L4/L5 遗留） |
| 技能7 | `docs/reviews/2026-09-10-skill7-specialization-nodes-review.md` / `...-rereview.md` | 第二轮复核 可提交 |
| 技能8 | `docs/reviews/2026-09-10-skill8-specialization-nodes-review.md` | 第5轮 提交（附 §15.7 剩余风险） |
| 技能9 | `docs/reviews/2026-09-10-skill9-specialization-nodes-review.md` | 第4轮 提交（附 §14.4/§15.4） |
| 模块化 | `docs/plans/2026-09-06-modular-skill-archetypes-and-specialization-plan.md` / `docs/reviews/2026-09-07-modular-skill-archetypes-implementation-review.md` | 提交；Task 4.1 未完成 |
| 伤害管线 | `docs/plans/2026-09-11-damage-pipeline-modernization-plan.md` | 2026-09-12 结项关闭 |

## 归属与优先级图例

- **B1**：立即收尾（设计裁决、玩家可见缺口、测试护栏、依赖清算、小修）。
- **B2**：并入 A 的结构清理（与技能10~12迁移/抽象化共享同一批文件改动）。
- **A**：抽象化工作包（技能1~9实现抽象化、技能10~12迁移、跨技能清算尾项）；开工前需先过设计流程（`*-design.md` → `*-plan.md`）。
- **O**：环境 / 工具 / 美术 / 流程类，不占 B1 的主线排期。
- **P1**：本批必做；**P2**：本批可延后或依附裁决/批次；**P3**：低优先。
- 勾选约定：`- [ ]` 未销项；`- [x]` 销项（须附验证证据或报告链接）。

---

## 1. 设计裁决清单（按影响面排序）

排序依据：① 涉及的系统/技能范围 → ② 是否可能直接改变玩家强度或产生滥用 → ③ 是否阻塞后续数据化/抽象化。

| 序 | 编号 | 裁决项 | 影响面（排序理由） | 当前实现状态 | 来源 |
|---|---|---|---|---|---|
| 1 | RD-01 | 专精树 Keystone 数量上限与互斥规则（group1 全互斥与 GDD §3.0/§3.2 矛盾） | 全部 9 棵树的构筑规则；当前 4 Keystone 全互斥仅为临时兜底 | 临时兜底已上线，规则未裁决 | skill2 第2轮 §6、第3轮 §4② |
| 2 | RD-02 | 981+982 满损血暴伤无上限（设计为线性百分比，收益随损血线性放大） | 数值平衡风险最大；可能催生极端 build | 按线性实现，未封顶 | skill9 §15.4-1 |
| 3 | RD-03 | L-E：形态中重施法会重新武装免死、覆盖附魔窗口 | 潜在滥用路径（重施法重置生存资源） | 行为已存在，设计未规范 | skill9 §15.4-2 |
| 4 | RD-04 | 技能7 数值近似群：772 区域总伤害改由异常承担（待数值验证）、751 PercentAdd-15%、752 中心流血加速=强度+20%、754 最小实现 | 技能7 强度可能被近似改变；772 是实打实的伤害结构调整 | 近似上线，未验证 | skill7 rereview §5-3/§5-5 |
| 5 | RD-05 | 855 effectiveness=0.5 | 技能8 核心减益强度乘区 | 按 0.5 实现待确认 | skill8 §15.7-2 |
| 6 | RD-06 | 455「下一次攻击 More+20%」实现为 2s 全程 buff；435 crit_bonus=0.05 未实现；452 是否吃专精版流云刺加成 | 技能4 机制保真度（3 个节点） | 455 近似上线；435/452 未实装 | skill4 §16.7 R3-12/R3-13 |
| 7 | RD-07 | C0 Toggle 口径（370/372 互斥与 Toggle 语义裁定） | 技能3 节点语义与数据 | 待裁决 | skill3 §16.4 |
| 8 | RD-08 | 153 饮血刃语义：DoT tick 回血 vs 击中触发；是否限定「仅流云刺造成的流血」 | 影响该玩家施加的所有流血 tick 回血（跨来源） | 按 tick 回血上线，未限定来源 | skill1 §6.5-2 |
| 9 | RD-09 | L-F：免死判定位于 BladeFormation 无敌之前 | 技能9 边缘生存时序（可能差异极小） | 现顺序上线，未裁决 | skill9 §15.4-3 |
| 10 | RD-10 | 100% 概率保护仅覆盖 dodge/block，是否扩展至其他防御路径 | 技能8 生存覆盖率定义 | 仅两路径实现 | skill8 §15.7-2 |
| 11 | RD-11 | 技能5 N12 语义群：535 击晕范围（设计仅普通怪）、552 落点形态、534 蓝尽中断也触发终结、512 溅射递归链过滤 | 技能5 命中与异常链语义 | 现状偏强/待确认 | skill5 N12 |
| 12 | RD-12 | N13 基底交付形态：设计「屏幕内随机位置轰击天降剑气」vs 实现 ±20° 扇形直射（Barrage/Skyfall 未区分） | 技能5 视觉/手感层偏差 | 现状上线，未区分模式 | skill5 N13 |
| 13 | RD-13 | 251 HeavyMomentum 绑定（倾向保留）；M6 `skill_spec_modifiers.json` HeavyMomentum_Node213 语义；251 满层重置流云刺 CD 的 GDD 出处 | 技能2 数据语义与文档一致性 | 绑定保留待正式裁决；CD 缺出处 | skill2 第3轮 §4 |
| 14 | RD-14 | 分裂/爆炸子投射物整结构克隆会继承 ignore_resist（253+211 组合下子剑气无视抗性） | 技能1+2 组合语义（跨技能） | 现状继承，语义合理待确认 | skill2 第3轮 §4 记录项 |
| 15 | RD-15 | 815 治疗与 811 层数近似口径（AilmentEngine Bleed 上限 2 层、Additive 合并） | 技能8 数值近似保真度 | 近似上线 | skill8 §15.7-2 |
| 16 | RD-16 | ReactiveWardComponent 复活或删除 | 技能4 技术去留（影响 R3-7 处理方式） | **已裁决（2026-09-13）：删除**（保留原型枚举位，仅删组件结构与 emplace 点；见 skill-abstraction 设计 §7） | skill4 §16.7 R3-7 |
| 17 | RD-17 | `primary_archetype` 死字段删除（SkillDefs.hpp:571 只写不读，需存档/序列化兼容核实） | 技能8 数据结构清理 | **已裁决（2026-09-13）：删除**（写入 15 处含 BladeBoomerang.cpp:109；删前核实存档布局） | skill8 §15.4 N4-3 |
| 18 | RD-18 | UMR 并轨启动时机 | 排期类（不阻塞内容） | 路线不变，未启动 | skill1 §5.3-4 |

### 1.1 裁决结论（2026-09-13 用户拍板）

- **RD-02 已裁决：百分比封顶**——982「孤注一掷」暴伤加成改为设总上限（封顶值待定稿）；981 生命锁 33% / 禁疗 / +33% More 保持不变。
- **RD-01 已裁决：分组互斥 + 每树 Keystone 数量上限**——取代现「技能2 {213,214,234,253} 全塞 group1 全互斥」兜底（分组与上限值待定稿）。
- **RD-03 已裁决：重施法不重置免死**——免死仅在进入形态时授予，重施法不重新武装。
- **RD-04 已裁决**：772 先做数值验证对齐总量；751/752/754 接受现近似。
- **RD-05 已裁决**：855 effectiveness 保持 0.5 并登记。
- **RD-06 已裁决**：455 修正为「下一次攻击」语义；435 crit_bonus 实装；452 定稿为「使用已专精版本」。
- **RD-08 已裁决**：153 饮血刃仅对**流云刺来源**流血 tick 回血（覆盖首版 D6「全来源、降数值」），回血比例定稿 1.0。
- **RD-09 已裁决**：免死/无敌时序保持现状并登记。
- **RD-10 已裁决**：100% 概率保护仅覆盖 dodge/block，不扩展。
- **RD-11 已裁决**：按 N12 收窄（Wave C 已落地，确认）。
- **RD-12 已裁决**：基底扇形剑气洪流（GDD L340 已修订，确认）。
- **RD-13 已裁决**：251 满层重置保留并补 GDD 出处（已完成）。
- **RD-14 已裁决**：接受子投射物继承 ignore_resist。
- **RD-15 已裁决**：接受 815/811 现近似口径。
- **RD-18 已裁决**：UMR 并轨暂不启动（延后）。
- **RD-16 / RD-17 已裁决**：删除（2026-09-13，见上表）。
- **RD-01 定稿**：按角色分组互斥，每组选 1；每树 Keystone 总上限 = 组数。技能2 形态组 {213,214,234} 选 1 + 强度组 {253} → 最多 2 个；其余树按同法归纳角色组。
- **RD-02 定稿**：982 暴伤加成总上限 **+200%**（等价最多按 50% 缺血计入）。
- **RD-06/452 定稿**：按 `skills.json:452` 数据「使用**已专精版本**」实现，吃专精版加成。
- **RD-07 定稿**：确认 Toggle 非设计概念（GDD B1.1 已去 [Toggle]）；370/372 按形态类互斥。
- 全部 RD 裁决闭环；设计产物为 `docs/designs/2026-09-13-skill-rd-rulings-design.md`（修订首版设计，§6.1 列 10 项已定稿）。
- **实施状态（2026-09-13）**：代码改动型 RD（RD-01/02/03/06/08 + RD-14 确认）已按 `docs/plans/2026-09-13-skill-rd-rulings-plan.md` 实施完成；`build.bat`(RelWithDebInfo) EXIT 0、`ctest -C RelWithDebInfo -L ci` 全绿、数据脚本三连 PASS。执行记录与残余风险见该 plan §9。首版设计 D1/D2/D6/D10 以本设计为准。

---

## 2. B1 销项清单（立即收尾）

### 2.1 实现缺口

- [ ] **B1-01** [P1] 技能3 四项机制实现：372 瞬移+静电场 / 375 引爆 / 331 溅射 / 355 剑阵元素附加（数据已就绪）— 源：skill3 §16.4 MI-2/3/4/5
- [ ] **B1-02** [P1] 技能4 C5 剩余约 13 节点：476 曝光（完全无实现，优先）、472 静电光环、474 冰龙卷、402/403/410/413/415/431/433/434/453/454/473/475 — 源：skill4 §16.4 R3-8
- [ ] **B1-03** [P2] 技能4：455 一次性语义修正（依 RD-06）+ 435 crit_bonus 实现 + 435 断言 — 源：skill4 §16.7 R3-12
- [ ] **B1-04** [P1] 技能4：35.0f 基伤改读 mechanics `470.base_damage`；Melee 反击 5 道剑气实体（M3/M11 形态）— 源：skill4 §16.7 R3-3 剩余
- [ ] **B1-05** [P2] 技能5 N12 修正集（依 RD-11）：535 收窄至普通怪、552 环形落点、554/532/533 硬编码外置、534/512 按裁决处理 — 源：skill5 N12
- [ ] **B1-06** [P2] 技能5 N13 基底天降形态实现（依 RD-12）— 源：skill5 N13
- [ ] **B1-07** [P2] 技能8 N4-2：RemoveByKind 增加 source_skill_id 过滤重载（Buff.hpp:277-280），SkillSystem.cpp:1923 传 8u — 源：skill8 §15.4
- [x] **B1-08** [P1] 技能9：990 冰增幅接入 `DamagePipeline::CalculateBatch`（当前仅 Calculate 单目标生效；CalculateBatch 已 deprecated、生产批量走 ResolveDamageBatch→逐目标 Calculate，属潜在一致性缺口而非线上缺陷）— 源：skill9 §14.4-1
- [x] **B1-09** [P1] 技能9：消除 EffectSystem 与 `StatsSystem::UpdateBuffs` 对 `ActiveEffectsComponent::Update` 的双重调用（双倍衰减隐患，既存问题）— 源：skill9 §14.4-2
- [x] **B1-10** [P1] 技能9：跨技能非法标签清理——`assets/data/skills.json:714`（技能2 `sword_skill`）、`:1456`（技能3 `sword_skill`）、`:6349`（技能10 `sword_skill`）改 `SwordSkill`；`:3552`（技能6 `Duration`）调查后处置；技能10 部分随 A-04 — 源：skill9 §14.4-4、M2 §11.5
- [x] **B1-11** [P1] 技能2：FlowingThrust.cpp:378-384 残留 `b.id.find(...)` 字符串比较整改（按 RendingWave 枚举化先例，对应硬否决 §7.2）— 源：skill2 第3轮 §4
- [x] **B1-12** [P1] 技能5 N10（调查项）：gen 脚本泛化副作用影响面确认（技能1 契约新增 130/153/211/230/232、`mastery_skill_trees.json` 技能1 新增 1115）— 源：skill5 N10

### 2.2 测试护栏

- [x] **B1-13** [P1] 技能3：互斥「双点被拒」正向用例补强（SkillCastConstraintService 测试）— 源：skill3 §16.4
- [x] **B1-14** [P1] 技能4：keystone 角色精确集合断言（现断言不拒绝多出的 Keystone，参考 SkillContractRegistryTests.cpp:82-110 加 count 相等）+ DamagePipelineP1Tests 新增 Batch/Calculate 反击一致性用例 — 源：skill4 §16.5
- [ ] **B1-15** [P2] 技能4：435 剑意回复断言（伴随 B1-03）— 源：skill4 §16.5
- [x] **B1-16** [P1] 技能7：775 Lightning×730 组合、752 次级撕裂中心测试覆盖；M4 projectile 链路测试（P2）— 源：skill7 rereview §5-5/§5-6
- [x] **B1-17** [P1] 技能8：`GetEffectiveSkillTags` 的 `role != Transmuter` 过滤补技能3~7 专项回归 — 源：skill8 §15.7
- [x] **B1-18** [P1] 技能2：`crit_chance` 未归一化疑点核实——**不成立**：`stats->crit_chance` 为归一化 0..1（Stats.hpp:115 默认 0.05、AttributePipeline.cpp:774 写回 /100），DamagePipeline.cpp:1324-1327 注释明示分数制、:1606/:1850 直接比较；SkillSystem 三处直拷（:1532/:1600/:2096）正确。原记录（skill2 §4、行号 1787）基于旧单位前提。遗留：技能1/2 审查文档中 crit 单位旧口径残留待文档清理 — 源：skill2 第3轮 §4
- [x] **B1-19** [P2] 技能2：技能3 373 / 技能6 633 的 `trigger_skill_id=2` 既有数据核实 — 源：skill2 第3轮 §4 记录项〔销项 2026-09-13：T6.4 核实真实链接为技能3 node 335（`skills.json:1999`）与技能6 node 635（`skills.json:4128`），373/633 为天赋树条目、无 trigger 字段；断言落在 `SkillContractRegistryTests.cpp`，数据零改动，见 §9〕
- [~] **B1-20** [P2] 模块化：复审 §9 建议项核销（more_damage_mult 单位断言、SpawnShadow 统一工厂、Rebake 幂等/拦截骰子分布契约测试），未闭环部分并入 B2 — 源：模块化实施复审 §9〔拦截骰子分布子项已由 2026-09-13 计划 D1.1 闭环（SkillSystemTests 骰子分布契约测试），其余子项待 Wave E〕

### 2.3 验证任务

- [ ] **B1-21** [P1] 技能5：531 护甲与 551 闪避实机观测一次（确认 BuffEffect.modifiers 生命周期）— 源：skill5 第三轮记录（阻塞登记：无交互运行环境；自动化部分证据见 §8）
- [ ] **B1-22** [P1] 技能8：元素路径与 AreaFieldDeliverySystem 生命周期运行验证 — 源：skill8 §15.7（阻塞登记：无交互运行环境；自动化部分证据见 §8）
- [ ] **B1-23** [P1] 技能7：772 区域伤害改由异常承担后的数值验证（总伤害对齐设计）— 源：skill7 rereview §5-3（阻塞登记：无交互运行环境；自动化部分证据见 §8）
- [ ] **B1-24** [P2] 技能9：VFX `MAX_PHANTOM_OVERLAYS` 叠加上限性能复核（建议走 performance 工作流）— 源：skill9 §14.4-3
- [x] **B1-25** [P2] 技能1：173 碎裂溅射 ×1.05 CombatV2 桩核销（CombatV2 已删除，确认桩是否残留）— 源：skill1 §6.5-1〔销项 2026-09-13：`rg -n "CombatV2" src/` = 0 行，桩随 `4e7d2ac2`「drop combat_v2」/0ef68d2d 清空，无残留；见 §9〕
- [ ] **B1-26** [P2] 模块化：性能基线复跑（10k 施法、vs 旧 find() ≥2.5x、零堆分配、含洗点+换装 Rebake）— 源：模块化实施复审 §10

---

## 3. B2 销项清单（并入 A 的结构清理）

- [x] **B2-01** [P1] 模块化 Task 4.1：Legacy `ChannelingComponent` 清理（现仍被 12 个文件引用：BeamChannelDeliverySystem / DamagePipeline / DamageMitigationService / HeavenlySwordDescent / MindBlade / InfiniteBlades / SkillDefs.hpp / BladeMasteryService / StatsSystem / InputSystem / GameplayState / SkillSystem）+ `BoomerangComponent` 兼容过渡字段 — 源：模块化计划 Task 4.1〔销项 0ef68d2d：A1-3 T3.2；rg `ChannelingComponent` in src/ = 0，见 §9〕
- [x] **B2-02** [P1] 双管线共存窗口审计（Legacy Channeling 消费方与 BeamChannel 管线并行未逐一审计）— 源：模块化实施复审 §10〔销项 0ef68d2d：A1-3 T3.1 审计回滚窗口为死代码；运行时「观察一拍」无环境未验证，见 §9〕
- [x] **B2-03** [P1] 技能6 特例（SwordArray 保留在 DamagePipeline）与技能9 ProcEngine 硬编码 rule 9 的通用化 — 源：模块化实施复审 §10〔销项 0ef68d2d：A1-4 T4.2；ProcEngine 无硬编码 rule9，SwordArray 归因统一，见 §9〕
- [x] **B2-04** [P1] 技能4：反击逻辑 5 处合并为单一 helper — 源：skill4 §16.7 R3-3〔销项 0ef68d2d：A1-4 T4.1 `ResolveSkill4Counter` 单实现，站点实为 3 处（原清单计数订正），见 §9〕
- [x] **B2-05** [P1] 技能4：TagRegistry 收敛单源 — 源：skill4 §16.7 R3-4〔销项 0ef68d2d：A1-1 T1.4，见 §9〕
- [x] **B2-06** [P1] 技能4：SkillSystem InitHooks 改读 mechanics + BuffEffect static const — 源：skill4 §16.7 R3-6〔销项 0ef68d2d：A1-1 T1.5，见 §9〕
- [x] **B2-07** [P2] 技能4：OrbitingSentinelDeliverySystem 拦截死分支真删除 — 源：skill4 §16.7 R3-5〔销项 0ef68d2d：A1-5 T5.5，见 §9〕
- [x] **B2-08** [P1] 技能4：ReactiveWard 处置（RD-16 已裁决删除：保留原型枚举位，删组件结构与 `BladeWard.cpp:113-117` emplace；归 A1-2）— 源：skill4 §16.7 R3-7〔销项 0ef68d2d：A1-2 T2.4（RD-16）；rg `ReactiveWardComponent` in src/ = 0，见 §9〕
- [x] **B2-09** [P2] 技能4：M7 476 锚点、M8 分配器不可达诊断、M9 `skill_4_tree.json` 双源 — 源：skill4 §16.7 R3-14〔销项 0ef68d2d：A1-5 T5.6，见 §9〕
- [x] **B2-10** [P1] 技能5 N10：`allocated_points.find(NODE)` 重复 6+ 处提取 helper；Baker flags 与行为层 `allocated_points` 双源语义统一 — 源：skill5 N10〔销项 0ef68d2d：A1-1 T1.6，`allocated_points.(find|contains|count)` 直查全仓归零，见 §9〕
- [x] **B2-11** [P2] 技能5：M3 ChannelingComponent 注释旧语义清理（SkillDefs.hpp:1197-1216，随 B2-01 一并做）— 源：skill5〔销项 0ef68d2d：A1-2 T2.1，见 §9〕
- [x] **B2-12** [P2] 技能5：M4 双组件结构冗余收敛（随 B2-01 一并做）— 源：skill5〔销项 0ef68d2d：A1-2 T2.5，见 §9〕
- [x] **B2-13** [P2] 技能3：373 O(N) 敌人遍历空间网格化（待 DoHit 签名扩展）— 源：skill3 §16.4〔销项 0ef68d2d：A1-5 T5.2，见 §9〕
- [x] **B2-14** [P2] 技能3：374 debuffId `std::string` 改 BuffId 枚举 — 源：skill3 §16.4〔销项 0ef68d2d：A1-5 T5.3，见 §9〕
- [ ] **B2-15** [P2] 技能3：351 数据消费统一、ArmorShred id 重命名（消除与 RendingWave.cpp:537 / FlowingThrust.cpp:412 的名称冲突互刷）— 源：skill3 §16.4（PARTIAL：ArmorShred 枚举化/迁移**已完成**——`BuffId::ArmorShred` 位于 `src/game/foundation/data/BuffIds.hpp`，`rg '"ArmorShred"' src/` == 0；仅余 351 数据消费重命名未做，见计划 §2.2）
- [ ] **B2-16** [P2] 技能7：714 粗粒度（skill_id==7）精确化（取 711 时其余路径已禁用，可选优化）— 源：skill7 rereview §5-1（OPEN 可选，见计划 §2.2）
- [x] **B2-17** [P2] 技能8：N4-3 `primary_archetype` 死字段删除（RD-17 已裁决删除；影响面：Baker 14 处 + `BladeBoomerang.cpp:109` + BakerTests 3 处 CHECK；先核实存档/序列化兼容，归 A1-2）— 源：skill8 §15.4〔销项 0ef68d2d：A1-2 T2.2（RD-17）；rg `primary_archetype` in src/ = 0，见 §9〕
- [ ] **B2-18** [P2] 技能1：Slow/Chill legacy buff 收编 ailment 契约（契约补注册时统一处理，与 HazardSystem 同型）— 源：skill1 §6.5-3（PARTIAL：`ApplyFrostSlowDebuff` 未走 ailment 契约，见计划 §2.2）
- [x] **B2-19** [P2] 技能1：第4轮遗留统一处置（MEDIUM-5 四处死数据 `snapshot.payload_context`、UMR 并轨跳过项、ShadowDuplicationHook 特例）— 源：skill1 §6.5-4〔销项 0ef68d2d：A1-2 T2.3 + A1-5 T5.7，见 §9〕
- [x] **B2-20** [P2] 模块化：逐节点「烘焙→消费」映射表纳入仓库 — 源：模块化实施复审 §9〔销项 0ef68d2d：A1-6 T6.1（`docs/designs/2026-09-13-skill-baker-consumer-map.md`）+ T6.2 断言，见 §9〕
- [x] **B2-21** [P3] 技能6 L5：DeliveryArchetypesTests.cpp:190-210 以技能6 充当 Skyfall 残留域夹具，换独立 ID — 源：skill6 §16.3〔销项 0ef68d2d：A1-6 T6.3，见 §9〕
- [x] **B2-22** [P2] 技能9：技能12「绝影共噬」节点行为实现 + 技能9 联动回归（归入 A-03）— 源：skill9 §14.4-4〔销项 0ef68d2d：A2-2 T9.3，绝影共噬 1217 落地，见 §9〕
- [x] **B2-23** [P2]（**保留/降级，已裁决 2026-09-13**）伤害管线：deprecated 批量入口（`DamagePipeline::CalculateBatch` + `ResolveDamageBatch`）**保留现状，仅记录**，不迁移测试/基准调用点（`tests/unit/DamageElementIndexTests.cpp:109/:167`、`tests/unit/EventConsistencyTests.cpp`、`tests/performance/DamagePipelineBenchmark.cpp`）。依据：设计 §7/§11.4——保留至有等价替代测试入口；无生产改动 — 源：B1-08 遗留登记
- [x] **B2-24** [P2] 技能1/2：热路径字符串比较残留收编——`DamageConditions.cpp:19`（`id.find("Slow")`）、`BladeFormation.cpp:483`（`id.find("ignite")`），沿用 B1-11 的 BuffKind 枚举化思路 — 源：B1-11 复审 F4〔销项 0ef68d2d：A1-1 T1.6 + A1-4 T4.3；rg `id.find("Slow"/"ignite"/"shock")` in src/ = 0，见 §9〕

---

## 4. A 工作包（抽象化 + 技能10~12 迁移）

> 开工前先走设计流程，产出 `*-design.md` 与 `*-plan.md`；B2 全部条目作为第一批改动并入。

- [ ] **A-01** 技能1~9 专精实现抽象化/模块化：技能1~9 行为文件合计约 4542 行（FlowingThrust 955 / RendingWave 700 / PhantomTrance 747 / BladeFormation 517 / InfiniteBlades 522 / SwordArray 495 / BladeBoomerang 383 / BladeWard 137 / MindBlade 86）；全 12 技能行为文件合计约 6615 行（另加 HeavenlySwordDescent 832 / SevenStarSlash 681 / BloodSea 560）、Baker 1117 行 18 case。需先重定 DoD（恢复薄装配 ≤100 行/文件，或承认节点逻辑内聚、只抽取「触发/效果/交付」三层参数化模式）
- [ ] **A-02** 技能10~12 迁移：SevenStarSlash / HeavenlySwordDescent / BloodSea（专精树数据 + 行为 + 契约 + 审查）；含 HeavenlySwordField / BloodSeaField 组件迁移（另立计划），依赖 HeavenlySwordDescent.cpp:184-400、BladeMasteryService.cpp:51-88、SkillSystem.cpp:1080-1092
- [ ] **A-03** 技能12 绝影共噬节点行为（与 A-02 合并）
- [x] **A-04** 技能10 非法标签清算尾项（B1-10 的剩余部分）——复核结论：数据侧已无非法标签，技能10 tags 全为已注册项，无需改动；技能10 契约已在 `assets/data/skill_contracts_compact.json` 注册。证据见 §8。
- [ ] **A-05** B2 结构清理全集（见第 3 节）

---

## 5. O 类（环境 / 工具 / 美术 / 流程）

- [ ] **O-01** [P2] 技能8 N4-5 环境稳定性 triage：ParticleTrailBenchmark 阈值失败（0.262<0.2）、MaterialVFXBenchmark 退出 0xC0000005、GPU Timer 抖动 — 源：skill8 §15.7
- [ ] **O-02** [P2] 伤害管线 release-gate 计时噪声 triage（P4 报告 §5，非阻塞）— 源：伤害管线计划 §结项状态
- [ ] **O-03** [P2] Rendering track：`nmd.tests.gpu.hardware` 移交项（dynamic_combat_emissive High tier GI delta 0.000589/0.000592 < 0.001；OccluderExtractPass High tier p95 0.392ms > 0.3ms）— 源：skill2 第2轮 §6
- [ ] **O-04** [P3] 技能6 L4：codebase-memory 图索引重建（`ResolveSwordArrayHeavenlyAttunementConversion` 指向已不存在的 SwordArray.cpp:52-66）— 源：skill6 §16.3
- [ ] **O-05** [P2] 技能9：图标占位与 skill_node_prompts 美术替换 — 源：skill9 §14.4-3
- [ ] **O-06** [P3] 技能4 R3-10：AGENTS.md +3 行归属确认（待作者）— 源：skill4 §16.7
- [ ] **O-07** [P3] 技能1/2：`nmd.tests.performance` 与 `nmd.tests.gpu.hardware` 环境性失败登记（机器波动/渲染 GI 基线）— 源：skill1 §6.5-5、skill2 §6

---

## 6. 已销项 / 登记接受（防重复入账）

- [x] 技能9 死锁链 930→931→934→935（旧 26 节点树重写为 29 节点，931 问题闭环）— skill9 §14.1 C1
- [x] 技能1 H9d 私有特例删除、MEDIUM-2 方案A（BuffEffect.source_skill_id）、MEDIUM-4 八项实装 — skill1 第6轮用户决策
- [x] 技能7 713 满点连发万剑 Bug 及 5 项 Major（703/775/772/714/735）修复 — skill7 rereview
- [x] 技能5 N1~N9、N11、501 复利缺陷、AttributePipeline 消费 BuffEffect.modifiers 接线 — skill5 第三轮
- [x] 技能4 R3-1/R3-2/R3-9/R3-11 及测试 — skill4 §16.6
- [x] 技能6 除 L4/L5 外全部（含 L6/L7/L8、M10 测试体系）— skill6 §16.1/§16.3
- [x] 技能2 第二轮全部 Blocker/High/Medium 代码项；M-A 经复核回滚为「保留待设计确认」（转入 RD-13）
- [x] 技能1 登记接受项：MEDIUM-5 死数据维持、UMR 并轨跳过、ShadowDuplicationHook 特例维持（如需清理见 B2-19）

---

## 7. 批次验收要求（销项证据标准）

- 每个销项条目关闭时：在本清单勾选并附证据（文件:行 / 报告链接 / 测试名）。
- B1 批次出口：`build.bat`（RelWithDebInfo，0 警告）→ `ctest -L ci` 全绿 → 涉及数据的改动跑 `gen_skill_contracts.py --check` / `sync_skill_node_icon_ids.py --check` / `gen_asset_registries.py`；性能相关条目按 performance 工作流单独复跑并登记基线。
- 裁决项关闭：结论写入对应设计文档或审查报告，并在第 1 节表中标注「已裁决」。
- 每批交付后按 `docs/workflows/review.md` 出复审报告，结论 `提交` 后方可进入下一批。

---

## 8. B1 第一波销项证据（2026-09-12）

批次计划：`docs/plans/2026-09-12-skill1-9-followup-B1-wave1-plan.md`；复审报告：`docs/reviews/2026-09-12-skill1-9-followup-b1-wave1-review.md`。

- **B1-08**：调用链 `DamagePipeline.hpp:42-54`（deprecated 注释）→ `DamageResolutionHooks.cpp:53-67` → `DamagePipeline.cpp:2246-2256`（逐目标 `Calculate`）→ `:1417-1425`（990）。护栏用例 `[Unit] DamagePipeline P1 - Frost amp consistent across single and batch`（7/7）。deprecated 清理登记 B2-23。
- **B1-09**：`StatsSystem::UpdateBuffs` 删除重复 `ActiveEffects::Update` 与 swordStepDrainMult 段，`EffectSystem.cpp:74-88` 成为唯一 owner 并接管 StatsDirty 感知。`ctest -L ci` 1/1、`ctest -L skill` 2/2。
- **B1-10**：`assets/data/skills.json` 三处 `sword_skill`→`SwordSkill`（技能2/3/10）；技能6 `Duration` 经查为 legacy 别名映射到 `Tag::DamageOverTime`（`SkillRegistry.cpp:656-660`），保留并登记设计词汇对齐项（N10 调查附录 A）。
  - **A-04 收尾复核（2026-09-13）**：`rg 'sword_skill|"Duration"' assets/data/` 仅剩 `skills.json:3552`（技能6 `Duration` legacy 别名，保留），技能10 无残留；技能10 tags = `["Physical","Melee","Attack","Area","Hit","SwordSkill"]`，全部命中 `TagRegistry.hpp` 注册项（含 `kLegacyTags` 别名集合，脚本 `audit_tags` 校验 `illegal tags: NONE`，技能树 `add_tags/remove_tags` 亦 `NONE`）；技能10 契约（26 节点/2 转质/6 触发/剑意 3 节点）已在 `skill_contracts_compact.json` 注册，无需重生成。校验：`python scripts/gen_skill_contracts.py --check --check-idempotency --check-determinism` → `[OK] skill_contract blocks are up to date.`（exit 0）；`python scripts/sync_skill_node_icon_ids.py --check` → 技能树 255/76 节点全部 unchanged、missing 0（exit 0）。结论：A-04 无数据改动，仅销项。
- **B1-11**：`FlowingThrust.cpp:381-385` 热路径改 `GetByKind`（Bleed/Ignite/Chill/Freeze/Slow）；新增 `BuffKind` 值 `Bleed=4..Slow=8`，`kBuffKindCount` 4→9（`DamageConditions.hpp:53`）；各创建点补 `.kind`。
- **B1-12**：`docs/reviews/2026-09-12-n10-generator-side-effect-investigation.md` 结论「一致」；`python scripts/gen_skill_contracts.py --check --check-idempotency --check-determinism` PASS（exit 0）。
- **B1-13**：`[Unit] SkillCastConstraintService - Contract guard evaluation`（66/66，含技能3 370+372 及 4~7/9 双转质拒绝）。
- **B1-14**：(a) `[Integration] SkillContract - Compact mapping materialized`（151/151，keystone 角色精确集合）；(b) 生产批量/单目标一致性用例同 B1-08。
- **B1-16**：`[Functional] MindBlade - 772 Lightning × 730 次级撕裂挂 775 压制`（7/7）；752/775 中心判定已由 `tests/functional/MindBladeNodes.cpp:796` 与 `:558` 覆盖，无需重复。
- **B1-17**：`[Unit] SkillBehaviorGuard - Transmuter mutex and scope policy`（44/44，含非转质 keystone exclusion），覆盖 `SkillSystem.cpp:2437-2443`。
- **B1-18**：`docs/reviews/2026-09-08-skill2-rending-wave-specialization-review.md` 追加 crit 单位口径更正备注。
- **B1-21/22/23**：阻塞登记——无交互运行环境，无法实机观测。复现步骤与自动化部分证据：
  - B1-21：运行技能5 施放 531/551，观测 `BuffEffect.modifiers` 在 buff 到期后从属性管线移除；自动化部分证据 `tests/functional/InfiniteBladesNodes.cpp` 技能5 功能套件。
  - B1-22：技能8 元素路径触发 AreaField 后观测生命周期回收；自动化部分证据 `tests/unit/AreaFieldDeliveryTests.cpp`（ci 套件内）。
  - B1-23：技能7 772 区域伤害由异常承担后的总伤害对数设计；自动化部分证据 `[Functional] MindBlade - 772 感电区域不叠加全额伤害 (M3)` 与新增 772×730 用例。
- 构建备注：`build.bat`（RelWithDebInfo）成功；新增/改动文件零告警，日志中 C4834/C4002 等告警位于本波未触及的既有文件（`PhantomTrance.cpp:257`、`SkillTreeController.cpp`、`Game.cpp:259`、`DamageElementIndexTests.cpp:92`、`DamagePipelineP3bTests.cpp:269`、`SwordArrayNodes.cpp:40`），属既有基线，需另行清理，非本波引入。
- 复审与修复：复审报告第 3 轮结论 `提交`（`docs/reviews/2026-09-12-skill1-9-followup-b1-wave1-review.md`）。过程中修复：F1（`FlowingThrust.cpp` 175 传染减速未授权改名回退 `FrostSlow`）、F7（补回 `.type=BuffType::SpeedDown`，并新增回归用例 `[Unit] FlowingThrust - 175 spread slow keeps SpeedDown type on refresh`）、F2（`Buff.hpp` `AddOrRefresh` 刷新同步 `type`/`kind`，旧存档升级）、F6（`DamageConditions.hpp` `kBuffKindCount` 编译期 `static_assert`）。剩余风险：F3 帧序后移 1 帧（接受）、F4（已登记 B2-24）、F5 测试 `const_cast` 卫生、B1-21/22/23 运行时未验证、并行链接瞬态 `LNK1136`、历史告警基线。

## 9. 销项证据补充（2026-09-13，commit `0ef68d2d`）

本批基于技能抽象化与 B2 清理（`docs/plans/2026-09-13-skill-abstraction-and-b2-cleanup-plan.md`，提交 `0ef68d2d`）及其终审 `docs/reviews/2026-09-13-final-review-skill-abstraction-b2-cleanup.md` 对账，补齐 §2/§3 勾选证据；B2-23 保留裁决不变。

### 9.1 B2 结构清理（0ef68d2d）

| ID | 清理任务（该批 plan） | 证据 |
|---|---|---|
| B2-01 | A1-3 T3.2 | `rg ChannelingComponent src/` = 0 |
| B2-02 | A1-3 T3.1 | 审计结论：Legacy 回滚窗口为死代码；运行时「观察一拍」无环境未验证 |
| B2-03 | A1-4 T4.2 | `ProcEngine.cpp:54-55` 通用 `required_skill_id`，无硬编码 rule9；SwordArray 归因统一 |
| B2-04 | A1-4 T4.1 | `ResolveSkill4Counter` 单实现，站点 3 处（原清单计数订正） |
| B2-05 | A1-1 T1.4 | `TagRegistry.hpp` 生成单源 |
| B2-06 | A1-1 T1.5 | `SkillSystem::InitHooks` 451/455/432/435 改读 `GetMech` |
| B2-07 | A1-5 T5.5 | OrbitingSentinel 拦截死分支删除 |
| B2-08 | A1-2 T2.4（RD-16） | `rg ReactiveWardComponent src/` = 0 |
| B2-09 | A1-5 T5.6 | M8 分配器不可达诊断 + M9 `skill_4_tree.json` max_points |
| B2-10 | A1-1 T1.6 | `allocated_points.(find\|contains\|count)` 直查全仓归零 |
| B2-11 | A1-2 T2.1 | Channeling 旧注释删除 |
| B2-12 | A1-2 T2.5 | `BoomerangComponent` 逐字段复核 |
| B2-13 | A1-5 T5.2 | 373 改 `SpatialGrid` 查询 |
| B2-14 | A1-5 T5.3 | 374 `debuffId` 改 `BuffId` 枚举 |
| B2-17 | A1-2 T2.2（RD-17） | `rg primary_archetype src/` = 0 |
| B2-19 | A1-2 T2.3 + A1-5 T5.7 | `SkillSnapshot::payload_context` 死数据删除 |
| B2-20 | A1-6 T6.1 + T6.2 | `docs/designs/2026-09-13-skill-baker-consumer-map.md` + `SkillBakerFlagConsumerTests.cpp` |
| B2-21 | A1-6 T6.3 | 技能6 夹具改独立 ID |
| B2-22 | A2-2 T9.3 | 绝影共噬 1217 窗口增益落地 |
| B2-24 | A1-1 T1.6 + A1-4 T4.3 | `rg 'id.find("Slow"\|"ignite"\|"shock")' src/` = 0 |

### 9.2 B1 补充

- **B1-19**（T6.4）：`trigger_skill_id=2` 真实承载为技能3 node 335（`skills.json:1999`）与技能6 node 635（`skills.json:4128`）；373/633 为天赋树条目、无 trigger 字段；`SkillContractRegistryTests.cpp` 增断言固定链接。
- **B1-25**：`rg -n "CombatV2" src/` = 0；CombatV2 由 `4e7d2ac2`（drop combat_v2）移除，桩于 `0ef68d2d` 清空。

### 9.3 保留未勾选

B1-15（伴随 B1-03 PARTIAL）、B1-20、B1-21/22/23、B1-24/26、B2-15（PARTIAL）、B2-16（OPEN 可选）、B2-18（PARTIAL），及后续计划新增项均维持未勾选。
