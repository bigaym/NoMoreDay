# UMR 技能专精改造第四批（技能 10 七星斩、技能 11 天剑降临、技能 12 血海）设计说明

- **文档状态**：待评审（v1.2：二轮代码实况复核闭环）
- **文档路径**：`docs/designs/2026-09-17-umr-skill-batch4-seven-stars-heavenly-sword-blood-sea-design.md`
- **设计日期**：2026-09-17
- **系统代号**：`UMR-SKILL-BATCH-4` (Unified Modifier Runtime - Skill Batch 4)
- **修订记录**：
  - v1.0：初稿（9 条 Canonical 记录 + 0 新增算子论证 + 8 退役键与 6 处 `mastery_skill_trees.json` 属性清理 + Baker 步骤 1 显式基准 + 行为层单源消费 Profile 重构）。
  - v1.1：首轮审查缺陷闭环（修复 2011190 节点 ID 笔误 1109->1119、清空 1207 damage_modifiers 并防范双重计算、改用 ResolveBakedProfile 标准基元、校准技能 11 sub_interval 为 0.5f、补齐 1107/1207 门禁死键防回潮）。
  - v1.2：二轮代码实况复核闭环（修正 `BakedDeliveryParams` 默认值表述、删除无消费点的 `del.range`/`del.sub_interval` 死写、补 `ResolveBakedProfile` 空指针守卫、删除不存在的 `HeavenlySwordFieldComponent::bonus_damage_mult` 与技能 10/11 无效 More 乘区、更正 `stat_modifiers` 的 StatType 语义、登记节点 1015 残留与 1207 描述文本偏差、钉死血海 More 乘算插入位置与 1202 加算项的顺序偏差、补充既有断言的同步改造任务）。
- **输入来源**：
  - `设计文档/统一修饰器运行时系统_UMR.md`
  - `设计文档/职业设计草案_剑修.md`（§3.10 七星斩、§3.11 天剑降临、§3.12 血海）
  - `设计文档/职业被动和技能设置.md`（§5.2 进阶专精招牌技能定义与分流规则）
  - `docs/designs/2026-09-13-skill-baker-consumer-map.md`（B2-20 烘焙→消费单源映射表，特别是技能 10~12 既有脱幅历史边界）
  - `docs/designs/2026-09-16-umr-skill-batch1-rending-wave-blade-formation-design.md`（Batch 1 批次基线）
  - `docs/designs/2026-09-17-umr-skill-batch2-blade-ward-infinite-blades-sword-array-design.md`（Batch 2 批次基线）
  - `docs/designs/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-design.md`（Batch 3 批次基线）
  - 代码实况：
    - [`src/game/systems/skill/SkillSpecializationBaker.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSpecializationBaker.cpp)
    - [`src/game/foundation/components/SkillDefs.hpp`](file:///d:/PRJ/NoMoreDay/src/game/foundation/components/SkillDefs.hpp)
    - [`src/game/systems/skill/behaviors/SevenStarSlash.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/SevenStarSlash.cpp)
    - [`src/game/systems/skill/behaviors/SevenStarSlashShared.hpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/SevenStarSlashShared.hpp)
    - [`src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp)
    - [`src/game/systems/skill/behaviors/HeavenlySwordDescent.hpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/HeavenlySwordDescent.hpp)
    - [`src/game/systems/skill/behaviors/BloodSea.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/BloodSea.cpp)
    - [`src/game/systems/skill/behaviors/BloodSea.hpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/BloodSea.hpp)
    - [`assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json`](file:///d:/PRJ/NoMoreDay/assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json)
    - [`assets/data/mastery_skill_trees.json`](file:///d:/PRJ/NoMoreDay/assets/data/mastery_skill_trees.json)
    - [`assets/data/skill_mechanics.json`](file:///d:/PRJ/NoMoreDay/assets/data/skill_mechanics.json)
    - [`scripts/validate_skill_spec_modifiers.py`](file:///d:/PRJ/NoMoreDay/scripts/validate_skill_spec_modifiers.py)

---

## 1. 背景与目标

### 1.1 背景与现状
在顺利完成 Batch 1（技能 2 裂空斩、技能 3 灵剑决）、Batch 2（技能 4 剑气护体、技能 5 万剑归宗、技能 6 剑阵·诛仙）与 Batch 3（技能 7 心剑·无影、技能 8 御剑·回旋、技能 9 绝影绝剑）的 UMR 交付算子并轨后，基础算子体系（OpCodes 30..42）已稳定覆盖基础 9 个常规技能。

然而，技能 10（七星斩 SevenStarSlash，剑圣招牌）、技能 11（天剑降临 HeavenlySwordDescent，天剑招牌）与技能 12（血海 BloodSea，魔剑招牌）作为进阶专精招牌技能，由于历史演进原因，存在更加显著的架构脱节与多源并存风险：

1. **Baker 阶段完全脱幅与基准缺失（脱幅状态）**：
   - 根据 [`docs/designs/2026-09-13-skill-baker-consumer-map.md`](file:///d:/PRJ/NoMoreDay/docs/designs/2026-09-13-skill-baker-consumer-map.md) §1:16 登记，`SkillSpecializationBaker.cpp` 中的两个核心 switch（`Bake()` 步骤 1 交付基准与步骤 2 节点修正）历史上仅涵盖 `case 1..9`，技能 10~12 完全命中 `default:` 空分支；
   - 导致 `SkillSystem::RebakeSkillProfiles` 对槽位中装配的 10~12 号技能烘焙出的 `BakedSkillProfile` 处于“半空心化”状态：
     - `out_profile.area_radius` 仅回退到 `skillData->GetParam("area_radius", 1.0f)`，而 `skills.json` 中技能 10/11/12 实际配置的键是 `radius` / `field_radius`，故恒回退为 `1.0f`；
     - `BakedDeliveryParams` 则完全保持结构体默认值 `del.duration = 1.0f`、`del.range = 200.0f`、`del.sub_interval = 0.0f`（`src/game/foundation/components/SkillDefs.hpp:629-633`），与三个技能的真实基准（无敌帧 0.5s / 领域 5.0s / 血海 4.8s，96 / 90 / 120）严重不符；
   - 无法享受装备词缀在 Baker 步骤 5 的自动折叠与全局统一结算。

2. **行为层跳过 Baker、直接读取 `GetMech` 与手写计算（双重事实源）**：
   - 技能 10（七星斩）：`SevenStarSlash.cpp` 在 `DoCast` 中绕过 `BakedSkillProfile`，直接调用 `GetMech` 读取 `1001/crit_chance_per_point`（+2%/点）与 `1015/invulnerable_duration_per_point`（+0.03s/点），在局部变量中加算；
   - 技能 11（天剑降临）：`HeavenlySwordDescent.cpp` 在 `DoCast` 中直接读取 `1101/field_radius_range_per_point`（+8%/点）、`1107/field_radius_mult`（0.7x 即 -30% 范围惩罚）与 `1119/duration_per_point`（+0.5s/点），完全在局部组装领域参数；
   - 技能 12（血海）：`BloodSea.cpp` 在 `DoCast` 中直接读取 `1201/damage_per_point`（+6%/点 More 增伤）、`1207/damage_mult`（1.1x 即 +10% More 增伤，且自身描述有 -40% 范围惩罚）与 `1219/duration_per_point`（+0.6s/点），完全在行为层就地计算。

3. **`mastery_skill_trees.json` 中残留错位的全局 `stat_modifiers` 与多重增伤分支（严重双重生效隐患）**：
   - 按 `src/game/foundation/components/Stats.hpp:224-284` 的 `StatType` 定义逐条核对（`mode` 语义见同文件 `ModifierMode`：0=Flat / 1=PercentAdd / 2=PercentMult），当前 6 处登记与节点描述**均不对应**，属于历史串写/跨技能漂移：
     - 技能 10 节点 1001：`stat_modifiers: [{"type": 17, "mode": 0, "value": 2.0}]` —— `type 17` 实为 **AttackSpeed**（而非“全局暴击率”），`mode 0` 为 Flat 定值；
     - 技能 11 节点 1101：`[{"type": 25, "mode": 1, "value": 8.0}]`、节点 1107：`[{"type": 25, "mode": 1, "value": -30.0}]` —— `type 25` 实为 **ResistLightning**（而非“全局范围”）；
     - 技能 11 节点 1119：`[{"type": 39, "mode": 0, "value": 1.0}]` —— `type 39` 实为 **HealthRegen**（而非“持续时间”）；
     - 技能 12 节点 1201：`[{"type": 13, "mode": 1, "value": 10.0}]` —— `type 13` 实为 **PoisonDamage**（而非“持续增伤”）；
     - 技能 12 节点 1207：`[{"type": 25, "mode": 1, "value": -40.0}]` —— 同为 **ResistLightning**（而非“范围缩小”）。
     - 这些条目会被 `StatsSystem.cpp:455-495` 以技能作用域写入玩家属性，属于**非预期的全局属性污染**，与专精节点的 `desc_key` 描述无关。清理前须逐节点复核 `desc_key` 与 `stat_modifier` 的一致性（见 Task 3.3），确认不存在任何被有意保留的真实收益后再清空。
   - **节点 1015 的已知残留（本批不清理，显式登记）**：节点 1015 除被迁移的 `invulnerable_duration_per_point` 外，另登记 `stat_modifiers: [{"type": 35, "mode": 1, "value": 20.0}]`（`type 35` = **DodgeChance**，PercentAdd +20%）。该条与本次退役的机制键无对应关系，为保证数值等价性不被扩大化，**本批明确保留**，并在风险表 R-04 中登记为待专项治理项。
   - 此外，**节点 1207 历史残留了 `damage_modifiers: [{"value": 30.0, "type": 2}]`（30% More）**，且 `BloodSea.cpp:544-546` 残留了 `bottomless_damage_mult = 1.1f`。若不清理，叠加 UMR `2012070`（1.10x）后将发生 `1.10 * 1.30 = 1.43x` 甚至三重增伤的毁灭性伤害膨胀；
   - **节点 1207 描述文本偏差（须同步修正）**：`mastery_skill_trees.json` 中节点 1207 的 `desc_key` 字段存放的是**本地化明文**而非间接键，其当前值为「血海范围缩小 40%，但伤害总增 (More) 30%。」。而运行时代码基准（`bottomless_damage_mult = 1.1f`）与本次冻结的 UMR `2012070`（+10%）均为 10%，文本已虚高 3 倍。本设计以**运行时数值等价**为准（10%），并要求同步修正描述文本（见 Task 3.4）；
   - 本次改造必须彻底清空上述全部 6 处 `stat_modifiers` 与 1207 的 `damage_modifiers` 为 `[]`，同时物理删除行为层残留的 `bottomless_damage_mult`，将 1207 增伤单源收敛于 UMR `2012070`（+10% More），彻底消灭多重计算与全局污染。

### 1.2 核心目标（Goals）

1. **0 新增算子、100% 复用既有 OpCodes 30..42（P0）**：
   - 经详尽梳理，技能 10、11、12 的所有纯数值专精需求完全落在既有算子空间内：
     - `SKILL_MORE_DAMAGE_MULT: 30`（技能 12: 1201 +6%/点, 1207 +10%）
     - `SKILL_BONUS_CRIT: 34`（技能 10: 1001 +2%/点）
     - `SKILL_AREA_MULT: 35`（技能 11: 1101 +8%/点, 1107 -30%; 技能 12: 1207 -40%）
     - `SKILL_DURATION_FLAT: 41`（技能 10: 1015 +0.03s/点; 技能 11: 1119 +0.5s/点; 技能 12: 1219 +0.6s/点）
   - **无需扩充任何新 OpCode、无需修改底层二进制头结构**，最大化保障运行时稳定性。

2. **规范化接入 9 条 Canonical 专精记录（P0）**：
   - **技能 10（2 条）**：
     - `2010010`（锋芒毕露）：`SKILL_BONUS_CRIT`（基础暴击率 +2%/点，param_f32: 0.02）；
     - `2010150`（踏虚）：`SKILL_DURATION_FLAT`（无敌帧时长 +0.03s/点，param_f32: 0.03）。
   - **技能 11（3 条）**：
     - `2011010`（天域增幅）：`SKILL_AREA_MULT`（领域半径 +8%/点，param_f32: 0.08）；
     - `2011070`（天穹贯星 Keystone）：`SKILL_AREA_MULT`（领域半径惩罚 -30%，param_f32: -0.30）；
     - `2011190`（久驻天域）：`SKILL_DURATION_FLAT`（领域持续时间 +0.5s/点，param_f32: 0.50）。
   - **技能 12（4 条）**：
     - `2012010`（血压潮升）：`SKILL_MORE_DAMAGE_MULT`（持续伤害 More +6%/点，param_f32: 0.06）；
     - `2012070`（无间血狱 Keystone）：`SKILL_MORE_DAMAGE_MULT`（More 增伤 +10%，param_f32: 0.10）；
     - `2012071`（无间血狱 Keystone）：`SKILL_AREA_MULT`（范围缩小 -40%，param_f32: -0.40）；
     - `2012190`（久驻血雾）：`SKILL_DURATION_FLAT`（血海持续时间 +0.6s/点，param_f32: 0.60）。

3. **退役 8 项机制键并清理 6 处 `mastery_skill_trees.json` 中的混淆属性（P0）**：
   - 退役上述数值在 [`assets/data/skill_mechanics.json`](file:///d:/PRJ/NoMoreDay/assets/data/skill_mechanics.json) 中的 8 个键：`10/1001/crit_chance_per_point`、`10/1015/invulnerable_duration_per_point`、`11/1101/field_radius_range_per_point`、`11/1107/field_radius_mult`、`11/1119/duration_per_point`、`12/1201/damage_per_point`、`12/1207/damage_mult`、`12/1219/duration_per_point`；
   - 清理 [`assets/data/mastery_skill_trees.json`](file:///d:/PRJ/NoMoreDay/assets/data/mastery_skill_trees.json) 中对应 6 个节点的 `stat_modifiers` 为 `[]`，杜绝全局属性污染与双重计算。

4. **烘焙基准显式化与行为层单源消费重构（P0）**：
   - 在 [`SkillSpecializationBaker.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSpecializationBaker.cpp) 步骤 1 中补齐 `case 10:`、`case 11:`、`case 12:`，**仅显式初始化步骤 3 会参与二次合成的 `out_profile.area_radius` 与 `del.duration`**（`del.range` / `del.sub_interval` 无消费点、无算子，按最小写入原则不写，见 §3.2 步骤 1）；
   - 步骤 2 明确 10~12 号技能的纯数值计算交由 UMR 承载，非 UMR 状态继续由现有 `SpecState` 承载，遵守 DoD#3 单源判定；
   - 步骤 3 通用合成逻辑自动生效（法耗、冷却、暴击、范围、持续时间、More 增伤）；
   - 重构 `SevenStarSlash.cpp`、`HeavenlySwordDescent.cpp`、`BloodSea.cpp` 的 `DoCast`：统一调用标准公共基元 `ResolveBakedProfile`（`src/game/systems/skill/SkillProfileResolve.hpp`），并按既有 Batch 1~3 惯例进行**空值守卫**（`profile == nullptr` 时回退到各自组件/`skills.json` 基准），然后按技能单源消费：
     - 技能 10：`profile->delivery.bonus_crit`、`profile->delivery.duration`；
     - 技能 11：`profile->area_radius`、`profile->delivery.duration`；
     - 技能 12：`profile->area_radius`、`profile->delivery.duration`、`profile->more_damage_mult`。

5. **离线门禁与全量测试闭环（P0）**：
   - 扩充 [`scripts/validate_skill_spec_modifiers.py`](file:///d:/PRJ/NoMoreDay/scripts/validate_skill_spec_modifiers.py) 的 6 项门禁，覆盖 9 条新增记录、等价性断言与退役键防回潮；
   - 新增 `tests/python/SkillSpecBatch4GateTest.py` 与 `tests/unit/SkillBatch4DeliveryOpTests.cpp`，保持全仓 100% 构建与回归测试全绿。

### 1.3 明确非目标（Non-Goals）

1. **非目标**：不将复杂形态、分支机制与触发逻辑迁入 UMR：
   - **技能 10**：1000 锁敌校准、1002 第7击特化增伤、1003 瞬星回冷退后硬直、1004 命门显现、1005 破军、1006 斩将、1007 七星归一锁定主目标、1008 孤曜孤立目标增伤、1009 剑流返还概率、1010 剑回斗转后续增益、1011 星痕追斩追加、1012 追星步位移回冷、1013 无尽七曜势覆盖、1014 斗柄回天、1016 残星换位背后落点、1017 御剑追影、1018 护盾、1019 生门回血、1020 剑痕残留、1021 天枢轮斩环切形态、1022 星坠四重斩形态、1023 裂曜、1024 痕灭引爆、1025 返星入步；
   - **技能 11**：1100 剑胚校准、1102 灵剑阶初击增伤、1103 灵压余震提频、1104 裂界中心增伤、1105 诛王精英增伤、1106 减速、1108 穿云余威剑痕、1109 灵锋献祭领域切割、1110 阶数上限、1111 剑雨回响追加、1112 灵轮续转攻速、1113 万象轮转初击减伤返还灵剑、1114 剑潮归鞘后续增伤、1115 天域锁界牵引、1116 场域共振提频、1117 剑阵同调同步切割、1118 异常增伤、1120 天相极化、1121 雷池脉冲、1122 霜星封界冻结、1123 炽阳焚城灼烧、1124 元素剥离降抗；
   - **技能 12**：1200 初始范围加算（8/16/24/32）、1202 渴血成锋层数增伤、1203 追随速度、1204/1206 低血增伤、1205 减速、1208 断脉余波、1209/1210/1212 吸血效率、1211 鲜血回灌爆发回复、1213 饮海而生过量转血渴、1214 猎命回潮、1215 贴身压制、1216 脉冲提频、1217 绝影共噬脉冲、1218 减速压制、1220 虚蚀瘴幕转虚空/侵蚀、1221 血潮奔流推进形态、1222 血环噬身绞杀圈、1223 噬骨余烬、1224 瘴海蚀骨降抗；
   上述逻辑继续由 C++ 行为层（`SevenStarSlash`、`HeavenlySwordDescent`、`BloodSea`）及对应生成的 `SpecState.gen.hpp` 承载。
2. **非目标**：不修改二进制运行时的底层内存对齐与 Schema 头结构；
3. **非目标**：本批次完成 10~12 招牌技能后，全职业 12 个主动技能专精即全部完成 UMR 纯数值交付层接入，不涉及新职业拓展。
4. **非目标**：不清理节点 1015 残留的 `stat_modifiers: [{"type": 35, "mode": 1, "value": 20.0}]`（DodgeChance +20%）。该残留与本次退役键无语义对应，清理会引入超出“迁移等价”口径的额外数值变更，故登记为已知遗留（R-04）并交由后续专项治理。
5. **非目标**：不为技能 10/11 引入 `SKILL_MORE_DAMAGE_MULT` 记录。二者当前均无 More 增伤专精需求，冻结 `more_damage_mult == 1.0f` 是刻意的等价值；任何“顺手乘一下”的写法都会破坏单源边界。
6. **非目标/本批刻意保留**：不删除已由 UMR 接管后失去生产读者的 `SpecState` 点数字段（`SevenStarSlashSpecStateGen::critChancePoints`/`voidTreadPoints`、`HeavenlySwordDescentCastSpecGen::celestialDomainPoints`/`enduringHeavenPoints`、`BloodSeaCastSpecGen::pressureTideRisePoints`/`lingeringBloodMistPoints`）。这些字段仍由生成的 SpecState 绑定、`assets/data/skill_specstate/skill_1{0,1,2}.json` 与既有测试引用，删除属独立的生成链路清理，超出本批“迁移等价”口径，故显式登记为保留项而非遗漏。
7. **非目标/本批刻意保留**：技能 10 步骤 1 写入的 `out_profile.area_radius = GetParam("radius", 96.0f)` 对技能 10 无行为层读者，仅为与技能 11/12 保持「步骤 3 统一乘算 `area_radius`」的一致基准，**不得据此认为它是七星斩的判定半径**（实际命中半径由行为层在 `SevenStarSlash.cpp` 中以 `baseRadius * 0.28/0.34/0.50` 派生）。保留该写入是刻意的统一基准决策。

---

## 2. 架构拓扑与职责边界

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│                 SpecializedSkill 专精加点 (allocated_points)                 │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │
                 ┌─────────────────────┴─────────────────────┐
                 ▼                                           ▼
    【UMR 纯数值交付层】 (数据驱动)                 【SpecState / 机制形态层】 (代码驱动)
    - SKILL_MORE_DAMAGE_MULT (30)               - 元素形态与转质 (1021/1022, 1121/1122/1123, 1220/1221/1222)
    - SKILL_BONUS_CRIT       (34)               - 触发契约 (TriggerRule: 1009, 1011, 1111, 1211)
    - SKILL_AREA_MULT        (35)               - 资源消耗与返还 (剑流、灵剑阶、血渴)
    - SKILL_DURATION_FLAT    (41)               - 协同与时序窗口 (1010, 1014, 1114, 1117, 1214, 1217)
                 │                              - 目标筛选与落点判定 (1000, 1007, 1016, 1115)
                 │                              - 条件与斩杀增伤 (1002, 1004, 1005, 1006, 1104, 1105, 1204, 1206)
                 │                                           │
                 └─────────────────────┬─────────────────────┘
                                       │ 汇流 (SkillSpecializationBaker::Bake)
                                       ▼
                      【BakedSkillProfile / BakedDeliveryParams】
                                       │
        ┌──────────────────────────────┼──────────────────────────────┐
        ▼                              ▼                              ▼
【SevenStarSlash::DoCast】     【HeavenlySwordDescent::DoCast】  【BloodSea::DoCast】
- 读 profile->delivery.bonus_crit - 读 profile->area_radius        - 读 profile->area_radius
- 读 profile->delivery.duration   - 读 profile->delivery.duration  - 读 profile->delivery.duration
                                                                   - 读 profile->more_damage_mult
     注：技能 10 与技能 11 本批均无 SKILL_MORE_DAMAGE_MULT 记录，
         故刻意不读取 profile->more_damage_mult；
         三个行为层均不读取 profile->effective_mana_cost（法耗由技能系统统一结算）。
         全部读取点均需 profile != nullptr 守卫，空档案时回退到原有基准。
```

---

## 3. 详细技术设计

### 3.1 交付算子复用性分析（0 新增算子论证）

Batch 4 所需的所有交付修饰算子均已在 Batch 1/2/3 中完成定义与工程落地：

| 算子枚举 | OpCode 值 | 类别 | 参数语义 | Batch 4 使用点 |
|---|---|---|---|---|
| `SKILL_MORE_DAMAGE_MULT` | 30 | `SkillDelivery` | `param_u32`: skill_id; `param_f32`: 每点 More 增伤倍率偏移 | 技能 12: 1201 (+6%), 1207 (+10%) |
| `SKILL_BONUS_CRIT` | 34 | `SkillDelivery` | `param_u32`: skill_id; `param_f32`: 每点基础暴击率绝对加成 | 技能 10: 1001 (+2%) |
| `SKILL_AREA_MULT` | 35 | `SkillDelivery` | `param_u32`: skill_id; `param_f32`: 每点范围半径倍率偏移 | 技能 11: 1101 (+8%), 1107 (-30%); 技能 12: 1207 (-40%) |
| `SKILL_DURATION_FLAT` | 41 | `SkillDelivery` | `param_u32`: skill_id; `param_f32`: 每点持续时间增减秒数 | 技能 10: 1015 (+0.03s); 技能 11: 1119 (+0.5s); 技能 12: 1219 (+0.6s) |

无需扩充 `ModifierOpCode` 枚举，亦无需在 `ModifierDelta` 中新增容器。

### 3.2 烘焙管线（Baking Pipeline）基准显式化

在 [`SkillSpecializationBaker.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSpecializationBaker.cpp) 的六步烘焙流中接入技能 10、11、12：

#### 步骤 1：基础参数显式初始化（消除隐式 1.0 与 0 依赖）

```cpp
case 10: // 七星斩
  // 斩击范围基准取 skills.json params.radius (96.0f)
  out_profile.area_radius = skillData->GetParam("radius", 96.0f);
  // 无敌帧时长基准取 skills.json params.invulnerable_duration (0.5f)
  del.duration = skillData->GetParam("invulnerable_duration", 0.5f);
  break;

case 11: // 天剑降临
  // 领域半径基准取 skills.json params.field_radius (140.0f)
  out_profile.area_radius = skillData->GetParam("field_radius", 140.0f);
  // 领域持续时间基准取 skills.json params.field_duration (5.0f)
  del.duration = skillData->GetParam("field_duration", 5.0f);
  break;

case 12: // 血海
  // 血海半径基准取 skills.json params.field_radius (120.0f)
  out_profile.area_radius = skillData->GetParam("field_radius", 120.0f);
  // 血海持续时间基准取 skills.json params.field_duration (4.8f)
  del.duration = skillData->GetParam("field_duration", 4.8f);
  break;
```

**步骤 1 写入范围的最小化原则（仅写步骤 3 会合成的字段）**：
- 步骤 3 的通用合成只对 `out_profile.area_radius`（`*= GetSkillAreaMult`）与 `del.duration`（`+= GetSkillDurationFlat`）做二次运算，因此只有这两个字段需要显式基准；
- 本批**不写入 `del.range` 与 `del.sub_interval`**：
  - `del.range`：不存在任何 range 专精算子，且 `SevenStarSlash` / `HeavenlySwordDescent` / `BloodSea` 三个行为层均不消费 `profile->delivery.range`（天剑初击半径仍直读 `skills.json` 的 `impact_radius`）。为技能 10/12 写入 `del.range = area_radius` 更是纯粹的复制死写；
  - `del.sub_interval`：同样无专精算子、无消费点（`profile->delivery.sub_interval` 目前仅被技能 3/5/6 消费）。领域脉冲间隔继续由 `PersistentFieldComponents.hpp` 的组件头默认值单源承载（天剑 `tick_interval = 0.5s`、血海 `tick_interval = 0.25s`）；
- 该最小化原则与本设计 §3.2 步骤 2 自述的“彻底杜绝双重判定与死写”保持一致。若后续确有行为层需要消费这两者，应同时提交“消费者改造 + 基准写入”，二者不可分离。


#### 步骤 2：专精节点分支与分工界定

`ApplyNodeModifiersToProfile` **不**为技能 10/11/12 新增分支，三者继续落入既有 `default: break;` 直接返回。
- **纯数值交付**：已完全由 UMR 算子（OpCode 30/34/35/41）接管并在步骤 3 统一合成；
- **机制与形态状态**：继续由各技能的 `ResolveSpecState` 驱动，不向 Baker `feature_flags` 引入冗余位，彻底杜绝双重判定与死写；
- **不留空分支**：与同一 switch 内既有惯例一致（如 `case 5` 节点 500 的「不再在 Baker 侧保留空分支」），不新增只含注释与 `break;`、行为上等价于 `default` 的空分支。

#### 步骤 3：UMR 交付增量一次性合成

通用合成逻辑自动生效，完全覆盖技能 10、11、12 的 9 条增量：
```cpp
out_profile.more_damage_mult *= specDelta.GetSkillMoreDamageMult(skill_id);
out_profile.delivery.bonus_crit += specDelta.GetSkillBonusCrit(skill_id);
out_profile.area_radius *= specDelta.GetSkillAreaMult(skill_id);
del.duration = std::max(0.0f, del.duration + specDelta.GetSkillDurationFlat(skill_id));
```

### 3.3 行为层单源消费重构

各技能 `DoCast` 接入统一烘焙快照模式，使用统一标准基元 [`src/game/systems/skill/SkillProfileResolve.hpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillProfileResolve.hpp)（杜绝手写私有函数与重复造轮子）。

1. **统一 Profile 解析调用与空值守卫**：
   ```cpp
   #include "game/systems/skill/SkillProfileResolve.hpp"
   ...
   BakedSkillProfile localProfile;
   const auto *profile = ResolveBakedProfile(registry, owner, kSkillId, localProfile);
   // 不变量：profile == nullptr ⇔ 该 owner 既无缓存档案、也无同 ID 专精槽。
   // 依据 Batch 1~3 既有惯例（SwordArray.cpp:132-134、BladeFormation.cpp:109/158/185/239-240、
   // MindBlade.cpp:65），所有消费点必须是「有档案取档案、无档案取原基准」的三元回退，
   // 严禁无条件解引用，否则独立单测/影子复制等无专精槽路径会空指针崩溃。
   // 哨兵守卫：skillData 缺失时 Bake 会早退并只写入带 skill_id 与 1.0f 默认值的哨兵档案
   // （SkillSpecializationBaker.cpp:26-31 / SkillSystem.cpp:2736-2742），非空却无意义；
   // 其默认值为正、无法用数值判据区分，而该情形下 skill 亦必为空指针。故凡有字段级回退的
   // 消费点一律写「profile && skill != nullptr ? profile->X : (skill ? skill->GetParam(...) : <默认基准>)」，
   // 使哨兵永不生效；技能 10 以 skillData 取参、且入口已对空 skillData 早退，不受此条约束。
   ```
   - 各技能的**空档案回退基准**（等价于迁移前的行为）：
     - 技能 10：`std::max(skillData->GetParam("invulnerable_duration", 0.5f), 0.0f)` 取无敌帧时长；暴击加成回退 `0.0f`；
     - 技能 11：领域半径回退 `(base_field_radius + spent_tiers * tier_radius_bonus) * 1.0f`，时长回退 `skill ? skill->GetParam("field_duration", kFieldDurationFallback) : kFieldDurationFallback`（`skill ?` 守卫避免空解引用）；
     - 技能 12：半径回退 `skill ? skill->GetParam("field_radius", kFieldRadiusDefault) : kFieldRadiusDefault`，时长回退 `skill ? skill->GetParam("field_duration", kFieldDurationDefault) : kFieldDurationDefault`，More 回退 `1.0f`（`skill ?` 守卫避免空解引用）。

2. **`SevenStarSlash.cpp`** 单源消费：
   - `invulnerableDuration = profile ? profile->delivery.duration : skillData->GetParam("invulnerable_duration", 0.5f);`（替代原来的 `invulnerable_duration + voidTreadPoints * GetMech(...)`，物理删除 `voidTreadDurationPerPoint`（声明行 `SevenStarSlash.cpp:380`））
   - `critChanceBonus = profile ? profile->delivery.bonus_crit : 0.0f;`（替代原来的 `critChancePoints * GetMech(...)`，物理删除 `critChancePerPoint`（声明行 `SevenStarSlash.cpp:368`））
   - **不引入 `profile->more_damage_mult`**：技能 10 本批无 `SKILL_MORE_DAMAGE_MULT` 记录，`more_damage_mult` 恒为 `1.0f`，且 `SevenStarSlash.cpp` 中并不存在名为 `baseDamageMultiplier` 的变量（斩击基础伤害变量为 `baseSlashDamage`，见 `SevenStarSlash.cpp:475`）。强行乘算属于无效死代码。

3. **`HeavenlySwordDescent.cpp`** 单源消费：
   - 领域半径缩放：`field.header.radius = (base_field_radius + spent_tiers * tier_radius_bonus) * (skill != nullptr && profile && base_field_radius > 0.0f ? profile->area_radius / base_field_radius : 1.0f);`（哨兵守卫：无 skillData 时档案为 1.0f 哨兵，比值会把半径压到 ~1，故以 `skill != nullptr` 排除；已同时包含 1101 +8%/点 与 1107 -30% 惩罚，物理删除 `celestial_domain_per_point`（声明行 `HeavenlySwordDescent.cpp:505`）与 `sky_piercing_range_mult`（声明行 `:508`））
   - 领域持续时间：`field.header.duration = (profile && skill != nullptr) ? profile->delivery.duration : (skill ? skill->GetParam("field_duration", kFieldDurationFallback) : kFieldDurationFallback);`（哨兵守卫同上，且回退分支同样以 `skill ?` 守卫，避免空解引用；已包含基础 5.0s + 1119 +0.5s/点，物理删除 `enduring_heaven_per_point`（声明行 `:517`））
   - **不引入 `field.bonus_damage_mult *= profile->more_damage_mult;`**：`HeavenlySwordFieldComponent`（`src/game/systems/skill/components/PersistentFieldComponents.hpp:6-21`）**不存在 `bonus_damage_mult` 成员**（仅 `impact_damage_mult` / `field_damage_mult`），该行无法编译；且技能 11 本批无 More 记录，即使成员存在也是 `*= 1.0f` 空操作。
   - `1107` 的 `impact_damage_bonus`（+0.35 初击增伤）属于机制层保留键，仍由行为层读取，不受本次迁移影响。

4. **`BloodSea.cpp`** 单源消费：
   - 基础持续时间：`field.header.duration = ((profile && skill != nullptr) ? profile->delivery.duration : (skill ? skill->GetParam("field_duration", kFieldDurationDefault) : kFieldDurationDefault)) + field_duration_per_bloodthirst * static_cast<float>(effective_consumed);`（哨兵守卫：skillData 缺失时档案为 1.0f 哨兵，须以 `skill != nullptr` 排除；回退分支同样以 `skill ?` 守卫）
     - 并**物理删除 `BloodSea.cpp:482-483`**（`field.header.duration += spec.lingeringBloodMistPoints * duration_per_point;`）与变量 `duration_per_point`（声明行 `:372`）——节点 1219 已由 UMR `2012190` 承接；
   - 基础半径：`field.header.radius = ((profile && skill != nullptr) ? profile->area_radius : (skill ? skill->GetParam("field_radius", kFieldRadiusDefault) : kFieldRadiusDefault)) + static_cast<float>(effective_consumed) * field_radius_per_bloodthirst;`（哨兵守卫同上，回退分支同样以 `skill ?` 守卫），随后保留 `BloodSea.cpp:480-481` 的 `+= spec.bloodCurtainOpeningPoints * radius_per_point`（节点 1200 未迁移，仍为机制表权威源）与 `:540` 的环形形态 `*= ring_radius_mult`；
   - 持续伤害增伤：**在 `BloodSea.cpp:484-485` 的原位置**写入 `field.bonus_damage_mult *= (profile ? profile->more_damage_mult : 1.0f);`（已包含 1201 +6%/点 与 1207 +10%），并**物理删除**：变量 `damage_per_point`（声明行 `:313`）、`bottomless_damage_mult`（声明行 `:330`）、`BloodSea.cpp:484-485` 的 `*= 1.0f + pressureTideRisePoints * damage_per_point`、`BloodSea.cpp:544-546` 的 `if (spec.bottomlessPurgatory) { ... *= bottomless_damage_mult; }`；
     - **顺序约束（必须遵守）**：技能 12 的 1202（`damage_per_point_per_bloodthirst`）在 `BloodSea.cpp:486-488` 以 `+=` 方式叠加到 `bonus_damage_mult`，而原 1207 的 `*= 1.1` 发生在 `:544`（即 1202 加算与 542 环形乘算之后）。把唯一的 `*= profile->more_damage_mult` 放在 `:484` 位置会使 1201 的乘算提前到 1202 加算之前，产生 `0.1 * K * ring` 量级的偏差（`K = effective_consumed * bloodthirstEdgePoints * damage_per_point_per_bloodthirst`；仅当 1202 未加点即 `K = 0` 时偏差为零）。该偏差是**有意接受的迁移代价**（换取单一事实源），须在本设计 §6 R-05 与验收快照测试中显式覆盖。


---

## 4. 节点数据配置契约与单一事实源治理

### 4.1 记录 ID 规则与通用头约束
严格遵循 UMR 规范：`modifier_id = 2,000,000 + node_id * 10 + op_index`。
- `priority`: `200`
- `profession_mask`: `1` (BladeAscendant)
- `weapon_class_mask`: `65535`
- `equip_slot_mask`: `0`
- `exclusive_group`: `0`
- `max_active`: `0`
- `debug_source`: `"skill_spec_node"`
- `min_player_level`: `1`
- 契约约束：`record.stacks == runtime.param_u32 == skill_id`，`record.stat_path == runtime.target`。

### 4.2 详细 Canonical 记录对照表（9 条）

| 记录 ID | 技能 | 节点 | OpCode | param_u32 (`stacks`) | param_f32 (`value`) | Canonical stat_path | operation | debug_name |
|---|---|---|---|---|---|---|---|---|
| `2010010` | 10 (七星斩) | 1001 (锋芒毕露) | `SKILL_BONUS_CRIT` | 10 | `0.02` | `skill.bonus_crit` | `add` | `SevenStarSlash_Node1001_BonusCrit` |
| `2010150` | 10 (七星斩) | 1015 (踏虚) | `SKILL_DURATION_FLAT` | 10 | `0.03` | `skill.duration_flat` | `add` | `SevenStarSlash_Node1015_VoidTreadDuration` |
| `2011010` | 11 (天剑降临) | 1101 (天域增幅) | `SKILL_AREA_MULT` | 11 | `0.08` | `skill.area_mult` | `mul` | `HeavenlySword_Node1101_AreaMult` |
| `2011070` | 11 (天剑降临) | 1107 (天穹贯星) | `SKILL_AREA_MULT` | 11 | `-0.30` | `skill.area_mult` | `mul` | `HeavenlySword_Node1107_AreaPenalty` |
| `2011190` | 11 (天剑降临) | 1119 (久驻天域) | `SKILL_DURATION_FLAT` | 11 | `0.50` | `skill.duration_flat` | `add` | `HeavenlySword_Node1119_DurationFlat` |
| `2012010` | 12 (血海) | 1201 (血压潮升) | `SKILL_MORE_DAMAGE_MULT` | 12 | `0.06` | `skill.more_damage` | `mul` | `BloodSea_Node1201_MoreDamage` |
| `2012070` | 12 (血海) | 1207 (无间血狱-增伤) | `SKILL_MORE_DAMAGE_MULT` | 12 | `0.10` | `skill.more_damage` | `mul` | `BloodSea_Node1207_MoreDamage` |
| `2012071` | 12 (血海) | 1207 (无间血狱-缩圈) | `SKILL_AREA_MULT` | 12 | `-0.40` | `skill.area_mult` | `mul` | `BloodSea_Node1207_AreaPenalty` |
| `2012190` | 12 (血海) | 1219 (久驻血雾) | `SKILL_DURATION_FLAT` | 12 | `0.60` | `skill.duration_flat` | `add` | `BloodSea_Node1219_DurationFlat` |

### 4.3 机制表（`skill_mechanics.json`）退役清单（8 项）

| 技能/节点 | mechanics 键名 | 迁移前用途 | 处置与唯一权威源 |
|---|---|---|---|
| 10/1001 | `crit_chance_per_point` | 基础暴击率 (+0.02/点) | **彻底删除**；权威源交由 UMR `2010010` |
| 10/1015 | `invulnerable_duration_per_point` | 无敌帧延长 (+0.03s/点) | **彻底删除**；权威源交由 UMR `2010150` |
| 11/1101 | `field_radius_range_per_point` | 领域半径 (+0.08/点) | **彻底删除**；权威源交由 UMR `2011010` |
| 11/1107 | `field_radius_mult` | 领域范围惩罚 (0.7x 即 -30%) | **彻底删除**；权威源交由 UMR `2011070` |
| 11/1119 | `duration_per_point` | 领域持续时间 (+0.5s/点) | **彻底删除**；权威源交由 UMR `2011190` |
| 12/1201 | `damage_per_point` | 持续增伤 (+0.06/点) | **彻底删除**；权威源交由 UMR `2012010` |
| 12/1207 | `damage_mult` | 无间血狱 More 增伤 (1.1x) | **彻底删除**；权威源交由 UMR `2012070` |
| 12/1219 | `duration_per_point` | 血海持续时间 (+0.6s/点) | **彻底删除**；权威源交由 UMR `2012190` |

### 4.4 `mastery_skill_trees.json` 属性与旧修饰器清理清单（6 处）

| 技能/节点 | 原属性/修饰器配置 | 清理后内容 | 治理理由与行为层配套 |
|---|---|---|---|
| 10/1001 | `stat_modifiers: [{"type": 17, "mode": 0, "value": 2.0}]`（`type 17` = **AttackSpeed** Flat +2） | `[]` | 该条与节点描述（暴击率）完全不符，属历史串写造成的全局攻速污染；暴击率权威源转为 UMR 交付算子 `2010010` |
| 11/1101 | `stat_modifiers: [{"type": 25, "mode": 1, "value": 8.0}]`（`type 25` = **ResistLightning** +8%） | `[]` | 与节点描述（领域半径）完全不符，属全局雷抗污染；半径权威源转为 UMR 交付算子 `2011010` |
| 11/1107 | `stat_modifiers: [{"type": 25, "mode": 1, "value": -30.0}]`（`type 25` = **ResistLightning** -30%） | `[]` | 同上，属全局雷抗污染；半径惩罚权威源转为 UMR 交付算子 `2011070` |
| 11/1119 | `stat_modifiers: [{"type": 39, "mode": 0, "value": 1.0}]`（`type 39` = **HealthRegen** Flat +1） | `[]` | 与节点描述（持续时间）完全不符，属全局生命回复污染；持续时间由 UMR 交付算子 `2011190` 承载 |
| 12/1201 | `stat_modifiers: [{"type": 13, "mode": 1, "value": 10.0}]`（`type 13` = **PoisonDamage** +10%） | `[]` | 与节点描述（持续增伤）完全不符，属全局毒伤污染；More 增伤由 UMR 交付算子 `2012010` 承载 |
| 12/1207 | `stat_modifiers: [{"type": 25, "mode": 1, "value": -40.0}]`（`type 25` = **ResistLightning** -40%）<br>`damage_modifiers: [{"value": 30.0, "type": 2}]`（PercentMult More +30%） | `stat_modifiers: []`<br>`damage_modifiers: []` | 雷抗污染清除；缩圈由 `2012071` 承载，More 增伤由 `2012070` 承载（+10% More，与 `bottomless_damage_mult = 1.1f` 运行时基准等价）。**清空旧 `damage_modifiers` 并物理删除 `BloodSea.cpp:544-546` 及声明行 `:330` 的 `bottomless_damage_mult`**，彻底杜绝 1.43x 伤害膨胀 |

**清理范围之外的显式登记（避免“已无全局污染”的误判）**：
- **节点 10/1015 保留 `stat_modifiers: [{"type": 35, "mode": 1, "value": 20.0}]`**（`type 35` = **DodgeChance** PercentAdd +20%）。该条与本次退役的 `invulnerable_duration_per_point` 无对应关系，为不扩大数值变更范围，本批明确保留，并由风险表 R-04 跟踪专项治理；
- **节点 12/1207 的 `desc_key` 描述文本须同步修正**：`desc_key` 存放明文「血海范围缩小 40%，但伤害总增 (More) 30%。」，与本批冻结的 10% 不符，须改为「血海范围缩小 40%，但伤害总增 (More) 10%。」（见 Task 3.4）；
- 技能 10/11/12 各节点上大量 `damage_modifiers`（如 1004/1005/1007/1008/1010/1022/1024、1104/1105/1113/1114/1118/1120/1121/1122/1123、1206/1214/1223）由 `DamagePipeline.cpp:1140/1322` 消费，属机制层增伤，**不属于本批范围**，仅 1207 因与 UMR `2012070` 同源重复才被清理。

---

## 5. 门禁守护与测试验证

### 5.1 门禁配置扩展 (`scripts/validate_skill_spec_modifiers.py`)

在 `MIGRATION_EQUIVALENCE` 中追加 9 条登记（严格修正 2011190 映射至 1119）：
```python
# Batch 4（技能 10/11/12，9 条）
(2010010, 10, 1001, "crit_chance_per_point", "raw", 0.02),
(2010150, 10, 1015, "invulnerable_duration_per_point", "raw", 0.03),
(2011010, 11, 1101, "field_radius_range_per_point", "raw", 0.08),
(2011070, 11, 1107, None, "literal", -0.30),
(2011190, 11, 1119, "duration_per_point", "raw", 0.50),
(2012010, 12, 1201, "damage_per_point", "raw", 0.06),
(2012070, 12, 1207, None, "literal", 0.10),
(2012071, 12, 1207, None, "literal", -0.40),
(2012190, 12, 1219, "duration_per_point", "raw", 0.60),
```

在 `DEAD_MECHANICS_KEYS` 中明确追加守护死键（覆盖 literal 迁移键，确保防回潮）：
```python
# Batch 4 彻底废弃键防回潮门禁（防止 literal 记录跳过退役检查）
(11, 1107, "field_radius_mult"),
(12, 1207, "damage_mult"),
```

在 `KEPT_MECHANICS_KEYS` 中维护保留键清单：
- `(10, 0, "slash_count")`
- `(11, 0, "field_damage_per_tier")`, `(11, 0, "base_resist_reduction")`, `(11, 0, "field_pulse_base_damage")`
- `(11, 1107, "impact_damage_bonus")`
- `(12, 0, "low_life_threshold")`, `(12, 0, "field_duration_per_bloodthirst")`, `(12, 0, "field_radius_per_bloodthirst")`
- `(12, 1200, "radius_per_point")`

> **约束力说明（避免误判为全部必需）**：`check_registry_reverse` 只对 `MIGRATION_EQUIVALENCE` 中登记过的**已迁移节点**（1001/1015/1101/1107/1119/1201/1207/1219）执行“未登记键即报错”的反查。上述清单中，唯一对门禁有实际约束力的是 `(11, 1107, "impact_damage_bonus")`（与 literal 行的 `field_radius_mult` 同处一节点）；其余 `node 0` 与 `node 1200` 条目属于“显式声明这些键仍由机制表承载”的文档性登记，不参与反查。保留它们是刻意的：一旦未来这些节点被纳入 UMR，清单会立即产生冲突提醒。禁止为“让门禁变绿”而向该清单塞入已退役键。

### 5.2 自动化测试集构建
1. **Python 离线门禁**：`tests/python/SkillSpecBatch4GateTest.py`，验证 9 条记录的 ID 编解码、契约与生成的 `.bin` 一致性，以及 `DEAD_MECHANICS_KEYS` 严格缺席；
2. **C++ 单元测试职责划分**：
   - **算子纯求值测试**（`tests/unit/SkillBatch4DeliveryOpTests.cpp`）：专注检验 `ModifierEvaluator::Evaluate` 纯数值算子对技能 10/11/12 各 OpCode 的单点与多点缩放行为，不依赖外部环境资产；
   - **Baker 综合烘焙测试**（`tests/unit/SkillSpecializationBakerTests.cpp`）：覆盖加点后 `SkillSpecializationBaker::Bake` 对步骤 1 基准、步骤 3 UMR 合成的综合断言，使用 `TestSetupScope` 与 `ReloadModifierRuntimeFromAsset()` 保障测试隔离性；
   - **迁移等价性快照断言（本批新增硬性要求）**：在 `SkillSpecializationBakerTests.cpp` 中为技能 10/11/12 各加一组“零点 / 单点 / 点满”快照，直接断言 Baker 产物，作为迁移前后等价的唯一可执行证据：
     - 技能 10：`area_radius == 96.0f`；0 点 `delivery.duration == 0.5f`，`max` 点（1015 = 3 点）`0.5 + 0.03*3 == 0.59f`（浮点比较用 `doctest::Approx`）；`delivery.bonus_crit == 0.02 * points(1001)`；`more_damage_mult == 1.0f`；
     - 技能 11：0 点 `area_radius == 140.0f`、`delivery.duration == 5.0f`；1101 点满（4 点）`area_radius == 140 * (1 + 0.08*4) == 184.8f`；1107 点亮（1 点）`area_radius == 140 * (1 - 0.30) == 98.0f`；1119 点满（3 点）`delivery.duration == 5.0 + 0.5*3 == 6.5f`；
     - 技能 12：0 点 `area_radius == 120.0f`、`delivery.duration == 4.8f`、`more_damage_mult == 1.0f`；1201 点满（4 点）`more_damage_mult == 1 + 0.06*4 == 1.24f`；1207 点亮 `more_damage_mult == 1.10f` 且 `area_radius == 120 * (1 - 0.40) == 72.0f`；1219 点满（3 点）`delivery.duration == 4.8 + 0.6*3 == 6.6f`；
   - **既有断言的同步改造（本批硬性前置）**：`tests/unit/SkillMechanicsRegistryTests.cpp:81-85`、`tests/functional/BloodSeaNodes.cpp:128/140/178`、`tests/functional/HeavenlySwordDescentNodes.cpp:128/178` 当前直接断言将被删除的 8 个机制键，必须在 Phase 3 内同步移除或改写（见 Task 3.3），否则 Phase 7 回归必然失败。

---

## 6. 风险分析与规避措施

| 风险点 | 严重级 | 影响表现 | 规避措施 |
|---|---|---|---|
| **R-01: `ResolveBakedProfile` 可能返回 `nullptr`** | 高 | `ResolveBakedProfile`（`SkillProfileResolve.cpp:9-39`）仅在「有缓存档案」或「能命中同 ID 专精槽并即时烘焙」时才返回非空；两条路径都不满足时**返回 `nullptr`，并不会自动兜底烘焙**。独立单测、影子复制等无专精槽路径会因此空指针崩溃；此外 `skillData` 缺失时 Baker 会早退并写入默认值为正（`duration=1.0f` / `area_radius=1.0f`）的**哨兵档案**，非空却无意义且无法用数值判据区分 | 三个行为层的每个消费点一律写成 `profile ? profile->X : <原基准>` 三元回退（对齐 `SwordArray.cpp:132-134`、`BladeFormation.cpp:109/158/185/239-240`、`MindBlade.cpp:65` 的既有惯例），禁止无条件解引用；凡有字段级回退的消费点进一步以 `profile && skill != nullptr` 排除哨兵档案（见 §3.3 第 1 条哨兵守卫），回退基准见 §3.3 第 1 条 |
| **R-02: 1207 双算子在同节点的生效** | 中 | 1207 同时缩圈 -40% 且增伤 +10% | 注册为 `2012070`（More 增伤）与 `2012071`（Area 缩圈）；schema 以记录数组中的独立条目表达同 `node_id` 的多算子，`check_registry_reverse` 对该节点登记空 retired 集合、由 `DEAD_MECHANICS_KEYS` 覆盖 `damage_mult`，二者不冲突 |
| **R-03: `mastery_skill_trees.json` 与 `skills.json` 的解析差异** | 中 | 招牌技能专精树在单独 JSON 中承载 | `SkillRegistry` 已将 `mastery_skill_trees.json` 完整加载入树，生成脚本与运行时通过同一 `SkillRegistry` 索引，不受物理存储文件分拆影响 |
| **R-04: 节点 1015 残留 `DodgeChance +20%` 全局属性** | 中 | 本批退役 1015 的机制键但不清理其 `stat_modifiers: [{"type":35,"mode":1,"value":20.0}]`，玩家仍获得全局限时闪避收益，与节点描述不符 | 显式登记为“已知残留”（见 §4.4）。不纳入本批是为了不扩大数值变更范围；须在后续专项批次中以同样的 `desc_key` ↔ `stat_modifier` 一致性流程清理，禁止在本批顺手删除（会破坏迁移等价性口径） |
| **R-05: 血海 More 乘算与 1202 加算项的顺序偏差** | 中 | 原 1207 的 `*= 1.1` 发生在 `BloodSea.cpp:544`（1202 加算与环形乘算之后）；单点收敛后 `*= profile->more_damage_mult` 落在 `:484` 位置，1201 的乘算提前到 1202 加算之前，偏差量级 `0.1 * K * ring`（`K = effective_consumed * bloodthirstEdgePoints * damage_per_point_per_bloodthirst`；`K = 0` 时无偏差） | 在 §5.2 迁移等价性快照中固定“1202 = 0”与“1202 > 0”两组用例，量化并记录该偏差；若团队判定不可接受，则改为在 `:544` 位置单点乘算（代价是 1201 的乘算也会后移，需重新评估等价值） |
| **R-06: 节点 1207 描述文本与运行时不符** | 中 | `desc_key` 明文写「伤害总增 (More) 30%」，冻结值为 10%，UI 谎报约 3 倍 | Task 3.4 同步修正为 10%；若本地化需要独立流程，须在本设计验收标准中登记为显式遗留项并给出跟踪单号，不得默认忽略 |
| **R-07: 既有测试断言未同步导致回归必红** | 高 | `SkillMechanicsRegistryTests.cpp:81-85`、`BloodSeaNodes.cpp:128/140/178`、`HeavenlySwordDescentNodes.cpp:128/178` 断言即将删除的键 | Task 3.3 与 Phase 3 数据删除同一原子提交内完成改造；Phase 7 必须运行全量 `ctest -L unit` 与功能测试，不允许只跑 `*SkillBatch4DeliveryOp*` 过滤后即宣告通过 |
