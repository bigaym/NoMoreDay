# 技能1~9收尾销项清单与设计裁决清单

- 日期：2026-09-12
- 性质：活文档（销项即勾选并附证据；裁决项完成后在本表标注结论并关联设计文档/审查报告）
- 目的：汇总技能1~9专精周期、模块化技能重构、伤害管线现代化三个工作流中所有「另开任务 / 登记项 / 待确认」，给出 B1 / B2 / A 归属与优先级，并按影响面对设计裁决项排序。
- 后续批次原则（已定）：先做 B1（立即收尾），B2 并入 A（抽象化工作包第一批改动），避免同一批文件改两遍。
- 状态：**CLOSED（结项归档，2026-09-17）**。本清单已随技能系统最终收尾包（`SKILL-SYSTEM-FINAL-CLOSURE`）完成全部条目的核销/转出，正式关闭，不再新增条目。
- 收尾盖戳（2026-09-17）：本轮最终收尾包交付 **F-06**（属性修饰符运行时来源通路修复——`src/game/contracts/impl/StatsSystem.cpp` 的 `apply_if_tags_match` 新增显式 `source_prebaked` 形参，恢复未声明 `required_tags` 的专精节点/技能修饰符交付）、**F-02**（只读档案安全查询统一——`SkillSystem::GetValidBakedSkillProfile` 取代 `GetBakedSkillProfile` 直连点并迁移调用位）、**F-01**（节点 1015 文案/数据/代码三方对齐，每级 0.03 秒），并补充 **11 项**自动化回归用例（`tests/unit/SkillSpecializationStatModifierTests.cpp`）。依据 `docs/plans/2026-09-17-skill-system-final-closure-plan.md` §3（Task 4.1）与 `docs/designs/2026-09-17-skill-system-final-closure-design.md` §3.4（Backlog 销项）。复核结论与测试证据由主代理按 `docs/workflows/review.md` 回填（**证据回填 2026-09-17**）：审查报告 `docs/reviews/2026-09-17-skill-system-final-closure-review.md`（第 1 轮结论 `修改`，仅因账本证据陈述不自洽；代码实现未发现 Blocker / High；第 2 轮跟进审查结论 `提交`，6 项发现全部闭合，仅余 R-1~R-7 已登记剩余风险）；`build.bat RelWithDebInfo` EXIT=0、0 错误、0 警告；四道离线门禁 `gen_skill_contracts.py --check --check-idempotency --check-determinism`、`sync_skill_node_icon_ids.py --check`、`validate_skill_spec_modifiers.py --check`、`gen_skill_mechanics_schema.py --check` 均 EXIT=0；定向 doctest `bin\NoMoreDayTests.exe --test-case="*SkillSpecializationStatModifier*,*SkillProfileResolveSentinel*,*SkillWrapupHardening*"` → 17 用例 / 97 断言、0 失败；`ctest --test-dir build -C RelWithDebInfo -L unit` 8/8、`-L integration` 6/6、`-L skill` 2/2、`-R nmd.tests.ci.nonperf` 1/1。

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

### 1.2 UMR 收尾与契约加固裁决（2026-09-17 用户拍板）

- **哨兵档案治理（决策 1）**：采纳方案 A——在 `ResolveBakedProfile` 统一校验有效性，若为哨兵直接返回 `nullptr`，使经该出口解析的行为层只需处理空指针语义。
  - 实证修订（2026-09-17 二轮复审）：`is_baked` 仅覆盖 `ResolveBakedProfile`；`SkillSystem::GetBakedSkillProfile`（约 30 处直连，含 `BeamChannelDeliverySystem.cpp:153`）不过滤，其空指针与数值守卫必须保留。
- **技能 10 判定半径（决策 2）**：采纳方案 A——接入行为层单源，`SevenStarSlash.cpp` 使用 `profile->area_radius` 作为基准进行多段几何派生。
- **技能 6 施法距离（决策 3）**：采纳方案 A——将 `del.range` 接入施法前置距离限制（选点距离不得超过 `del.range`）。
- **技能 10 节点 1015 闪避（决策 4）**：采纳方案 A——清理 `mastery_skill_trees.json` 中该节点误映射的 `stat_modifiers`。
  - 实证修订（2026-09-17 二轮复审）：原文"全局属性污染"不成立——节点 1015 不在 skill 10 的 `skill_contract.nodes[]` 中，`NodeContractData` 默认 `ScopePolicy::SkillOnly`（`SkillContract.hpp:64`），且全仓 `StatType::DodgeChance` 不经 `GetStatWithTags` 查询，故该修饰符极可能不生效；须先以 `skill_id = 0 / 10` 双断言证伪。
  - 语义修订：该节点 `desc_key` 描述的是"延长无敌帧前后容错窗口 + 降低 20%/40%/60% 减速与击退影响"，与闪避无关，属误映射。原 `mode:1 (PercentAdd)` 3 点为 `base × (1+0.6)`，故"局部 `Flat 20.0f`"既非旧语义等价、亦非文案语义，不得硬编码。
  - 处置：见 `docs/plans/2026-09-17-umr-skill-wrapup-and-contract-hardening-plan.md` Task 3.0~3.4。**已裁决方案甲**：清空误映射 `stat_modifiers`，不新增闪避 Buff；节点保留 `2010150` 无敌时长效果；文案字面语义（减速/击退）登记为 F-01，方案乙已否决。
- **装备范围折叠（决策 5）**：暂时跳过，专精收尾暂不引入装备外部范围折叠。
- **技能 9 节点 930 协同（决策 6）**：采纳方案 A——定稿为与位移/剑技协同（绝影姿态下施放位移/剑技暴击生成残影斩击）。
- **技能 4 节点 473/474 异常强度（决策 7）**：采纳方案 B——挂靠统一基础异常强度（AilmentEngine 基础单层强度 1.0），无需点数加成。
- **天剑降临多元素形态（决策 8）**：采纳方案 A——本次做设计定稿与数据契约，后续作为独立深化包落地。
- **模块化重构 DoD 目标调整（决策 9）**：调整初始“单个文件 ≤ 100 行”的僵化指标（该指标曾误导致所有技能实现被删除后重新重构补上），调整为以职责内聚、分层清晰、无跨系统 hack 为核心。
- **职业路线规划（决策 10）**：采纳方案 A——深挖剑修纵深，暂不开启其余 5 个职业的 build 与技能设计。
- **测试环境配置污染治理（决策 11）**：采纳方案 B——在测试/构建脚本层面自动清理或还原 `settings.json`。
  - 实证修订（2026-09-17 二轮复审）：仓库无 `.github` 目录，"仅在 CI 下还原"为死代码；改为在 `TestSetupScope`（`tests/TestCommon.hpp`）做条件保存/还原，本地与 CI 一致有效。

### 1.3 二轮代码实证复审新增跟催项（2026-09-17）

- [x] **F-01 节点 1015 字面语义未落地**：`desc_key` 所述"降低减速与击退影响"当前无对应 `StatType`（`Stats.hpp:224-284` 无减速/击退抗性枚举），需策划确认是补枚举还是改文案。 〔销项 2026-09-17（SKILL-SYSTEM-FINAL-CLOSURE F-01）：裁定为“改文案并同步数据/代码”落地——`assets/data/mastery_skill_trees.json` 节点 1015（踏虚）`desc_key` 改为“延长七星斩起手与收招阶段的无敌帧持续时间，每级延长 0.03 秒容错窗口”，同步修正 `设计文档/职业设计草案_剑修.md` 对应条目；参数权威值与回归基线见 `tests/unit/SkillSpecializationBakerTests.cpp`、`tests/unit/SkillWrapupHardeningTests.cpp`。依据 `docs/designs/2026-09-17-skill-system-final-closure-design.md` §3.4-3。〕
- [x] **F-02 `GetBakedSkillProfile` 直连残余**：约 30 处调用不经 `is_baked` 过滤，本轮仅登记，不在范围内修改。 〔销项 2026-09-17（SKILL-SYSTEM-FINAL-CLOSURE F-02）：新增安全只读查询 `SkillSystem::GetValidBakedSkillProfile`（`src/game/systems/skill/SkillSystem.hpp` / `SkillSystem.cpp`，未烘焙档案返回 `nullptr`），并迁移原直连调用位；回归由哨兵用例 `tests/unit/SkillProfileResolveSentinelTests.cpp` 与新用例集 `tests/unit/SkillSpecializationStatModifierTests.cpp` 覆盖。依据 `docs/designs/2026-09-17-skill-system-final-closure-design.md` §3.4-3。〕
- [ ] **F-03 技能 10 范围算子缺失**：`skill_spec_modifiers.json` 中技能 10 无 `SKILL_AREA_MULT`，`SevenStarSlash` 半径单源接入后仍无实际缩放，属预防性重构。 〔仍开放：最终收尾包 §3.4 未授权核销，维持登记（预防性重构，非本轮范围）。〕
- [ ] **F-04 门禁中止语义**：`build.bat:288-313` 9 处 precheck 均不阻断构建，需统一（既有缺陷，非本轮新增）。 〔仍开放：§1.4 实施期实证已证“前提不成立、9 处 precheck 均已 `if errorlevel 1 exit /b 1`、无需统一”，但设计 §3.4 未授权核销，维持登记并标注该实证结论。〕
- [x] **F-05 技能 9 节点 930/993、天剑降临三系合流**：仍在设计定稿阶段，实施另立计划。 〔销项 2026-09-17：按裁决从“技能债务”移入**未来玩法特性清单**，不作为技能专精主线交付项（技能 9 节点 930/993 残影斩击、技能 11 多元素形态均属玩法内容扩展包）。依据 `docs/designs/2026-09-17-skill-system-final-closure-design.md` §2.2 非目标 2 与 §3.4-6。〕
- [x] **F-06 专精节点无 tags 属性交付整体失效（高）**：`StatModifier.required_tags` 在 JSON 缺省时为 `Tag::None`（`Stats.hpp:399-406`），而 `StatsSystem.cpp:318` 的 `is_baked = (mod.required_tags == Tag::None)` 快路径会据此跳过 `<:487>` 处的专精节点修饰符；`AttributePipeline` 亦不折叠专精节点修饰符（仅 `global_mods` 与装备词缀）。故**所有未声明 `required_tags` 的专精节点属性修饰符在全局与技能域均不生效**，节点 1015 仅是其中一例。影响面超出本轮范围，需专项确认是补数据还是修交付路径。
   - **量化（2026-09-17 复核）**：`rg '"required_tags"' assets/data/mastery_skill_trees.json` 无命中；非空 `stat_modifiers` 的专精节点共 **42** 个（`mastery_skill_trees.json` 32 + `skills.json` 10），即全部处于静默失效状态，含技能 1–9 存量数据。建议独立立项并按技能分组回归（复核意见：不应在本轮 UMR 收尾内一并修复）。
   - **测试盲区登记（2026-09-17 三轮复审）**：本轮 Task 3.0 的灵敏度正对照只证明全局修饰符探针路径（`StatsSystem.cpp:318-323` 的 `apply_if_tags_match` 经 ModifierList 调用方）存活，**未覆盖专精节点应用路径**（`StatsSystem.cpp:487` 对 `specialized_slots` 的 `stat_modifiers`）。因此该门禁无法区分「节点 1015 修饰符被正确忽略」与「专精节点应用路径整体失效」两种解释；补正对照需一个 `required_tags != Tag::None` 的节点修饰符实例（合成夹具亦可），随本项一并修复。
  - **销项 2026-09-17（SKILL-SYSTEM-FINAL-CLOSURE F-06）**：`src/game/contracts/impl/StatsSystem.cpp` 的 `apply_if_tags_match` 新增显式 `source_prebaked` 形参（不再以 `required_tags == Tag::None` 快路径吞掉专精节点修饰符），5 处调用位（ModifierList / Astrolabe / SkillModifierComponent / GlobalModifierComponent / specialized_slots）按语义传参，恢复全部未声明 `required_tags` 的专精节点与技能修饰符交付；新增 11 项回归用例 `tests/unit/SkillSpecializationStatModifierTests.cpp`（覆盖技能域缩放与隔离、全局域、Keystone 排除、Transmuter、SkillModifierComponent / GlobalModifierComponent 回归、哨兵断言与资产不变量），由 `tests/CMakeLists.txt` 自动收集。依据 `docs/designs/2026-09-17-skill-system-final-closure-design.md` §3.4-3 与 `docs/plans/2026-09-17-skill-system-final-closure-plan.md` §3（F-06）。
- **F-07 `settings.json` 写入点需持续看护**：`QualityTierManager::Initialize`（`QualityTierManager.cpp:123`）无条件写回基准分/时间戳。**写入面穷举口径（2026-09-17 复检修订）**：(a) 显式字面量传参 7 处 / 5 文件——`MaterialLightingBenchmark.cpp:119`、`GPUABIBindingTierIntegrationTest.cpp:23`、`RenderSystemPhaseDToggleSmokeTest.cpp:63`、`MaterialLightingIntegrationTest.cpp:55`+`:86`、`VFXSequencerTest.cpp:152`+`:464`；(b) **无参默认实参路径** 1 处——`JFAPassUpsampleMaskTest.cpp:217`（`QualityTierManager.hpp:100` 默认实参即仓库 `settings.json`，无参调用同样写回）；(c) **间接路径** 1 例——`RenderSystemInitializeFailureTest.cpp` 的两个失败用例经 `RenderSystem::Initialize()` 触达 `RenderSystem.cpp:969`，且该写入发生在能力门禁 `:1008-1026` **之前**，必然先写后败。上述 7 文件现已全部接入 `TestSetupScope`；后续新增 `Initialize` 调用方须同时穷举 (a)(b)(c) 三类路径并加夹具，否则 DoD §1.5 回归。**根治候选（本轮未采纳）**：为 `QualityTierManager` 增加一个不写盘的测试入口（如 `InitializeForTesting()`）或让持久化由显式参数控制，可彻底消除 (b) 默认实参路径；本轮为控制生产接口改动面，仍采用夹具级方案。**轻量化候选**：当前 settings 快照/还原与 `Logger`/`ItemFactory`/技能表初始化耦合同一 `TestSetupScope`，使渲染/性能用例承担非必要初始化开销与技能子系统耦合；后续可拆出仅做 settings 快照/还原的轻量 guard。
- **F-08 离线 schema 扫描器形参误解析**：`gen_skill_mechanics_schema.py` 会把形参 `skillId` 解析到同名文件级常量 `constexpr uint32_t skillId = 0`（`CombatSystem.cpp:198`），伪造出 `0.0.base_range` 三元组并使 `--check` 失败。新增机制读取的形参需避开该命名。

### 1.4 实施期实证修订（2026-09-17，UMR 收尾落地）

- **Task 3.0 正对照被证伪**：计划原设断言 (2)"skill_id=10 时应可观测"不成立，实测该修饰符为双重惰性（见 F-06）。门禁结论改由断言 (1)（全局域未污染）加灵敏度对照（基线 `dodge_chance=0.15` → 探针须读出 15.0）支撑；`SkillWrapupHardeningTests.cpp` 已按实况断言并留证，Task 3.2/3.3 结论不变。
- **Task 5.2 前提不成立**：`build.bat` 既有 9 处 precheck 均已 `if errorlevel 1 exit /b 1`（计划 C-8 描述有误），无需统一；新增 2 处沿用同模式，共 11 处一致。
- **Task 5.3 死代码不存在**：`build.bat` 中无 `GITHUB_ACTIONS` / `settings.json` / `git checkout` 处理，无需删除。
- **范围扩展（边界说明）**：为补足 DoD §1.5，`TestSetupScope` 已接入 7 个文件——`tests/integration/{RenderSystemPhaseDToggleSmokeTest,GPUABIBindingTierIntegrationTest,MaterialLightingIntegrationTest}.cpp`（首轮）、`tests/unit/{VFXSequencerTest,RenderSystemInitializeFailureTest}.cpp`、`tests/performance/MaterialLightingBenchmark.cpp`、`tests/integration/JFAPassUpsampleMaskTest.cpp`（复检补齐 (a)(b)(c) 三类路径）。均属同主题必要扩展，计划文件清单已同步登记。
- **生成物更新**：`assets/data/skill_mechanics_schema.json` 的 `dynamic_keys` 新增 `base_range`（helper 形参改名 `beamSkillId` 后由扫描器归类为运行时变量键），`--check` 绿色。
- **构建脚本注释编码风险**：`build.bat` 中新增的中文 `REM` 行在 cmd 解析下可能因 UTF-8 字节与当前代码页错位而被截断为命令（实测报 `'...' is not recognized as an internal or external command`，构建仍继续但留下噪声）。该行为非致命、但会污染预检日志，本轮改为 ASCII 注释；后续在 `.bat` 内新增中文注释需实测解析后再提交。

### 1.5 独立扫描复审整改（2026-09-17，仓库根 `review.md`，11 项）

第三轮交付后由独立子代理对全部变更文件做了一次静态扫描，提出 11 项发现（1 High / 3 bug-medium / 3 maint / 3 test / 1 bug-low），**全部采纳并修复**：

- **[High] 空指针回归**：`ResolveBakedProfile` 第二步出口由「恒返回 `&scratch`」改为「哨兵返回 `nullptr`」后，唯一传 `fallbackSpec` 的生产调用方 `PhantomTrance.cpp` 无条件解引用返回值；技能 9 不在技能表时（异常态）会崩溃。已在消费侧补空指针守卫，回退到默认构造的 `PhantomTranceParams`（`PhantomTrance.cpp` `ResolveParams`）。**教训：出口契约变化必须逐一核对调用方，而非只看被改函数。**
- **[bug-low] 射程约束可绕过**：`SwordArray.cpp` 新增的施法落点钳制只覆盖新建分支，675 移形换阵的「重按挪阵」分支仍直接写原始目标点。已把落点解析上提为函数开头的单源 `cast_target`，两条路径共用；并新增功能用例 `675 Relocate clamps out-of-range target to cast range`（超距目标 → 400.0f，修复前会得到 10000.0f）。
- **[maint] 死分支与字面量漂移**：`SevenStarSlash.cpp` 的 `skillData ? ... : 96.0f` 分支不可达（前文已校验非空）；`96.0f` 与 Baker case 10 重复。已改为直接 `skillData->GetParam("radius", ...)`，并把默认值上提为 `SevenStarSlashConstants.hpp` 的 `kSevenStarSlashBaseRadius`（零依赖头），Baker case 10 与行为层回退共用。
- **[maint-medium] 哨兵半防护**：`BeamChannelDeliverySystem.cpp` 的 `is_baked` 过滤只加在半径上，同一 `profile` 仍在 `effective_tags` / `more_damage_mult` / `bonus_crit` 等处被无过滤解引用。已改为在 `GetBakedSkillProfile` 查询后集中归一（非 `is_baked` 即置空），半径处去掉冗余判断；不引入 `ResolveBakedProfile`（该路径逐帧执行，需避开回退重烘焙）。
- **[bug-medium ×2 + maint] `settings.json` 夹具加固**：`TestCommon.hpp` 的快照读取经 `istreambuf_iterator` 遇 I/O 错误会静默截断，配合「内容不同才写回」逻辑会以残缺快照**覆盖**真实文件；快照时文件不存在则完全不清理用例新建的文件；还原写失败被静默吞掉。已分别补：按文件大小校验完整性（残缺即放弃快照）、原本不存在时析构删除用例产物、`flush()` + `good()` 校验并 `DOCTEST_WARN_MESSAGE` 提示兜底命令。
- **[test-medium] 门禁断言恒真**：Task 3.0 的 `skill_id==0` 相等断言因 `ScopePolicy::SkillOnly` 结构性恒真，不构成可证伪证据。已改以数据契约为主要证据（`SkillRegistry::Get().GetSkillTree(10)->nodes[1015].stat_modifiers.empty()`，回写数据前必然失败），作用域探针降级为「不得回归」锁定项。
- **[test-low] 出口覆盖不全**：`SkillProfileResolveSentinelTests.cpp` 原先只钉住第一步（缓存命中）出口；已补第二步（`fallbackSpec` 未注册 → Bake 出哨兵 → `nullptr`）与第三步（未注册 ID 命中 `specialized_slots` → `nullptr`）两个用例，均对修复前实现可证伪。
- **[test-low] 用例重复**：`SwordArrayNodes.cpp` 的钳制断言与单元用例重复，且缺「653 随身剑垒 + 超距目标」覆盖。已删除重复断言，新增子用例「653 Mobile Fortress aura ignores out-of-range target」（落点须为施法者而非钳制后的 400）。
- **验证证据**：`build.bat RelWithDebInfo` EXIT=0 且告警数 0；新增两处 precheck（`validate_skill_spec_modifiers.py --check`、`gen_skill_mechanics_schema.py --check`）均 OK；`ctest -L unit` 8/8、`-L integration` 6/6、`-R nmd.tests.ci.nonperf` 1/1；技能 6/10、Wrapup、ProfileResolve 定向用例 35 用例 / 732 断言全绿；`gen_skill_contracts.py --check` EXIT 0；`settings.json` SHA256 与基线一致（未污染）。
- **本轮未采纳项**：`review.md` 未提出、但 §1.3 F-06 登记的「专精节点 `stat_modifiers` 全局静默失效」（42 个节点）与 F-02（其余 `GetBakedSkillProfile` 直调点）仍为独立立项范围，不在本轮修复。


### 1.6 第五轮复检整改（2026-09-17，`docs/reviews/...-review-round5.md`）

§1.5 的整改由独立复检子代理只读复核，判定 8/11 为**真正修复**，3 项为部分修复，**结论 `修改`**。已全部闭合：

- **[High] 夹具反向数据丢失（§1.5 整改自身引入）**：`TestCommon.hpp` 的三条「放弃快照」分支（`file_size` 失败 / 打开失败 / 读截断）都直接 `return`，而 `m_settingsFileExisted` 直到成功分支末尾才置位 → 析构走「快照时不存在」路径，把**真实的** `settings.json` 当作用例产物删除；注释宣称的「保守放弃」与代码相反。已改为三态：确认文件存在即置 `m_settingsFileExisted`，放弃分支统一置 `m_settingsSnapshotAbandoned`，析构先判存在性（删产物）再判放弃态（仅告警、绝不删改）。
- **[Medium] 清理失败静默**：`std::filesystem::remove(kSettingsFilePath, ec)` 丢弃 `ec` → 清理失败无任何可观测信号。已补 `DOCTEST_WARN_MESSAGE`。
- **[Medium] 哨兵消费点漏改**：`BeamChannelDeliverySystem.cpp` 的 `ResolveSkill7Element` 仍自行 `GetBakedSkillProfile` 且不判 `is_baked`，哨兵 `effective_tags` 可渗入（当前仅因默认 `Tag::None` 而无害）。已同补 `is_baked` 过滤并注明语义。
- **[Low] 重复断言未清干净**：`SwordArrayNodes.cpp` 保留的 560f 钳制断言与单测重复，且对修复前实现不失败（非独立证据）。已删除，子用例更名 `603 cast range parity`，注释显式交叉引用单测与 675 子用例。
- **[Low] 语义偏差未披露**：mobile-aura 覆盖现也作用于 675 挪阵分支，设计未明写。已在 design §2.2.2 补「落点单源（v1.3 修正）」说明及净影响可忽略的理由，测例由三种覆盖扩为四种。
- **[Low] 头文件重量**：为单个 `constexpr float` 让烘焙 TU 包含 `SevenStarSlashShared.hpp`（连带 `SkillSystem.hpp` / `SkillRegistry.hpp` / `SevenStarSlashSpecState.gen.hpp` / `BladeResourceService.hpp`）。已析出零依赖头 `SevenStarSlashConstants.hpp`。
- **[BestPractice] `build.bat` 注释冗余**：5 行语义重叠 REM 精简为 3 行。
- **[Low] 未跟踪文件**：8 个未跟踪文件（含 `SevenStarSlashConstants.hpp`）须显式 `git add`，禁止 `git add -u`。
- **验证证据（全部实跑）**：`build.bat RelWithDebInfo` EXIT=0、告警 0 命中，两道新门禁 OK 且先于 `gen_modifier_runtime_binary`；定向 80 用例 / 1184 断言全过；`ctest -L unit` 8/8、`-L integration` 6/6、`-R nmd.tests.ci.nonperf` 1/1；三道 Python `--check` 门禁 EXIT=0；`settings.json` SHA256 与基线一致。
- **保留为独立跟催项**：§6 最佳实践 1（为 `TestSetupScope` 构造可注入 I/O 失败的最小单测，需先抽象文件系统访问）；最佳实践 2/3（`is_baked` 生产者契约文档、`GetBakedSkillProfile` 直连点静态清单）与 F-02 合并立项；UI 展示路径（`GameUiSnapshotBuilder.cpp:406`、`PlayerHUD.cpp:144`）遍历哨兵档案维持既有残余判定。


---

## 2. B1 销项清单（立即收尾）

### 2.1 实现缺口

- [x] **B1-01** [P1] 技能3 四项机制实现：372 瞬移+静电场 / 375 引爆 / 331 溅射 / 355 剑阵元素附加（数据已就绪）— 源：skill3 §16.4 MI-2/3/4/5〔销项 2026-09-13：Wave C C1.1-C1.4；372/331/375 `BladeFormation.cpp:373/540/691-699/746`，355 `SummonAISystem.cpp:164-168` 改读 `array_haste_pct`；M-1/M-3/375 引爆复核见 review §8.1，见 §9.4〕
- [x] **B1-02** [P1] 技能4 C5 剩余约 13 节点：476 曝光（完全无实现，优先）、472 静电光环、474 冰龙卷、402/403/410/413/415/431/433/434/453/454/473/475 — 源：skill4 §16.4 R3-8〔销项 2026-09-13：Wave C C2.1-C2.3；476 元素曝光入 `BladeWard.cpp`，472/474 及其余十二节点消费 `skill_mechanics.json:275-382`，见 §9.4〕
- [x] **B1-03** [P2] 技能4：455 一次性语义修正（依 RD-06）+ 435 crit_bonus 实现 + 435 断言 — 源：skill4 §16.7 R3-12〔销项 2026-09-13：455 见 `docs/plans/2026-09-13-skill-rd-rulings-plan.md` §4 B3/§10 Wave D；435 见 Wave C C2.4（`SkillSystem.cpp:1250-1270`），见 §9.4〕
- [x] **B1-04** [P1] 技能4：35.0f 基伤改读 mechanics `470.base_damage`；Melee 反击 5 道剑气实体（M3/M11 形态）— 源：skill4 §16.7 R3-3 剩余〔销项 2026-09-13：`DamageInterceptors.hpp:55` 改读 `GetMech(4,470,"base_damage",35.0f)`；counter_swords 见 Wave C C2.5，见 §9.4〕
- [x] **B1-05** [P2] 技能5 N12 修正集（依 RD-11）：535 收窄至普通怪、552 环形落点、554/532/533 硬编码外置、534/512 按裁决处理 — 源：skill5 N12〔销项 2026-09-13：Wave C C3.1-C3.4（535 `BeamChannelDeliverySystem.cpp:953-967`、552 `:1114-1116`、532/533 外置、534/512 按 D8），见 §9.4〕
- [x] **B1-06** [P2] 技能5 N13 基底天降形态实现（依 RD-12）— 源：skill5 N13〔销项 2026-09-13：RD-12 裁决基底为扇形剑气洪流，无 Skyfall 值（`DeliveryArchetypes.hpp:153-156` 仅 ContinuousLaser/BarrageEmitter），GDD L340 已修订，无代码改动，见 §9.4〕
- [x] **B1-07** [P2] 技能8 N4-2：RemoveByKind 增加 source_skill_id 过滤重载（Buff.hpp:277-280），SkillSystem.cpp:1923 传 8u — 源：skill8 §15.4〔销项 2026-09-13：Wave C C4.1-C4.2（`Buff.hpp:319-331` 重载 + `SkillSystem.cpp:1982-1988` 传 8u），见 §9.4〕
- [x] **B1-27** [P3] 机制键消费侧盲区：代码 `GetMech` 消费但 `skill_mechanics.json` 表内缺失的键不告警（现 `SkillMechanicsRegistry` 仅覆盖「表内键名拼写 / 未消费」方向）— 源：followup review §5 M-2 / §8.1 / §8.4（R3 追加项）〔核销 2026-09-13：**前提不成立**——消费侧校验早在 `31cc67e6` 已实现：`src/game/foundation/data/SkillMechanicsRegistry.cpp:314-320` 方向二（`schema.readTuples` 缺失告警）、`:321-327` 方向三（动态键缺失），加载期 `:161` 无条件调用且 warning-only；本次仅补隔离回归测试 `tests/unit/SkillMechanicsKeySchemaTests.cpp:139`（`[Unit] SkillMechanicsKeySchema - Consumed Tuple Missing Is Diagnosed`，辅助 `:48`）；`build.bat` EXIT=0、`-L unit` 8/8、`-L ci` 1/1、`gen_skill_mechanics_schema.py --check` OK。残留：生成器 `--check` 对 missing key 仅 `WARN` 且退出 0，如需硬门禁另立；review §5 M-2 口径过时，以代码为准，见 §9.4〕
- [x] **B1-28** [P2] GDD §3.5「血相改写」分支与数据漂移 — 源：followup review §8.4 / rd-rulings plan §9-3 / 本次文档债务修订（设计 `docs/designs/2026-09-13-skill12-branchD-gdd-data-alignment-design.md`，已落地）〔销项 2026-09-13：用户裁定 D1=a/D2=a/D3=a/D4=a/D5=a；GDD §5.3.2 全 25 节点名称与前置对齐数据 `name_key`/`prerequisites`、多前置用「或」（引擎 OR：`SkillSystem.cpp:2230-2255` 命中即 break、`UISkillTalentTree.cpp:95-115`、`UISkillSpecRenderer.cpp:45-66`），1223/1224 采用数据 `desc_key`，并同步 §5.3.1/关键节点清单/§5.4 交叉引用；自检 `rg "污秽侵蚀|渴血回流|腐血蔓延|滞腐深渊" GDD` = 0、程序化对拍 names/prereq 25/25 一致。仅 GDD 文档改动，零运行时影响；原「`{1202:3 且 1209:3}`（且非或）」有误已订正，见 §9.4〕
- [x] **B1-29** [P3] 技能12 数据文案债务（B1-28 裁定 D5=b）：`assets/data/mastery_skill_trees.json` 的 `desc_key` 与机制不符——1220（`:2238`）写「抗性侵蚀 5/10/15」但 `max_points=1` 且机制为固定值（`skill_mechanics.json:1111-1116`）、漏报约 45% 物理占比转虚空；1223（`:2312`）`1s/2s/3s`+`15%/30%/45%` 与 `miasma_duration_per_point`/`void_damage_per_point`（`:1130-1132`）不符；1224（`:2342`）`4/8/12` 与 `resist_shred_per_point=2.0`（`:1134-1135`）不符。需核对档位后修数据显示文案 — 源：B1-28 设计提案 §3.3〔销项 2026-09-13：核对 `BloodSea.cpp:116-129,337-403,421` 机制真值后改写三处 `desc_key`（仅 3 行文案、机制零改动）——1220 固定值「伤害 +18%、脉冲 +8%、抗性侵蚀 +4、物理占比 45%」、1223 每点「0.25s/0.5s/0.75s、脉冲 +6%/12%/18%」、1224 每点「+2/+4/+6」；同步 GDD `设计文档/职业设计草案_剑修.md:1260-1262`；验证 `validate_json` / `gen_skill_contracts.py --check --check-idempotency --check-determinism` / `sync_skill_node_icon_ids.py --check` / `gen_skill_mechanics_schema.py --check` 全 EXIT 0，`build.bat` EXIT 0 零警告、`-l ci` 1/1、`-l skill` 2/2，见 §9.4〕
- [x] **B1-08** [P1] 技能9：990 冰增幅接入 `DamagePipeline::CalculateBatch`（当前仅 Calculate 单目标生效；CalculateBatch 已 deprecated、生产批量走 ResolveDamageBatch→逐目标 Calculate，属潜在一致性缺口而非线上缺陷）— 源：skill9 §14.4-1
- [x] **B1-09** [P1] 技能9：消除 EffectSystem 与 `StatsSystem::UpdateBuffs` 对 `ActiveEffectsComponent::Update` 的双重调用（双倍衰减隐患，既存问题）— 源：skill9 §14.4-2
- [x] **B1-10** [P1] 技能9：跨技能非法标签清理——`assets/data/skills.json:714`（技能2 `sword_skill`）、`:1456`（技能3 `sword_skill`）、`:6349`（技能10 `sword_skill`）改 `SwordSkill`；`:3552`（技能6 `Duration`）调查后处置；技能10 部分随 A-04 — 源：skill9 §14.4-4、M2 §11.5
- [x] **B1-11** [P1] 技能2：FlowingThrust.cpp:378-384 残留 `b.id.find(...)` 字符串比较整改（按 RendingWave 枚举化先例，对应硬否决 §7.2）— 源：skill2 第3轮 §4
- [x] **B1-12** [P1] 技能5 N10（调查项）：gen 脚本泛化副作用影响面确认（技能1 契约新增 130/153/211/230/232、`mastery_skill_trees.json` 技能1 新增 1115）— 源：skill5 N10

### 2.2 测试护栏

- [x] **B1-13** [P1] 技能3：互斥「双点被拒」正向用例补强（SkillCastConstraintService 测试）— 源：skill3 §16.4
- [x] **B1-14** [P1] 技能4：keystone 角色精确集合断言（现断言不拒绝多出的 Keystone，参考 SkillContractRegistryTests.cpp:82-110 加 count 相等）+ DamagePipelineP1Tests 新增 Batch/Calculate 反击一致性用例 — 源：skill4 §16.5
- [x] **B1-15** [P2] 技能4：435 剑意回复断言（伴随 B1-03）— 源：skill4 §16.5〔销项 2026-09-13：随 B1-03 Wave C C2.4，见 §9.4〕
- [x] **B1-16** [P1] 技能7：775 Lightning×730 组合、752 次级撕裂中心测试覆盖；M4 projectile 链路测试（P2）— 源：skill7 rereview §5-5/§5-6
- [x] **B1-17** [P1] 技能8：`GetEffectiveSkillTags` 的 `role != Transmuter` 过滤补技能3~7 专项回归 — 源：skill8 §15.7
- [x] **B1-18** [P1] 技能2：`crit_chance` 未归一化疑点核实——**不成立**：`stats->crit_chance` 为归一化 0..1（Stats.hpp:115 默认 0.05、AttributePipeline.cpp:774 写回 /100），DamagePipeline.cpp:1324-1327 注释明示分数制、:1606/:1850 直接比较；SkillSystem 三处直拷（:1532/:1600/:2096）正确。原记录（skill2 §4、行号 1787）基于旧单位前提。遗留：技能1/2 审查文档中 crit 单位旧口径残留待文档清理 — 源：skill2 第3轮 §4
- [x] **B1-19** [P2] 技能2：技能3 373 / 技能6 633 的 `trigger_skill_id=2` 既有数据核实 — 源：skill2 第3轮 §4 记录项〔销项 2026-09-13：T6.4 核实真实链接为技能3 node 335（`skills.json:1999`）与技能6 node 635（`skills.json:4128`），373/633 为天赋树条目、无 trigger 字段；断言落在 `SkillContractRegistryTests.cpp`，数据零改动，见 §9〕
- [x] **B1-20** [P2] 模块化：复审 §9 建议项核销（more_damage_mult 单位断言、SpawnShadow 统一工厂、Rebake 幂等/拦截骰子分布契约测试），未闭环部分并入 B2 — 源：模块化实施复审 §9〔销项 2026-09-13：more_damage_mult/SpawnShadow/Rebake 幂等见模块化复审 §8 整改确认（`SkillSpecializationBakerTests.cpp:113-166` 幂等用例）；拦截骰子分布见 followup plan D1.1（`SkillSystemTests.cpp:1210-1262`，400 次统计带）。原「其余子项待 Wave E」注记作废，见 §9.4〕

### 2.3 验证任务

- [x] **B1-21** [P1] 技能5：531 护甲与 551 闪避实机观测一次（确认 BuffEffect.modifiers 生命周期）— 源：skill5 第三轮记录（阻塞登记：无交互运行环境；自动化部分证据见 §8） 〔销项 2026-09-17：已由既有自动化功能测试 `tests/functional/InfiniteBladesNodes.cpp`（技能5 功能套件）提供充分逻辑证明，正式销项；实机观测仍受“无交互运行环境”阻塞，不再由本清单跟踪。依据 `docs/designs/2026-09-17-skill-system-final-closure-design.md` §3.4-4 与 `docs/plans/2026-09-17-skill-system-final-closure-plan.md` §1.4。〕
- [x] **B1-22** [P1] 技能8：元素路径与 AreaFieldDeliverySystem 生命周期运行验证 — 源：skill8 §15.7（阻塞登记：无交互运行环境；自动化部分证据见 §8） 〔销项 2026-09-17：已由既有自动化集成测试 `tests/unit/AreaFieldDeliveryTests.cpp` 提供充分逻辑证明，正式销项；实机观测仍受“无交互运行环境”阻塞，不再由本清单跟踪。依据 `docs/designs/2026-09-17-skill-system-final-closure-design.md` §3.4-4 与 `docs/plans/2026-09-17-skill-system-final-closure-plan.md` §1.4。〕
- [x] **B1-23** [P1] 技能7：772 区域伤害改由异常承担后的数值验证（总伤害对齐设计）— 源：skill7 rereview §5-3（阻塞登记：无交互运行环境；自动化部分证据见 §8） 〔销项 2026-09-17：已由既有自动化功能测试 `tests/functional/MindBladeNodes.cpp`（772 感电区域不叠加全额伤害与 772×730 用例）提供充分逻辑证明，正式销项；实机观测仍受“无交互运行环境”阻塞，不再由本清单跟踪。依据 `docs/designs/2026-09-17-skill-system-final-closure-design.md` §3.4-4 与 `docs/plans/2026-09-17-skill-system-final-closure-plan.md` §1.4。〕
- [x] **B1-24** [P2] 技能9：VFX `MAX_PHANTOM_OVERLAYS` 叠加上限性能复核（建议走 performance 工作流）— 源：skill9 §14.4-3 〔转出 2026-09-17：移交性能专项 Track 持续跟踪，移出技能专精主线，转出至性能/渲染 Track（承接文件待渲染 Track 计划登记）。依据 `docs/designs/2026-09-17-skill-system-final-closure-design.md` §3.4-5 与 `docs/plans/2026-09-17-skill-system-final-closure-plan.md` §1.4。〕
- [x] **B1-25** [P2] 技能1：173 碎裂溅射 ×1.05 CombatV2 桩核销（CombatV2 已删除，确认桩是否残留）— 源：skill1 §6.5-1〔销项 2026-09-13：`rg -n "CombatV2" src/` = 0 行，桩随 `4e7d2ac2`「drop combat_v2」/0ef68d2d 清空，无残留；见 §9〕
- [x] **B1-26** [P2] 模块化：性能基线复跑（10k 施法、vs 旧 find() ≥2.5x、零堆分配、含洗点+换装 Rebake）— 源：模块化实施复审 §10 〔转出 2026-09-17：移交性能专项 Track 持续跟踪，移出技能专精主线，转出至性能/渲染 Track（承接文件待渲染 Track 计划登记）。依据 `docs/designs/2026-09-17-skill-system-final-closure-design.md` §3.4-5 与 `docs/plans/2026-09-17-skill-system-final-closure-plan.md` §1.4。〕

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
- [x] **B2-15** [P2] 技能3：351 数据消费统一、ArmorShred id 重命名（消除与 RendingWave.cpp:537 / FlowingThrust.cpp:412 的名称冲突互刷）— 源：skill3 §16.4〔销项 2026-09-13：ArmorShred 枚举化/迁移完成（`BuffId::ArmorShred` 位于 `src/game/foundation/data/BuffIds.hpp`，`rg '"ArmorShred"' src/` == 0；多层 stacks 断言 `RendingWaveNodes.cpp:615-656`）；**351 数据消费完成**——`melee_orbit_radius/speed/tick_interval/base_damage/hit_radius` 由 `BladeFormation.cpp:266,371,372` 与 `SummonAISystem.cpp:55-64` 经 `GetFloat(3,351,...)` 消费（5 键与硬编码等值、行为不变），`skill_mechanics_schema.json` 已重生成（472 entries）；`build.bat` EXIT=0 零警告、`-L ci` 1/1、1525/1525 用例全绿，见 §9.4〕
- [x] **B2-16** [P2] 技能7：714 粗粒度（skill_id==7）精确化（取 711 时其余路径已禁用，可选优化）— 源：skill7 rereview §5-1〔销项 2026-09-13：C5.1 评估后**拒绝**（收益不足、风险不划算且无玩家可见影响），证据 `SkillSpecializationBaker.cpp:1070-1079` 代码注释 + 计划评审剩余风险 5，见 §9.4〕
- [x] **B2-17** [P2] 技能8：N4-3 `primary_archetype` 死字段删除（RD-17 已裁决删除；影响面：Baker 14 处 + `BladeBoomerang.cpp:109` + BakerTests 3 处 CHECK；先核实存档/序列化兼容，归 A1-2）— 源：skill8 §15.4〔销项 0ef68d2d：A1-2 T2.2（RD-17）；rg `primary_archetype` in src/ = 0，见 §9〕
- [x] **B2-18** [P2] 技能1：Slow/Chill legacy buff 收编 ailment 契约（契约补注册时统一处理，与 HazardSystem 同型）— 源：skill1 §6.5-3〔销项 2026-09-13：方案 B 落地——`assets/data/ailment_contracts.json:+75-86` 补注册 `Slow`（`legacy_buff_type=SpeedDown`、`damage_tag=None`）+ `AilmentEngine.cpp:449-455 LoadBuiltins` 注册；新增单源构建口 `AilmentAdapter::BuildMoveSpeedDebuff`（`AilmentEngine.cpp:524-547`）供 `FlowingThrust.cpp:65-78` 与 `HazardSystem.cpp:379-383` 复用；D5 前置修正 `TagFromString("None")`（`TagRegistry.hpp:161-164`）、`DefaultDamageTag(Slow)=Tag::None`、`Tick()` 的 `None` 跳过守卫（`AilmentEngine.cpp:822-826`）；3 处护栏测试零 diff；新用例 `tests/unit/AilmentEngineTests.cpp:283-381`；`build.bat` EXIT=0 零警告、`-l ci/skill/unit/integration/combat/contract` 全绿，见 §9.4〕
- [x] **B2-19** [P2] 技能1：第4轮遗留统一处置（MEDIUM-5 四处死数据 `snapshot.payload_context`、UMR 并轨跳过项、ShadowDuplicationHook 特例）— 源：skill1 §6.5-4〔销项 0ef68d2d：A1-2 T2.3 + A1-5 T5.7，见 §9〕
- [x] **B2-20** [P2] 模块化：逐节点「烘焙→消费」映射表纳入仓库 — 源：模块化实施复审 §9〔销项 0ef68d2d：A1-6 T6.1（`docs/designs/2026-09-13-skill-baker-consumer-map.md`）+ T6.2 断言，见 §9〕
- [x] **B2-21** [P3] 技能6 L5：DeliveryArchetypesTests.cpp:190-210 以技能6 充当 Skyfall 残留域夹具，换独立 ID — 源：skill6 §16.3〔销项 0ef68d2d：A1-6 T6.3，见 §9〕
- [x] **B2-22** [P2] 技能9：技能12「绝影共噬」节点行为实现 + 技能9 联动回归（归入 A-03）— 源：skill9 §14.4-4〔销项 0ef68d2d：A2-2 T9.3，绝影共噬 1217 落地，见 §9〕
- [x] **B2-23** [P2]（**保留/降级，已裁决 2026-09-13**）伤害管线：deprecated 批量入口（`DamagePipeline::CalculateBatch` + `ResolveDamageBatch`）**保留现状，仅记录**，不迁移测试/基准调用点（`tests/unit/DamageElementIndexTests.cpp:109/:167`、`tests/unit/EventConsistencyTests.cpp`、`tests/performance/DamagePipelineBenchmark.cpp`）。依据：设计 §7/§11.4——保留至有等价替代测试入口；无生产改动 — 源：B1-08 遗留登记
- [x] **B2-24** [P2] 技能1/2：热路径字符串比较残留收编——`DamageConditions.cpp:19`（`id.find("Slow")`）、`BladeFormation.cpp:483`（`id.find("ignite")`），沿用 B1-11 的 BuffKind 枚举化思路 — 源：B1-11 复审 F4〔销项 0ef68d2d：A1-1 T1.6 + A1-4 T4.3；rg `id.find("Slow"/"ignite"/"shock")` in src/ = 0，见 §9〕

---

## 4. A 工作包（抽象化 + 技能10~12 迁移）

> 开工前先走设计流程，产出 `*-design.md` 与 `*-plan.md`；B2 全部条目作为第一批改动并入。

- [x] **A-01** 技能1~9 专精实现抽象化/模块化：重构重定 DoD 指标（2026-09-17 裁决调整：初始“单个文件 ≤100 行”指标曾误导致过早删除实现后又重新重构补上，现正式废除机械行数限制，改为以职责内聚、分层清晰、消除重复样板与跨系统 hack 为核心验收准则；全 12 技能行为层统一维持单源与 UMR 交付架构）。 〔销项 2026-09-17（SKILL-SYSTEM-FINAL-CLOSURE）：本项 DoD 重新定义随最终收尾包正式生效——机械「单个文件 ≤ 100 行」行数指标正式废除（2026-09-17 裁决，见本清单 §1.2 决策 9）；验收口径改为职责内聚 / 分层清晰 / 消除重复样板与无跨系统 hack，全 12 技能行为层统一维持单源与 UMR 交付架构（SpecState 单源支撑）。依据 `docs/designs/2026-09-17-skill-system-final-closure-design.md` §3.4-1 与 `docs/plans/2026-09-17-skill-system-final-closure-plan.md` §1.4。〕
- [x] **A-02** 技能10~12 迁移：SevenStarSlash / HeavenlySwordDescent / BloodSea（专精树数据 + 行为 + 契约 + 审查）；含 HeavenlySwordField / BloodSeaField 组件迁移（另立计划），依赖 HeavenlySwordDescent.cpp:184-400、BladeMasteryService.cpp:51-88、SkillSystem.cpp:1080-1092 — 销项 2026-09-14（Track A-02）：SpecState 抽象化 DoD 全达标（D-A5/D-A1 闭环、L-2 关闭、生成器门禁 3/3、ci/skill/integration/unit 标签全绿），证据见 `docs/reviews/2026-09-14-skill-abstraction-a02-review.md` 与 §9.5；HeavenlySwordField / BloodSeaField 组件迁移仍按「另立计划」独立跟踪
- [x] **A-03** 技能12 绝影共噬节点行为（与 A-02 合并）— 销项 2026-09-14：节点 1217 早于本 Track 经 B2-22（A2-2 T9.3，`0ef68d2d`）落地，本 Track 复核节点行为与技能9 联动回归全绿，随 A-02 一并销项，证据见 §9.5
- [x] **A-04** 技能10 非法标签清算尾项（B1-10 的剩余部分）——复核结论：数据侧已无非法标签，技能10 tags 全为已注册项，无需改动；技能10 契约已在 `assets/data/skill_contracts_compact.json` 注册。证据见 §8。
- [x] **A-05** B2 结构清理全集（见第 3 节）〔销项 2026-09-17：§3 全部 B2 条目（B2-01~B2-24）均已勾选销项并附证据，见 §3 与 §8/§9.4；依据 `docs/designs/2026-09-17-skill-system-final-closure-design.md` §3.4-2（“B2 全部 24 项此前已全部落地”）与 `docs/plans/2026-09-17-skill-system-final-closure-plan.md` §1.4。〕

---

## 5. O 类（环境 / 工具 / 美术 / 流程）

- [x] **O-01** [P2] 技能8 N4-5 环境稳定性 triage：ParticleTrailBenchmark 阈值失败（0.262<0.2）、MaterialVFXBenchmark 退出 0xC0000005、GPU Timer 抖动 — 源：skill8 §15.7 〔转出 2026-09-17：转出至性能/渲染 Track（承接文件待渲染 Track 计划登记），移出技能专精主线。依据 `docs/designs/2026-09-17-skill-system-final-closure-design.md` §3.4-5 与 `docs/plans/2026-09-17-skill-system-final-closure-plan.md` §1.4。〕
- [x] **O-02** [P2] 伤害管线 release-gate 计时噪声 triage（P4 报告 §5，非阻塞）— 源：伤害管线计划 §结项状态 〔转出 2026-09-17：转出至性能/渲染 Track（承接文件待渲染 Track 计划登记），移出技能专精主线。依据 `docs/designs/2026-09-17-skill-system-final-closure-design.md` §3.4-5 与 `docs/plans/2026-09-17-skill-system-final-closure-plan.md` §1.4。〕
- [x] **O-03** [P2] Rendering track：`nmd.tests.gpu.hardware` 移交项（dynamic_combat_emissive High tier GI delta 0.000589/0.000592 < 0.001；OccluderExtractPass High tier p95 0.392ms > 0.3ms）— 源：skill2 第2轮 §6 〔转出 2026-09-17：转出至性能/渲染 Track（承接文件待渲染 Track 计划登记），移出技能专精主线。依据 `docs/designs/2026-09-17-skill-system-final-closure-design.md` §3.4-5 与 `docs/plans/2026-09-17-skill-system-final-closure-plan.md` §1.4。〕
- [x] **O-04** [P3] 技能6 L4：codebase-memory 图索引重建（`ResolveSwordArrayHeavenlyAttunementConversion` 指向已不存在的 SwordArray.cpp:52-66）— 源：skill6 §16.3 〔转出 2026-09-17：转出至性能/渲染 Track（承接文件待渲染 Track 计划登记），移出技能专精主线。依据 `docs/designs/2026-09-17-skill-system-final-closure-design.md` §3.4-5 与 `docs/plans/2026-09-17-skill-system-final-closure-plan.md` §1.4。〕
- [x] **O-05** [P2] 技能9：图标占位与 skill_node_prompts 美术替换 — 源：skill9 §14.4-3 〔转出 2026-09-17：转出至性能/渲染 Track（承接文件待渲染 Track 计划登记），移出技能专精主线。依据 `docs/designs/2026-09-17-skill-system-final-closure-design.md` §3.4-5 与 `docs/plans/2026-09-17-skill-system-final-closure-plan.md` §1.4。〕
- [x] **O-06** [P3] 技能4 R3-10：AGENTS.md +3 行归属确认（待作者）— 源：skill4 §16.7 〔转出 2026-09-17：转出至性能/渲染 Track（承接文件待渲染 Track 计划登记），移出技能专精主线。依据 `docs/designs/2026-09-17-skill-system-final-closure-design.md` §3.4-5 与 `docs/plans/2026-09-17-skill-system-final-closure-plan.md` §1.4。〕
- [x] **O-07** [P3] 技能1/2：`nmd.tests.performance` 与 `nmd.tests.gpu.hardware` 环境性失败登记（机器波动/渲染 GI 基线）— 源：skill1 §6.5-5、skill2 §6 〔转出 2026-09-17：转出至性能/渲染 Track（承接文件待渲染 Track 计划登记），移出技能专精主线。依据 `docs/designs/2026-09-17-skill-system-final-closure-design.md` §3.4-5 与 `docs/plans/2026-09-17-skill-system-final-closure-plan.md` §1.4。〕

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

B1-21/22/23（运行时采证阻塞）、B1-24/26（性能），以及 A 工作包、O 类条目维持未勾选。

### 9.4 Wave C / 裁决补充销项（2026-09-13）

来源：`docs/plans/2026-09-13-skill-followup-plan.md` §5 Wave C（C1/C2/C3/C4/C5）与 `docs/plans/2026-09-13-skill-rd-rulings-plan.md`；复审 `docs/reviews/2026-09-13-skill-followup-review.md` 结论 **提交**（§8.3 证据：`build.bat` EXIT=0 零警告、`ctest -L ci` 连续 5 轮全绿 1512 用例/113293 断言、数据脚本三连 PASS）。

| ID | 实现来源 | 证据 |
|---|---|---|
| B1-01 | Wave C C1.1-C1.4 | 372/331/375 `BladeFormation.cpp:373/540/691-699/746`；355 `SummonAISystem.cpp:164-168` 改读 `array_haste_pct`（原 `:172` 硬编码 1.50f） |
| B1-02 | Wave C C2.1-C2.3 | 476 元素曝光 `BladeWard.cpp`；472/474 与 402~475 十二节点消费 `skill_mechanics.json:275-382` |
| B1-03 | Wave C C2.4 + rd-rulings plan §4 B3 / §10 Wave D | 435 `crit_bonus`（`skill_mechanics.json:330`）消费于 `SkillSystem.cpp:1250-1270`；455 改「下一次攻击」一次性 More+20%（`DamagePipeline.cpp` 按 `attack_key`/`source_cast_id` 聚合消费） |
| B1-04 | Wave C C2.5 + 470 键 | `DamageInterceptors.hpp:55` 读 `GetMech(4,470,"base_damage",35.0f)`；`counter_swords`（`:356`）Melee 反击 5 道剑气（复用 `ResolveSkill4Counter`） |
| B1-05 | Wave C C3.1-C3.4 | 535 `BeamChannelDeliverySystem.cpp:953-967`；552 `:1114-1116`；532/533 外置；534/512 按 D8 |
| B1-06 | RD-12 裁决 | 基底=扇形剑气洪流；GDD L340 已修订；无 Skyfall 值（`DeliveryArchetypes.hpp:153-156`），代码零改动 |
| B1-07 | Wave C C4.1-C4.2 | `Buff.hpp:319-331` `RemoveByKind(kind, source_skill_id)` 重载；`SkillSystem.cpp:1982-1988` 传 8u；FreeCast 异 kind/异 source/通配三路径测试 |
| B1-15 | Wave C C2.4 | 随 B1-03 435 断言 |
| B1-20 | 模块化复审 §8 + Wave D D1.1 | more_damage_mult 消费 / SpawnShadow 工厂 / Rebake 幂等（`SkillSpecializationBakerTests.cpp:113-166`）；骰子分布统计断言 `SkillSystemTests.cpp:1210-1262` |
| B2-16 | C5.1 评估 | 拒绝（收益不足）；`SkillSpecializationBaker.cpp:1070-1079` 注释 + 计划评审剩余风险 5 |
| B1-27 | `31cc67e6` 已实现的消费侧校验 + 本次隔离测试 | `src/game/foundation/data/SkillMechanicsRegistry.cpp:314-327`；`tests/unit/SkillMechanicsKeySchemaTests.cpp:48,139` |
| B1-28 | 用户裁定 D1-D5 + 子代理落地 | GDD `设计文档/职业设计草案_剑修.md` §5.3.2 全 25 节点对齐数据 `name_key`/`prerequisites`；提案 `docs/designs/2026-09-13-skill12-branchD-gdd-data-alignment-design.md`；自检旧名 `rg` 0 命中、25/25 对拍一致 |
| B2-15 | 351 机制键消费统一 | `BladeFormation.cpp:266,371,372`、`SummonAISystem.cpp:55-64` 改读 `GetFloat(3,351,...)`；测试 `tests/functional/BladeFormationNodes.cpp:185-233`；`skill_mechanics_schema.json` 重生成；`build.bat` EXIT=0 零警告、`-l ci` 1/1、1525/1525 |
| B1-29 | 技能12 三节点 desc 文案对齐机制 | `assets/data/mastery_skill_trees.json:2238,2312,2342`（仅 `desc_key`）；GDD `设计文档/职业设计草案_剑修.md:1260-1262`；验证四脚本 EXIT 0、`build.bat` 零警告、1525/1525 |
| B2-18 | 方案 B 落地（补注册 Slow + 单源构建口） | `assets/data/ailment_contracts.json:75-86`；`AilmentEngine.cpp:449-455,524-547,822-826`；`TagRegistry.hpp:161-164`；`FlowingThrust.cpp:65-78`；`HazardSystem.cpp:379-383`；测试 `tests/unit/AilmentEngineTests.cpp:283-381`；`build.bat` 零警告、6 标签全绿

### 9.5 Track A-02 销项补充（2026-09-14）

来源：`docs/plans/2026-09-14-skill-abstraction-track-a02-plan.md`、`docs/designs/2026-09-14-skill-abstraction-track-a02-design.md`；复审 `docs/reviews/2026-09-14-skill-abstraction-a02-review.md` 结论 **提交**。

- **A-02 技能10~12 SpecState 抽象化**：技能10/11/12 的手写 `*PointBinding` / `*FlagBinding` / `*MechBinding` 表全部退役，统一由生成头 + `SpecStateTable` 驱动。D-A5 闭环（`rg "struct (SevenStarSlash|HeavenlySword|BloodSea)(Point|Flag|Mech)?Binding" src/` = 0），D-A1（12 技能 `*SpecState` 均为 int 点数 + bool 标志 POD），L-2 关闭（`SpecStateTableTests.cpp` 新增 `CheckTableMatchesRuntime` 逐绑定对拍运行期 `ReadPoints` / `HasNode`）。技能10 转质 1021/1022 经 `assets/data/skill_specstate/skill_10.json` flag 绑定 + 包装层 `SkillSystem::GetActiveTransmuterNode` 覆盖保留语义（生成头 `generated/SevenStarSlashSpecState.gen.hpp` 重生成，17 点 + 8 flag）。60 个 `GetMech` 机制系数与 10 个兜底字面量经独立复审与 HEAD 逐字节一致。
  证据：`build.bat RelWithDebInfo` EXIT 0、零新增告警；`ctest -L ci` 1/1、`-L skill` 2/2、`-L integration` 6/6、`-L unit` 8/8（全量 17/19，2 例 GPU/性能抖动见 O-01/O-07，与本 Track 代码路径无关）；生成器 `gen_skill_contracts.py --gen-specstate --check --check-idempotency --check-determinism`、`gen_skill_mechanics_schema.py --check`、`sync_skill_node_icon_ids.py --check` 三连 EXIT 0。详见复审报告「验证证据」。
- **A-03 技能12 绝影共噬**：节点 1217 行为早于本 Track 经 B2-22（A2-2 T9.3，`0ef68d2d`）落地；本 Track 复核节点行为与技能9 联动回归全绿，随 A-02 一并销项。
- 范围声明：A-02 原条目括注的 HeavenlySwordField / BloodSeaField 组件迁移仍按「另立计划」独立跟踪，不在本次销项范围。
- 遗留跟进（不阻塞）：`tests/functional/HeavenlySwordDescentNodes.cpp`、`BloodSeaNodes.cpp` 的 10 个 `DoCast` 兜底 `constexpr` 字面量暂无直接断言（复审 Low-2）；本地 `clang-format` 因仓库 `.clang-format:78` `Standard: Cpp20` 与工具版本不兼容而不可用（复审 R3）。 |

---

## 10. 链接/索引校验（2026-09-17 收尾归档）

本轮收尾编辑仅新增文件引用路径；另以 `rg` + `Test-Path` 对全文既有引用做了一次全量抽检（56 个唯一仓库相对路径）。结论如下：

- **本轮新增/引用的仓库路径均已确认存在**：`src/game/contracts/impl/StatsSystem.cpp`、`src/game/systems/skill/SkillSystem.hpp`、`src/game/systems/skill/SkillSystem.cpp`、`tests/functional/InfiniteBladesNodes.cpp`、`tests/unit/AreaFieldDeliveryTests.cpp`、`tests/functional/MindBladeNodes.cpp`、`tests/unit/SkillProfileResolveSentinelTests.cpp`、`tests/unit/SkillSpecializationBakerTests.cpp`、`tests/unit/SkillWrapupHardeningTests.cpp`、`tests/CMakeLists.txt`、`assets/data/mastery_skill_trees.json`、`设计文档/职业设计草案_剑修.md`、`docs/plans/2026-09-17-skill-system-final-closure-plan.md`、`docs/designs/2026-09-17-skill-system-final-closure-design.md`。
- **本轮交付的新增测试文件（已落地、尚未纳入版本控制）**：`tests/unit/SkillSpecializationStatModifierTests.cpp` 已随本轮交付存在于工作区（`git status --short` 显示 `?? tests/unit/SkillSpecializationStatModifierTests.cpp`），经 `tests/CMakeLists.txt` 的 `file(GLOB_RECURSE … CONFIGURE_DEPENDS "*.cpp")` 重新配置后编入套件，定向复跑 `bin\NoMoreDayTests.exe --test-case="*SkillSpecializationStatModifier*"` 为 **11/11 用例通过**；截至本次归档该文件尚未 `git add`，仍不属版本控制追踪内容（不得据本条表述为“尚未检出/未实现”）。
- **既有引用未发现坏链**：全文 56 个唯一路径引用均可在仓库中解析；原文 `docs/reviews/...-review-round5.md` 为有意省略占位写法、`设计文档/审查报告` 为行文表述而非路径，二者均非失效链接。
- **索引一致性**：本清单未登记于 `docs/` 或 `conductor/` 的任何索引/目录文件，无需同步索引项；本次仅在文末新增 §10，既有 §1~§9 的编号与章节位置均未改动。
