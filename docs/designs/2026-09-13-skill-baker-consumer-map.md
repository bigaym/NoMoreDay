# 节点烘焙 → 消费映射表（B2-20）

> 本文档登记 `SkillSpecializationBaker` 的全部「写入」与「消费点」的对应关系，
> 是 DoD#3「节点判定单源」与 Pillar 6「数据单一来源」的守护基线。
> 每条结论均可通过文末 §6 的 `rg` 命令复核。

| 项 | 值 |
|---|---|
| 对应任务 | 计划 T6.1 / B2-20（烘焙→消费映射表） |
| 上游设计 | `docs/designs/2026-09-13-skill-abstraction-and-b2-cleanup-design.md`（§3 Pillar 6、§4.4.1、§4.7 DoD#3） |
| 写入源文件 | `src/game/systems/skill/SkillSpecializationBaker.cpp`（1104 行） |
| 数据载体 | `src/game/foundation/components/SkillDefs.hpp`：`BakedDeliveryParams`(:614-657)、`BakedSkillProfile`(:662-687)、`PhantomTranceParams`(:571-610)、`ActiveSkillsComponent`(:694-705) |
| 覆盖范围 | switch#1 交付原型（`Bake()` :62-115）9 个技能 case；switch#2 节点修正（`ApplyNodeModifiersToProfile()` :227-1033）9 个技能 case（技能 1-9；技能 10 绝影七杀 `SevenStarSlash` 不经过 Baker switch，无 flags 写入） |
| 统计 | 表 A：26 行；表 B：229 行；表 C：29 行（逐位）；疑似死写：见 §5 |

> **覆盖范围说明**：技能 10/11/12 **不经** `SkillSpecializationBaker` 的 flag switch——`SkillSpecializationBaker.cpp` 的两个 switch（`Bake()` :63-104、`ApplyNodeModifiersToProfile()` :226-899）仅含 `case 1..9`；其节点判定由行为层 `Resolve*SpecState`（技能10 `SevenStarSlashShared.hpp::ResolveSpecState`、技能11 `HeavenlySwordDescent`、技能12 `BloodSea`）单源承担。故本映射表与 `tests/unit/SkillBakerFlagConsumerTests.cpp` 的覆盖范围**限技能 1-9，不适用于技能 10/11/12**。

---

## 1. 总述：Baker 写 → 消费的单源原则

### 1.1 Pillar 6 与 DoD#3

上游设计 `docs/designs/2026-09-13-skill-abstraction-and-b2-cleanup-design.md` 在五大支柱之外新增第 6 条：

> **Pillar 6（:109）**：数据单一来源（Single Source of Truth，本次新增）——任一节点的「是否点亮」只有一处权威判定；数值只来自 `skill_mechanics.json` / 契约生成物。

> **DoD#3（:215）**：节点判定单源——不存在「同一节点同时用 flags 与 `allocated_points` 两处判定」；由 B2-20 映射表 + 断言守护。

> **§4.5 单一来源（:197）**：行为层只经统一 helper 读数据；Baker 只烘焙交付参数。**B2-20 映射表登记每个 Baker flag 的消费点**。

B2-20 立项时判定为「成立（缺失）」：

> **:72**：`B2-20 烘焙→消费映射表 | 成立（缺失） | 仓库无该表；flags 无消费点问题（§4.2）无法系统性发现`

实施要求（:252）：`B2-20 烘焙→消费映射表 | 新建文档+断言 | A1-6 | 支撑 DoD#3`。
评审记录（:360）：`映射表（B2-20）流于形式、与实际不同步 → 加测试断言每个 Baker flag 有消费点`。

### 1.2 本表的单源语义

本文档把「写入→消费」分成三种情形，与 §4.4.1 flags 用途二分准则一致：

1. **交付参数消费（Baker 单源供给）**：行为/系统只读 `delivery.*` 参数位，不反向判定节点 → 归表 A / 表 B 的「字段消费点」列。
2. **节点语义消费（flags 作「是否点亮」判定）**：消费点 `feature_flags & bit` 与 `allocated_points` 判定并存 → 属于 DoD#3 要消弭的双源风险，本文档逐位登记（表 C），供迁移 helper 时核对。
3. **写而不读（死写 / 死位）**：Baker 写入但全仓无任何消费 → §5 列出，属于 B2-20 要暴露的「flags 无消费点问题」。

**命名约定**：`技能:N` 表示 `skill_id=技能` 的节点 `N`；`Baker case N :行` 表示 Baker 内 `case N:` 的写入位置；node 中文名为行为文件枚举常量旁注释（见 §6 证据命令 E1）。

---

## 2. 表 A：交付参数写入（switch#1）→ 消费点

写入源：`SkillSpecializationBaker.cpp::Bake()` 的 `switch (profile->skill_id)`（:62-115）与后处理（:159-174）。
消费点仅列**生产代码**；`tests/unit/SkillSpecializationBakerTests.cpp` 为测试断言，不视为消费。

| # | 技能 | 写入（Baker:行） | 字段 | 消费点（file:line，读取内容） | 备注 |
|---|---|---|---|---|---|
| A1 | 1 | case 1 :64 | `secondary_archetype=DirectStrike` | **无** | 疑似死写 §5-D1 |
| A2 | 1 | case 1 :65 | `speed=400` | `FlowingThrust.cpp:122`（冲刺基础速度 400） | 流云刺突进 |
| A3 | 1 | case 1 :66 | `duration=0.375` | **无**（`FlowingThrust.cpp:297,302,316` 硬编码 `0.375f`） | 疑似死写 §5-D2 |
| A4 | 2 | case 2 :71 | `speed=1.0`（相对倍率） | `RendingWave.cpp:147,177`（`delivery.speed` 作弹速倍率；272 雷光弹速覆盖） | 裂空斩弹速 |
| A5 | 2 | case 2 :72 | `range=500` | `RendingWave.cpp:194`（`delivery.range` 作 maxRange） | 最大射程 |
| A6 | 2 | case 2 :73 | `area_radius=35` | `RendingWave.cpp:149`（`delivery.area_radius`） | 初始范围 |
| A7 | 3 | case 3 :76 | `projectile_count=3` | `BladeFormation.cpp:164`；`SkillDisplayPreviewService.cpp:28` | 灵剑数量 |
| A8 | 4 | case 4 :79 | `secondary_archetype=ReactiveWard` | **无** | 疑似死写 §5-D1 |
| A9 | 5 | case 5 :82 | `duration=5.0` | **无**（`InfiniteBlades.cpp:128` 硬编码 `beam.max_channel_time=5.0f`） | 疑似死写 §5-D2 |
| A10 | 5 | case 5 :83 | `sub_interval=0.3` | `InfiniteBlades.cpp:152-153`（`beam.tick_interval`） | 引导发射间隔 |
| A11 | 6 | case 6 :86 | `area_radius=150` | `SwordArray.cpp:165`（`array.radius`） | 剑阵半径 |
| A12 | 6 | case 6 :87 | `duration=5.0` | `SwordArray.cpp:164`（`array.duration`）；`AreaFieldDeliverySystem.cpp:469-470`（resDuration） | 剑阵持续时间 |
| A13 | 6 | case 6 :88 | `sub_interval=0.5` | `SwordArray.cpp:166,259`；`AreaFieldDeliverySystem.cpp:472-473` | 剑阵 tick 间隔 |
| A14 | 7 | case 7 :91 | `duration=5.0` | **无**（引导时长走 `beam.max_channel_time`，`BeamChannelDeliverySystem.cpp:501,984`） | 疑似死写 §5-D2 |
| A15 | 7 | case 7 :92 | `sub_interval=0.3` | **无**（技能 7 无任何 `delivery.sub_interval` 读取） | 疑似死写 §5-D3 |
| A16 | 7 | case 7 :96 | `range=GetFloat(7,0,"base_range",350)` | `BeamChannelDeliverySystem.cpp:210-211`（`delivery.range` 作 maxRange） | 心剑射程 |
| A17 | 8 | case 8 :100 | `speed=GetParam("speed",400)` | `BladeBoomerang.cpp:124`（`del.speed` 基础速度）→ `:132`（剑意增幅）、`:194,213`（初速/飞行速度）、`:244`（折返速度=1.5×） | 回旋飞剑速度 |
| A18 | 8 | case 8 :101 | `range=GetParam("max_distance",300)` | `BladeBoomerang.cpp:242`（`bc.max_distance=delivery.range`） | 最大飞行距离 |
| A19 | 8 | case 8 :103 | `duration=0.0` | `BladeBoomerang.cpp:243`（`bc.hover_duration=delivery.duration`；810 覆写） | 滞空时长 |
| A20 | 9 | case 9 :106 | `secondary_archetype=None` | **无** | 疑似死写 §5-D1 |
| A21 | 9 | case 9 :108 | `speed=GetParam("dash_speed",600)` | **无**（`PhantomTrance` 无 `delivery.speed` 读取） | 疑似死写 §5-D4 |
| A22 | 9 | case 9 :110 | `duration=GetFloat(9,0,"form_duration",3)` | 经 :111 镜像写入 `trance.duration_sec`（见 B9-975） | 形态时长 |
| A23 | 9 | case 9 :111 | `trance.duration_sec=duration` | `PhantomTrance.cpp:217-218,239-240,720-721,745` | 形态时长 |
| A24 | 3 | 后处理 :160-165 | `projectile_count=1 / *=2`（依 flags） | `BladeFormation.cpp:161-171`（has_giant_sword / max_swords） | 330 巨剑、311 无尽剑匣 |
| A25 | 6 | 后处理 :169-173 | `effective_tags=Fire`、`sub_interval=0.5`（依 flags） | `SwordArray.cpp:221`（is_fire_field）；`AreaFieldDeliverySystem.cpp:472-473` | 670 焚天烈焰阵互斥 |
| A26 | 全部 | base init :34-56 | `effective_mana_cost`（:34-41 技能 5/7 特化）、`effective_tags`（:42）、`projectile_count`（:43-46）、`area_radius`（:47-50）、`proc_coefficient`（:51-54）、`more_damage_mult=1`（:55）、`effective_charges`（:56） | `SkillSystem.cpp:1974`（mana）、`BeamChannelDeliverySystem.cpp:771,1071`（技能 7 mana）、`SkillDisplayPreviewService.cpp:26-28`、各行为文件（见 §3 节点行） | 交付原型基线 |

---

## 3. 表 B：节点修正（switch#2）逐 node 写入 → 消费点

写入源：`ApplyNodeModifiersToProfile()`（:227-1033），9 个 `case` 对应技能 1-9。
约定：

- **f|=N** = `delivery.feature_flags |= N`（十六进制/十进制见 `()`）。
- **tags→X** = `delivery.effective_tags` 元素转换。
- **字段消费**列给出读取同一交付字段的行为/系统位置；**flag 消费**列给出 `feature_flags & bit` 判定点。
- 若某写入既无字段消费也无 flag 消费，标注「**死写/死位**」，语义列给「疑似」。
- 消费点行号均指生产代码文件（`src/game/systems/skill/...` 前缀省略）。

### 3.1 技能 1 · 流云刺（FlowingThrust）

| Node | 写入（Baker:行） | 字段消费 | flag 消费 | 语义 |
|---|---|---|---|---|
| 100 | 无 | — | — | 基础（stat_modifiers） |
| 101 | `effective_mana_cost*=(1-0.15p)`（:233） | `SkillSystem.cpp:1974` | — | 聚气减耗 |
| 102 | `bonus_crit+=0.02p`（:237） | `FlowingThrust.cpp:235,331` | — | 剑心会心 |
| 103 | `more_damage_mult*=(1+0.10p)`（:240） | `FlowingThrust.cpp:203,322` | — | 流意增伤 |
| 110 | `effective_cooldown-=p`、`more_damage_mult*=(1-0.15p)`（:243-244） | `SkillSystem.cpp:1765,2032`；`DamagePipeline.cpp:337` | — | 穿刺 |
| 111 | `effective_charges+=p`、`effective_cooldown*=(1+0.15p)`（:247-248） | `SkillSystem.cpp:1738-1739,1937-1938`；`FlowingThrust.cpp:794-795` | — | 连环 |
| 112 | `f|=256(0x100)`（:255） | — | `FlowingThrust.cpp:183` | 蓄势 Momentum |
| 113 | `f|=4(0x004)`（:258） | — | `FlowingThrust.cpp:128` | 风行 Windwalker |
| 114 | `riding_wind_bonus_crit+=0.08p`（:261） | `FlowingThrust.cpp:140` | — | 御风而行（运行时查剑步状态） |
| 115 | `f|=16(0x010)`（:265） | — | **死位** | 势不可挡 Relentless |
| 130 | `sub_count=1`（:268）、`f|=2(0x002)`（:269） | sub_count：**无**（sub_count 仅技能 8 消费） | `FlowingThrust.cpp:129` | 残影 Afterimage |
| 131 | 无 | — | — | 暗影领域 ShadowDomain |
| 132 | `f|=32(0x020)`（:274） | — | `FlowingThrust.cpp:130` | 影袭 ShadowStrike |
| 133 | `f|=8(0x008)`（:277）、`tags→Teleport`（:278） | `FlowingThrust.cpp:239,250,334`（effective_tags） | `FlowingThrust.cpp:131` | 置换 Swap |
| 134 | `more_damage_mult*=(1+0.25p)`、`area_radius*=(1+0.20p)`（:281-282） | `FlowingThrust.cpp:202,322` | — | 影闪 ShadowBlitz |
| 150 | `f|=64(0x040)`（:285） | — | **死位** | 弱点洞悉 WeakPoint |
| 154 | `effective_charges=1`、`effective_cooldown=8`、`effective_mana_cost*=2`、`more_damage_mult*=2`、`bonus_crit+=1`（:288-292） | `SkillSystem.cpp:1738-1739,1937-1938,1974`；`FlowingThrust.cpp:203,794-795` | — | 孤注一掷 AllIn |
| 155 | `f|=128(0x080)`（:295） | — | **死位** | 斩断宿命 SeverFate |
| 170/172 | `tags→(火/冰)`（:300） | `FlowingThrust.cpp:239,250,334` | — | 地狱火 / 寒霜风 |

### 3.2 技能 2 · 裂空斩（RendingWave）

| Node | 写入（Baker:行） | 字段消费 | flag 消费 | 语义 |
|---|---|---|---|---|
| 200 | `area_radius*=(1+0.1p)`、`range*=(1+0.1p)`（:308-309） | `RendingWave.cpp:149,194` | — | 基础范围 |
| 201 | `effective_mana_cost=max(0,-1p)`（:312） | `SkillSystem.cpp:1974` | — | 节能 |
| 202 | `more_damage_mult*=(1+0.1p)`（:315） | `RendingWave.cpp:150` | — | 增伤 |
| 203 | 无 | — | — | 基础 |
| 210 | `projectile_count+=p`、`more_damage_mult*=penalty`（:321-323） | `RendingWave.cpp:258`、`more` 经 :150 | — | 多重裂空 |
| 211 | `sub_count=3`（:326）、`f|=4(0x004)`（:327） | sub_count：**无**（仅技能 8 消费） | `RendingWave.cpp:185` | 碎裂之刃 Fracture |
| 212 | 无 | — | — | 基础 |
| 213 | `f|=8(0x008)`（:333） | `HeavyMomentum_Node213` 提供 PhysicalDamage +22% percent-mult（id 2002103）（见 `docs/designs/2026-09-13-skill-followup-design.md` §4.4） | `RendingWave.cpp:186` | 万剑归宗-残篇 Scatter |
| 214 | `f|=64(0x040)`（:336） | — | `RendingWave.cpp:182` | 星环护体 Orbit |
| 215 | `f|=128(0x080)`（:339） | — | `RendingWave.cpp:187` | 灵剑追击 SpiritPursuit |
| 230 | `f|=1(0x001)`（:342） | — | `RendingWave.cpp:184` | 回旋劲 Boomerang |
| 231 | 无 | — | — | 基础 |
| 232 | `pull_radius=120`（:348）、`f|=(1\|16)`（:349） | `RendingWave.cpp:390-397`（pull_radius） | `RendingWave.cpp:389`（16） | 引力陷阱 GravityWell |
| 233 | `pull_radius=120*(1+0.2p)`（:352）、`f|=(1\|16\|512)`（:353） | `RendingWave.cpp:390-397` | `RendingWave.cpp:389`（16）、:391（512） | 深渊边缘 AbyssEdge |
| 234 | `f|=32(0x020)`（:356） | — | `RendingWave.cpp:183` | 时空停滞 TimeLock |
| 235 | 无 | — | — | 基础 |
| 250 | 无 | — | — | 基础 |
| 251 | `f|=4096(0x1000)`（:365） | — | `RendingWave.cpp:109` | 剑意爆发 IntentBurst |
| 252 | 无 | — | — | 基础 |
| 253 | `f|=32768(0x8000)`（:371） | — | `RendingWave.cpp:141` | 湮灭波 ObliterationWave |
| 254 | `f|=65536(0x10000)`（:374） | — | `RendingWave.cpp:410` | 回响斩 EchoedSlash（Trigger） |
| 255 | 无 | — | — | 基础 |
| 270 | `tags→(火)`（:384） | `RendingWave.cpp:168` | — | 元素 |
| 271 | 无 | — | — | 基础 |
| 272 | `tags→(雷)`（:395）、`speed=1.0+speedBonusPct/100`（:400） | `RendingWave.cpp:147,177` | — | 雷光弹速 |
| 273 | 无 | — | — | 基础 |
| 274 | `armor_pen=5p`（:406）、`f|=4194304(0x400000)`（:407） | `DamageMitigationService.cpp:141-146`（armor_pen+tags 判定） | `DamageMitigationService.cpp:141` | 灵根破壁 |
| 275 | 无 | — | — | 基础 |

### 3.3 技能 3 · 灵剑决（BladeFormation）

| Node | 写入（Baker:行） | 字段消费 | flag 消费 | 语义 |
|---|---|---|---|---|
| 300 | `projectile_count+=p`（:419） | `BladeFormation.cpp:164` | — | 剑数 |
| 301 | `sub_interval=(haste_pct/100)*p`（:423） | `BladeFormation.cpp:206`（freqInc） | — | 疾风意 |
| 302 | `more_damage_mult*=(1+phys_pct/100*p)`（:427） | `BladeFormation.cpp:179` | — | 锋灵 |
| 303 | `f|=256(0x100)`（:430） | — | `BladeFormation.cpp:238` | 元素核心 ElementalCore |
| 310 | `range=200*(1+range_pct/100*p)`（:434） | `BladeFormation.cpp:133-134`（search_radius） | — | 索敌半径 |
| 311 | `f|=1(0x001)`（:438） | — | `BladeFormation.cpp:176` | 无尽剑匣 InfiniteSheath |
| 312 | `effective_mana_cost*=(1-cost_red_pct/100*p)`（:442）、`f|=512(0x200)`（:443） | `SkillSystem.cpp:1974` | `BladeFormation.cpp:140` | 灵力网络 Network |
| 313 | `f|=16(0x010)`（:446） | — | `BladeFormation.cpp:117` | 神速 Godspeed |
| 314 | `f|=32(0x020)`（:449） | — | `BladeFormation.cpp:118` | 凝神 Concentrate |
| 315 | `f|=64(0x040)`（:452） | — | `BladeFormation.cpp:145,148` | 御剑共振 SwordStepResonance |
| 330 | `area_radius=80`（:456）、`f|=2(0x002)`（:457） | **死写**（area_radius：技能 3 无读者；`BladeFormation` 不读 `delivery.area_radius`） | `BladeFormation.cpp:111` | 巨剑 GiantSword |
| 331 | `bonus_crit+=crit_pct/100*p`（:461）、`f|=1024(0x400)`（:462） | `BladeFormation.cpp:251` | **死位**（同节点 331） | 会心 |
| 332 | `bonus_crit_damage+=cd_pct/100*p`（:466） | `BladeFormation.cpp:252` | — | 会伤 |
| 333 | `f|=2048(0x800)`（:469） | — | **死位**（效果走 `allocated_points` `BladeFormation.cpp:184-186`） | 剑压 SwordPressure |
| 334 | `f|=4096(0x1000)`（:472） | — | **死位**（效果走 `allocated_points` `BladeFormation.cpp:187-188`） | 崩山 Crush |
| 335 | 无 | — | — | 触发节点（Trigger 契约由 Baker `SyncTriggerRules` :1035-1102 注册） |
| 350 | `f|=8192(0x2000)`（:477） | — | `BladeFormation.cpp:153` | 灵剑护体 Ward |
| 351 | `f|=4(0x004)`（:480） | — | `BladeFormation.cpp:112` | 剑刃环绕 BladeOrbit |
| 352 | `f|=16384(0x4000)`（:483） | — | `BladeFormation.cpp:158` | 反击剑网 RetaliationWeb |
| 353 | `f|=8(0x008)`（:486） | — | `BladeFormation.cpp:113` | 不朽 Immortality |
| 354 | `f|=32768(0x8000)`（:489） | — | `BladeFormation.cpp:119` | 剑鸣回响 SpellEcho |
| 355 | `f|=65536(0x10000)`（:492） | — | `BladeFormation.cpp:124` | 剑阵共鸣 ArrayResonance |
| 370/372 | `tags→(火/雷)`（:497） | `BladeFormation.cpp:125,127` | — | 元素火 / 元素雷 |
| 371 | `f|=131072(0x20000)`（:501） | — | **死位**（效果走 `allocated_points` `BladeFormation.cpp:190-194`） | 烈焰舞 BlazingDance |
| 373 | `f|=262144(0x40000)`（:504） | — | **死位**（效果走 `allocated_points` `BladeFormation.cpp:196-197`） | 电弧链 ArcChain |
| 374 | 无 | — | — | 蚀灵腐蚀 SpiritCorrosion |
| 375 | `f|=524288(0x80000)`（:509） | — | **死位**（效果走 `allocated_points` `BladeFormation.cpp:203`） | 蓄能 Charge |

### 3.4 技能 4 · 剑气护体（BladeWard）

| Node | 写入（Baker:行） | 字段消费 | flag 消费 | 语义 |
|---|---|---|---|---|
| 400 | `f|=1(0x001)`（:516） | — | **死位** | 金钟罩 GoldenBell |
| 401 | `f|=2(0x002)`（:518） | — | **死位** | 拨云见日 Deflection |
| 411 | `f|=4(0x004)`（:520） | — | **死位** | 五行御守 FiveGuard |
| 412 | `f|=8(0x008)`（:522） | — | `BladeWard.cpp:125` | 不动如山 Mountain |
| 430 | `f|=16(0x010)`（:524） | — | **死位** | 剑意格挡 IntentBlock |
| 451 | `f|=32(0x020)`（:526） | — | **死位** | 借力打力 CounterSpeed |
| 452 | `f|=64(0x040)`（:528） | — | **死位**（Trigger 契约经 `SyncTriggerRules` :1068-1069 注册，非 flags） | 瞬身反打 BlinkCounter |
| 470 | `sub_count=5`（:530）、`f|=128(0x080)`（:531） | sub_count：**无**（仅技能 8 消费） | `BladeWard.cpp:126` | 剑气反震 CounterBlade |
| 471 | `f|=256(0x100)`（:533） | — | **死位** | 以眼还眼 Vengeance |
| 472 | `tags→(雷)`（:538）、`f|=512(0x200)`（:542） | `BladeWard.cpp:130`（tags 判定） | `BladeWard.cpp:130` | 雷霆法环 StaticField |
| 473 | `f|=2048(0x800)`（:547） | — | `BladeWard.cpp:132` | 雷贯长虹 ThunderCascade |
| 474 | `tags→(冰)`（:539）、`f|=1024(0x400)`（:544） | `BladeWard.cpp:131`（tags 判定） | `BladeWard.cpp:131` | 霜铠 FrostArmor |

### 3.5 技能 5 · 万剑归宗（InfiniteBlades / BeamChannelDeliverySystem）

| Node | 写入（Baker:行） | 字段消费 | flag 消费 | 语义 |
|---|---|---|---|---|
| 500 | `effective_mana_cost*=(1-red)`（:555） | `SkillSystem.cpp:1974` | — | 基础节能 |
| 501 | `f|=1(0x001)`（:559） | — | `BeamChannelDeliverySystem.cpp:997` | 剑意共鸣 Resonance |
| 502 | `more_damage_mult*=`（:561）、`pull_radius=30`（:562）、`f|=2(0x002)`（:563） | pull_radius：技能 5 无读者（pull_radius 仅技能 2/8 消费） | **死位**（效果走 `allocated_points` `InfiniteBlades.cpp:328`） | 陨铁 MeteoricIron |
| 503 | `f|=4(0x004)`（:565） | — | **死位**（效果走 `allocated_points` `GameplayState.cpp:487-497`） | 灵动引导 Fluidity |
| 510 | `effective_mana_cost*=(1+0.30)`（:567）、`f|=8(0x008)`（:568） | `SkillSystem.cpp:1974` | `InfiniteBlades.cpp:237` | 神识锁定 MindLock |
| 511 | `range=`（:570）、`speed=1000*`（:571）、`f|=16(0x010)`（:572） | range/speed：技能 5 无 `delivery.range/speed` 读者 | `BeamChannelDeliverySystem.cpp:1125,1181` | 无处遁形 NoEscape |
| 512 | `f|=32(0x020)`（:574） | — | **死位**（效果走 `allocated_points` `InfiniteBlades.cpp:268`） | 天降命印 FateMark |
| 513 | `f|=64(0x040)`（:576） | — | **死位** | 天诛 Execution |
| 514 | `sub_count=3`（:578）、`f|=128(0x080)`（:579） | sub_count：**无** | **死位**（效果走 `allocated_points` `InfiniteBlades.cpp:357`） | 剑刃风暴 BladeStorm |
| 515 | `f|=256(0x100)`（:581） | — | `BeamChannelDeliverySystem.cpp:1091` | 万剑归阵 BladesToArray |
| 530 | `f|=512(0x200)`（:583） | — | `DamageMitigationService.cpp:318` | 气定神闲 Composure |
| 531 | `f|=1024(0x400)`（:585） | — | `BeamChannelDeliverySystem.cpp:1010` | 不坏剑身 SteeledBody |
| 532 | `f|=2048(0x800)`（:587） | — | `BeamChannelDeliverySystem.cpp:1052` | 灵气充盈 AbundantQi |
| 533 | `more_damage_mult*=(1+1.50)`（:589）、`area_radius=max(,70)`（:591）、`f|=4096(0x1000)`（:592） | area_radius：技能 5 无读者 | `BeamChannelDeliverySystem.cpp:1155` | 巨剑术 ColossalBlades |
| 534 | `f|=8192(0x2000)`（:594） | — | `BeamChannelDeliverySystem.cpp:926` | 天剑降世 SwordGod |
| 535 | `f|=16384(0x4000)`（:596） | — | **死位**（效果走 `allocated_points` `BeamChannelDeliverySystem.cpp:927,953`） | 余波 Shockwave |
| 550 | `f|=32768(0x8000)`（:598） | — | `GameplayState.cpp:480-484` | 御剑行 WalkThePath |
| 551 | `f|=65536(0x10000)`（:600） | — | `GameplayState.cpp:481,504`；`InfiniteBlades.cpp:157` | 御剑风雷 SwordStepChannel |
| 552 | `range=GetFloat circle_radius 150`（:602）、`f|=131072(0x20000)`（:603） | range：**无**（技能 5 无 `delivery.range` 读者） | `BeamChannelDeliverySystem.cpp:1113` | 随影 FollowingShadow |
| 553 | `f|=262144(0x40000)`（:605） | — | **死位**（效果走 `allocated_points` `InfiniteBlades.cpp:403`） | 剑意回流 IntentSiphon |
| 554 | `bonus_crit=100*0.01`（:607-608）、`f|=524288(0x80000)`（:609） | bonus_crit：技能 5 无读者 | `InfiniteBlades.cpp:203` | 意气爆发 IntentBurst |
| 555 | `bonus_crit_damage+=`（:611）、`f|=1048576(0x100000)`（:612） | `InfiniteBlades.cpp:213`（意念合一：以 bonus_crit_damage 增伤）；`BeamChannelDeliverySystem.cpp:1177`（光束击落段，技能 5/7 共用） | `InfiniteBlades.cpp:212` | 意念合一 Multiplier |
| 570 | `tags→(火)`（:616）、`more_damage_mult*=2`（:618）、`sub_interval/=(1-0.60)`（:620）、`f|=2097152(0x200000)`（:621） | `InfiniteBlades.cpp:152-153`（tick_interval）、tags | **死位**（技能 5 无 0x200000 读取） | 流星火雨 MeteorShower |
| 571 | `f|=4194304(0x400000)`（:623） | — | **死位** | 末日余烬 DoomsdayAsh |
| 572 | `tags→(冰)`（:627）、`sub_interval/=(1+0.50)`（:630）、`f|=8388608(0x800000)`（:631） | `InfiniteBlades.cpp:152-153`、tags | **死位**（该位由技能 6 675 消费，技能 5 无） | 冰风暴 Blizzard |
| 573 | `f|=16777216(0x1000000)`（:633） | — | **死位** | 绝对零度 AbsoluteZero |
| 574 | `armor_pen=min(30,pen)`（:651）、`f|=33554432(0x2000000)`（:652） | `DamageMitigationService.cpp:169-177` | `DamageMitigationService.cpp:169` | 灵根感应 ElementalAttunement |
| 575 | `f|=67108864(0x4000000)`（:654） | — | **死位** | 大灾变 Catastrophe |

### 3.6 技能 6 · 剑阵·诛仙（SwordArray / AreaFieldDeliverySystem）

| Node | 写入（Baker:行） | 字段消费 | flag 消费 | 语义 |
|---|---|---|---|---|
| 600 | `duration+=0.5p`（:663） | `SwordArray.cpp:164`；`AreaFieldDeliverySystem.cpp:469-470` | — | 灵气流转 Duration |
| 601 | `area_radius*=`（:665） | `SwordArray.cpp:165` | — | 虚空法网 Expansion |
| 602 | `more_damage_mult*=`（:667） | `SwordArray.cpp:245,308` | — | 极刑 Torment |
| 603 | `effective_mana_cost*=`（:669）、`range=`（:670） | mana：`SkillSystem.cpp:1974`；range：**无**（技能 6 无 `delivery.range` 读者） | — | 阵基稳固 Structure |
| 610 | `more_damage_mult*=(1-0.15)`（:674）、`f|=16(0x010)`（:675） | `SwordArray.cpp:245,308` | `SwordArray.cpp:97`；`SkillSystem.cpp:1948` | 双生剑阵 TwinArrays |
| 611 | `effective_mana_cost*=(1+0.30)`（:677）、`f|=32(0x020)`（:678） | `SkillSystem.cpp:1974` | `SwordArray.cpp:100`；`SkillSystem.cpp:1949` | 三才阵 TriFormation |
| 612 | `f|=64(0x040)`（:680） | — | **死位** | 剑气共鸣 Resonance |
| 613 | `f|=128(0x080)`（:682） | — | `SwordArray.cpp:250` | 千丝万缕 Connection |
| 614 | `f|=256(0x100)`（:684） | — | `SwordArray.cpp:251` | 流云穿阵 DashTrigger |
| 615 | `f|=512(0x200)`（:686） | — | **死位** | 御剑阵威 SwordStepArray |
| 630 | `f|=1(0x001)`（:690） | — | `SwordArray.cpp:181` | 迟缓剑压 SlowPressure |
| 631 | `f|=2(0x002)`（:692） | — | `SwordArray.cpp:186` | 破甲剑意 ArmorIntent |
| 632 | `f|=1024(0x400)`（:694） | — | **死位** | 虚弱领域 Weaken |
| 633 | `f|=4(0x004)`（:696） | — | `SwordArray.cpp:195` | 绝命法场 ExecuteField |
| 634 | `area_radius*=(1-0.30)`（:698）、`f|=2048(0x800)`（:699） | `SwordArray.cpp:165` | `SwordArray.cpp:203` | 剑阵牢笼 Cage |
| 635 | `f|=4096(0x1000)`（:701） | — | **死位** | 阵斩回响 ArrayEcho |
| 650 | `f|=8192(0x2000)`（:705） | — | **死位** | 阵眼 Core |
| 651 | `f|=16384(0x4000)`（:707） | — | **死位** | 灵力泉涌 ManaSpring |
| 652 | `f|=8(0x008)`（:709） | — | `SwordArray.cpp:212` | 意念合一 MindUnity |
| 653 | `more_damage_mult*=(1-0.50)`（:711）、`f|=32768(0x8000)`（:712） | `SwordArray.cpp:245,308` | `SwordArray.cpp:152` | 随身剑垒 MobileAura |
| 654 | `f|=65536(0x10000)`（:714） | — | **死位** | 剑神领域 CooldownRecovery |
| 655 | `f|=131072(0x20000)`（:716） | — | **死位** | 法阵回护 ArrayWard |
| 670 | `tags=Fire`（:720）、`f|=262144(0x40000)`（:721） | tags | `SwordArray.cpp:221` | 焚天烈焰阵 FireField |
| 671 | `f|=524288(0x80000)`（:723） | — | **死位** | 炼狱余火 InfernalGround |
| 672 | `tags=Lightning`（:725）、`sub_interval=1.0`（:726）、`f|=1048576(0x100000)`（:727） | `SwordArray.cpp:166,259`；`AreaFieldDeliverySystem.cpp:472-473` | `SwordArray.cpp:229` | 九幽雷池 LightningField |
| 673 | `f|=2097152(0x200000)`（:729） | — | `SwordArray.cpp:234` | 连珠落雷 ChainThunder |
| 674 | `f|=4194304(0x400000)`（:731） | — | **死位** | 法阵侵蚀 ArrayCorrosion |
| 675 | `f|=8388608(0x800000)`（:733） | — | `SwordArray.cpp:113`；`SkillSystem.cpp:1943` | 移形换阵 Relocate |

### 3.7 技能 7 · 心剑·无影（BeamChannelDeliverySystem；节点判定经 `HasNode7(node)` 单源 :166-167）

> 技能 7 的节点点亮判定已在 A1 R2 收敛为 `BeamChannelDeliverySystem.cpp` 的 `HasNode7(node)`（`GetSkill7Point`/`skills::ReadPoints`，仅运行时点数，:166-167），不再读取 `feature_flags`，离线烘焙与运行时点数不一致的双源问题由单源消除。下表第 4 列给出 `HasNode7` 调用行（:185-204、:246、:587），仅表示节点判定位置，不是 flag 消费。

| Node | 写入（Baker:行） | 字段消费 | 节点判定（HasNode7） | 语义 |
|---|---|---|---|---|
| 700 | `effective_mana_cost*=`（:743） | `BeamChannelDeliverySystem.cpp:771,1071`；`SkillSystem.cpp:1974` | — | 基础减耗 |
| 701 | `more_damage_mult*=`（:745） | `BeamChannelDeliverySystem.cpp:244,684` | — | 基础增伤 |
| 702 | `area_radius=base_radius*`（:749） | `BeamChannelDeliverySystem.cpp:223-224` | — | 基础半径 |
| 703 | `range=`（:753） | `BeamChannelDeliverySystem.cpp:210-211`（maxRange） | — | 基础射程 |
| 710 | `f|=1(0x001)`（:756） | — | `:246,587` | 心剑基石 |
| 711 | `f|=2(0x002)`（:758） | — | `:185` | 心剑·碎甲 |
| 712 | `f|=4(0x004)`（:760） | — | `:186` | 心剑·裂骨 |
| 713 | `f|=8(0x008)`（:762） | — | `:187` | 心剑·疾风 |
| 715 | `f|=16(0x010)`（:764） | — | `:188`；pts715 另读于 :294 | 心剑·连斩 |
| 730 | `f|=32(0x020)`（:766） | — | `:189` | 心剑·重击 |
| 731 | `f|=64(0x040)`（:768） | — | `:190` | 心剑·破盾 |
| 732 | `effective_mana_cost*=(1+0.50)`（:770）、`f|=128(0x080)`（:771） | `BeamChannelDeliverySystem.cpp:771,1071`（增耗） | **死位** | 心剑·燃灵 |
| 733 | `f|=256(0x100)`（:773） | — | `:191` | 心剑·蓄力 |
| 734 | `f|=512(0x200)`（:775） | — | `:204`、:460（神游脱战） | 心剑·神游 |
| 735 | `f|=1024(0x400)`（:777） | — | `:192`、:230（精准切割） | 心剑·精准 |
| 750 | `f|=2048(0x800)`（:779） | — | `:193` | 心剑·裂空 |
| 751 | `f|=4096(0x1000)`（:781） | — | `:194`、:107（空间粉碎） | 心剑·粉碎 |
| 752 | `f|=8192(0x2000)`（:783） | — | `:195` | 心剑·穿透 |
| 753 | `f|=16384(0x4000)`（:785） | — | `:196` | 心剑·震荡 |
| 754 | `f|=32768(0x8000)`（:787） | — | `:197`、:399（剑意化无） | 心剑·化无 |
| 755 | `f|=65536(0x10000)`（:789） | — | **死位**（pts755 读于 :179 用于 :300 心念反哺） | 心念反哺 |
| 770 | `tags=Cold`（:791）、`f|=131072(0x20000)`（:792） | `BeamChannelDeliverySystem.cpp:71-81`（元素解析） | `:198` | 冰封心剑 |
| 771 | `f|=262144(0x40000)`（:794） | — | `:199`、:270（极寒碎骨） | 极寒碎骨 |
| 772 | `tags=Lightning`（:796）、`f|=524288(0x80000)`（:799） | `BeamChannelDeliverySystem.cpp:71-81` | `:200` | 雷殛心剑 |
| 773 | `f|=1048576(0x100000)`（:801） | — | `:201` | 雷殛·过载 |
| 774 | `f|=2097152(0x200000)`（:803） | — | `:202` | 雷殛·连锁 |
| 775 | `f|=4194304(0x400000)`（:805） | — | `:203`；`DamageMitigationService.cpp:202-207` | 心念灭抗 |

### 3.8 技能 8 · 御剑·回旋（BladeBoomerang / BoomerangDeliverySystem / ElementPathSystem）

> 技能 8 是「字段消费、flag 全死」的典型：交付参数（`bc.*` 回填）在 `BladeBoomerang.cpp:242-261` 消费，元素路径参数在 `BoomerangDeliverySystem.cpp:181-185` 组装；**除 850 Magnet、854 Giant 两位外，其余 26 个 Baker 写入的 flag 位均无读取**（枚举 `BladeBoomerangFlags` 定义了 Hovering/Magnet/Giant 三位，见 `BladeBoomerang.cpp:69-73`）。

| Node | 写入（Baker:行） | 字段消费 | flag 消费 | 语义 |
|---|---|---|---|---|
| 800 | `effective_mana_cost-=p`（:813）、`f|=1(0x001)`（:815） | `SkillSystem.cpp:1974` | **死位** | 轻灵 Lightweight |
| 801 | `speed*=mult`（:818）、`range*=mult`（:819）、`f|=2(0x002)`（:820） | `BladeBoomerang.cpp:124`（del.speed）、`:242`（del.range 派生 max_distance） | **死位** | 极速 VelocityNode |
| 802 | `f|=4(0x004)`（:822） | — | **死位** | 锋锐 Sharpness |
| 803 | `return_damage_mult=1+0.10p`（:824）、`f|=8(0x008)`（:825） | `BladeBoomerang.cpp:246` → `BoomerangDeliverySystem.cpp`（折返伤害） | **死位** | 回馈 Feedback |
| 810 | `duration=GetFloat hover_duration 0.8`（:827）、`f|=16(0x010)`（:828） | `BladeBoomerang.cpp:243`（hover_duration） | **死位**（枚举 Hovering 定义但无读取） | 滞空切割 Hovering |
| 811 | `bleed_chance=0.25p`（:830）、`f|=32(0x020)`（:831） | `BladeBoomerang.cpp:326` | **死位** | 流血 Bleed |
| 812 | `crit_mult_vs_bleeding=0.15p`（:833）、`f|=64(0x040)`（:834） | `DamagePipeline.cpp:614-617` | **死位** | 致命暴击 CritMulti |
| 813 | `f|=128(0x080)`（:836） | — | **死位** | 音爆 SonicBoom |
| 814 | `f|=256(0x100)`（:838） | — | **死位** | 噬血 BloodRip |
| 815 | `heal_bleed_pct=1.0`（:840）、`f|=512(0x200)`（:841） | `BladeBoomerang.cpp:259` → `BoomerangDeliverySystem.cpp:242-243` | **死位** | 风眼 EyeOfStorm |
| 830 | `sub_count=2`（:843）、`f|=1024(0x400)`（:844） | `BladeBoomerang.cpp:273`（sub_count>=2 三刃判定） | **死位** | 三重刃 TriBlade |
| 831 | `side_angle_mult=1-0.10p`（:846）、`f|=2048(0x800)`（:847） | `BladeBoomerang.cpp:261,276`（侧刃角） | **死位** | 无尽刃舞 BladeDance |
| 832 | `catch_mana=2p`（:849）、`f|=4096(0x1000)`（:850） | `BladeBoomerang.cpp:256` → `BoomerangDeliverySystem.cpp:198-200`（接剑回蓝） | **死位** | 接剑 Catch |
| 833 | `combo_attack_speed=15p`（:853）、`f|=8192(0x2000)`（:854） | `BladeBoomerang.cpp:257` → `BoomerangDeliverySystem.cpp:212-216`（AttackSpeed buff） | **死位** | 连击 Combo |
| 834 | `step_extend_sec=0.5p`（:856）、`f|=16384(0x4000)`（:857） | `BladeBoomerang.cpp:258` → `BoomerangDeliverySystem.cpp:221-225`（御剑步延长） | **死位** | 御剑接踵 SwordStepCatch |
| 835 | `f|=32768(0x8000)`（:859） | — | **死位** | 回旋舞 ReturnDance |
| 850 | `f|=65536(0x10000)`（:861） | — | `BladeBoomerang.cpp:186`（Magnet） | 磁力场 Magnet |
| 851 | `pull_radius_mult=1+0.15p`（:863）、`f|=131072(0x20000)`（:864） | `BladeBoomerang.cpp:260` → `BoomerangDeliverySystem.cpp:129` | **死位** | 广域 Area |
| 852 | `intent_gain_chance=0.15p`（:866）、`f|=262144(0x40000)`（:867） | `BladeBoomerang.cpp:398` | **死位** | 剑意汲取 IntentGain |
| 853 | `intent_scaling=0.02p`（:869）、`f|=524288(0x80000)`（:870） | `BladeBoomerang.cpp:127-130` | **死位** | 心剑合一 MindIntent |
| 854 | `sub_count=0`（:872）、`giant_armor_scale=0.05`（:873）、`f|=1048576(0x100000)`（:874） | `BladeBoomerang.cpp:171-172,273,389`（巨阙体型/三刃禁用/硬直） | `BladeBoomerang.cpp:187`（Giant） | 巨阙 Giant |
| 870 | `tags=Fire`（:876）、`f|=2097152(0x200000)`（:877） | tags | **死位** | 烬火路径 AshPath |
| 871 | `path_amp=0.15p`（:879）、`f|=4194304(0x400000)`（:880） | `BladeBoomerang.cpp:253` → `DamageMitigationService.cpp:222-229`（路径增伤） | **死位** | 燎原之势 Wildfire |
| 872 | `tags=Lightning`（:882）、`f|=8388608(0x800000)`（:883） | tags | **死位** | 雷电剑 Electro |
| 873 | `arc_freq_mult=1+0.30p`（:885）、`f|=16777216(0x1000000)`（:886） | `BladeBoomerang.cpp:255` → `BoomerangDeliverySystem.cpp:185` → `ElementPathSystem.cpp:183,283,307` | **死位** | 高压 HighVoltage |
| 874 | `path_width_mult=1+0.15p`、`path_duration_mult=same`（:888-889）、`f|=33554432(0x2000000)`（:890） | `BladeBoomerang.cpp:251-252` → `BoomerangDeliverySystem.cpp:181-185` | **死位** | 元素余波 ElementalWake |
| 875 | `path_pen=0.06p`（:892）、`f|=67108864(0x4000000)`（:893） | `BladeBoomerang.cpp:254` → `DamageMitigationService.cpp:188-195`（路径穿透） | **死位** | 路径穿透 PathPen |
| 876 | `element_shield_pct=0.15`（:895）、`f|=134217728(0x8000000)`（:896） | `ElementPathSystem.cpp:73`（clamp 0..1）、`DamageMitigationService.cpp:233-238` | **死位** | 元素护体 ElementShield |

### 3.9 技能 9 · 绝影绝剑（PhantomTrance；字段经 `delivery.trance` 消费）

> 技能 9 与技能 8 同型：全部写入走 `delivery.trance.*` 字段，由 `PhantomTrance.cpp`（入口 `GetTranceParams` :71 返回 `profile->delivery.trance`）消费；**28 个 Baker 写入的 flag 位全部无读取**（`PhantomTrance.cpp` 全文无 `feature_flags`）。flag 值因各位号在技能 9 内部独立使用，与其余技能同号位不冲突（见 §4 说明）。

| Node | 写入（Baker:行） | 字段消费 | flag 消费 | 语义 |
|---|---|---|---|---|
| 902 | `trance.move_speed_pct+=`、`trance.dodge_pct+=`（:904-906）、`f|=4(0x004)`（:908） | `PhantomTrance.cpp:156-157,162-163` | **死位** | 幻影步 |
| 913 | `trance.weaken_on_pass_pct=`（:910）、`f|=256(0x100)`（:912） | `PhantomTrance.cpp:387,683` | **死位** | 枯萎 |
| 914 | `trance.void_gift_mana_per_sec=`、`trance.void_gift_dr_pct=`（:914-916）、`f|=512(0x200)`（:918） | `PhantomTrance.cpp:644-650,187-188` | **死位** | 虚空恩赐 |
| 934 | `trance.atk_cast_speed_pct=`（:920）、`f|=16384(0x4000)`（:922） | `PhantomTrance.cpp:179-183` | **死位** | 迅捷 |
| 935 | `f|=268435456(0x10000000)`（:926）（仅置位，无字段写入） | — | **死位** | （未定义字段） |
| 954 | `trance.time_reversal_sec=`（:928）、`f|=524288(0x80000)`（:929） | `PhantomTrance.cpp:575-579` | **死位** | 时光倒流 |
| 955 | `trance.focus_mana_reduce_pct=`（:931）、`f|=1048576(0x100000)`（:933） | `SkillSystem.cpp:1993-1994`（形态内减耗） | **死位** | 绝影 |
| 972 | `trance.transmuter_tag=Lightning`（:935-937）、`f|=8388608(0x800000)`（:938） | `PhantomTrance.cpp:511-513,603-604,670-674` | **死位** | 转质·雷 |
| 973 | `trance.overload_speed_pct=`（:940）、`f|=16777216(0x1000000)`（:942） | `PhantomTrance.cpp:193-197` | **死位** | 过载 |
| 974 | `trance.recovery_pct=`（:944）、`f|=8(0x008)`（:946） | `PhantomTrance.cpp:173-174` | **死位** | 回灵 |
| 975 | `trance.duration_sec=`（:948-950）、`f|=1(0x001)`（:951） | `PhantomTrance.cpp:217-218,239-240,720-721,745` | **死位** | 形态时长 |
| 976 | `trance.burst_damage_mult=`（:953）、`f|=2(0x002)`（:955） | `PhantomTrance.cpp:499,506` | **死位** | 爆发 |
| 977 | `trance.cheat_death_hold=true`（:957）、`f|=16(0x010)`（:958） | `CombatSystem.cpp:551-553`；`PhantomTrance.cpp:233` | **死位** | 免死 |
| 978 | `trance.rebirth_lost_pct=`、`trance.rebirth_flat_pct=`（:960-962）、`f|=32(0x020)`（:964） | `PhantomTrance.cpp:594-598` | **死位** | 涅槃 |
| 979 | `trance.ward_pct=`（:966）、`f|=64(0x040)`（:968） | `PhantomTrance.cpp:255,758` | **死位** | 护盾 |
| 980 | `trance.void_body=true`（:970）、`f|=128(0x080)`（:971） | `PhantomTrance.cpp:233,239,683,753` | **死位** | 虚体 |
| 981 | `trance.death_seal=true`（:973）、`f|=1024(0x400)`（:974） | `PhantomTrance.cpp:207,222,748` | **死位** | 死印 |
| 982 | `trance.last_stand_crit_pct=`（:976）、`f|=2048(0x800)`（:979） | `PhantomTrance.cpp:341,692` | **死位** | 背水 |
| 983 | `trance.death_spiral_count=`、`trance.death_spiral_damage_pct=`（:981-983）、`f|=4096(0x1000)`（:985） | `PhantomTrance.cpp:657,664-665` | **死位** | 死亡螺旋 |
| 984 | `trance.bloodthirst_pct=`（:987）、`f|=8192(0x2000)`（:988） | `PhantomTrance.cpp:587-588` | **死位** | 嗜血 |
| 985 | `trance.blink=true`（:990）、`f|=32768(0x8000)`（:991） | `PhantomTrance.cpp:710` | **死位** | 闪现 |
| 986 | `trance.cooldown_flat_reduce=`（:993）、`effective_cooldown=max(1,-)`（:995）、`f|=65536(0x10000)`（:997） | `SkillSystem.cpp:1765,2032`（effective_cooldown）；`cooldown_flat_reduce` 本身无消费（见 §5-D5） | **死位** | 冷却缩减 |
| 987 | `trance.intent_per_sec=`（:999）、`f|=131072(0x20000)`（:1001） | `PhantomTrance.cpp:634-638` | **死位** | 剑意再生 |
| 988 | `trance.sword_step_dodge_pct=`、`trance.sword_step_drain_mult=`（:1003-1005）、`f|=262144(0x40000)`（:1006） | `PhantomTrance.cpp:168-169`；`EffectSystem.cpp:79`（连击点流失倍率） | **死位** | 剑步 |
| 989 | `trance.transmuter_tag=Cold`（:1008）、`f|=2097152(0x200000)`（:1009） | `PhantomTrance.cpp:511-513,603-604,670-674` | **死位** | 转质·冰 |
| 990 | `trance.frost_amp_pct=`（:1011）、`f|=4194304(0x400000)`（:1013） | `DamagePipeline.cpp:75,89`（冰冻/冰缓增伤） | **死位** | 寒霜增幅 |
| 991 | `trance.enchant_pen_per_intent_pct=`、`trance.enchant_pen_cap_pct=`（:1015-1018）、`f|=33554432(0x2000000)`（:1019） | `PhantomTrance.cpp:440,459-460` | **死位** | 附魔穿透 |
| 992 | `trance.enchant_refresh_on_kill=true`（:1021）、`f|=67108864(0x4000000)`（:1022） | `CombatSystem.cpp:391`（击杀刷新附魔） | **死位** | 附魔刷新 |
| 993 | `trance.echo_synergy=true`（:1024）、`f|=134217728(0x8000000)`（:1025） | `PhantomTrance.cpp:263` | **死位** | 回响协同 |

---

## 4. 表 C：`feature_flags` 位清单

- 位值 = 十进制（十六进制）。
- 「产出」列按 `技能:节点` 列出写入该位的全部 Baker case（switch#2）。
- 「消费」列按技能列出唯一消费点；`—` 表示该技能无任何读取 → 该技能下此位为**死位**。
- **注意**：位号在**各技能内独立**使用（每技能一份 `BakedDeliveryParams`），跨技能同号位互不冲突；表 C 的「位」是全局视角的位号清单。
- **技能 9 的位号与其字段消费完全脱钩**（见 3.9 表头说明）。技能 9 的 28 个位在数值上与技能 1-8 同号位重叠，但因每技能独立 profile 互不干扰；下表产出列统计技能 1-8（技能 9 的死位结论见 3.9），唯一技能 9 独占位 268435456（0x10000000）单独列出。

| 位(dec/hex) | 产出（技能:node） | 消费（技能:file:line） | 死位技能 |
|---|---|---|---|
| 1 (0x001) | 2:230, 3:311, 4:400, 5:501, 6:630, 7:710, 8:800 | 2:`RendingWave.cpp:184`；3:`BladeFormation.cpp:176`；5:`BeamChannelDeliverySystem.cpp:997`；6:`SwordArray.cpp:181`；7:`BeamChannelDeliverySystem.cpp:246,587`（节点判定，非技能7 交付路径消费） | 4, 8 |
| 2 (0x002) | 1:130, 3:330, 4:401, 5:502, 6:631, 7:711, 8:801 | 1:`FlowingThrust.cpp:129`；3:`BladeFormation.cpp:111`；6:`SwordArray.cpp:186`；7:`BeamChannelDeliverySystem.cpp:185`（节点判定，非技能7 交付路径消费） | 4, 5, 8 |
| 4 (0x004) | 1:113, 2:211, 3:351, 4:411, 5:503, 6:633, 7:712, 8:802 | 1:`FlowingThrust.cpp:128`；2:`RendingWave.cpp:185`；3:`BladeFormation.cpp:112`；6:`SwordArray.cpp:195`；7:`BeamChannelDeliverySystem.cpp:186`（节点判定，非技能7 交付路径消费） | 4, 5, 8 |
| 8 (0x008) | 1:133, 2:213, 3:353, 4:412, 5:510, 6:652, 7:713, 8:803 | 1:`FlowingThrust.cpp:131`；2:`RendingWave.cpp:186`；3:`BladeFormation.cpp:113`；4:`BladeWard.cpp:125`；5:`InfiniteBlades.cpp:237`；6:`SwordArray.cpp:212`；7:`BeamChannelDeliverySystem.cpp:187`（节点判定，非技能7 交付路径消费） | 8 |
| 16 (0x010) | 1:115, 2:232, 3:313, 4:430, 5:511, 6:610, 7:715, 8:810 | 2:`RendingWave.cpp:389`；3:`BladeFormation.cpp:117`；5:`BeamChannelDeliverySystem.cpp:1125,1181`；6:`SwordArray.cpp:97`、`SkillSystem.cpp:1948`；7:`BeamChannelDeliverySystem.cpp:188`（节点判定，非技能7 交付路径消费） | 1, 4, 8 |
| 32 (0x020) | 1:132, 2:234, 3:314, 4:451, 5:512, 6:611, 7:730, 8:811 | 1:`FlowingThrust.cpp:130`；2:`RendingWave.cpp:183`；3:`BladeFormation.cpp:118`；6:`SwordArray.cpp:100`、`SkillSystem.cpp:1949`；7:`BeamChannelDeliverySystem.cpp:189`（节点判定，非技能7 交付路径消费） | 4, 5, 8 |
| 64 (0x040) | 1:150, 2:214, 3:315, 4:452, 5:513, 6:612, 7:731, 8:812 | 2:`RendingWave.cpp:182`；3:`BladeFormation.cpp:145,148`；7:`BeamChannelDeliverySystem.cpp:190`（节点判定，非技能7 交付路径消费） | 1, 4, 5, 6, 8 |
| 128 (0x080) | 1:155, 2:215, 4:470, 5:514, 6:613, 7:732, 8:813 | 2:`RendingWave.cpp:187`；4:`BladeWard.cpp:126`；6:`SwordArray.cpp:250` | 1, 5, 7, 8 |
| 256 (0x100) | 1:112, 3:303, 4:471, 5:515, 6:614, 7:733, 8:814 | 1:`FlowingThrust.cpp:183`；3:`BladeFormation.cpp:238`；5:`BeamChannelDeliverySystem.cpp:1091`；6:`SwordArray.cpp:251`；7:`BeamChannelDeliverySystem.cpp:191`（节点判定，非技能7 交付路径消费） | 4, 8 |
| 512 (0x200) | 2:233, 3:312, 4:472, 5:530, 6:615, 7:734, 8:815 | 2:`RendingWave.cpp:391`；3:`BladeFormation.cpp:140`；4:`BladeWard.cpp:130`；5:`DamageMitigationService.cpp:318`；7:`BeamChannelDeliverySystem.cpp:204,460`（节点判定，非技能7 交付路径消费） | 6, 8 |
| 1024 (0x400) | 3:331, 4:474, 5:531, 6:632, 7:735, 8:830 | 4:`BladeWard.cpp:131`；5:`BeamChannelDeliverySystem.cpp:1010`；7:`BeamChannelDeliverySystem.cpp:192,230`（节点判定，非技能7 交付路径消费） | 3, 6, 8 |
| 2048 (0x800) | 3:333, 4:473, 5:532, 6:634, 7:750, 8:831 | 4:`BladeWard.cpp:132`；5:`BeamChannelDeliverySystem.cpp:1052`；6:`SwordArray.cpp:203`；7:`BeamChannelDeliverySystem.cpp:193`（节点判定，非技能7 交付路径消费） | 3, 8 |
| 4096 (0x1000) | 2:251, 3:334, 5:533, 6:635, 7:751, 8:832 | 2:`RendingWave.cpp:109`；5:`BeamChannelDeliverySystem.cpp:1155`；7:`BeamChannelDeliverySystem.cpp:194,107`（节点判定，非技能7 交付路径消费） | 3, 6, 8 |
| 8192 (0x2000) | 3:350, 5:534, 6:650, 7:752, 8:833 | 3:`BladeFormation.cpp:153`；5:`BeamChannelDeliverySystem.cpp:926`；7:`BeamChannelDeliverySystem.cpp:195`（节点判定，非技能7 交付路径消费） | 6, 8 |
| 16384 (0x4000) | 3:352, 5:535, 6:651, 7:753, 8:834 | 3:`BladeFormation.cpp:158`；7:`BeamChannelDeliverySystem.cpp:196`（节点判定，非技能7 交付路径消费） | 5, 6, 8 |
| 32768 (0x8000) | 2:253, 3:354, 5:550, 6:653, 7:754, 8:835 | 2:`RendingWave.cpp:141`；3:`BladeFormation.cpp:119`；5:`GameplayState.cpp:480-484`；6:`SwordArray.cpp:152`；7:`BeamChannelDeliverySystem.cpp:197,399`（节点判定，非技能7 交付路径消费） | 8 |
| 65536 (0x10000) | 2:254, 3:355, 5:551, 6:654, 7:755, 8:850 | 2:`RendingWave.cpp:410`；3:`BladeFormation.cpp:124`；5:`GameplayState.cpp:481,504`、`InfiniteBlades.cpp:157`；8:`BladeBoomerang.cpp:186` | 6, 7 |
| 131072 (0x20000) | 3:371, 5:552, 6:655, 7:770, 8:851 | 5:`BeamChannelDeliverySystem.cpp:1113`；7:`BeamChannelDeliverySystem.cpp:198`（节点判定，非技能7 交付路径消费） | 3, 6, 8 |
| 262144 (0x40000) | 3:373, 5:553, 6:670, 7:771, 8:852 | 6:`SwordArray.cpp:221`；7:`BeamChannelDeliverySystem.cpp:199,270`（节点判定，非技能7 交付路径消费） | 3, 5, 8 |
| 524288 (0x80000) | 3:375, 5:554, 6:671, 7:772, 8:853 | 5:`InfiniteBlades.cpp:203`；7:`BeamChannelDeliverySystem.cpp:200`（节点判定，非技能7 交付路径消费） | 3, 6, 8 |
| 1048576 (0x100000) | 5:555, 6:672, 7:773, 8:854 | 5:`InfiniteBlades.cpp:212`；6:`SwordArray.cpp:229`；7:`BeamChannelDeliverySystem.cpp:201`（节点判定，非技能7 交付路径消费）；8:`BladeBoomerang.cpp:187` | 无 |
| 2097152 (0x200000) | 5:570, 6:673, 7:774, 8:870 | 6:`SwordArray.cpp:234`；7:`BeamChannelDeliverySystem.cpp:202`（节点判定，非技能7 交付路径消费） | 5, 8 |
| 4194304 (0x400000) | 2:274, 5:571, 6:674, 7:775, 8:871 | 2:`DamageMitigationService.cpp:141`；7:`BeamChannelDeliverySystem.cpp:203`（节点判定，非技能7 交付路径消费） | 5, 6, 8 |
| 8388608 (0x800000) | 5:572, 6:675, 8:872 | 6:`SwordArray.cpp:113`、`SkillSystem.cpp:1943` | 5, 8 |
| 16777216 (0x1000000) | 5:573, 8:873 | **无任何技能消费** | 5, 8 |
| 33554432 (0x2000000) | 5:574, 8:874 | 5:`DamageMitigationService.cpp:169` | 8 |
| 67108864 (0x4000000) | 5:575, 8:875 | **无任何技能消费** | 5, 8 |
| 134217728 (0x8000000) | 8:876 | **无任何技能消费** | 8 |
| 268435456 (0x10000000) | 9:935 | **无任何技能消费** | 9 |

**表 C 要点**：

1. 全仓除 Baker 外的 `feature_flags &` 读取点**恰为 §3 各表的 flag 消费列**（证据 E2）；除此之外不存在任何其他读取（含 UI/适配层/`SkillSpecModifierAdapter`、`GameUi*`）。
2. 位 1..8388608 均被至少一个技能消费；**位 16777216、67108864、134217728、268435456 完全无消费**（全死位）。
3. 位 16（0x010）被技能 6 消费（610 双生剑阵）但技能 1 的 115 写入同号位——两技能共享位号、互不干扰，但**技能 1 115 自身是死位**（`FlowingThrust` 只读 4/2/32/8/256）。
4. 技能 7 的全部写入位在 A1 R2 后不再经 `feature_flags` 读取（节点判定改走 `HasNode7`）；其中位 128（732）、65536（755）本就无任何消费，属死位（见 3.7）。
5. 技能 7 的双通道 `Has7(1,710)` 已在 A1 R2 收敛为 `HasNode7(710)`（仅运行时点数，:246、:587），不再消费 flag；表 C 中技能 7 的 `BeamChannelDeliverySystem.cpp` 行号均为节点判定，属非技能 7 交付路径消费。位 8 的 `BladeWard.cpp:125` 为 412 不动如山唯一消费。

---

## 5. 疑似死写清单

以下为「Baker 写入但全仓生产代码无消费」的条目。全部为**疑似**：已用 §6 证据命令复核，若后续新增消费点（或消费点经 `allocated_points`/mech 而非 flags/字段），需同步更新本表。

| 编号 | 位置 | 写入内容 | 已检索的模式 | 说明 |
|---|---|---|---|---|
| D1 | case1 :64 / case4 :79 / case9 :106 | `secondary_archetype`（3 处） | `rg "secondary_archetype" src/ tests/` | 生产代码零读取；唯一命中在 `tests/unit/SkillSpecializationBakerTests.cpp`（测试断言）。字段在 `SkillDefs.hpp:615` 有定义与注释（'组合第二原型'），但无交付/行为消费。 |
| D2 | case1 :66 / case5 :82 / case7 :91 | `duration`（0.375 / 5.0 / 5.0） | `rg "delivery\.duration" src/game` | 全仓唯一 `delivery.duration` 读取为 `SwordArray.cpp:164`、`AreaFieldDeliverySystem.cpp:469-470`（均技能 6）。技能 1/5/7 的 duration 分别被行为硬编码覆盖：`FlowingThrust.cpp:297,302,316`（0.375f）、`InfiniteBlades.cpp:128`（max_channel_time=5.0f）、`BeamChannelDeliverySystem.cpp:501,984`（max_channel_time）。 |
| D3 | case7 :92 | `sub_interval=0.3` | `rg "delivery\.sub_interval" src/game` | 读取点均属技能 3/5/6（`BladeFormation.cpp:206`、`InfiniteBlades.cpp:152-153`、`SwordArray.cpp:166,259`、`AreaFieldDeliverySystem.cpp:472-473`、`BeamChannelDeliverySystem.cpp:1002-1003`）；技能 7 无读取。 |
| D4 | case9 :108 | `speed=GetParam("dash_speed",600)` | `rg "delivery\.speed" src/game` | 读取点仅技能 1（`FlowingThrust.cpp:122`）与技能 2（`RendingWave.cpp:145,147,177`）；`PhantomTrance` 不读 `delivery.speed`。 |
| D5 | case9 :993（986 节点） | `trance.cooldown_flat_reduce=` | `rg "cooldown_flat_reduce" src/` | 仅 Baker 写入处命中；Baker :995 已把该值折算进 `effective_cooldown`，字段本身无消费。 |
| D6 | case5 :570/572、case8 多数节点 | `range` / `speed` / `area_radius` / `sub_count` / `pull_radius` / `bonus_crit` / `bonus_crit_damage` 的部分写入 | `rg "delivery\.(range\|speed\|area_radius\|sub_count\|pull_radius\|bonus_crit\|bonus_crit_damage)" src/game` + `rg "del\.(range\|speed\|...)" src/game`（`BladeBoomerang` 用 `del.` 别名读取，如 :123-124） | 这些字段的读取者均限定于特定技能（range：2/3/7/8；speed：1/2/8；area_radius：1/2/6/7；sub_count：8；pull_radius：2/8；bonus_crit：1/3/7；bonus_crit_damage：3/5/7）。写入方超出读者技能集合的即为死写，逐条已在表 B 标注（技能 5 的 511/552 range、511 speed、533 area_radius、554 bonus_crit、555 bonus_crit_damage；技能 6 的 603 range；技能 1/2/4/5/6 的 sub_count 等）。 |
| D7 | switch#2 全表 182 处写入行 `feature_flags |=` | 见表 C「死位技能」列 | `rg "feature_flags &" src/game --glob '!SkillSpecializationBaker.cpp'` | 写而不读的 flag 位：技能 1（16/64/128）、技能 3（1024/2048/4096/131072/262144/524288）、技能 4（1/2/4/16/32/64/256）、技能 5（2/4/32/64/128/16384/262144/2097152/4194304/8388608/16777216/67108864）、技能 6（64/512/1024/4096/8192/16384/65536/131072/524288/4194304）、技能 7（全部写入位，A1 R2 收敛后不再读 flag）、技能 8（除 850/854 外全部）、技能 9（全部 28 位）。 |
| D8 | case9 :926（935 节点） | `f|=268435456`（无字段写入） | `rg "268435456" src/game` | 唯一命中为 Baker 写入；无消费，且该节点无任何 `delivery.trance` 字段产出。 |
| D9 | 技能 7 全 case | `bonus_crit` / `bonus_crit_damage`（**未写入**） | `rg "delivery\.(bonus_crit\|bonus_crit_damage)" src/game` | **反向观察（读而不写）**：技能 7 在 `BeamChannelDeliverySystem.cpp:279`（critChanceFor）、`:290`（critMultFor）、`:1177`（击落段）读取这两个字段，但 Baker case7 从不写入 → 技能 7 下恒为默认值 0（`SkillDefs.hpp:629,631` 默认）。非死写，但提示技能 7 的会心/会伤加成可能缺失烘焙来源，建议与调平核实。 |

---

## 6. 证据（rg 命令与命中）

所有结论可复核。以下命令均在仓库根执行（`--color=never` 防止 `-n` 被解析为 `--replace`）。

### E1 节点名 / kSkillId 枚举（表 B 语义来源）

```bash
rg --color=never -n "constexpr uint32_t" src/game/systems/skill/behaviors/FlowingThrust.cpp    # 技能1 FlowingThrustNodes
rg --color=never -n "constexpr uint32_t" src/game/systems/skill/behaviors/RendingWave.cpp      # 技能2 RendingWaveNodes
rg --color=never -n "constexpr uint32_t" src/game/systems/skill/behaviors/BladeFormation.cpp   # 技能3 BladeFormationNodes
rg --color=never -n "constexpr uint32_t" src/game/systems/skill/behaviors/BladeWard.cpp        # 技能4 BladeWardNodes
rg --color=never -n "constexpr uint32_t" src/game/systems/skill/behaviors/InfiniteBlades.cpp   # 技能5 InfiniteBladesNodes（500-575）
rg --color=never -n "constexpr uint32_t" src/game/systems/skill/behaviors/SwordArray.cpp       # 技能6 SwordArrayNodes
rg --color=never -n "constexpr uint32_t" src/game/systems/skill/behaviors/BladeBoomerang.cpp   # 技能8 BladeBoomerangNodes + BladeBoomerangFlags(:69-73)
rg --color=never -n "kSkillId" src/game/systems/skill/behaviors/*.hpp src/game/systems/skill/behaviors/*.cpp  # 行为归属确认
```

### E2 feature_flags 读取点全集（表 C 依据；排除 Baker 自身）

```bash
rg --color=never -n "feature_flags &" src/game --glob "!SkillSpecializationBaker.cpp"
```
命中（全部，即表 C 消费列全集）：
- `DamageMitigationService.cpp:141`（1<<22）、`:169`（33554432）、`:318`（512）
- `BeamChannelDeliverySystem.cpp:185-204,246,587`（`HasNode7` 运行时节点判定，非 `feature_flags` 消费）、`:926,997,1010,1052,1091,1113,1125,1155,1181`（技能 5 `feature_flags` 读取）
- `BladeFormation.cpp:111,112,113,117,118,119,124,140,145,148,153,158,176,238`
- `BladeWard.cpp:125,126,130,131,132`
- `BladeBoomerang.cpp:186`（Magnet 65536）、`:187`（Giant 1048576）
- `GameplayState.cpp:480,481,504`（550/551）
- `InfiniteBlades.cpp:157,203,212,237`
- `RendingWave.cpp:109,141,182,183,184,185,186,187,389,391,410`
- `SwordArray.cpp:97,100,113,152,181,186,195,203,212,221,229,234,250,251`
- `SkillSystem.cpp:1943,1948,1949`
- （Baker 自身 167 处写入不列出）

### E3 交付字段读取点（表 A / 表 B 字段消费列）

```bash
rg --color=never -n "delivery\.(speed|range|duration|sub_count|sub_interval|pull_radius|armor_pen|bonus_crit|bonus_crit_damage|secondary_archetype)" src/game
rg --color=never -n "del\.(speed|range|duration|sub_count|sub_interval|pull_radius|armor_pen|bonus_crit|bonus_crit_damage|secondary_archetype|feature_flags|giant_armor_scale|pull_radius_mult|return_damage_mult|side_angle_mult)" src/game --glob "!SkillSpecializationBaker.cpp"   # BladeBoomerang 用 del. 别名读取（:123-124,242-273）
rg --color=never -n "\.trance\." src/game/systems/skill --glob "!SkillSpecializationBaker.cpp"   # 技能9 trance 消费
rg --color=never -n "delivery\.(return_damage_mult|bleed_chance|crit_mult_vs_bleeding|catch_mana|combo_attack_speed|step_extend_sec|heal_bleed_pct|pull_radius_mult|side_angle_mult|giant_armor_scale|intent_gain_chance|intent_scaling|path_width_mult|path_duration_mult|path_amp|path_pen|arc_freq_mult|element_shield_pct)" src/game  # 技能8 专项
```

### E4 死写佐证（硬编码覆盖 / 无读取）

```bash
rg --color=never -n "0\.375f" src/game/systems/skill/behaviors/FlowingThrust.cpp   # :297,302,316 覆盖 case1 duration
rg --color=never -n "max_channel_time" src/game/systems/skill/behaviors/InfiniteBlades.cpp src/game/systems/skill/BeamChannelDeliverySystem.cpp  # :128 / :501,:984
rg --color=never -n "secondary_archetype" src tests   # 仅 Baker + 测试
rg --color=never -n "cooldown_flat_reduce" src         # 仅 Baker :993
rg --color=never -n "feature_flags" src/game/systems/skill/behaviors/PhantomTrance.cpp   # 无命中 → 技能9 全死位
rg --color=never -n "delivery\.speed" src/game/systems/skill/behaviors/PhantomTrance.cpp  # 无命中 → case9 dash_speed 死写
```

### E5 设计文档依据

```bash
rg --color=never -n "Pillar|DoD" docs/designs/2026-09-13-skill-abstraction-and-b2-cleanup-design.md  # :109,:197,:215,:72,:252,:360
```

---

## 7. 已知限制与后续动作

1. **`proc_coefficient`**（`SkillDefs.hpp:657`、Baker :51-54 写入）未在任何消费搜索中命中，疑似另一处死写；本文档未列入 §5 主表（未做全仓 grep 确认），建议补查。
2. **`riding_wind_bonus_crit`** 的消费（`FlowingThrust.cpp:140`）依赖运行时「剑步状态」，Baker 侧无法静态确认是否生效，已按字段消费登记。
3. 本表为**快照**；按评审结论（:360）应配套**测试断言**：为每个 Baker 写入的 flag 位断言 §4 消费列非空（技能 9 的字段消费另以 `trance` 断言），防止映射表与实际脱钩。
4. 技能 7 的 `Has7` 双通道（flags ∨ allocated_points）曾是 DoD#3 点名的「同节点双判定」；已在 A1 R2 收敛为 `HasNode7`（仅运行时点数，`GetSkill7Point`/`ReadPoints`），本文档表 B 3.7 与表 C 已同步为节点判定。