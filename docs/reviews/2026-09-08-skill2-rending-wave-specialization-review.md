# 技能2（裂空斩）专精树节点审查

- 日期：2026-09-08
- 范围：技能2「裂空斩」专精树 28 节点（200-275）实现完整性
- 基准：`设计文档/职业设计草案_剑修.md` §3.2（行171-223）；方法沿用 `2026-09-07-skill1-specialization-nodes-review.md`
- 结论：**修改**（Blocker ×7，High ×12，Medium ×11）

---

## 1. 审查方法

四源对照：① GDD §3.2 设计草案 → ② `assets/data/skills.json` 内嵌 `talent_tree`（行684-1136）与渲染副本 `assets/data/skill_2_tree.json` → ③ 契约 `stat_modifiers` + `assets/data/skill_contracts_compact.json`（行46-88）+ 内嵌 `skill_contract`（行1137-1368）→ ④ 代码 `SkillSpecializationBaker.cpp` case 2（行255-288）、行为层 `behaviors/RendingWave.cpp`（98行）、共享交付系统。

判定：✅符合 ⚠️部分 ❌不符 ✖️无实现。

## 2. 逐节点矩阵

| 节点 | GDD 设计 | 数据 desc/max | 契约 | 代码 | 判定 |
|---|---|---|---|---|---|
| 200 剑气纵横 | 宽度+飞行距离 10-40% (0/4) | 同desc / **max5** | **{t31 ProjectileCount +10/点}** ❌ | Baker:256 area_radius×(1+0.1p)（宽度✓，距离✖） | ❌ C3/H1 |
| 201 凝神 | 法力消耗 -1...4点 (0/4) | 同desc / **max5** | **{t29 CD减+10/点, t32 范围+10/点}** ❌ | 无分支 | ❌ C3 |
| 202 锋芒 | 物伤 +10-50% (0/5) | 同 / 5 | 无 | 无分支、无sm | ✖️ |
| 203 气劲爆发 | 异常效果 +15-60% (0/4) | 同 / 4 | 无（sm=[]） | 无分支 | ✖️ |
| 210 多重剑气 | +1/+2/+3 扇形、每发伤害 -25/20/15% (0/3) | 同 / 3 | 无 | Baker:258 projectile_count+=p；扇形在 RendingWave:60-62 ✓；**减伤✖** | ⚠️ M2 |
| 211 碎裂之刃 | 命中或最大距离分裂 3 道**追踪**剑气 (0/1) | 同 / 1 | **role=Keystone**（应普通） | Baker:260 sub_count=3/flags4；ProjectileSystem:965-988 分裂**无追踪**、pierce=false、0.6s 硬编码；**基础99穿透致首击永不分裂（H12）** | ⚠️ H5/H12 |
| 212 连锁反应 | 小剑气追踪角+15-45%、速度提升 (0/3) | 同 / **max5** | 无 | 无分支（无追踪可强化） | ✖️ |
| 213 万剑归宗-残篇 | Keystone：命中引爆散落 8 道微型穿刺剑气 (0/1) | 同 / 1 | Keystone+excl_group1+HeavyMomentum | Baker:263 flags\|=8 → **flag 8 被 BoomerangDeliverySystem:79 解读为接剑返还**；引爆✖ | ❌ H4 |
| 214 星环护体 | Keystone：环绕周身 3s 持续切割 (0/1) | 同 / 1 | Keystone+excl_group1 | 无分支 | ✖️ |
| 215 灵剑追击 | **Synergy**：灵剑伴飞 20% 效力 (0/1) | 同 / 1 | **role=Keystone**（应 Synergy） | 无分支 | ❌ C1 |
| 230 回旋劲 | 最大距离折返、折返伤害 -30% (0/1) | 同 / 1 | **role=Synergy**（应普通） | Baker:265 Boomerang archetype ✓；**-30%✖**；**生命周期1.2s < 回程1.333s，半空即被销毁（C7）** | ❌ C7/M3 |
| 231 重叠打击 | 折返命中 More +15-60% (0/4) | 同 / **max3**（档位不足） | 无 | 无分支；**回程未ClearHits，二次伤害直接被跳过（H10）** | ✖️ H1/H10 |
| 232 引力陷阱 | 折返瞬间最远端微型黑洞牵引 (0/1) | 同 / **max3** | 无 | Baker:268 pull_radius=120/strength250，HoverApex 牵引 ✓ | ⚠️ H1 |
| 233 深渊边缘 | 牵引范围+20-60%、消散击晕 (0/3) | 同 / **max5** | **role=Trigger（塞入254参数 eff0.4/ICD2.0）** | **被实现为 234 的停滞形态**（flags32→Hover） | ❌ C1/C2 |
| 234 时空停滞 | **Keystone**：最远端停滞旋转 2s 剑气风暴 (0/1) | 同 / **max5** | **role 未标（真 Keystone 丢失）** | 效果被 233 抢占；Hazard 固定 6dmg/0.2s×1s | ❌ C2/H6 |
| 235 御剑引力 | 御剑步时牵引+15-45%、击碎50-150% (0/3) | 同 / 3 | 无 | 无分支 | ✖️ |
| 250 剑气烙印 | 命中25-100%几率烙印(受暴伤+4%, max5层) (0/4) | 同 / **max3** | **role=Transmuter + resist=TypeB_Shred**（应普通） | **被实现为 物理→Void 元素转换**（Baker:274-276） | ❌ C1/C2/C4 |
| 251 剑意爆发 | ≥5层剑意施放消耗全部，每层范围+8%暴击+3% (0/1) | 同 / 1 | **role=Keystone + TypeB_Shred + HeavyMomentum**（应 SwordIntent） | **零实现**；resist 错型注入 VFX；**错绑SwordFlow致标准剑意5-9层被锁死无法施放（H11）** | ❌ C1/C2/H3/H11 |
| 252 无底深渊 | 消耗剑意施放 10-30% 返还法力 (0/3) | 同 / **max5** | **role=SwordIntent**（应普通） | **被实现为 251**：无条件消耗全部剑意→数量×2/半径×1.5/More×2 + **给技能1减CD硬编码**（RendingWave:49-58） | ❌ C2/H3 |
| 253 湮灭波 | Keystone：满层(10)剑意→巨波 More+80% 无视50%物抗 (0/1) | 同 / 1 | Keystone | **被实现为 255**：命中必回1层剑意（RendingWave:86-93） | ❌ C2 |
| 254 回响斩 | **Trigger**：消耗剑意施放时向背后触发裂空斩(40%, CD2s) (0/1) | 同 / 1 | **role=Keystone，trigger 全空** | 零实现；触发链被错挂到 233；**ProcEngine TargetPolicy 缺失 Backward（M11）** | ❌ C1/C2/H9/M11 |
| 255 意念回流 | 消耗剑意的裂空斩每命中 10-30% 几率回1层 (0/3) | 同 / **max5** | 无 | 零实现（被 253 抢占） | ❌ C2 |
| 270 霜寒之刃 | **Transmuter**：转冰霜，100%寒冷，满血冻结1s (0/1) | 同 / **max3** | Transmuter ✓ | Baker:281 转Cold ✓；**flags512 无消费者，寒冷/冻结✖**；**无damage_modifiers，管线实为100%物伤（C6）** | ❌ C4/C6/M9 |
| 271 冰晶碎裂 | 冻结敌冰爆 20-60% 范围伤害+1层寒冷 (0/3) | 同 / 3 | 无 | 无分支 | ✖️ |
| 272 雷光 | **Transmuter**：转闪电，弹速+100%，暴击感电 (0/1) | 同 / **max3** | **role=Passive + resist=TypeA_Penetration**（应 Transmuter） | **Baker 无 272 分支**（conv 永不调用）；**同C6伤害管线无转换（C6）** | ❌ C5/C6 |
| 273 感电传导 | 感电向1...3名连锁，每次30% (0/3) | 同 / **max5** | 无 | 无分支 | ✖️ |
| 274 灵根亲和 | 任意Transmuter：对应元素穿透+5-20% 仅本技能 (0/4) | 同 / **max5** | **无 resist_model（TypeA 被错绑 272）** | 无分支；抗性数值系统全库不存在 | ✖️ C1/H7 |
| 275 异常扩散 | 击杀传染150%效力至5名，每传染+2法力 (0/3) | 同 / 1 | **role=Keystone**（应普通） | 无分支 | ✖️ C1 |

汇总：✅0 ｜ ⚠️3（210/211/232）｜ ❌17 ｜ ✖️8。**四源在节点语义与战斗交付管线上系统性错位。**

## 3. 分级问题清单

### CRITICAL（Blocker）

- **C1 契约角色系统性错位（技能1 C6 同型复发）**：内嵌契约（skills.json:1150-1366）与 compact 契约对 Trigger/Synergy/Transmuter/SwordIntent/Resist/Keystone 全部错绑（详矩阵）。真 Keystone 234 完全未标；211/215/251/254/275 因 max_points==1 被生成器默认推导为 Keystone（`scripts/gen_skill_contracts.py:418-427` 已知缺陷）。**技能1 修复后未重跑生成器/未覆盖技能2**。
- **C2 行为层按错误映射实现**：`RendingWave.cpp:19-23` 常量名自证错位（`TimeLock=233`、`VoidConvert=250`、`IntentBurst=252`、`IntentGain=253`），Baker case 2 同构。玩家点 233/250/252/253 得到的是 234/虚构Void/251/255 的效果；真节点 234/251/252/253/254/255 及其设计效果零实现。且 252 分支存在**跨技能硬编码副作用**：`slot.id == 1` 给流云刺减 CD（RendingWave.cpp:55）。
- **C3 stat_modifiers 枚举错位（技能1 C3 同型）**：200 = t31(ProjectileCount)+10/点（desc 是宽度+距离）→ 点满 = **每次施放 ~51 道扇形剑气**（RendingWave.cpp:38 extraWaves=projectile_count-1，叠加扇形 spread 放大）；201 = t29(CooldownReduction)+t32(AreaScale) 各+10/点（desc 是法力 -1...4 点，法力降低无承载）。`Stats.hpp:224-279` 枚举确认 t29=CooldownReduction、t31=ProjectileCount、t32=AreaScale。
- **C4 转换器互斥失效 + 错误互斥锁死构建**：`StatsSystem.cpp:469-479` 按 `role==Transmuter` + `active_transmuter_node_by_skill` 互斥。技能2 role=Transmuter 的是 {270,250}（错），272 是 Passive → 玩家可同点 270+272（双重转换并存且 272 无转换实现）；而点 250（烙印）会占用 Transmuter 槽 → **与 270 互斥，锁死正常冰转构建**。
- **C5 272 雷光零实现**：`SkillBehaviorBase.hpp:45` case 272→Tag::Lightning 转换定义存在（注释 "was 250 typo"），但 Baker case 2（255-288）无 272 分支，永不调用。闪电转换、弹速+100%、暴击感电全无。
- **C6 伤害管线元素转换空转（270/272 假转换）**：`DamagePipeline.cpp:680-740` 计算初始伤害时以 `skill_data->tags` 将基底定为 `Tag::Physical`，后续 Conversion 循环严格依赖 `tree->nodes.at(node_id).damage_modifiers` 或实体 `SkillModifierComponent` 中类型为 `ModifierType::Convert` 的转换规则。现状：`skills.json` 中 270/272 的 `damage_modifiers` 为**空数组**；`RendingWave.cpp:76-77` 仅为天人感应（`attunement`）挂载了转换器，**从未给霜寒/雷光注入 Convert 类型的 DamageModifier**。Baker 修改 `effective_tags` 仅改变了标签，进入伤害管线的实例池依然是 100% 纯物理伤害，完全吃物理增伤和敌人物理抗性，冷/电元素转换在核心伤害端完全无效。
- **C7 折返剑气生命周期倒挂在半空中销毁（230 假折返）**：`RendingWave.cpp:69` 设置投射物生命周期为固定值 `proj.lifeTime = 1.2f`（非 Hover 时）；而折返组件计算的单程去程时间是 `bc.returnTimer = maxRange / baseSpeed = 400.0f / 300.0f = 1.333s`。投射物在飞行 1.2 秒时触发 `ProjectileSystem.cpp:544`（`lifeTime <= 0.0f`），直接调用 `Destroy` 将实体销毁！剑气连折返顶点（1.333s）都无法到达便在半空中凭空消失，折返机制在运行时完全空转死锁。（对比参考实现 `BladeBoomerang.cpp:68` 显式将 `p.lifeTime` 设为 `3.0f` 以容纳全程）。

### HIGH

- **H1 max_points 与 GDD 大面积不符（14 节点）**：档位不足：231(3vs4)、250(3vs4)；机制节点膨胀：234(5vs1)、232(3vs1)、270(3vs1)、272(3vs1)；数值节点膨胀：200(5vs4)、201(5vs4)、212(5vs3)、233(5vs3)、252(5vs3)、255(5vs3)、273(5vs3)、274(5vs4)。
- **H2 基底偏差**：mana_cost 15 vs 草案 10；cooldown 0.1 vs 草案「无冷却」；charge_count=3 凭空引入充能概念（skills.json:665-677）。
- **H3 剑意爆发门槛与收益缺失**：`ConsumeSwordIntent`（SkillSystem.cpp:2259-2299）无「≥5层」门槛；RendingWave.cpp:49-58 直接 spend=res->current（1 层也全额消耗并强化），收益为数量×2/半径×1.5/More×2，非设计的每层范围+8%/暴击+3%。
- **H4 flag 8 位空间跨节点污染**：Baker:264 给 213 设 flags|=8；`BoomerangDeliverySystem.cpp:76-90` 将 flag 8 解读为「接剑返还」（技能3 CatchBlade 语义）且 `BoomerangComponent.catch_by_owner` 默认 true（DeliveryArchetypes.hpp:78）→ 213+230 组合下折返剑气接住时 -1s CD +1 剑意（与 213 无关）；213 本体（碰撞引爆 8 道微型剑气）零实现。
- **H5 分裂语义缺失 + 覆盖冲突**：Split 子弹直线飞行无追踪、pierce=false、lifeTime=0.6s 硬编码（ProjectileSystem.cpp:976-988）——「追踪剑气」不存在，212「追踪角度」无对象；RendingWave.cpp:72-73 中 Hover 覆盖 Split（on_death 单值），211+233 组合分裂丢失。
- **H6 时空停滞数值背离**：错位实现在 233 上的 Hover→Hazard：固定 `20.0f × 0.3 = 6 dmg/0.2s`、持续 1s、半径×1.5（ProjectileSystem.cpp:1045-1081，hover 默认值 Projectile.hpp:129-131）vs 设计「2s 高频段伤害风暴（基于技能伤害）」。
- **H7 抗性/穿透零数值交付**：`resist_model` 全库唯一消费链是 VFX 渲染标记（SkillSystem.cpp:201-242 编码 → GPUSkillEffectSystem/GPUSkillEffectSystem 渲染），无任何穿透/削抗数值系统。274 灵根亲和（TypeA 穿透 5-20%）零实现；契约错绑使 251（TypeB_Shred）/272（TypeA）在 VFX 层触发错型表现。
- **H8 关键节点矩阵测试零断言**：`tests/fixtures/skill_specialization_keynodes.json:10-11` skill 2 key_nodes=[213,214,230,233,250,252,270]（**沿用错位清单**）；`SkillKeyNodeMatrixIntegrationTests.cpp:113-117` 仅断言「场上存在 Projectile」。7 个关键节点的特有效果无任何断言——错误实现被测试固化。
- **H9 触发链语义全错**：契约 233 Trigger → Baker:116-126 写 TriggerRule（listen=OnSkillHit、cast_skill_id=2、eff=0.4、ICD=2s、TargetPolicy=Victim）→ 点 233 后每次裂空斩命中自动追加一道 40% 效力裂空斩朝向受害者（ProcEngine.cpp:18 `trigger_depth >= 2` 防无限递归，但仍存在 2 层自触发链）。设计 254 = 消耗剑意施放时向**背后**触发。触发条件、方向、绑定节点全错。
- **H10 折返命中去重未重置致二次伤害落空（231 重叠打击彻底失效）**：`Projectile.hpp:74-92` 通过 `hit_cache` 和 `overflow_hits` 记录已命中实体，`ProjectileSystem.cpp:591` 在碰撞时若 `proj.HasHit(target)` 直接 `return true` 跳过。`BoomerangDeliverySystem.cpp` 状态机在 `HoverApex` 或 `Returning` 阶段**从未调用 `proj.ClearHits()`**（该系统的 ECS View 甚至根本没有获取 `Projectile` 组件）。导致去程击中过的敌人，在剑气折返飞回穿过时因已被记录而判定忽略，折返伤害被 100% 跳过！节点 231「重叠打击」（折返命中 More +15%...60%）在底层碰撞逻辑上根本不可能生效。
- **H11 剑意资源类型错绑与档位锁死（251 爆发机制断裂）**：`RendingWave.cpp:39-58` 错将基础资源 `SwordIntent` 混淆为进阶专精资源 `SwordFlow`（且 39-43 行凭空引入了根据 flow 增加攻速/范围/额外波次的非 GDD 机制）；在 51 行写死：若非 SwordFlow 则 `spend = DEFAULT_MAX_SWORD_INTENT (10)`。导致标准剑意玩家在拥有 5、6、7、8、9 层剑意时，调用 `ConsumeSwordIntent` 必然因未满 10 层而直接返回 false！GDD 规定的「≥5层剑意即可消耗全部并强化」被硬生生锁死为「非满层不可触发」。
- **H12 99 穿透压制分裂触发（211 碎裂之刃首击分裂失效）**：GDD 211 要求“每道剑气在命中第一个敌人或达到最大距离时分裂”。`RendingWave.cpp:70` 写死 `proj.pierce = true; proj.pierceCount = 99;`，而 `ProjectileSystem.cpp:625-636` 只有在穿透计数耗尽时才会标记 `hitLimitReached` 并触发 `OnDeathBehavior::Split`。因此剑气命中第一个敌人时会直接穿过去，绝不分裂，只能一直飞到最大射程消亡时才在空中分裂。

### MEDIUM

- **M1** desc 前置档位文本与实际 max_points 脱节 ×4：213「连锁反应2/3」(max5)、250「凝神3/4」(max5)、251「剑气烙印2/4」(max3)、275「灵根亲和2/4」(max5)。
- **M2** 210 扇形减伤缺失：每发伤害 -25/20/15% 无实现（RendingWave 无 per-wave 减伤）。
- **M3** 230 折返伤害 -30% 缺失：BoomerangComponent 折返✓，Returning 阶段无 -30% 处理。
- **M4** 200「飞行距离」无承载：delivery.range 未设置，实际射程=speed300×life1.2=360 码（fallback 400 未用上）。
- **M5** keystone_exclusion_groups {213,214} 为数据侧自主设计（GDD 未声明）；230 vs 234 折返/停滞形态冲突亦无互斥，需设计确认。
- **M6** UMR 孤立记录语义不符：`assets/data/modifier_v2/skill_spec_modifiers.json` id=2002103 `HeavyMomentum_Node213` = PhysicalDamage +22% percent-mult——与 213 desc（引爆散落剑气）完全无关。
- **M7** cost_affixes {213,214,251:HeavyMomentum}：251 被错误附加成本词缀（StatsSystem.cpp:486-490 会应用），需与设计确认。
- **M8** 契约 sword_intent_node_ids=[252]（设计点名 251/253/255）：UI 剑意交互标记指向错误节点。
- **M9** 270 附带异常缺失：flags 512 无消费者，「命中100%施加寒冷、满血敌强制冻结1s」零实现（仅标签转换）。
- **M10** GDD 内部矛盾沿袭：§3.2 Keystone 4 个超 §3.0 契约上限 2-3；气劲爆发 id 笔误 202（数据已正确用 203，建议修订 GDD）。
- **M11** 触发引擎指向策略缺失（254 回响斩背后释放无承载）：GDD 254 要求“向你背后方向触发一道额外的裂空斩”。`TriggerRuleComponent.hpp:15-20` 的 `TriggerTargetPolicy` 仅支持 `Self`, `Victim`, `Attacker`, `GroundTarget`，既无 `Backward` 策略，`ProcEngine.cpp` 亦无法在施法事件中解析相反发射角。建议在 `RendingWave::DoCast` 消耗剑意时直接发射反向（`-baseDir`）伴生剑气，或扩展 TriggerTargetPolicy。

## 4. JSON 与设计矛盾（另见 H1/H2/M1）

| 项 | GDD | 数据 | 性质 |
|---|---|---|---|
| 201 凝神 | 法力 -1...4点 | stat_modifiers = CD减+范围 | 枚举错位（C3） |
| 200 剑气纵横 | 宽度+距离 | t31 投射物+10/点 | 枚举错位灾难（C3） |
| 234 时空停滞 | Keystone | max_points=5 + role 未标 | 节点膨胀+role丢失（H1/C1） |
| 270/272 Transmuter | 各 1 点互斥 | max_points=3 + 272 role=Passive | 膨胀+互斥失效（H1/C4） |
| 270/272 转换修饰器 | 100% 对应元素转换 | damage_modifiers=[] 且代码未注入 | 伤害管线转换空转（C6） |
| 230 折返生命周期 | 最大射程折返 | lifeTime 1.2s < returnTimer 1.33s | 投射物早夭销毁（C7） |
| 231 折返二次受击 | 折返命中 More+15-60% | 回程未清理 hit_cache | 二次伤害判定直接跳过（H10） |
| 251 剑意门槛 | ≥5层剑意消耗全部 | 错绑SwordFlow致固定10层门槛 | 5-9层门槛施放锁死（H11） |
| 211 首击分裂 | 首击或最大距离分裂 | pierce=true 且 pierceCount=99 | 穿透压制首击分裂失效（H12） |
| 251/254 | SwordIntent/Trigger | Keystone+resist错绑 | 角色错位（C1） |
| 基底 | 无冷却/法力10 | CD 0.1 + 充能3 + 法力15 | 基底偏差（H2） |

## 5. 架构偏差（UMR §8 与管线断层）

- `skill_spec_modifiers.json` 全库唯一记录挂在技能2（M6），但 op 语义（+22% 物伤）与节点 desc 无对应，属于占位/遗留数据而非节点效果定义。
- Baker case 2 仍以硬编码节点分支承载形态效果（§8 违规，与技能1相同）；修正方向同技能1整改路线：Transmuter/Trigger/Trigger参数走契约数据，节点效果逐步迁移 modifier_v2 数据定义。
- 行为层 `RendingWaveNodes` 常量（RendingWave.cpp:19-23）将错误映射固化为代码命名，加剧可读性伤害。
- **投射物生命周期与折返状态机断层（C7/H10）**：`RendingWave.cpp` 设置投射物生命 1.2s，未感知 `BoomerangComponent` 去程 1.333s + 悬停 0.3s + 回程的总生命需求；且折返进入 `Returning` 阶段时未解耦清理 `Projectile::ClearHits()`，导致多段命中语义在底层被穿透防重逻辑吞噬。
- **元素转换在静态数据与动态管线中割裂（C6）**：`SkillSpecializationBaker` 仅变更 `effective_tags`，而 `DamagePipeline` 转换核心依赖 `damage_modifiers`（Convert 类型）。技能数据层缺少 `damage_modifiers` 且行为层未向实体注入转换器，造成“UI/标签以为转了元素，底层伤害仍然走纯物理”的静动态脱节。
- **资源抽象越权与混淆（H11）**：行为层越过基础 `SwordIntent` 抽象，直接硬编码 `BladeResourceKind::SwordFlow`（剑势专精资源），既在基础技能内塞入非 GDD 的流系增强，又通过 `DEFAULT_MAX_SWORD_INTENT` 常量斩断了普通剑意的门槛施放路径。

## 6. 符合项

- 28 节点全存在，id/名称/desc 文本与 GDD 逐字一致（气劲爆发 id 笔误已在数据侧正确化为 203）。
- `skill_2_tree.json` 渲染副本与内嵌树 x/y/prerequisites **完全一致**（自动比对 0 差异）；契约 min/max=28 与节点数一致。
- 210 投射物 +1/点、扇形发射；230 Boomerang 折返状态机；232 顶点牵引（pull_radius 120 / strength 250，HoverApex 阶段生效，BoomerangDeliverySystem.cpp:44-59）；211 sub_count=3 分裂链路存在（语义部分）。
- 274 前置「任意 Transmuter」语义正确（270|272 各 1 点即满足）。
- ProcEngine 递归深度保护存在（trigger_depth ≥ 2 拒绝）。
- `DEFAULT_MAX_SWORD_INTENT=10`（SkillDefs.hpp:29）与设计满层一致；GainSwordIntent/ConsumeSwordIntent 基础设施完备。
- 技能2 是全库唯一具备 UMR 记录的技能——数据化方向已有先例（尽管该记录语义不符）。

## 7. 整改路线（五阶段）

1. **契约重生成与数据修复（Blocker，先决）**：
   - 修复 `scripts/gen_skill_contracts.py` Keystone 默认推导（max_points==1 ≠ Keystone）与技能2 映射；
   - 重生成并核对：Trigger=254、Synergy=215、Transmuter=[270,272]、SwordIntent=[251,253,255]、Resist={274:TypeA_Penetration}、Keystone=[213,214,234,253]；
   - 在 `skills.json` 为 270/272 配置 `damage_modifiers`（Convert 1.0f 到 Cold/Lightning）；
   - 修正 200/201 stat_modifiers 枚举（移除 200 的 ProjectileCount，修复 201 法力扣减承载），为 202 补齐 PhysicalDamage (type=9)。
2. **底层投射物与折返状态机加固（Blocker）**：
   - 在 `BoomerangDeliverySystem.cpp` 中：当投射物进入 `Returning` 阶段时，显式获取其 `Projectile` 组件并调用 `proj.ClearHits()`，解封折返二次命中判定（H10）；
   - 在 `RendingWave.cpp` 中：若开启折返，`proj.lifeTime` 必须设为足够全程的时间（如 3.0s），彻底消除 1.2s 早夭销毁（C7）；
   - 211 碎裂之刃：首击敌人时立即触发分裂逻辑，不受 99 穿透压制（H12）。
3. **Baker+行为层按真语义重绑**：
   - `RendingWaveNodes` 常量改真实映射（TimeLock=234、IntentBurst=251、IntentGain=255、新增 Brand=250）；
   - 删除 250 VoidConvert、slot.id==1 硬编码以及非 GDD 的 SwordFlow 凭空增益；
   - 统一使用 `BladeResourceService::Consume` 正确判定 ≥5 层并按每层计算范围与暴击加成（H11）；
   - 254 回响斩在消耗剑意施放时直接发射反向（`-baseDir`）伴生波刃（40% 效力，CD 2s）（M11）；
   - 补 272 Baker case 与伤害转换挂载（C5/C6）；252 改 10-30% 返还法力；253 满层巨波（More+80%+50%物抗穿透）；255 概率回剑意。
4. **机制补全**：
   - 追踪小剑气（对接 SeekerComponent/HomingTag）、210 扇形减伤、230 折返 -30% 伤害在 Returning 阶段生效、231 重叠打击 More 增伤在 Returning 阶段生效；
   - 补齐 203/212/214(星环护体对接OrbitingSentinel)/215(灵剑追击)/233(真:牵引+击晕)/235/271/273/274/275；
   - flag 位空间按技能隔离审查（H4）。
5. **测试重建与护栏**：
   - 关键节点矩阵断言真实机制效果（覆盖投射物寿命充足不早夭、折返可造成二次伤害、真实元素转换伤害池计算、消耗与回复剑意）；
   - 新增契约不变量测试（角色绑定 vs GDD 清单）防错位复发。

---

## 附录：检索命令

```powershell
# 契约与数据
rg -n '"skill_id": 2' assets/data/skill_contracts_compact.json
rg -n '"node_id": (2[0-7][0-9])' assets/data/skills.json
# Baker 与行为层
rg -n "case 2:" src/game/systems/skill/SkillSpecializationBaker.cpp
rg -n "RendingWaveNodes" src/game/systems/skill/behaviors/RendingWave.cpp
# 折返生命与命中去重
rg -n "returnTimer|ClearHits" src/game/systems/skill/BoomerangDeliverySystem.cpp
# 伤害管线转换链与修饰器
rg -n "CONVERSION_ORDER|damage_modifiers" src/game/systems/combat/DamagePipeline.cpp
# 资源混淆
rg -n "SwordFlow" src/game/systems/skill/behaviors/RendingWave.cpp
# 枚举与共享机制
rg -n "ProjectileCount|CooldownReduction" src/game/foundation/components/Stats.hpp
rg -n "hasCatchRefund|catch_by_owner" src/game/systems/skill/BoomerangDeliverySystem.cpp
rg -n "trigger_depth" src/game/systems/skill/ProcEngine.cpp
# 测试
rg -n '"skill_id": 2' tests/fixtures/skill_specialization_keynodes.json
```

---

# 第二轮（跟进）审查 — 整改验证

- 日期：2026-09-08（同日跟进）
- 审查轮次：跟进审查（第 2 轮）
- 审查目标：验证工作区变更对第一轮全部发现项（Blocker×7 / High×12 / Medium×11）的整改情况，并对新增代码做同等严格审查
- 结论：**修改**（本轮新发现 Blocker ×1，High ×6，Medium ×6，Low ×3；首轮 30 项中 24 项完成整改、3 项部分整改、3 项未整改）
- 基准：同第一轮（`设计文档/职业设计草案_剑修.md` §3.2 行171-223，本轮以 Read 直读原文复核，排除终端编码干扰）

## 1. 输入与变更文件边界

`git status --short`：22 个文件，+1189/−202，未提交。范围对齐良好，除一处越界：

- `application/config/settings.json`（benchmarkScore/updatedAtUtc）：运行测试游戏时自动写出的运行时配置，与本次整改无关，**应在提交前还原**（M-F）。

## 2. 首轮发现项整改核对

| 首轮 | 项 | 状态 | 证据 |
|---|---|---|---|
| C1 | 契约角色全错绑 | ✅ | Trigger=254、Synergy=215、Transmuter=[270,272]、SwordIntent=[251,253,255]、Keystone=[213,214,234,253]、Resist={274:TypeA}；`gen_skill_contracts.py:428` 修复 Keystone 推导（`elif not explicit_keystone_ids and max_points == 1`）；skill_mechanics.json 28 节点全参数化 |
| C2 | 行为层错映射 | ✅ | 常量真映射（TimeLock=234 等）；旧 `slot.id==1` 跨技能减 CD 已删，测试负向断言 CD 不变 |
| C3 | 200/201/202 修饰符 | ✅ | 200/201 stat_modifiers 清空、202 补 type=9 |
| C4 | Transmuter 互斥失效 | ✅ | 250 移出 Transmuter，270/272 互斥生效（Keystone 组问题另见 M-B） |
| C5 | 272 零实现 | ✅ | Baker/RendingWave/Convert/加速链实现（部分死代码见 M-C） |
| C6 | 伤害转换空转 | ✅ | damage_modifiers Convert(1.0) + SkillModifierComponent 注入 + AilmentApplier Chill/Freeze/Shock |
| C7 | 折返半空销毁 | ✅ | lifeTime 3.5s（需求 2.744s）+ Returning 时 ClearHits |
| H1 | max_points 14 节点 | ✅ | 全部按 GDD 修正（另顺带修正 275：1→3）；以 GDD 原文逐项复核 |
| H2 | 基底数值 | ✅ | mana 15→10、CD 0.1→0、charge 3→0 |
| H3 | 251 无门槛 | ✅ | ≥5 层、消耗全部、每层范围 8%/暴击 3% |
| H4 | flag 8 冲突 | ⚠️ | 213 引爆已实现；但接剑门控用 `bc.skill_id != 2` 硬编码补丁，未做位隔离 |
| H5 | 分裂无追踪 | ✅ | homing 子弹 1.2s + Seeker/Homing/SkillModifierComponent 传递 + 213 Explode pierce count=5 |
| H6 | 停滞数值 | ✅ | 2s 风暴 / 0.2s tick / 0.5× 伤害，从 mech 读取 |
| H7 | 274 穿透零数值 | ✅ | DamageMitigationService `res -= pen/100`（5-20%，mech 驱动） |
| H8 | 测试重建 | ✅ | 新增 `tests/functional/RendingWaveNodes.cpp`（423 行，28 case 覆盖 200-275 全节点，数值断言具体）；key_nodes fixture 修正；触发测试 233→254（但该断言固化了错误轨道，见 H-A） |
| H9 | 253/254 触发链错位 | ⚠️ | DoCast 反向伴生波轨道正确（40%/ICD 2s/向背后）；**旧 ProcEngine 轨道未拆除 → 双发（H-A）** |
| H10 | 折返无 ClearHits | ✅ | BoomerangDeliverySystem Returning 分支 `proj->ClearHits()` |
| H11 | SwordFlow 混淆 | ❌ | 门槛与 CD 部分修复；**非 GDD 的 flow 增益/加波残留（H-F）** |
| H12 | 99 穿透压制分裂 | ✅ | 分裂/引爆路径与 pierce 策略调整，分裂子弹独立参数 |
| M1 | desc 前置档位 | ✅ | max 修正后文本一致 |
| M2 | 210 扇形减伤 | ✅ | 0.75/0.80/0.85（mech: damage_penalty_pt1/2/3） |
| M3 | 230 折返减伤 | ✅ | returning_damage_mult 0.7 × 231 (1.15/点) |
| M4 | 200 距离承载 | ✅ | range 700 + 70/点 |
| M5 | Keystone 互斥自主设计 | ❌ | 未获设计确认，反而扩大为 4 Keystone 全互斥（M-B） |
| M6 | HeavyMomentum_Node213 | ❌ | skill_spec_modifiers.json 未在变更中（未整改） |
| M7 | cost_affixes 含 251 | ❌ | 内嵌契约 251 仍带 `"cost_affix": "HeavyMomentum"`（M-A） |
| M8 | sword_intent_node_ids | ✅ | [251,253,255]，affects_sword_intent 同步 |
| M9 | 270 异常零实现 | ✅ | Chill/Freeze/Shock 契约 + AilmentEngine 映射 + DoHit 施加链 |
| M10 | GDD 内部矛盾 | ❌ | 文档未处理（属设计侧） |
| M11 | Trigger 无 Backward 策略 | ✅ | 采用整改路线建议的 DoCast 直接发射（但双轨并存 → H-A） |

统计：✅ 24，⚠️ 3（H4/H9/H11），❌ 3（M5/M6/M7；另 M10 文档项未动）。

## 3. 本轮新发现项

### Blocker

- **B1 [违反 code_standard §7.2 硬否决] 战斗核心逻辑使用字符串比较/字符串键分支**：
  - `src/game/systems/skill/behaviors/RendingWave.cpp` DoHit（约行606/666/709-713）：`b.id == "Frozen"`、`b.id.find("Shock") != npos`、`b.id.find("Chill")` —— `BuffType::Freeze/Shock` 枚举已存在且 `b.type` 可直接比较，字符串部分既冗余又违反 §7.2。
  - `src/game/systems/combat/DamagePipeline.cpp:1072-1076`：`effects->Get("QiBrand")` 在伤害管线（每次伤害结算）按字符串键查烙印层数；`Buff.hpp:172` 的 `Get(const std::string&)` 为线性遍历字符串比较，`Get(BuffId)` 重载（:182）内部仍转字符串。
  - 修复建议：给 `BuffEffect` 增加数值类别（uint8 kind 或专用 BuffType），`ActiveEffectsComponent` 提供 type 查找重载；DoHit 全部改为枚举比较。

### High

- **H-A 254 回响斩双发双轨**：`SkillSpecializationBaker.cpp:118-126`（Bake）与 `:539-570`（分配时）的通用逻辑仍为契约 `trigger_nodes[0]=254` 写 `TriggerRule`（listen=OnSkillHit、cast_skill_id=2、eff=0.4、ICD=2.0、TargetPolicy=Victim）→ `ProcEngine.cpp:177` TriggerCast 走**完整 DoCast** 朝受害者；同时 `RendingWave::DoCast` 消耗剑意时又直接反向发射。结果：触发条件（每命中 vs 设计「消耗剑意施放时」，GDD 行213）、方向（朝受害者 vs 设计「向背后」）、次数（双发）三重偏离。`SkillBehaviorGuardTests` 的 254 dispatch 断言固化了错误轨道。修复：契约 254 `trigger_skill_id` 置 0 或 Baker 跳过 254，仅保留 DoCast 轨道；或为 TriggerTargetPolicy 增加 Backward 并删除 DoCast 轨道（二选一，不可并存）。
- **H-B 255 缺「消耗剑意」限定**：`RendingWave.cpp:495-500` 仅判断 `pts_255 > 0` 即对每次命中 roll 回剑意；GDD 行214 限定「**消耗剑意的**裂空斩每命中一名敌人」。当前 40% 回响斩与普通施放命中均触发，剑意回复率远超设计并与 251/254 形成循环加速。修复：DoCast 消耗剑意时在 payload_context 或投射物组件打标记，DoHit 校验后再 roll。
- **H-C payload_context 丢失全局乘区**：`RendingWave.cpp:225-233/335-343/425-433` 三处自建 payload_context，`.more_damage = moreDamageMult`（:146，来自 Baker 节点级乘区）——未乘 `stats->damage_multipliers[0]`（玩家全局 More；标准路径 `SkillSystem.cpp:1785` 为 `ctx.more_damage = exec.snapshot.stats.damage_multipliers[0]`）。玩家装备/天赋的全局增伤对本技能全部投射物（含 hover/sentinel/spirit）失效。修复：`.more_damage = stats->damage_multipliers[0] * moreDamageMult * scaleEffectiveness`。
- **H-D 触发伤害未隔离 DoHit 副作用**：271 冰爆（`RendingWave.cpp:583-589`）与 273 连锁（:645-651）以 `skill_id=2 + Tag::Hit` 直接 `ResolveDamage`；`DamagePipeline.cpp:289-296` 对非 DoT 命中统一派发 OnSkillHit，而 `SkillSystem.cpp:917-918` 的 hitFunc **不检查 `trigger_depth`**（事件已携带该字段，:286）。冰爆/连锁伤害命中会再次执行 DoHit 全部 per-hit 节点：250 烙印 roll、255 回剑意、275 传染、213 引爆判定重复触发（效果放大）。element_tag 无 Cold/Lightning 恰好挡住冰爆/连锁的自递归，但这是隐式依赖而非显式防护（对照 `FlowingThrust.cpp:550` 的显式防递归先例）。修复：DoHit 入口校验 `trigger_depth == 0` 或主命中标记。
- **H-E 253 用哨兵值传递布尔语义**：isObliteration 分支 `proj.snapshot.armor_pen += 500.0f`，`DamageMitigationService.cpp:116` 以 `proj->snapshot.armor_pen >= 500.0f || (proj->arcWidth >= 100.0f)` 判定后 `effective_armor *= 0.5`。armor_pen 数值语义被污染（同字段 274 用 5.0=5%，单位混乱；若任何系统消费投射物 snapshot 的 armor_pen 即变成 500% 穿透）；`arcWidth >= 100` 为死条件（arcWidth 默认 0，`Projectile.hpp:32`，全库仅 `GameplayRenderAdapter.cpp:460` 渲染消费）。mech 已定义 `253.physical_ignore_res_pct=50.0` 却未消费。修复：DeliveryArchetypes 增加 `bool ignore_resist` 标志（或专用 flag 位），删除 arcWidth 条件。
- **H-F 首轮 H11 部分未整改——SwordFlow 非 GDD 增益残留**：`RendingWave.cpp:258-259` `if (currentSwordFlow >= 5) extraWaves...`（flow 5/8/10 加波）与 :174-182 每层流势速度×1.04/半径×1.03 增益仍在，GDD §3.2 无此机制。另 251 满层时 `seven_star_shared::ResetSkillCooldown(kFlowingThrustSkillId)` 重置流云刺 CD 在 GDD §3.2 无出处，需与 §3.1 交叉确认（若确认为联动设计应在设计文档补记）。

### Medium

- **M-A 首轮 M7 未整改**：skills.json 内嵌契约 251 仍带 `cost_affix: HeavyMomentum`（StatsSystem.cpp:486-490 会应用），首轮已判定为待设计确认的错绑。
- **M-B 首轮 M5 以扩大互斥代替设计确认**：`keystone_exclusion_groups` {213,214,234,253} 全部挂 group 1 → 4 个 Keystone **跨分支全互斥**（分支 A 与分支 C 的 Keystone 亦互斥），GDD §3.2 无任何 Keystone 互斥表述。需设计裁决：GDD §3.0 上限、§3.2 四 Keystone 与互斥关系三者对齐。
- **M-C mech 参数定义未消费（数据驱动断链）**：`253.physical_ignore_res_pct`（代码走哨兵）；`211.split_damage_mult=0.5`/`split_speed_mult=0.8`（恰好等于 `Projectile.hpp:119-120` 默认值但从未读取）；`273.chain_damage_pct=30.0`（`RendingWave.cpp:644` 硬编码 0.30f）；`272.speed_bonus_pct=100.0`（Baker `del.speed *= 2.0f` 设置后 DoCast 硬编码 `baseSpeed=300.0f` 不读 delivery.speed → Baker 加速为死代码）。后续数值调参不会生效，易误导。
- **M-D 死标志位**：Baker 设置的 flags 2(212)/256(231)/512(233)/1024(235)/2048(250)/8192(252)/16384(203)/131072(255)/1<<18..1<<23(270,271,272,273,275) 无任何读取者（效果经 getPts 直查点数实现）；实际消费位仅 1,4,8,16,32,64,128,4096,32768,65536,1<<22。flags 与 getPts 双轨并存，应统一为一种传递方式。
- **M-E GetFloat 热路径字符串键**：`SkillMechanicsRegistry.cpp:76-91` 三层 unordered_map + std::string key，DoHit per-hit 调用 2-4 次（250/255/273）。无堆分配（字面量 SSO）未达 §7.2 硬否决，但建议节点参数在 Bake 期烘焙进 profile，或按 (skill_id,node_id) 预取缓存。
- **M-F 越界触及**：settings.json 应还原（见第 1 节）。

### Low

- **L1** 233 击晕施加于 apex 结束（hover_timer 到期）而非「消散时」，语义近似可接受，建议注释说明。
- **L2** 275 isKilled（`RendingWave.cpp:663` `any_of<KilledTag> || health<=0`）正确性依赖 OnSkillHit 在伤害应用后派发这一管线顺序，成立但耦合脆弱，建议注释固化或显式传参。
- **L3** `SkillRegistry.cpp:549` ValidateBladeAscendantResistCoverage 在无技能 1-9 契约时早退——测试宽容化，可能掩盖契约加载不全的问题，建议限定于测试构建。

## 4. 质量与风险评估

- 架构方向正确：契约重生成器修复推导缺陷、skill_mechanics.json 全参数化、行为层常量真映射、测试从「零断言」重建为 423 行全节点覆盖，符合整改路线 5 阶段要求。
- 主要风险集中在**触发链与乘区链**：254 双轨（H-A）、255 无限定（H-B）、触发伤害未隔离（H-D）三者叠加会显著放大剑意回复与命中触发效果，实际数值远超 GDD 预期。
- §7.2 硬否决（B1）为规范级阻塞，修复成本低（枚举已存在）。
- 编码说明：PowerShell 管道重定向会产生 GBK 乱码假象，Read 直读源文件中文正常，源文件编码无问题。

## 5. 最佳实践建议

1. 哨兵值/几何字段承载布尔语义（H-E）改为显式标志字段；`arcWidth` 这类渲染参数不得进入逻辑判定。
2. 事件携带的 `trigger_depth` 应在 hitFunc 入口统一校验（H-D），而不是依赖 tag 巧合防递归；在 `FlowingThrust.cpp:550` 先例基础上形成通用防护点。
3. 自建 payload_context 的行为必须以 `stats->damage_multipliers[0]` 为基（H-C），可提取公共 helper 避免每个行为重抄。
4. 机制参数（mech）与代码硬编码应单向：要么全量接线（M-C），要么删除死参数，避免「看起来可调」。
5. flags 传递与 getPts 直查二选一（M-D）；死位清除。

## 6. 剩余风险

- GDD §3.0/§3.2 的 Keystone 数量上限与互斥矛盾（M10/M-B）未裁决前，4 Keystone 全互斥只是临时兜底。
- GPU 门（nmd.tests.gpu.hardware）失败：`dynamic_combat_emissive` High tier Paired GI delta 0.000589/0.000592 低于阈值 0.001、OccluderExtractPass High tier p95 0.392ms 超 0.3ms 预算（Ultra tier 通过）。属渲染 GI 基线/性能问题（RTX 4070 SUPER 真实硬件），**与本次技能/战斗变更无关**，移交 rendering track 跟进。
- 275 传染后传染目标再被其他源击杀的连锁语义（异常传染不造成伤害，无递归，已确认）。

## 7. 验证证据

- `build.bat`（RelWithDebInfo, j=7）：构建成功，exit=0。
- `ctest --test-dir build -C RelWithDebInfo`：21 项测试，20 通过、1 失败（仅 nmd.tests.gpu.hardware，见第 6 节）；skill/unit/integration/progression 标签全部通过。
- 数据一致性：skill_contracts_compact.json 与 skills.json 内嵌契约逐项比对一致；skill_2_tree.json 前置调整（271←203 需 2 点、273←272 需 1 点）与 GDD 一致。

## 8. 下一步动作

1. **[Blocker]** B1 枚举化 DoHit/烙印查找（§7.2）。
2. **[High]** H-A 拆除 254 旧 TriggerRule 轨道并修正 SkillBehaviorGuardTests 断言；H-B 255 加消耗剑意标记；H-C payload_context 乘全局乘区；H-D DoHit 入口校验 trigger_depth；H-E 253 改显式标志；H-F 删除 SwordFlow 增益/加波（满层重置 CD 待设计确认）。
3. **[Medium]** M-A/M-B/M6/M10 提交设计侧裁决清单；M-C 补接线或删死参数；M-F 还原 settings.json。
4. GPU 门失败移交 rendering track（非本变更范围）。

---

# 第三轮（修复验证）审查 — 结论：提交

- 日期：2026-09-08
- 审查轮次：修复验证（第 3 轮）
- 审查目标：验证第二轮 Blocker/High/Medium 发现项的修复；复审采用三路并行子代理（行为层簇 / Baker+契约数据簇 / 管线+触发防护簇）+ 主代理收尾，统一构建后全量测试验证
- 结论：**提交**（第二轮 12 项代码发现项全部闭环；M-A 经证据复核回滚为「保留待设计确认」；剩余项均为设计侧/其他技能跟进，不阻塞）

## 1. 修复闭环核对

| 第二轮发现 | 状态 | 修复方式与证据 |
|---|---|---|
| B1 [Blocker] §7.2 字符串分支 | ✅ | RendingWave.cpp DoHit 全部枚举化：Chill→`BuffType::SpeedDown`（AilmentEngine.cpp:145-146 映射证据：SpeedDown/Slow/Chill 同族）、Frozen→`BuffType::Freeze`、Shock→`BuffType::Shock`；Buff.hpp 新增 `BuffKind` 数值类别 + `ActiveEffectsComponent::GetByKind`（整数线性查找，无字符串构造），QiBrand 施加处（:492-504）设置 kind，DamagePipeline.cpp:1072 改 `GetByKind(BuffKind::QiBrand)`。rg 验证：RendingWave/DamagePipeline 无 `b.id ==`/`b.id.find`/`Get("QiBrand")` 残留 |
| H-A 254 双发 | ✅ | 数据侧禁用引擎轨道：254 `trigger_skill_id` 2→0（skills.json:1274 与 compact 同步，`gen_skill_contracts.py --check --check-idempotency --check-determinism` 全 PASS——生成器对该字段为透传设计无需改动）；Baker:124/:571 既有 `trigger_skill_id != 0` 过滤自动生效；SkillBehaviorGuardTests 254 断言负向化（无引擎派生 SkillExecution、无 254 cooldown、契约锁定 trigger_skill_id==0/eff≈0.4/ICD≈2.0）；DoCast 反向轨道保留 |
| H-B 255 无限定 | ✅ | DoCast 消耗剑意（consumedIntent>0）时对主波 `emplace<IntentConsumedCastTag>`（DeliveryArchetypes.hpp:247-257，standard_layout+trivially_destructible static_assert）；255 roll gate `pts_255 > 0 && all_of<IntentConsumedCastTag>(attacker)`（:520）；254 回响斩显式传 false、214 哨兵锚定玩家本体不参与 roll |
| H-C 全局乘区丢失 | ✅ | 5 处全部并入 `stats->damage_multipliers[0] * moreDamageMult`：214 轨道 :225、sentinel :241、hover :328、主波 :348、215 灵剑 :447（与 SkillSystem.cpp:1785 标准路径一致） |
| H-D 触发伤害扩散 | ✅ | 双层防护：①SkillSystem.cpp:921 hitFunc `evt.trigger_depth == 0` 门（覆盖 TriggerCast 链 depth≥1）；②新增 `Tag::SecondaryHit = 1ULL<<38`（TagRegistry.hpp:44 Mechanism 段），271 冰爆/:616 与 273 连锁/:685 生产侧打标，hitFunc 与 Hit-tag 资源分支（命中回剑意 :634、御剑步回蓝 :699）均排除 SecondaryHit——调查证实 271/273 直连 ResolveDamage 时 depth 恒 0（ResolveEventAttackerContext 从玩家实体解析 cast_id=0），单靠 depth 防护不覆盖，故需标记双保险 |
| H-E 253 哨兵值 | ✅ | Projectile.hpp:136 新增 `bool ignore_resist = false;`（与 armor_pen 同链路传递），RendingWave :286 设置，DamageMitigationService.cpp:121 消费；`armor_pen >= 500` 哨兵与 `arcWidth >= 100` 死条件删除；mech `253.physical_ignore_res_pct=50.0` 接线为 `effective_armor *= (1 - pct/100)`（JSON 缺失回退 50，语义等价） |
| H-F SwordFlow 残留 | ✅ | flow 每层速度/半径增益与 5/8/10 加波删除；同步删除 tests/functional/SkillBehaviors.cpp 4 个对应 SUBCASE（1463→1282 行）；251 满层重置流云刺 CD 保留并注释「待设计确认」 |
| M-A 251 cost_affix | ↩️ 回滚 | 修复尝试删除后 `CombatAntiMetaLayerTests.cpp:99` 失败（`with_stacked == with_single`）——该测试固化反堆叠递减系统：213+251 同源 HeavyMomentum 叠加 0.44 需被 clamp。证据复核：GDD 行211 节点名即「重势」（HeavyMomentum 直译），绑定命名自洽；**恢复原绑定**（compact cost_affixes + skills.json 内嵌契约），正式裁决移交设计侧（同 M7 首轮定性） |
| M-C 参数断链 | ✅ | 273 `chain_damage_pct`（:662-666 GetFloat/100）；272 Baker 改 `del.speed = 1 + speed_bonus_pct/100`（:350-361）+ 关联修正 case2 默认 delivery.speed 600→1.0（DoCast 改为 `baseSpeed = 300×delivery.speed`，600 为旧死值防 180000 爆速）；211 split_damage_mult/split_speed_mult 施法期读 mech 写父投射物（:296-316，SpawnSplitProjectiles 整结构克隆父字段全链生效，ProjectileSystem.cpp 零改动） |
| M-D 死标志位 | ✅ | 12 位删除（2/256/1024/2048/8192/16384/131072/1<<18..1<<21/1<<23，逐位 rg 验证无消费者），512(233) 因 stun_on_apex_end 实际消费（:364）保留，1<<22(253 消费) 保留 |
| M-F settings.json | ✅ | benchmarkScore/updatedAtUtc 已还原（内容 diff 为空，仅行尾规范化） |

收尾补齐（子代理白名单受限，由主代理完成）：SkillSystem.cpp 三处 SecondaryHit 过滤；SkillBehaviors.cpp 4 个 flow SUBCASE 删除；`EngineTriggerMatrixSkillIds()`（11 技能，排除技能2）新增于 SkillKeyNodeMatrixTestHelpers.hpp，SkillKeyNodeMatrixIntegrationTests.cpp:219/:259 基线改用（254 退出引擎触发矩阵）。

## 2. 白名单偏差记录（子代理披露，均接受）

1. `Projectile.hpp:131-136`：ignore_resist 定义落位（消费侧契约锁定 `proj->ignore_resist`，DeliveryArchetypes 方案收敛为 Projectile 字段 + IntentConsumedCastTag 标记，注释已说明）。
2. `TagRegistry.hpp:44`：SecondaryHit 枚举（H-D 生产侧所需，主代理任务书指定）。

## 3. 验证证据

- `build.bat` RelWithDebInfo j=7：成功，exit=0（含全部修复与收尾）。
- `ctest`（排除 gpu.hardware）：**20/20 通过**，其中 ci.nonperf 聚合 1310 cases（106329 断言）、performance/combat.perf.baseline 通过。
- 过程中的两次失败均已归因闭环：①AntiMeta 失败 → M-A 回滚恢复（见上表）；②ParticleTrailBenchmark dispatchOverheadMs 0.363>0.2 → 并行负载波动，单独复跑通过。
- `nmd.tests.gpu.hardware` 仍失败（第二轮已归因：渲染 GI 配对基线，与技能/战斗无关，rendering track 跟进）。
- 静态复核：rg 确认无字符串比较/哨兵值/死 flags 残留；ignore_resist 链路（定义→设置→消费）与 IntentConsumedCastTag 链路完整；5 处 payload 全局乘区覆盖。

## 4. 剩余事项（不阻塞提交）

- **设计侧裁决清单**：①M-A 251 HeavyMomentum 绑定（倾向保留：GDD 命名对应 + 反堆叠测试依赖）；②M-B 4 Keystone 全互斥 group1 与 GDD §3.0/§3.2 矛盾；③M6 skill_spec_modifiers.json HeavyMomentum_Node213 语义；④251 满层重置流云刺 CD 的 GDD 出处补记。
- **技能1 后续项**（超范围）：FlowingThrust.cpp:378-384 仍存 `b.id.find(...)` 字符串比较（§7.2 同款问题），建议后续按 RendingWave 模式整改。
- **记录项**：分裂/爆炸子投射物整结构克隆会继承 ignore_resist（253+211 组合下子剑气无视抗性，语义合理待设计确认）；SkillSystem.cpp:1787 crit_chance 疑似未归一化（未验证，仅记录）——**更正（2026-09-12，B1-18）：已核实 `crit_chance` 为 0..1 分数制，旧记录单位前提作废**：`Stats.hpp:115` 默认 0.05、`AttributePipeline.cpp:773-775` 写回时 /100，`DamagePipeline.cpp:1324-1327` 注释明示分数制且 `:1606`/`:1850` 直接比较，`SkillSystem` 三处直拷（`:1532`/`:1600`/`:2096`）均正确；技能3 373/技能6 633 的 trigger_skill_id=2 为既有数据未核实。

## 5. 最终结论

第二轮全部 Blocker/High 代码发现项修复并通过全量测试验证，代码规范硬否决（§7.2）已消除，数据驱动断链已闭合。**结论：提交**（附第 4 节设计侧跟进清单）。
