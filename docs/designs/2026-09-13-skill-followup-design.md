# 技能专精收尾与设计修订 — 设计文档

- 日期：2026-09-13
- 性质：修订上游基线 `设计文档/职业设计草案_剑修.md`（直接改原文），并固化技能1~12 收尾的设计裁决
- 关联：`docs/plans/2026-09-12-skill1-9-followup-backlog.md`（活清单）、`docs/plans/2026-09-13-skill-abstraction-and-b2-cleanup-plan.md`（上一批，已提交 0ef68d2d）
- 下游：`docs/plans/2026-09-13-skill-followup-plan.md`

## 1. 背景

上一批（技能抽象化 + B2 清理，commit `0ef68d2d`）已闭合大部分结构清理项。对账后（见计划文档状态表）剩余工作分三类：

1. **设计裁决**：backlog §1 的 RD 清单共 12 项由用户于 2026-09-13 拍板，其中 4 项要求**改设计**（D4/D6/D9/RD-13）。
2. **实现缺口**：技能3/4/5/8 有数据键已就绪但无消费、或行为缺失（B1-01/02/03/04/05/06/07）。
3. **结构与测试债务**：B2-15/B2-18 残留、生成器 Keystone 推导缺陷、技能10 系数双源、测试隔离、文档口径。

## 2. 目标与非目标

**目标**

- 将 12 项用户裁决固化为行为/数据/文本；4 项改设计同步写入 GDD 原文并保持全链一致。
- 闭合全部非性能 B1/B2 项与本次新发现的债务（技能10 双源、机制表键名校验、测试隔离、973 豁免复核、文档口径）。
- A-01 抽象化目标明确为**技能1~12**（见计划 Wave E）。

**非目标**

- 性能类：Tracy 基线补采、B1-24、B1-26、O-01/02/03/07、伤害管线计时噪声。
- RD-18 UMR 并轨（用户裁决延后，不启动）。
- B2-23 deprecated `CalculateBatch`/`ResolveDamageBatch` 迁移（裁决保留）。

## 3. 用户裁决记录（2026-09-13）

| 编号 | 事项 | 裁决 |
|---|---|---|
| D1 | RD-01 Keystone 上限/互斥 | 撤销 Keystone 互斥，仅保留 Transmuter 互斥（GDD 仅规定 Transmuter 互斥） |
| D2 | RD-02 981+982 满损血暴伤 | 暂不加封顶，保持线性 |
| D3 | RD-03 形态中重施法重置免死/附魔 | 禁止重置 |
| D4 | RD-07 技能3 灵剑决基底 Toggle | **改设计**：原 Toggle 设计超模，不按原设计实现 |
| D5 | RD-06 技能4 455/435/452 | 按设计实现（455 一次性 + 剑意、435 crit_bonus、452 专精版结算） |
| D6 | RD-08 153 饮血刃 | **改设计**：保留全来源流血 tick 回血，下调回血数值 |
| D7 | RD-10 100% 概率保护范围 | 暂不扩展，维持 dodge/block |
| D8 | RD-11 技能5 N12 | 按设计收窄（535/552/534/512） |
| D9 | RD-12 技能5 N13 天降形态 | **改设计**：不做 Barrage/Skyfall 双形态，收敛单一形态 |
| D10 | RD-14 子投射物 ignore_resist | 不继承 |
| D11 | §11.9 禁疗×增疗 | 禁疗优先，1217 增疗在禁疗窗内不结算 |
| D12 | A-01 抽象化 DoD | 不追求 ≤100 行/文件，追求尽力而为的设计目标 |
| 补充 | RD-18 UMR 并轨 | 延后 |
| 补充 | RD-13 技能2 251 | 改设计（补 GDD 出处、修正角色/绑定语义） |
| 补充 | GDD 修订落点 | 直接改 `设计文档/职业设计草案_剑修.md` 原文 |
| 补充 | A-01 目标范围 | 技能 1~12（非仅 1~9） |
| 补充 | 运行时验证 | 优先现行测试环境，不行则 headless 采证 |

## 4. 设计修订

### 4.1 skill3 灵剑决基底形态（D4）

- **问题**：GDD §3.3 L228/L229 定义为 `[Toggle]`「每秒消耗 5 法力维持、最大 3 灵剑」——折算为近乎永久的灵剑输出，强度超模；实现为施放型（`skills.json:1449` mana_cost=25、`:1450` cooldown=5.0），灵剑 `lifetime=-1` 亦为永久。两者都缺少到期约束。
- **修订**：取消 Toggle 维持语义，明确为**限时召唤的施放型**：
  - 标签由 `[Toggle]` 改为 `[Spell][Minion-like][Physical]`（移除 Toggle）。
  - 施放消耗 25 法力、冷却 5s，召唤灵剑上限 3 柄，**持续 8s 后自行消散**，期间可重施刷新（重施不叠加数量）。
  - 不再有「每秒 5 法力维持/法力不足消散」规则。
- **落点**：GDD L228（标签）、L229（基底描述）；数据 `skills.json:1449/1450`（保留）、新增灵剑持续时长（结构体 NSDMI 或 mechanics 键 `duration`，实施期定）；让 `BladeFormation` 不再以 `lifetime=-1` 驻留。
- **不变量**：370/372 互斥（`max_transmuters=1`）、370/372 元素转换绑定、技能3 专精节点语义不受影响。

### 4.2 skill1 节点 153 饮血刃（D6）

- **问题**：GDD L156 定义「击中流血敌人会治疗自身（相当于造成的流血伤害的 100%）」；实现 `AilmentEngine.cpp:839-846` 读 `GetFloat(1,153,"lifesteal_ratio",1.0f)` 按**该次全部来源**的实际 DoT 伤害吸血（全来源行为保留）。
- **修订**：保留「全来源流血 tick 回血」语义，**回血比例由 100% 下调为 30%**（`skill_mechanics.json:14-16` `lifesteal_ratio` 1.0 → 0.30），GDD L156 文案同步改为「30%」。
- **理由**：全来源叠加曾导致异常流派的治疗溢出；降数不改机制，避免跨来源限制带来的实现与测试复杂度。
- **影响**：仅 `AilmentEngine` 数值入口；同技能 170~175 异常体系共享 tick 但不改数值。

### 4.3 skill5 万剑归宗基底交付形态（D9）

- **问题**：GDD §3.5 L340 要求「向屏幕内随机位置轰击天降剑气」，实现为对目标方向 ±20° 扇形直射（`BeamChannelDeliverySystem.cpp:1192`），未区分 Barrage/Skyfall。
- **修订**：**取消基底的天降随机落点语义，统一为扇形剑气洪流（Barrage）**；天降/光柱轰击语义保留给 mastery 技能11「天剑降临」与节点 534「天剑降世」（招一柄通天巨剑轰击光标），不在 skill5 基底重复。
  - GDD L340 改为：引导时持续向目标方向成扇形发射剑气，每 0.3s 发射 3 枚飞剑，每枚造成 40% 物理伤害，引导期间每秒消耗 20 法力。
- **落点**：GDD L340；`InfiniteBlades.cpp:127` `BarrageEmitter` 保留为唯一基底模式。注：`BeamChannelMode` 枚举仅有 `ContinuousLaser`/`BarrageEmitter` 两值（`DeliveryArchetypes.hpp:153-156`），不存在 `Skyfall`；「天降轰击」语义由独立的 `SkyfallImpactDelivery` 原型承载（`DeliveryArchetypes.hpp:133`、`AreaFieldDeliverySystem.cpp:29-32`），归技能11/节点534 使用，本批无需处置。
- **影响**：节点 510/511/533/534/570-575 的落点/频率语义；534 巨剑术仍按现有实现。

### 4.4 skill2 节点 251 剑意爆发 / HeavyMomentum（RD-13）

- **问题**：GDD L210 定义 251「消耗全部剑意，每层 +8% 范围、+3% 暴击」，**全文无「重势」字样，也无「满层重置流云刺 CD」出处**；实现 `RendingWave.cpp:104-126` 在消耗满 10 层时重置技能1 流云刺 CD，并带待裁决注释（`:121`）。另：生成器对**无显式 `keystone_node_ids` 配置**的技能（skill1/3/10）将所有单点节点默认推导为 Keystone，属推导缺陷（251 所在的 skill2 有显式列表，不受影响，251 现为 Passive）。
- **修订**：
  1. **保留**满层（10 层）重置流云刺 CD 的联动，并将其**写入 GDD L210** 作为 251 的显式效果（补出处，消除 TODO）。
  2. 251 契约角色**保持 Passive**（`skills.json:1329-1344` 现状即正确），其剑意绑定语义由 `affects_sword_intent=true` 承载（已就绪）；`cost_affix=HeavyMomentum` 绑定保留。生成器兜底推导缺陷由计划 Wave A 单独修正（仅影响 skill1/3/10，不改 251）。
  3. `assets/data/modifier_v2/skill_spec_modifiers.json:26-37` `HeavyMomentum_Node213` 语义在 GDD/映射文档登记：node 213 提供 PhysicalDamage +22% percent-mult（id 2002103）。
- **落点**：GDD L210；`skill_contracts_compact.json` skill1/3/10 补显式 `keystone_node_ids` 后重生成；`gen_skill_contracts.py` Keystone 兜底推导删除（见计划 Wave A）。

### 4.5 ProcEngine 终结帧口径（§11.8）

- 裁决：`IsDeathSealActive` 的窗口判定**排除 `ending` 终结帧**（终结帧不再触发 DeathSeal 规则）。现状 `ProcEngine.cpp:59-65` 已按此实现，本设计确认其为预期时序，不改行为。

### 4.6 数值与口径决定（主代理）

| 项 | 结论 |
|---|---|
| RD-04 技能7 数值近似（772 区域伤害转异常、751/752/754） | 维持近似口径并在机制表注释登记；数值对齐验证随运行时采证（计划 Wave D） |
| RD-05 技能8 855 effectiveness | 维持 0.5 |
| RD-09 免死判定 vs BladeFormation 无敌顺序 | 维持现状（免死结算在前）并登记为设计口径 |
| RD-15 技能8 815 治疗 / 811 层数近似 | 维持现有近似（Bleed 上限 2 层、Additive 合并），登记为保真度上限 |
| §11.9 禁疗×增疗 | 按 D11：禁疗优先 |

### 4.7 复审裁决（2026-09-13 审查拍板）

第 1 轮审查（`docs/reviews/2026-09-13-skill-followup-review.md`）发现 C1.1 实现与设计伪代码存在偏差，用户拍板：

| 项 | 裁决 | 落点 |
|---|---|---|
| 372 静电场伤害 | **零伤害**：场纯感电触发，伤害全部由落雷本体承担；删除 `field.damage` 附加路径，不做数值化 | `BladeFormation.cpp`（删 `field.damage` 赋值）、`BeamChannelDeliverySystem.cpp:1324-1331`（tick 端按零伤害修订注释与施加路径） |
| 372 触发时序 | **按 GDD 改 DoHit**：瞬移后命中才释放静电场（现 DoCast 施放即落场为偏差）；顺带补瞬移距离/落点约束 | `BladeFormation.cpp:373-426`；GDD L276 语义不动 |
| 375 引爆异常 | **消费异常**：引爆时移除目标身上的对应元素异常栈，对齐 GDD「引爆该元素异常」字面语义 | `BladeFormation.cpp`（引爆结算段）；`Skill3FollowupTests` 补断言 |

## 5. 非设计性的实现缺口

以下为纯实现/数据消费缺口，不改变设计，细则见计划文档：B1-01（skill3 372/375/331/355）、B1-02（skill4 ~13 节点 + 476 曝光）、B1-03（435 crit_bonus）、B1-04（skill4 `counter_swords` 5 道反击剑气实体）、B1-05（skill5 N12 收窄）、B1-06（并入 4.3）、B1-07（skill8 `RemoveByKind` 过滤重载）、B2-15（ArmorShred 枚举化）、B2-18（FlowingThrust Apply 路径收编）、B2-24 残留、技能10 系数单源、机制表键名校验、测试隔离、973 豁免复核、文档口径。

## 6. 影响面与回退

- **数据**：`skills.json`（skill3 持续时长）、`skill_mechanics.json`（153 lifesteal_ratio、skill3 duration 等）、`skill_contracts_compact.json`（skill1/3/10 补显式 `keystone_node_ids` 后重生成）。所有改动须过 `gen_skill_contracts.py --check --check-idempotency --check-determinism` 与 `sync_skill_node_icon_ids.py --check`。
- **存档/序列化**：契约重生成影响契约加载校验，须复核 `SkillContractRegistryTests`；无存档字段增删。
- **生成器**：`gen_skill_contracts.py` 删除「无显式 Keystone 列表时单点节点默认推导为 Keystone」的兜底分支（`gen_skill_contracts.py:454-455`），需要 Keystone 的技能一律走显式 `keystone_node_ids` 配置；重生成 skill1/3/10 契约。
- **测试**：技能3/4/5/8 功能与契约测试；新增 skill3/skill5/skill4 节点断言；测试隔离修复。
- **性能**：无热路径结构变化；不触碰分配/线程约束。
- **回退**：按 wave 独立可回退；GDD 改动为文本，可 diff 还原。

## 7. 可观察验收标准

- GDD 四处修订文本与实现/数据/exports 一致（含标签、数值、模式名）。
- 技能3 灵剑限时消散、技能1 153 = 30%、技能5 基底单形态、技能2 251 CD 联动有断言覆盖。
- 全部非性能 B1/B2 项勾选并附 `文件:行`/测试名证据。
- 出口：`build.bat`（RelWithDebInfo，0 警告）→ `ctest -L ci` 全绿 → 数据校验脚本 PASS。

## 8. 未决与风险

- 运行时采证（B1-21/22/23、772、§11.9）优先现行测试环境，失败转 headless；若均不可行则登记为未验证风险。
- 技能3 灵剑「持续 8s」为本次新定数值，需在实施期确认与专精节点（如 Count/刷新）无冲突。
- `BeamChannelMode` 无 `Skyfall` 枚举值（仅 `ContinuousLaser`/`BarrageEmitter`）；天降语义由 `SkyfallImpactDelivery` 原型承载，本批不触碰。
- A-01（技能1~12 抽象化）为大工作包，需独立 Track 设计，不与本批混排。

## 9. 设计口径登记（RD-04/05/09/15、§11.9）

本节把 §4.6 的「维持现状」数值/裁决固化为可追溯登记（计划 `docs/plans/2026-09-13-skill-followup-plan.md` §4 B2.6）。均为设计口径确认，**无行为变更**；机制表 `assets/data/skill_mechanics.json` 本批不触碰，故登记集中于此文档，并与 backlog `docs/plans/2026-09-12-skill1-9-followup-backlog.md` §1 RD 表交叉引用。

| 编号 | 裁决（§4.6） | 现状实现落点 | 备注 |
|---|---|---|---|
| RD-04 | 技能7 数值近似维持：772 区域伤害改由异常承担、751/752/754 近似 | `src/game/systems/skill/BeamChannelDeliverySystem.cpp:177-200`（751/752/754/772 点亮与消费）、`:399`（754 剑意化无）、`:705`（772 神雷天罡）；`SkillSpecializationBaker.cpp:779-796` | 维持近似并在机制表注释登记；数值对齐验证随运行时采证（计划 D1.5） |
| RD-05 | 技能8 855 effectiveness 维持 `0.5` | `SkillSpecializationBaker.cpp:1076`（skill8 node 855）；`src/game/systems/skill/behaviors/BladeBoomerang.cpp:57`（`ColossusEcho = 855`）；`ProcEngine.cpp:43`（施法前置） | 保真度上限，维持 0.5 |
| RD-09 | 免死判定 vs BladeFormation 无敌顺序维持现状（免死结算在前） | `src/game/systems/skill/behaviors/PhantomTrance.cpp:204`（`ApplyDeathSeal`）与 `:657/:692/:749` 免死结算段；BladeFormation 无敌判定顺序不变 | 「免死结算在前」为设计口径，非缺陷 |
| RD-15 | 技能8 815 治疗 / 811 层数维持近似（Bleed 上限 2 层、Additive 合并） | `src/game/systems/skill/BoomerangDeliverySystem.cpp:241`（815 风眼流血转治疗）；`src/game/systems/skill/behaviors/BladeBoomerang.cpp:319`（811 放血）；`SkillSpecializationBaker.cpp:828/838` | 登记为保真度上限 |
| §11.9（D11） | 禁疗优先：1217 增疗在禁疗窗内不结算 | `src/game/systems/skill/behaviors/BloodSea.cpp:191-192`（逆脉锁血禁疗期间禁止一切治疗）、`:277/:351-353/:450`（1217 增疗受 `ApplyHealing` 禁疗优先拦截） | 采用 D11「禁疗优先」 |

- 关联：backlog §1（RD 表）、计划 §4 B2.6；本节替代「机制表注释登记」路径（机制表属数据文件，本批不动）。
