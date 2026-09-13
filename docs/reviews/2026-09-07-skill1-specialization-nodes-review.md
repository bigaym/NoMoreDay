# 技能1（流云刺）精通节点实现审查报告

- **日期**：2026-09-07
- **审查范围**：技能系统技能1（流云刺，skill_id=1）行为实现、28 个技能精通节点的实现与配置一致性、底层交付及全局契约对齐
- **审查基准**：
  - 权威源头设计文档：`设计文档/职业设计草案_剑修.md`（§3.1 流云刺）
  - 既有配置与契约：`assets/data/skills.json`（talent_tree 28 节点 desc_key/stat_modifiers + 内嵌 skill_contract）、`assets/data/skill_contracts_compact.json`（skill_id=1 条目）、`assets/data/skill_1_tree.json`（树拓扑）
- **实现侧代码**：`src/game/systems/skill/SkillSpecializationBaker.cpp`、`src/game/systems/skill/behaviors/FlowingThrust.cpp`、`src/game/systems/skill/ShadowDuplicationHook.cpp`、`src/game/systems/skill/SkillSystem.cpp`、`src/game/foundation/data/TagRegistry.hpp`
- **结论**：**修改（Block）**
- **版本说明**：
  - 第 1 轮初审：建立 28 节点初步三源对照矩阵，定位 C1~C5、H1~H7、M1~M4 基础缺陷。
  - 第 2 轮深度复核（本次更新）：引入源头《职业设计草案_剑修.md》深入比对并排查底层运行时，定位出**基底 CD=0 逻辑坍塌（C0）**、**契约表源头系统性错位（C6）**、**节点点数与前置系统性崩坏（H8）**、**FlowingThrust.cpp 运行时 Bug 集合（H9）**、**全局 sword_skill 标签大小写失效（H10）**等深层系统性隐患，全面刷新核对矩阵与整改路线。

---

## 1. 审查方法

对技能1的 28 个节点做「四源深度对照」：

| 信息源 | 文件 | 语义 |
|---|---|---|
| **源头设计（GDD）** | `设计文档/职业设计草案_剑修.md` §3.1 | 职业与技能系统纲领性定义（基底CD/充能、定位清单、节点真身与最大点数） |
| **设计描述（desc）** | `assets/data/skills.json` talent_tree[].desc_key | 玩家界面可见的节点效果设计目标 |
| **数据与契约** | `stat_modifiers` 字段 + `skill_contracts_compact.json` | 机器可读的角色（Keystone/Trigger/Synergy/Transmuter/Passive）、resist_model、trigger 参数 |
| **底层代码实现** | Baker 烘焙层 + 行为层 + 交付/状态系统（StatsSystem/充能/Projectile/Buff） | 实际运行效果与数值逻辑 |

核对手段：`rg` 全局检索节点 id 处理分支、StatType 枚举语义比对（`Stats.hpp:224`）、Tag 大小写解析检验（`TagRegistry.hpp`）、Baker/行为层逐行与调用链审查、单测覆盖验证。

## 2. 逐节点核对矩阵（28 节点全表）

判定含义：✅ 符合 ｜ ⚠️ 部分符合/有偏差 ｜ ❌ 不符 ｜ ✖️ 无实现

| 节点 | 名称 | desc 摘要（skills.json） | 契约与设计源头（compact / 草案） | 代码实现现状 | 判定 |
|---|---|---|---|---|---|
| 100 | 迅捷之刃 | 攻击速度 +5%/10%/15%/20%（max 5） | 草案为 (0/4)；契约无 role | Baker:180-182 `del.speed += 40/点`（移速）；StatsSystem 消费 `t16` = **暴击伤害 +5/点** | ❌ 三方不一致（C3 + H8） |
| 101 | 气聚 | 施放时法力消耗降低 15%...45%（max 5） | 草案为 (0/3)；契约无 | Baker:183-185 法力 ×(1-0.15p)；**多出 more ×(1+0.05p)**；5点满降75%超上限 | ❌（H4 + H8） |
| 102 | 剑心洞明 | 基础暴击率 +2%...10%（max 5） | 草案为 (0/5)；契约无 | 全局无任何处理代码 | ✖️（C4） |
| 103 | 流云劲 | 伤害提升 10%...40%（max 4） | 草案为 (0/4)；契约无 | Baker:186-187 `more_damage_mult ×(1+0.10p)`，4 点=+40% | ✅ 符合 |
| 110 | 贯日 | 冷却 -1s，伤害 -15%（max 1） | 草案为 (0/1)；Keystone | Baker:188-191 CD/减伤；多出无限穿透 flag 1；**因基底 CD=0，减 CD 恒为 0（变纯负面节点）** | ❌（C0 + H3） |
| 111 | 连环 | 最大充能 +1/+2，充能时间 +15%（max 3） | 草案为 (0/2)；契约无 | 全局无处理；**因基底 CD=0，充能时间 +15% 乘基数 0 依然是 0** | ✖️（C0 + C4 + H8） |
| 112 | 势如破竹 | 位移距离 +10%...40%；每移动 10 码 More +2%（max 5） | 草案为 (0/4)；契约无 | Baker:192-193 `speed += 30/点`（5 点=+37.5%，档位不齐）；More 增伤未实现 | ❌（H5 + H8） |
| 113 | 风行者 | 疾风状态 2s：移速 +40%、无视体积碰撞、激活御剑步（max 1） | 草案为 (0/1)；Keystone (affects_sword_step) | FlowingThrust:48-50 挂 SwordStep 与 PhaseTag，**但 swift.modifiers 为空且无 StatsDirty，移速加成实际为 0%** | ❌（H9a） |
| 114 | 御风而行 | 御剑步期间近战暴击 +8%...24%、击中回蓝 3...9（max 5） | 草案为 (0/3)；**契约误绑 Trigger（真实归属应为 134）** | 按错误契约驱动写入 TriggerRuleComponent（触发裂空斩），desc 效果零实现 | ❌ 契约严重错位（C6 + H8） |
| 115 | 无止境 | 击杀时 20%...60% 几率回复 1 充能（max 3） | 草案为 (0/3)；契约无 | 全局无处理（对 0 CD 技能回充能毫无意义） | ✖️（C0 + C4） |
| 130 | 留影 | 施放时起点留残影 4s，模仿下一近战技能（40% 伤害）（max 3） | 草案为 (0/1)；**契约误标为 Synergy（真实归属应为 132）** | FlowingThrust:57 `SpawnShadowEcho` 写死 **30% 伤害、1.5s 存活**（非下一技能，仅自身延迟回放） | ❌（H1 + C6 + H8） |
| 131 | 影域 | 残影模仿技能范围 +15%/30%/45%（max 3） | 草案为 (0/3)；契约无 | `stat_modifiers={t31/m1/v15}`，t31=**ProjectileCount**（+15 投射物/点）；残影范围无实现 | ❌（C3） |
| 132 | 影之突袭 | 拥有残影时施放此技能 → 残影也向目标位置发起流云刺（max 5） | 草案为 (0/1) **真实 Synergy 归属**；契约无 | 全局无处理代码（现存残影仅为 0.15s 延迟单次复读，无伴随突刺） | ✖️（C4 + C6 + H8） |
| 133 | 移形换位 | 流云刺变传送（+Teleport/-Movement），起终点物理爆炸（max 1） | 草案为 (0/1)；Keystone | FlowingThrust:86-95 暴击 20%×点数置 `ShadowKillArrayReady`（影杀阵复制孤儿机制）；传送爆炸零实现 | ❌（C5） |
| 134 | 瞬狱影爆 | 爆炸伤害+25%...75%/半径+20%...60%；命中 3+ 敌触发裂空斩（max 5） | 草案为 (0/3) **真实 Trigger 归属**；契约缺失（被误绑给 114） | 全局无处理代码 | ✖️（C4 + C6 + H8） |
| 135 | 虚实相生 | 离开残影区域获得敏捷 200%...600% 护盾 3s（max 3） | 草案为 (0/3)；契约无 | 全局无处理代码 | ✖️（C4） |
| 150 | 要害感知 | 对高生命(>80%)或受控敌人暴击倍率 +15%...60%（max 4） | 草案为 (0/4)；契约无 | 全局无处理代码 | ✖️（C4） |
| 151 | 重创 | 命中 25%...100% 几率造成流血（max 3） | 草案为 (0/4)；契约无 | 全局无处理代码；流血引擎仅技能8使用；desc 为 4 档 vs JSON 3 点矛盾 | ✖️（C4 + H8） |
| 152 | 动脉割裂 | 对流血敌人 75%...225% 几率施加护甲击碎（max 5） | 草案为 (0/3)；**契约误绑 sword_intent=true** | 全局无处理代码；契约键错位（见 C6） | ✖️（C4 + C6 + H8） |
| 153 | 饮血刃 | 击中流血敌人治疗自身流血伤害 100%（max 5） | 草案为 (0/1)；契约无 | 全局无处理代码 | ✖️（C4 + H8） |
| 154 | 孤注一掷 | 移除充能、CD 增至 8s、法力翻倍、必定暴击、More +100%（max 1） | 草案为 (0/1) Keystone；**契约误绑 TypeB_Shred** | 全局无处理代码；契约键错位（见 C6） | ✖️（C4 + C6） |
| 155 | 斩断因果 | 强化版击杀 25%...75% 重置 CD；触发回复 1 层剑意（max 3） | 草案为 (0/3) **真实剑意交互点归属**；契约缺失 | 全局无处理代码；契约键错位（见 C6） | ✖️（C4 + C6） |
| 170 | 劫火 | 物理转火焰 [Fire]；点燃；突进留 2s 燃烧余烬；与凛风互斥（max 5） | 草案为 (0/1) **Transmuter**；契约 Transmuter | Baker 调 `ResolveElementalConversion` 按点数分派（满5点无转换）；点燃与余烬零实现 | ❌（C1 + H8） |
| 171 | 业火焚途 | 余烬持续+0.5s...1.5s/宽度增加/站余烬火伤+6%...18%（max 5） | 草案为 (0/3)；**契约误标为 Transmuter 导致互斥** | 与 170 挤同分支互相覆盖；行为层多出元素护盾且**存在无条件白给 Bug**；desc 三项全无 | ❌（C1 + H2 + H7 + H9b + H8） |
| 172 | 凛风 | 物理转冰霜 [Cold]；减速 30%；冻结额外 50% 伤害；与劫火互斥（max 5） | 草案为 (0/1) **Transmuter**；**契约缺失（未列入 transmuters）** | Baker case 1 无 172 分支；运行时互斥无法承载；全局无处理代码 | ✖️（C2 + C6 + H8） |
| 173 | 霜凝寒骨 | 减速敌人额外施加寒冷；冻结后碎裂溅射（max 5） | 草案为 (0/3)；契约无 | 全局无处理代码 | ✖️（C4 + H8） |
| 174 | 灵根侵蚀 | 命中元素异常施加元素侵蚀，降抗 3...12/层（max 5） | 草案为 (0/4) **真实 TypeB_Shred 归属**；契约缺失 | 全局无处理代码；契约键错位（见 C6） | ✖️（C4 + C6 + H8） |
| 175 | 元素余韵 | 20%...60% 几率将异常传染给 200 码内敌人（max 5） | 草案为 (0/3)；契约无 | 全局无处理代码 | ✖️（C4 + H8） |

**统计汇总**：完全符合 1（103）；部分符合但有严重代码/契约缺陷 5（100、101、112、113、130）；完全不符/逻辑颠倒 4（110、114、170、171）；完全零实现 18（102、111、115、131-135、150-155、172-175）。

## 3. 问题分级清单

### CRITICAL（阻塞级）

#### C0. 流云刺基底数据错误（CD=0 / 充能=1），导致极速流整个分支数学逻辑坍塌
- 位置：`assets/data/skills.json:7-20`（`cooldown: 0.0, charge_count: 1`）
- 源头依据：`设计文档/职业设计草案_剑修.md:117` 明确流云刺基底为 **“初始充能: 2，冷却: 4s”**。
- 致命连锁后果：
  1. 节点 110（贯日）`-1s CD`：`SkillSpecializationBaker.cpp:190` 计算 `max(0.0f, 0.0f - 1.0f) = 0.0f`。基底无 CD 时，减 CD 恒等于 0，贯日直接变成白扣 15% 伤害的**纯负面废节点**！
  2. 节点 111（连环）`最大充能 +1/+2，充能时间 +15%`：因 CD=0，每帧瞬间回满充能，充能时间增加失去基数（`0.0f * 1.15 = 0.0f`），充能成长机制失效。
  3. 节点 115（无止境）`击杀几率回充能`、节点 154（孤注一掷）`移除充能，CD增至 8s` 完全脱离基底设计。

#### C1. `ResolveElementalConversion` 按「点数」而非「节点」决定元素
- 位置：`src/game/systems/skill/behaviors/SkillBehaviorBase.hpp:22-47`
- 函数签名接收 `element_node_id` 但 `(void)element_node_id` 显式丢弃；`switch(points)`：1 点=Fire、2 点=Cold、3 点=Lightning、**4-5 点=default 无转换**。
- 后果：
  - 节点 170 劫火加 1 点转火，加 2 点变冰，加 3 点变电，**加满 5 点反而完全没有转换**；
  - 节点 171 业火焚途在 Baker 中与 170 挤在同分支（`Baker:199`），相互覆盖元素属性；
  - 同一严重错误函数被技能 2/3/5 复用（`Baker:234、261、313`）。

#### C2. 凛风支线（172-175）整线不可用
- 节点 172-175 在 Baker `case 1` 与行为层均无分支（全局代码零命中）。
- `skill_contracts_compact.json` 技能1 `transmuter_node_ids=[170,171]` 遗漏 172，Transmuter 运行时互斥系统（`StatsSystem.cpp:469-479`）无法生效，凛风支线四个节点玩家完全无法使用。

#### C3. stat_modifiers 数据与语义错位（数据错误 + 危险副作用）
- `StatType` 枚举定义（`Stats.hpp:224-265`）：**16=CritDamage**、**31=ProjectileCount**（AttackSpeed=17、AreaScale=32）。
- 节点 100 `stat_modifiers={t16/m1/v5}` 实际给**暴击伤害 +5/点**（StatsSystem 消费），desc 写攻速，Baker 写移速——三方割裂。
- 节点 131 `{t31/m1/v15}` 实际给**投射物数量 +15/点**——desc 是残影范围 15-45%，每点给玩家凭空增加 15 个投射物，存在极大数值隐患。
- 消费链路确认存在：`StatsSystem.cpp:484`、`UISkillTalentTree.cpp:421-422`、`SkillDisplayPreviewService.cpp:76`。

#### C4. 18 个节点零代码实现
- 102、111、115、131-135、150-155、172-175 共 18 个节点在技能底层无有效代码路径（Baker 无分支、行为层无处理、无通用机制承载）。
- 流血引擎（`BuffType::Bleed` + `AilmentEngine.cpp:52`）仅技能 8 使用；151 缺失导致 151→152→153 整条流血链悬空。

#### C5. 节点 133 实现为孤儿机制，传送与爆炸完全缺失
- `FlowingThrust.cpp:86-95`：暴击时几率挂 `ShadowKillArrayReady`，驱动 `ShadowDuplicationHook.cpp` 生成 50% 伤害影杀阵克隆，此机制脱离当前任何设计。
- desc 声明的传送（+Teleport/-Movement）与起终点物理范围爆炸全局无实现。

#### C6. 契约表系统性全盘串位（Trigger / Synergy / Resist / SwordIntent / Transmuter）
对照《职业设计草案_剑修.md》§3.1 的 `[关键节点清单]`，`skill_contracts_compact.json` 与 `skills.json` 内嵌契约存在系统性错乱：
1. **Trigger**：契约绑节点 **114**（御风而行）；设计案明确是 **134（瞬狱影爆）**（触发裂空斩 60% 效力，CD 3s）；114 本是被动暴击回蓝。生成契约时把 134 的触发参数强行塞给了 114。
2. **Synergy**：契约绑节点 **130**（留影）；设计案明确是 **132（影之突袭）**（残影同步流云刺）。
3. **Resist Model**：契约把 `TypeB_Shred` 绑在 **154**（孤注一掷）；设计案明确是 **174（灵根侵蚀）**（元素异常固定值降抗）。
4. **Sword Intent**：契约绑在 **152**（动脉割裂）；设计案明确是 **155（斩断因果）**（强化流云刺击杀重置 CD + 回 1 层剑意）。
5. **Transmuter**：契约设为 `[170, 171]`；设计案明确是 `[170, 172]` 互斥；171 是 170 的子节点，绝非转质。

### HIGH（严重）

| # | 问题 | 证据 / 源码位置 |
|---|---|---|
| H1 | 节点 130 留影数值不符：desc 残影持续 4s / 模仿伤害 40%；代码写死 30% 伤害、1.5s 存活 | `FlowingThrust.cpp:57`；`SkillSystem.cpp:1316-1369` |
| H2 | 节点 171 行为层多出未声明效果：给「元素之体」+8 全抗护盾；desc 的余烬延时/宽度/火伤三项均无实现 | `FlowingThrust.cpp:51-54` |
| H3 | 节点 110 贯日多出未声明效果：无限穿透（flag 1 → pierceCount=999）；desc 只有 CD-1s 与伤害-15% | `Baker:189`；`FlowingThrust.cpp:46,68` |
| H4 | 节点 101 气聚多出独立增伤与超标减耗：多出 `more ×(1+0.05p)`；法力减免 5 点达 75% 超 45% 上限 | `Baker:183-185` |
| H5 | 节点 112 势如破竹数值档位不齐且缺少增伤：speed+30/点（5点=+37.5%）；“每多移动 10 码 More +2%”全无实现 | `Baker:192-193` |
| H6 | 契约键严重错位（见 C6 对照表）：114/130/152/154/171 全盘错位 | `skill_contracts_compact.json:16-39`；`职业设计草案_剑修.md:119-125` |
| H7 | 节点 171 误标为 Transmuter 契约：导致点 170+171 时触发运行时互斥被 StatsSystem 忽略 | `skill_contracts_compact.json:12-15`；`StatsSystem.cpp:469-479` |
| H8 | 节点点数（`max_points`）与前置需求体系性崩坏：170/172 转质在草案为 (0/1) 被机械膨胀为 5 点；130/132/153 机制节点 (0/1) 膨胀为 3/5 点；节点 152 需求 `重创 3/4` 但 151 只有 3 点；110/112/113/153/175 需求上限脱节 | `skills.json`；`职业设计草案_剑修.md:128-167` |
| H9 | `FlowingThrust.cpp` 隐藏运行时 Bug 集合：<br>1. 节点 113 移速加成实际为 0%（`swift.modifiers` 为空且无 StatsDirty）；<br>2. 节点 171 元素护盾只要点 170 就会无条件白给；<br>3. `DirectStrikeComponent` 在投射物上被 `ProjectileSystem:87` 直接跳过（摆设死代码）；<br>4. 基础技能“命中回蓝”丢失，且硬编码削减技能 2 冷却 0.75s；<br>5. 基础流云刺默认 99 次穿透，破坏 110 穿透机制价值 | `FlowingThrust.cpp:48-96`；`ProjectileSystem.cpp:85-89` |
| H10 | 全局标签大小写缺陷：`skills.json` 配置蛇形 `"sword_skill"` vs `TagRegistry.hpp:87` 大驼峰 `"SwordSkill"`，精确匹配导致全局剑系技能无法被识别出 `Tag::SwordSkill` | `skills.json:15`；`TagRegistry.hpp:87,122-127` |

### MEDIUM

| # | 问题 | 说明 |
|---|---|---|
| M1 | 契约与描述冲突根因 | 契约 114 挂载 Trigger 纯属将 134 瞬狱影爆数据嫁接错误的产物，需回归设计草案复位 |
| M2 | 残影重放机制差异 | `SpawnShadowEcho` 仅为延迟 0.15s 单次复读自身，并非真正模仿下一近战攻击的驻场残影 |
| M3 | 单测覆盖几乎为零 | `SkillSpecializationBakerTests.cpp`（319 行）技能1 仅测 node 100 移速（:97），无核心机制数值断言 |
| M4 | 缺少数据驱动标签转化字段 | 底层 `SkillDefs.hpp:387` 和 `SkillSystem.cpp:2115` 原本已支持在天赋节点 JSON 中配置 `add_tags` 与 `remove_tags`，但 `skills.json` 全局节点均未提供这组字段，逼迫代码层走散落的硬编码分派 |

## 4. JSON 配置与设计源头矛盾清单

1. **基底属性矛盾**：`skills.json` 写死 `cooldown=0.0 / charge_count=1`；设计草案明确指定为 `初始充能: 2，冷却: 4s`，导致整个冷却缩减与充能增长分支直接失去数学意义。
2. **节点点数与档位矛盾**：
   - 节点 100：desc「攻速 5/10/15/20%」为 4 档，`max_points=5`（草案为 4 点）；
   - 节点 101：desc「15%...45%」为 3 档，`max_points=5`（草案为 3 点）；
   - 节点 151：desc「25%...100%」为 4 档，`max_points=3`（草案为 4 点）；
   - 节点 170 / 172：转质节点在设计案中均为单点 `(0/1)`，JSON 机械设为 5 点，直接诱导了按点数决定元素的错误代码实现；
   - 节点 130 / 132 / 153：单点机制节点被机械膨胀为 3 点或 5 点，但并未配置逐点递增数值。
3. **前置依赖文本与数据矛盾**：
   - 节点 152 描述写 `*{需求: 重创 3/4}*`，但重创（151）在 JSON 中 `max_points: 3`，玩家根本无法点到 4 点；
   - 节点 110（`气聚 2/3` vs 101 max=5）、112（`迅捷之刃 2/4` vs 100 max=5）、113（`连环 2/2` vs 111 max=3）、153（`动脉割裂 2/3` vs 152 max=5）、175（`灵根侵蚀 2/4` vs 174 max=5）的描述需求档位与父节点实际点数上限全面脱节。
4. **契约全盘错位**：114(Trigger)、130(Synergy)、154(Shred)、152(SwordIntent)、171(Transmuter) 五大核心契约全面偏离源头草案设计。
5. **标签命名风格脱节**：`skills.json` 使用蛇形 `"sword_skill"`，而引擎底层注册大驼峰 `"SwordSkill"`，且未做大小写容错。

## 5. 架构偏差（对照设计文档 §8 UMR 约束）

- `assets/data/modifier_v2/skill_spec_modifiers.json` 全库仅 1 条记录（skill 2/node 213 HeavyMomentum），**技能1 零记录**；
- 技能1 的 28 节点效果全部走 Baker 硬编码 `switch(skill_id) + if(node_id)` 与行为层散点实现——正是设计文档 §8 明令禁止的「在适配器中写死节点 id、倍率或平铺值常量」；
- `ApplyNodeModifiersToProfile`（Baker:173-409）单函数承担全部 12 个技能的节点分派，236 行巨型 switch，随节点增长持续膨胀；
- 节点效果三处散落（Baker 数值/行为层机制/StatsSystem 数据），与「modifier_v2 数据定义 + runtime record 过滤执行」的目标形态相悖。

## 6. 符合项（确认有效）

1. **节点 103 流云劲**：`more_damage_mult ×(1+0.10p)`，4 点=+40%，与 desc 完全一致（Baker:186-187）；
2. **节点 114 触发链路**：契约数据驱动 → `TriggerRuleComponent`（rule_id=114、OnSkillHit、eff 0.6、ICD 3.0、TargetPolicy=Victim），Bake（Baker:115-126）与 SyncTriggerRules（Baker:411-452）双路径正确；
3. **节点 113**：SwordStep buff 2s + PhaseTag（无视体积碰撞）与 desc 主体一致（移速待核 buff 表）；
4. **节点 130 框架**：残影生成/重放框架存在且接线正确（数值偏差见 H1）；
5. **拓扑与契约结构**：28 节点数与契约 `min_nodes=max_nodes=28` 一致；`skill_1_tree.json` 与 `skills.json` 的坐标/前置/名称一致；
6. **转质互斥基础设施**：StatsSystem 的 Transmuter 运行时互斥、Keystone 排除、ScopePolicy 过滤、cost_affix 应用均已实现且被技能1 数据路径覆盖（虽然技能1 自身数据尚未正确使用）。

## 7. 建议整改实施顺序（五阶段递进路线）

1. **第一阶段：校准数据源与契约（基准归位，前置阻塞）**
   - **修正技能基底**（`skills.json`）：依据《职业设计草案_剑修.md》将流云刺基底修正为 `cooldown: 4.0, charge_count: 2`；修正标签命名为 PascalCase 大驼峰 `"SwordSkill"`（彻底解决 C0、H10 根源缺陷）。
   - **修正 28 节点点数与前置**（`skills.json`）：将 170、172、130、132、153 归位为 1 点机制节点；统一 151 为 4 档；纠正前置点数需求与描述的档位矛盾（彻底消除 H8）。
   - **纠正契约表全盘串位**（`skill_contracts_compact.json` 及内嵌契约）：
     - Trigger 挂载至 **134（瞬狱影爆）**；
     - Synergy 挂载至 **132（影之突袭）**；
     - `TypeB_Shred` 挂载至 **174（灵根侵蚀）**；
     - `sword_intent` 挂载至 **155（斩断因果）**；
     - 转质互斥组修正为 `[170, 172]`，移出 171（彻底消除 C6、H7）。
   - **修复 stat_modifiers 枚举错位**：节点 100 改为 `t17 (AttackSpeed)` 或交由代码/UMR 承载；节点 131 改为 `t32 (AreaScale)` 并修正数值（彻底消除 C3）。
   - **补齐数据驱动标签**：在节点 170/172 补全 `"add_tags": ["Fire"]`/`["Cold"]` 与 `"remove_tags": ["Physical"]`（激活底层数据驱动链路，M4）。

2. **第二阶段：重构 Baker 与底层通用分派**
   - 废除 `ResolveElementalConversion` 的 points 分派逻辑，改为按 `node_id` 分派或直接消费节点 `add_tags`（消除 C1）。
   - 彻底梳理 `Baker:178-205` 的 `case 1` 分支，拆除硬编码无限穿透（110）与非法 more 增伤（101）。

3. **第三阶段：修复行为层运行时逻辑与代码漏洞（`FlowingThrust.cpp`）**
   - 补齐节点 113 移速加成（规范调用 `seven_star_shared::GrantSwordStep(..., 40.0f)`）（消除 H9a）；
   - 修复节点 171 元素护盾泄漏漏洞，增加对 171 点数的强门控（消除 H9b）；
   - 拆除投射物实体上的摆设 `DirectStrikeComponent` 死代码（消除 H9c）；
   - 补齐基础流云刺命中回蓝，清理硬编码削减技能 2 CD 的私有特例（消除 H9d）；
   - 修复基础投射物默认 99 次穿透，仅在专精支持时启用穿透（消除 H9e）。

4. **第四阶段：落实未实装节点与 UMR 迁移**
   - 按《职业设计草案_剑修.md》逐步实装 133 传送与起终点爆炸、134 裂空斩触发、150-155 流血与处决链条、172-175 凛风与元素侵蚀；
   - 拆除 133 影杀阵孤儿机制与 `ShadowDuplicationHook.cpp` 特例（消除 C5）；
   - 将数值节点效果逐步并轨进 `modifier_v2`（§8 UMR 合规）。

5. **第五阶段：自动化测试护栏与回归**
   - 编写 `SkillSpecializationBakerTests.cpp` 单元测试，覆盖 28 节点烘焙数值、基底 4s CD 缩减、Trigger 触发、转质互斥与移速 Buff 检验；
   - 运行 `build.bat`（RelWithDebInfo）验证零警告 + `ctest` 相关套件全绿。

---

## 附：本次审查使用的证据检索命令摘要

- `rg -n --glob "*.cpp" "ApplyNodeModifiersToProfile|SkillSpecializationBaker::" src/` → Baker 实现定位
- `rg -n "node_id == 154|== 155\b|== 111\b|== 115\b|..." src/` → 零实现节点确认（仅 InventorySystem 的物品 id 102 误中，非节点）
- `rg -n --glob "*.cpp" --glob "*.hpp" "stat_modifiers" src/` → 数据消费链路（StatsSystem/UI/Preview）
- `rg -n "enum class StatType" -A 45 src/game/foundation/components/Stats.hpp` → t16/t31 语义确认
- `rg -n -i "bleed|流血" src/game/` → 流血引擎归属确认（仅技能8）
- `rg -n "charge_count|max_charges|charges" src/game/systems/skill/ src/game/contracts/` → 充能通用机制确认
- `rg -n "Tag::SwordSkill|sword_skill" src/ assets/` → 标签大小写断层排查
- `python scratch/check_prereqs.py` → 28 节点描述与前置点数全量对照
- `view_file 设计文档/职业设计草案_剑修.md:115-180` → 权威设计基底与关键节点清单确认

---

# 第 3 轮：修复跟进审查（2026-09-07）

- **审查范围**：针对第 2 轮问题清单的整改变更（+1224/-166，17 个文件：数据契约 ×3、核心代码 ×6、测试 ×8）。
- **结论**：**修改**（较第 2 轮「修改 Block」大幅收窄：上轮系统性三层错位已修复，本轮遗留 1 项 High 功能缺口 + 4 项 Medium，剩余工作量集中于传送爆炸伤害路径一处）。

## 3.1 验证证据

- `build.bat`（RelWithDebInfo）构建成功，含 asset 校验与 render ABI 治理；
- `ctest` 相关 8 套件全绿：`nmd.tests.unit` / `integration` / `skill.unit` / `skill.integration` / `ci.nonperf` / `combat.unit` / `combat.integration` / `combat.parity.unit`；
- 新增测试约 474 行（`tests/unit/SkillSpecializationBakerTests.cpp`），覆盖基底充能/CD 时序、节点 101/102/110/111/112/113/130/133/134/154/170/172 烘焙断言、`ResolveElementalConversion` 全技能映射、节点 150 伤害公式（231/165/231）、节点 174 叠层上限、节点 155 击杀重置。

## 3.2 上轮问题修复确认

| 上轮编号 | 修复情况 | 证据 |
|---|---|---|
| C0 基底 CD=0/充能 1 | ✅ | `skills.json` `cooldown:4.0, charge_count:2`；充能时序测试 |
| C1 转质按 points 分派 | ✅ | `SkillBehaviorBase.hpp:24-58` 改为 node_id 分派，顺带修复 272(原 250 typo)，保留 legacy fallback |
| C2 关键节点清单错位 | ✅ | compact 契约、fixture、helpers 三处同步为 {113,132,134,155,170,172} |
| C3 stat_modifiers 枚举错位 | ✅ | 节点 100→t17(AttackSpeed)、131→t32(AreaScale)，Baker 对应分支改由数据承载 |
| C4 命中时效果缺失 | ✅（主体） | `DamagePipeline.cpp:1003-1046` 节点 150 重做；`FlowingThrust.cpp` DoHit 全面重写实装 151/152/153/170/172/173/174/175/115/155 |
| C5 Trigger 串位 | ✅ | 契约 Trigger 挂 134（60% 效力、ICD 3s），guard/integration 测试同步 114→134 |
| C6 resist_model/scope 串位 | ✅（数据层） | `TypeB_Shred` 154→174；转质互斥组 [170,172] |
| H6 无限穿透 | ✅ | Baker 删除 flag 1；`proj.max_pierce = 99`（基础「路径全体命中」语义，110 的 kUnlimitedPiercing 已拆除） |
| H8 点数上限膨胀 | ✅ | 17 个节点 max_points 全表校准（100:4、101:3、111:2、130/132/153/170/172:1 等） |
| H9a 113 移速 | ✅ | `seven_star_shared::GrantSwordStep(..., 2.0f, 40.0f)` |
| H9b 171 护盾泄漏 | ✅ | 旧 ElementBody ResistAll buff 删除，171 改 FireDamage buff 且有 `infernalPoints > 0` 门控 |
| H9c 投射物摆设 DirectStrike | ✅ | 投影路径不再 emplace DirectStrikeComponent |
| H9d 回蓝/CD 特例 | ◐ | 基础回蓝 3.0 已补；但「命中削减技能 2 CD 0.75s」特例保留（现注释为剑意资源链路，需设计确认） |
| H10 标签大小写断层 | ✅ | `"SwordSkill"` 大驼峰 + `TagFromString` 兼容别名 |
| M4 标签数据驱动 | ✅ | 133 `add_tags:[Teleport]/remove_tags:[Movement]`，170/172 Fire/Cold；新增 `Tag::Teleport = 1ULL << 23` |

附带修复：`dir` 零向量 NaN 保护；`Momentum=112/PrisonSlash=133` 常量错位修正（旧代码 114 误标 Momentum）；DoHit 残影归属解析（`SummonComponent→owner`）；充能系统全链路（`BakedSkillProfile.effective_charges` 贯穿 Bake/UpdateCooldowns/TryCast/115，含 110 贯 CD 参与充能回填）。

## 3.3 本轮新发现

### HIGH-1：133 传送爆炸伤害路径断裂——134 增伤与元素转换对爆炸不生效

- `FlowingThrust.cpp` DoCast 的 `spawnExplosion` 将 `more_damage_mult` 乘入 burstEnt 的 `CombatStats` 快照（`snap.damage_multipliers *= moreDamageMult`）并修正 `crit_chance`，但 **`ProjectileSystem.cpp:113-127` 的 DirectStrike 伤害路径读取的是 `registry.get<CombatStats>(ds.owner)`（owner 实时值）**，快照成为死数据；
- `ProjectileSystem.cpp:119` `pool.Add(Tag::Physical, baseDmg)` 硬编码物理池，且请求未设置 `payload_context` → `DamagePipeline.cpp:562-565` 的元素 tags 与 `:885` 的 `payload more` 均不生效；
- 净效果：爆炸仅 `ds.radius = 35 × area_radius` 生效；**134「传送爆炸伤害 +25...75%」、103/112/154 的 more、170/172 元素转换对爆炸全部丢失**；
- 测试缺口：无爆炸伤害数值断言（现有测试仅覆盖 profile 字段与充能时序）；
- 修复方向：DirectStrike 路径的 `DamageRequest` 补充 `payload_context`（more/tags/crit），或 `ProjectileSystem` 优先消费 strike 实体上的 CombatStats 快照。

### MEDIUM-1：114 御风而行暴击加成条件丢失、回蓝数值偏移

- desc「处于御剑步状态期间，近战攻击获得 8%...24% 额外暴击率」——Baker 烘焙为**无条件常驻** `del.bonus_crit += 8p`，御剑步（PhaseTag）条件丢失；
- 回蓝实现 `3.0 + 3p`（1/2/3 点 = 6/9/12），超出 desc 的 3...9 区间（实现者将基础回蓝并入 114 区间）。

### MEDIUM-2：174 SkillOnly scope 契约与运行时不符

- 契约 `scope_policies "174": "SkillOnly"`，但元素侵蚀 debuff 是目标身上的全局 `ResistFire/ResistCold` Flat 减抗，**任何技能**对其伤害均受益；`BuffEffect` 无来源技能归属字段，`DamagePipeline` 的 `can_apply_scope`（:569-584）仅过滤 attacker 树节点 stat modifier，不覆盖 victim debuff——实际效果强于设计。

### MEDIUM-3：内嵌契约 role 标注错误（130/153 = Keystone）

- `skills.json` 内嵌契约中 130（留影）、153（饮血刃）标注 `role: "Keystone"`，草案中 Keystone 仅为 113/154；与 compact 契约 key_nodes（不含 130/153）互相矛盾；
- 影响限于视觉/UI 层（`BuildSkillVfxNodeRoleMask` 置 Keystone VFX 位、`UISkillSpecRenderer::ClassifyNodeVisual` 分类显示），无战斗逻辑影响，但属于数据标注错误，应归位为 Passive（或空 role）。

### MEDIUM-4：desc 与实现的机制偏差（降级/简化实装，需设计侧确认接受度）

| 节点 | desc | 实现 |
|---|---|---|
| 170/171 | 突进路径留 2s 燃烧余烬；站在余烬上获火伤加成 | 无余烬实体生成；171 简化为施放时无条件 AttackUp buff（FireDamage +6p、2+0.5s） |
| 172 | 对已被冻结的敌人 +50% 伤害 | 未实现 |
| 173 | 寒冷叠满冻结后 15...45% 几率碎裂（上次暴击 30% 冰霜溅射） | 仅实装叠寒冷，碎裂未实现 |
| 153 | 治疗相当于流血伤害的 100% | 固定 +20 回血 |
| 175 | 每次传染有 1s 冷却 | ICD 未实现 |
| 112 | 每多移动 10 码该次伤害 More +2% | 静态化 `more ×(1+3p)`（点数驱动，与位移无关） |
| 135 | 离开残影区域时获得护盾 | 施放留影时立即获得（敏捷×2、3s），且经 `exec.active_nodes` 而非 profile flags 读取（来源与其余节点不一致） |

### LOW-1：`ResolveElementalConversion` 节点 570 无显式 case

570 依赖 `points==1 → Fire` 的 legacy fallback 命中，功能正确但依赖 points 编码约定；若后续 570 调整为非 Fire 元素将静默出错。建议补显式 case。

### LOW-2：DoHit 充能上限 fallback 硬编码

115/155 分支中 `maxCharges` fallback `: 2`（FlowingThrust.cpp DoHit 击杀段）与 `skills.json` `charge_count` 隐性耦合，基底调整时需双处同步。

## 3.4 遗留风险（上轮 §7 未完成阶段）

- **阶段五 UMR 并轨未启动**：`assets/data/modifier_v2/skill_spec_modifiers.json` 本次未变更，技能1 仍零记录，28 节点效果仍走 Baker 硬编码分派（§5 架构偏差维持原状）；
- **C5 残余**：`ShadowDuplicationHook.cpp` 特例与 `TryCast` 的 `ExecutePreCastShadowDuplication` 保留（旧 133 影杀阵孤儿机制已随 PrisonSlash 删除）；
- **H9d 残余**：命中削减技能 2 CD 0.75s 的私有特例保留，需产品侧确认是否为剑修资源系统设计意图。

## 3.5 第 3 轮证据检索命令摘要

- `rg -n "SkillOnly|scope_policy" src/game/contracts/impl/StatsSystem.cpp src/game/systems/combat/DamagePipeline.cpp src/game/systems/skill/SkillSystem.cpp` → scope 过滤消费端确认
- `rg -n "SpecNodeRole::Keystone" src/` → Keystone 消费端（仅 UI/VFX 层）确认
- `rg -n "DirectStrike" src/game/systems/skill/ProjectileSystem.cpp` → 爆炸伤害路径确认（Physical 硬编码 @ :119）
- `rg -n "attacker_stats\s*=|payload_context|GetBakedSkillProfile" src/game/systems/combat/DamagePipeline.cpp` → 快照/上下文消费确认（:619 owner 实时值；:267 仅 parent_skill_cd）
- `ctest --test-dir build -C RelWithDebInfo -R "nmd.tests.(skill.unit|skill.integration|unit|integration|combat.*|ci.nonperf)$"` → 8 套件全绿

## 3.6 整改建议（收窄范围）

1. **必须**（本轮结论依据）：修复 HIGH-1——DirectStrike 爆炸请求携带 `payload_context`（more/tags/crit）或消费 strike 快照，并补充爆炸伤害数值测试（含 134 增伤与 170/172 元素转换两断言）；
2. **建议同批**（低成本数据修正）：MEDIUM-3（130/153 role 归位）；
3. **建议排期**：MEDIUM-1（114 条件化 + 数值对齐）、MEDIUM-2（debuff 来源归属或契约改注）、MEDIUM-4 逐项与设计侧确认接受度后关闭或实装；
4. **后续阶段**：UMR 并轨（§7 阶段五）维持原路线不变。

---

# 第 4 轮：修复复审（2026-09-07）

- **审查范围**：第 3 轮 HIGH-1 + MEDIUM-3 的整改变更（9 个文件：组件 ×1、核心代码 ×3、数据 ×2、测试 ×3；累计 diff +1360/-177）。
- **结论**：**提交**（两项整改均落地且独立验证通过；新发现 1 项 Medium 属既有缺陷、非本批引入，列入遗留排期，不阻塞本批）。

## 4.1 整改确认

| 上轮编号 | 修复情况 | 证据 |
|---|---|---|
| HIGH-1 爆炸伤害路径断裂 | ✅ | 见 §4.1.1 分层核实 |
| MEDIUM-3 130/153 role 误标 | ✅ | `skills.json` 130/153 `role: Keystone→Passive`（经 `scripts/gen_skill_contracts.py` 重新生成）；compact 契约 skill 1 新增 `passive_node_ids:[130,153]`；矩阵测试 helpers 期望清单与 fixture 同步为 `{113,130,132,134,153,155,170,172}` |

### 4.1.1 HIGH-1 修复分层核实

1. **载体**：`DeliveryArchetypes.hpp` `DirectStrikeComponent` 新增 `bool has_payload` + `DamagePayloadContext payload_context`；`static_assert`（standard_layout / trivially_destructible）保持成立，渲染 ABI 治理通过；
2. **填充**：`FlowingThrust.cpp` `spawnExplosion` 删除死数据 `CombatStats` 快照，改填 `payload_context`：`base_damage_min/max` 取 owner 武器伤害、`crit_chance` 做 `/100` 归一化、`more_damage` 为 134 增伤乘数、`effective_tags` 取 profile 有效标签——第 3 轮指出的三处断层（快照死数据、Physical 硬编码、无 payload_context）全部覆盖；
3. **消费**：`ProjectileSystem.cpp:113-127` 在 `has_payload` 时将 `base_pool` 置空并透传 payload_context，否则回退原 owner 实时 stats 旧行为（非爆炸投射物零影响）；
4. **第二根因（子代理主动发现，复审确认属实）**：`DamagePipeline.cpp:468-495` 在 `base_pool` 存在正值时非模拟路径走 `CombatV2RuntimeFacade.Execute` 并提前 return，legacy payload 消费点（`:884-885`）不执行——**空池是 payload 生效前提**，仅填 payload_context 不清池仍会断裂；该根因已随第 3 步的置空一并修复；
5. **标签贯通**：`DamagePipeline.cpp` 事件派发处 `combined_hit_tags |= payload_context->effective_tags`，保证 `DoHit`（170/172 元素转换、异常应用）拿到元素。

### 4.1.2 MEDIUM-3 修复核实

- 根源确认为生成器默认推导：`scripts/gen_skill_contracts.py:418-427` 对 `max_points==1` 节点默认推导 Keystone，130/153 需 `passive_node_ids` 显式声明归位——修复方式正确（数据经生成器再生成，非手改产物）；
- 矩阵测试期望清单扩容（+130/+153）与 `LoadCompactContractBuckets`（`tests/SkillKeyNodeMatrixTestHelpers.hpp:168-269`，`passive_node_ids` 消费段为既有代码）的契约汇聚逻辑一致；key_nodes 语义从「六大类节点」扩展为「契约显式声明的特殊节点」，属结构必然，可接受。

## 4.2 新增测试质量

`tests/unit/SkillSpecializationBakerTests.cpp:853-971` 新增 `[Unit] FlowingThrust - 133 Swap Explosion Damage Payload`：

- **完整链路**（`TryCast` → `SkillSystem::Update` 触发 DoCast → `ProjectileSystem::Update` 结算），非直调内部函数，测试有效性高；
- 三断言齐备：134 爆炸增伤精确 ×1.25（基线对比、epsilon 0.02）；170 命中事件携带 Fire 标签且目标获得 Ignite（Burn）；172 携带 Cold 标签——即第 3 轮 §3.6 要求的全部测试项；
- 隔离手段得当：`runtime.trigger_cooldowns[134]=999.0f` 压住触发派生伤害（避免污染爆炸基线）、`crit_chance=0` 关暴击保证数值可精确对比、重置玩家位置消除起点/终点双爆炸叠加伪影。

## 4.3 验证证据（复审独立重跑）

- `build.bat`（RelWithDebInfo）构建成功（BUILD_EXIT=0）；
- `ctest` 7 套件独立复跑全绿：`unit` / `integration` / `skill.unit` / `skill.integration` / `combat.unit` / `combat.integration` / `combat.parity.unit`（10.46s，0 失败）。

## 4.4 本轮新发现

### MEDIUM-5：payload crit_chance 单位不一致——6 处填充点未归一化（既有缺陷，非本批引入）

- 消费端约定：`DamagePipeline.cpp:1052-1053` 将 `payload_context->crit_chance` 视为归一化小数（`[0,1]`，`×100` 还原）；
- 填充点全景（~~全部传 `stats->crit_chance` 百分数原值，仅本批爆炸路径正确 `/100`~~——**旧单位措辞，已作废，见本节末更正**）：
  - `BeamChannelDeliverySystem.cpp:106`（百分数）；
  - `BeamChannelDeliverySystem.cpp:420`（**混合单位**：`stats->crit_chance + (chan.bonus_crit_chance / 100.0f)`——bonus 归一化了、stats 没有）；
  - `ShadowDuplicationHook.cpp:72`（百分数）；
  - `SkillSystem.cpp:1282 / 1346 / 1776`（百分数，存入 snapshot，消费链未逐行核实）；
- **当前未爆发的掩盖机制**：`DamagePipeline.cpp:468-495` 在 `base_pool` 存在正值时走 CombatV2 提前 return，legacy payload 消费段不执行；`BeamChannelDeliverySystem.cpp:144/146` 确认带 Physical 池 → bug 潜伏；
- 本批爆炸路径是**首个 payload-only 消费者**（`base_pool` 置空），故必须归一化（已正确）；
- 风险：任何后续把现有 payload 请求改为 payload-only 的改动都会引爆（crit_chance 被 clamp 到 100% = 必定暴击）；
- 修复建议：统一在填充侧归一化（6 处逐一修正），并补一条 payload crit 单位守卫测试；建议随 MEDIUM-1 一并排期。

> **更正（2026-09-13，B1-18 收口；commit `dbda488d`）**：以上单位前提**已作废**——`CombatStats::crit_chance` 与 `payload_context->crit_chance` 均为**归一化分数 `[0,1]`**，并非百分数，消费端不以 `×100` 还原。现行口径：`Stats.hpp:115` 默认 `0.05f`；`SkillSpecializationBaker.cpp:234-235`（`bonus_crit` 归一到 0..1）；填充侧直接传 `stats->crit_chance`，见 `BeamChannelDeliverySystem.cpp:376/:682/:1232`、`RendingWave.cpp:221`、`FlowingThrust.cpp:328-330`（注释「payload crit_chance 与 CombatStats 统一为分数制 [0,1]」）；消费侧 `DamagePipeline.cpp:1851-1855`（`snap.crit_chance >= 1.0f` 必暴）。因此 §5.1.1 所列 `/100.0f` 修法亦随之作废；`riding_wind_bonus_crit` 现为分数（`SkillSpecializationBakerTests.cpp:396` 断言 3 点 = `0.24f`，非 24）。本节与 §5.1.1 仅作历史记录保留，实际口径以本更正为准；无源码改动。

### 附带说明（非缺陷）

- 爆炸数值基线变化：旧死快照路径下 CombatV2 结算 ≈35，修复后走 legacy 公式 ≈52（10 + 35×1.2），与投射物伤害口径对齐，属修复意图内变化；
- `has_payload=false` 回退路径完整保留旧行为，其他 DirectStrike 使用方（若有）零影响；
- Slow/Chill 异常对爆炸目标不实际应用，受 `ailment_contracts.json` 契约注册限制（既有机制，与 §3.3 MEDIUM-2 同源）。

## 4.5 遗留清单（维持第 3 轮排期建议）

- MEDIUM-1（114 御剑步条件 + 回蓝数值）、MEDIUM-2（174 debuff 来源归属）、MEDIUM-4（desc 偏差表逐项与设计侧确认）、LOW-1（570 显式 case）、LOW-2（DoHit 充能上限 fallback）——本批未处理，维持原建议；
- MEDIUM-5（新增）加入排期；
- 架构级遗留维持：UMR 并轨未启动、`ShadowDuplicationHook` 特例、命中削减技能 2 CD 0.75s 特例待设计确认。

## 4.6 第 4 轮证据检索命令摘要

- `rg -n "has_payload|payload_context" src/game/systems/skill/ProjectileSystem.cpp src/game/systems/skill/behaviors/FlowingThrust.cpp src/game/foundation/components/DeliveryArchetypes.hpp` → 载体/填充/消费三段核实
- `rg -n "has_candidate_runtime_base|CombatV2RuntimeFacade" src/game/systems/combat/DamagePipeline.cpp` → 第二根因核实（:468-495 提前 return）
- `rg -n "keystone|passive|max_points" scripts/gen_skill_contracts.py` → 生成器默认推导规则核实（:418-427）
- `rg -n "passive_node_ids" tests/SkillKeyNodeMatrixTestHelpers.hpp assets/data/skill_contracts_compact.json` → 契约汇聚与期望清单一致性核实
- `ctest --test-dir build -C RelWithDebInfo -R "nmd.tests.(skill.unit|skill.integration|unit|integration|combat.unit|combat.integration|combat.parity.unit)$"` → 7 套件独立复跑全绿

---

# 第 5 轮：遗留整改复审（2026-09-07）

- **审查范围**：第 4 轮遗留清单中「无需设计决策」项的整改——MEDIUM-5（payload crit 归一化统一）、MEDIUM-1（114 条件化；数值部分核实后不改）、LOW-1（570 显式 case）、LOW-2（充能 fallback 去硬编码）。MEDIUM-2/4、H9d 残余需设计决策，未在本批。
- **结论**：**提交**（复审发现并补修 1 处漏改的活跃点 `FlowingThrust.cpp:253`，其余全部确认落地）。

## 5.1 整改确认

| 遗留编号 | 修复情况 | 证据 |
|---|---|---|
| MEDIUM-5 payload crit 归一化 | ✅（9 处活跃点全修） | 详见 §5.1.1 |
| MEDIUM-1 114 条件化 | ✅ | `SkillDefs.hpp:565` 新增 `BakedSkillProfile.riding_wind_bonus_crit`（百分数语义）；`SkillSpecializationBaker.cpp:209` 从 `del.bonus_crit` 无条件注入改为 profile 字段记录；`FlowingThrust.cpp:102-104` 以 `any_of<PhaseTag>(owner)`（与 113 GrantSwordStep 同一状态源）条件读取，爆炸路径 `:194-197` 与投射物路径 `:253` 注入当次交付；测试 `SkillSpecializationBakerTests.cpp:476-488` 断言未烘焙进 `delivery.bonus_crit` + 字段值 24（3 点） |
| MEDIUM-1 回蓝数值 | ✅（核实后不改） | 设计文档 `设计文档/职业设计草案_剑修.md:139` 原文：「…且每次近战击中回复 3...9 点法力」——114 额外提供 3/6/9（每点 3），基底回蓝 3.0 为技能 1 自身效果（第 3 轮已确认），`3.0 + 3p` 实现与 desc 一致；且回蓝已按 `any_of<PhaseTag>` 条件化（`FlowingThrust.cpp:344`），与「处于御剑步状态期间」的 desc 修饰范围一致 |
| LOW-1 570 显式 case | ✅ | `SkillBehaviorBase.hpp:32` 新增 `case 570:` 返回 `Tag::Fire`（skills.json 570 天火流星固定 Fire，与点数无关）；连带修正编码了 legacy 错误行为的契约测试 `SkillBehaviorGuardTests.cpp:389/404`（原断言 570 二点→Cold，实为 points-fallback 残骸，改为一点→Fire） |
| LOW-2 充能 fallback | ✅ | `FlowingThrust.cpp:545-549`：profile 无效时改经 `SkillRegistry::Get().GetSkill(kSkillId)` 读 `max_charges`，字面量 2 退化为理论兜底并注释耦合来源 |

### 5.1.1 MEDIUM-5 修复明细与「死数据」判定

**活跃修复 8 处**（子代理修 7 处 + 复审补修 1 处）：

| 位置 | 修法 |
|---|---|
| `BeamChannelDeliverySystem.cpp:106` | `stats->crit_chance / 100.0f` |
| `BeamChannelDeliverySystem.cpp:420` | 混合单位修正：`(stats->crit_chance + chan.bonus_crit_chance) / 100.0f` |
| `BeamChannelDeliverySystem.cpp:573` | 同 420（第 4 轮报告漏列，子代理补发现） |
| `BladeBoomerang.cpp:70` | `/100.0f`（第 4 轮报告漏列） |
| `RendingWave.cpp:74` | `/100.0f`（第 4 轮报告漏列） |
| `FlowingThrust.cpp:253` 投射物路径 | `/100.0f`——**复审补修**：子代理遗漏本处；经 `ProjectileSystem.cpp:787`（`DamagePool base` 空池构造）核实投射物命中请求 `has_candidate_runtime_base=false`，走 legacy payload 段被 `×100` 活跃消费，属必暴级活跃 bug 而非潜伏 |
| `DamagePipeline.cpp:1052-1053` | 注释补充「填充方必须 /100 归一化」约定 |

**维持原样 4 处（死数据判定成立）**：`SkillSystem.cpp:1287/:1351/:1782`、`ShadowDuplicationHook.cpp:77` 写入的均为 `snapshot.payload_context`，全 src 检索确认无任何读取点（行为层 `FlowingThrust.cpp` 交付时以实时 stats 重建 payload，不读快照字段），该链不会到达 `DamagePipeline:1053` 消费段。维持「记录性快照」语义；若未来有消费方接入，须先归一化。

> **更正（2026-09-13）**：本节 §5.1.1 及 §4.4 MEDIUM-5 的「百分数」措辞与 `/100.0f` 归一化修法均为旧单位前提，已作废；crit 单位统一为分数 `[0,1]`（消费端不 `×100`），当前填充侧直接传 `stats->crit_chance`。详见 §4.4 MEDIUM-5 末更正。

## 5.2 复审验证（独立重跑）

- `build.bat` 构建成功（BUILD_EXIT=0）；
- `ctest` 7 套件全绿（10.42s，0 失败）——含复审补修 `:253` 后的最终代码；
- `riding_wind_bonus_crit` 全链核实：定义（SkillDefs.hpp:565）→ 烘焙（Baker:209）→ 条件读取（FlowingThrust:104）→ 注入（:194/:253，两处均随百分数总和统一 `/100`）→ 测试断言（:487）；`delivery.bonus_crit` 无其他消费点，无遗漏路径。

## 5.3 需设计决策的遗留项（未在本批处理，待产品/设计侧表态）

1. **MEDIUM-2**（174 SkillOnly scope）：方案 A——`BuffEffect` 增加 `source_skill_id` 归属字段（数据结构变更，波及 buff 叠加/序列化）；方案 B——契约改注承认全局减抗语义。二选一需设计确认意图。
2. **MEDIUM-4** desc 偏差 8 项（§3.3 表）：170/171 余烬、172 冻结增伤、173 碎裂、153 治疗量、175 ICD、112 移动增伤、135 护盾时机与读取路径——逐项决定「降级接受（改 desc）」或「补实装（开实现任务）」。
3. **H9d 残余**：命中削减技能 2 CD 0.75s 私有特例是否为剑意资源链路设计意图。
4. **UMR 并轨**（阶段五）：路线不变，启动时机由排期决定。

## 5.4 第 5 轮证据检索命令摘要

- `rg -n "snapshot.payload_context" src/` → 死数据判定（仅 4 处写入、零读取）
- `rg -n -A12 "DamagePool base;" src/game/systems/skill/ProjectileSystem.cpp` → 投射物请求空池核实（:787，活跃消费链判定依据）
- `rg -n "delivery\.bonus_crit|riding_wind_bonus_crit" src/ tests/` → 114 全链与无遗漏路径核实
- `rg -n "crit_chance" src/game/systems/skill/behaviors/FlowingThrust.cpp` → 两处 payload 填充点归一化确认（:194/:253）
- `ctest --test-dir build -C RelWithDebInfo -R "nmd.tests.(skill.unit|skill.integration|unit|integration|combat.unit|combat.integration|combat.parity.unit)$"` → 7 套件全绿

---

# 第 6 轮复审：设计决策落地（方案 A + H9d 删除 + MEDIUM-4 八项实装）

## 6.0 用户决策（2026-09-07）

1. **MEDIUM-2 → 方案 A**：`BuffEffect` 增加 `source_skill_id` 归属字段，实现 SkillOnly 减抗过滤。
2. **MEDIUM-4 → 补实装**：8 项 desc 偏差全部实装，数值写入 json 配置（数值强度由复审方按设计区间拍板）。
3. **H9d → 删除**：命中削减技能 2 CD 的私有特例移除，此类特性后续放入装备词缀体系。
4. **UMR 并轨 → 跳过**：不在本批启动。

## 6.1 MEDIUM-2 方案 A 最终形态：聚合式减抗过滤

**复审定论（第 6 轮关键发现）**：174 元素侵蚀的减抗 debuff 在修复前**全工程零生效**——`AttributePipeline` 不消费 `ActiveEffectsComponent`（modifiers 聚合来源仅 affix/SoulEater/ModifierList/MonsterAffix/Astrolabe）、combat_v2 无 resist 处理、`CombatSystem.cpp:71` 等全部只读 `CombatStats.resistances[]` 烘焙值，victim debuff 的 `modifiers` 命中均为写入侧。§3.2/§4 中「实际效果强于设计」的立论有误，真实情况是零效果。第 6 轮初版「补偿式过滤」（把「已生效」的减抗加回去）建立在错误前提上，会让其他技能被错误加抗 12%，已否决并返工。

**终态实现（聚合式，debuff 生效 + 过滤一次完成）**：
- `Buff.hpp:87-90` 新增 `int source_skill_id = 0`；`to_json:103`、`from_json:127-128`（可选字段，旧存档兼容）、`AddOrRefresh:148` 取最新归属。
- `DamageMitigationService::ApplySkillScopedResistEffects`（hpp:20-29、cpp:19，调用点 Apply :72）：遍历 defender 的 `ActiveEffectsComponent.effects`，`source_skill_id != 0 && != skill_id` 跳过；否则累加匹配元素（`(ResistFire && Fire) || (ResistCold && Cold)`）的 Flat 值 `/100`。skill_id==0（普攻/荆棘等无归属伤害）仅受 source==0 全局 debuff 影响。
- `DamagePipeline.cpp:1312` CalculateBatch lambda `debuff_resist_aggregate` 委托同服务（SIMD :1369、标量 :1478），双路径数值一致。
- `FlowingThrust.cpp:449` 174 施加处填 `source_skill_id = kSkillId`。
- 测试重写 `tests/unit/DamagePipelineUnifiedEntryTests.cpp:341-398`：基础抗 0.35 + erosion −12 Flat（source=技能1）→ 技能 1 伤害 77（res 0.23）、其他技能 65（res 0.35，SkillOnly 过滤）、source=0 时双 77（全局生效）。

## 6.2 H9d 删除

- `FlowingThrust.cpp:317-321` 删除命中削减技能 2 CD 0.75s 特例（保留剑意与重铸窗口逻辑）。
- `tests/integration/SkillSystemTests.cpp:499-500` 负向断言：命中后技能 2 CD 保持 3.0f 不变。
- `skills.json` 无残留字段（技能 2 CD-0.75s 特例解除）。

## 6.3 MEDIUM-4 八项实装

**数据载体**：生成器（`gen_skill_contracts.py`）节点契约仅透传 `id/max_points`，无自定义字段通道 → 新建 `assets/data/skill_mechanics.json`（键=技能 id → 节点 id → 数值表）+ `src/game/foundation/data/SkillMechanicsRegistry.hpp/.cpp`（对齐 `AilmentRegistry` 的 EnsureLoaded/ResetForTests 单例模式），加载点 `src/app/Game.cpp:259`。代码侧全部 `GetFloat(skill, node, key, default)` 读取（default=设计值兜底），无硬编码数值。

**数值决策表（进 skill_mechanics.json，节点 id → 字段）**：

| 节点 | 字段与数值 |
|---|---|
| 170 | ember_duration 2.0 / ember_width 60 / ignite_magnitude 15 / ignite_duration 3 / ember_tick_interval 0.25 |
| 171 | ember_width_per_point_pct 25 / ember_duration_per_point 0.5 / fire_damage_pct_per_point 6 / standing_buff_duration 0.25 |
| 172 | slow_magnitude 0.3 / slow_duration 2.5 / frozen_more_mult 1.5 |
| 173 | chill_slow_pct 20 / chill_duration 3 / chill_stacks_to_freeze 3 / freeze_duration 2.5 / shatter_chance_pct_per_point 15 / shatter_radius 200 / shatter_crit_mult 0.3 |
| 175 | spread_icd 1.0 / spread_radius 200 / spread_chance_pct_per_point 20 / spread_ignite_magnitude 15 / spread_ignite_duration 3 / spread_slow_magnitude 0.3 / spread_slow_duration 2.5 |
| 112 | dash_range_pct_per_point 10 / damage_more_per_10yd 2 |
| 135 | leave_radius 80 / ward_dex_mult_per_point 2 / ward_duration 3 |
| 153 | lifesteal_ratio 1.0 |

**逐项实现位置**（行为主体 `FlowingThrust.cpp`，新组件 `FlowingThrustComponents.hpp`）：
- **170 余烬带**：DoCast Swap 分支（:187-221）生成 `FlowingEmberZoneComponent` 实体；`UpdateFlowingThrustEmbers`（:715-800，`SkillSystem.cpp:1097-1099` 驱动）按线段距离判定，`AilmentApplier::Apply(Ignite)` 复用现有点燃，`zone.ignited` 去重。
- **171 站位加成**：旧无条件 AttackUp 删除；玩家站自己余烬上 → `InfernalPath` buff（FireDamage PercentAdd 6%×点，duration 0.25s 逐 tick 刷新，离开自然过期，:769-793）。
- **172 冻结增伤**：减速用 legacy `FrostSlow` buff（SpeedDown −30%，与 `HazardSystem::ApplyChillDebuff` 同型；`AilmentType::Slow` 无契约注册，原路径静默失效）；冻结 +50% More 注入 `DamagePipeline.cpp:890-907` per-instance 乘区（skill_id==1 且非 DoT，读 frozen_more_mult）——技能 1 交付链路为 payload-only 空池请求（`DamagePipeline.cpp:478` 仅 base_pool 非空才走 CombatV2），走 legacy 段，机制生效。
- **173 寒冷→冻结→碎裂**：命中减速敌叠 `FrostChill`（−20%/层，3 层 → 移除并挂 `Frozen` 2.5s，:438-475）；碎裂（:522-558）roll 15%×点，读新组件 `LastCritDamageComponent`（`Combat.hpp:29-35`，记录点 `DamagePipeline.cpp:1241-1246`），来源校验（本人/召唤物）后对半径 200 内**所有**敌人各发一次溅射（`base_pool.Add(Tag::Cold, lastCrit×0.3)` + `Cold|DamageOverTime|Area` 标签防递归）。**第 6 轮修正**：初版单目标（`break`）不符合「对周围敌人」的 AOE 语义，改为全体。
- **175 传染 ICD**：命中带对应元素异常目标 → ICD 检查（`FlowingThrustStateComponent.last_infect_time`，时间源 `GetTime()`）→ roll 20%×点 → 半径 200 内**最近**敌人传染（初版按 view 迭代序取第一个，非确定性，已改最近目标）。
- **153 饮血刃**：`AilmentEngine.cpp:47-58` helper（DoT source 分配了 153 判定）+ 治疗块（:686-711，插在 `DamagePipeline::Execute` 扣血与 popup 之间）：`heal = result.damage.total_damage × lifesteal_ratio`，走 BloodSea 同款治疗链（stats.health 同步 HealthComponent、StatsDirty、`CreateOnHeal` 事件）。**语义解读**：治疗挂在流血 DoT tick 实际结算上——施加者分配 153 后，其造成的每次流血 tick 按实际扣血量等额回血（不限流云刺施加的流血）。若产品侧要限定「仅流云刺造成的流血」，需 DoT 记录来源技能 id 后加过滤。
- **112 势如破竹**：烘焙改 `feature_flags |= 256`（`SkillSpecializationBaker.cpp:200-207`，不再无条件改 speed/more）；运行时（FlowingThrust.cpp:147-183）传送距离 ×(1+0.10×点)，位移 More ×(1+0.02×floor(实际距离/10)) 经 `payload_context.more_damage` 下游消费；未分配零影响。
- **135 虚实相生**：`UpdateFlowingThrustPhantomShield`（:838-918，`SkillSystem.cpp:1100` 驱动）遍历残影（SummonComponent.owner 归属）判定玩家进出 80 码区，离开触发 Ward。**第 6 轮二次修正（阻断级）**：初版走 `BuffEffect MaxBarrier Flat` 修饰符为零生效路径（AttributePipeline 不聚合 ActiveEffects modifiers，且重算清零 max_barrier）；而引擎已有 Ward 机制（`RegenerationSystem.hpp:88-100`：barrier 超过 max_barrier 的部分按 barrier_decay 指数衰减，`CombatSystem.cpp:580-592` 承伤直接消耗 barrier）→ 终态改为直接 `stats->barrier += dex × 2 × 点` + 挂 `BarrierComponent`，buff 仅保留 UI 展示（modifiers 空）。
- 测试：新建 `tests/functional/FlowingThrustNodes.cpp`（6 用例：170 余烬生成+点燃+到期、171 站位 18/离开消失、172 冻结 165 vs 110、173 多目标碎裂、175 ICD 三段、153 治疗+对照组、112 距离 110/more 1.22+对照组、135 barrier 100+BarrierComponent）；`SkillSpecializationBakerTests.cpp` 112 断言更新为 feature_flags。

## 6.4 验证

- 每批子代理交付后复审方独立重跑：`build.bat` BUILD_EXIT=0 ×3（第一批终态、批 2a、批 2b+135 修正）；
- `ctest` 7 套件（unit/integration/combat.unit/combat.integration/combat.parity.unit/skill.unit/skill.integration）100% passed ×3；
- 复审独立核实点：`DamagePipeline.cpp:478` CombatV2 分派条件（payload-only 走 legacy）、`DamagePipeline.cpp:890-907` 冻结 more 位置、`AttributePipeline.cpp:551-599` modifiers 来源清单、`RegenerationSystem.hpp:88-100` Ward Mode、`Execute` 第 4 参数为 show_vfx 非 is_simulation（碎裂溅射真扣血）、`FlowingThrust.cpp:557` 溅射请求带池走 CombatV2 桩（数值 ×1.05，既有迁移状态）。

## 6.5 第 6 轮遗留与风险

1. **173 碎裂溅射数值经 CombatV2 桩**（×1.05 系数，`base_pool` 非空请求）：伤害生效但数值偏 +5%，属既有迁移状态，CombatV2 迁移后自动消除。
2. **153 语义解读**需产品侧确认（tick 回血 vs 击中触发，见 §6.3）。
3. **Slow/Chill 用 legacy buff** 而非 ailment 契约（契约未注册导致原路径静默失效）——与 HazardSystem 同型，后续契约补注册时统一收编。
4. **第 4 轮遗留维持**：MEDIUM-5 四处死数据 snapshot.payload_context 维持原样（零读取点）、UMR 并轨跳过、ShadowDuplicationHook 特例维持。
5. 全量 21 套件中 `nmd.tests.performance`（机器波动）与 `nmd.tests.gpu.hardware`（本地 GPU 门禁）2 个环境性失败与本批无关。

## 6.6 第 6 轮结论

**提交**。用户四项决策全部落地并验证：方案 A 聚合式过滤（含补偿式返工与零生效路径定论）、H9d 删除（负向断言保护）、MEDIUM-4 八项全部实装（数值入 skill_mechanics.json，两处阻断级修正：173 单目标→AOE、135 零生效→barrier 超额段）。三条独立全绿验证。


