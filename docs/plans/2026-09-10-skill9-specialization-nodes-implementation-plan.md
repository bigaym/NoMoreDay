# 技能9「绝影绝剑」专精节点重构实施计划

- 依据审查：`docs/reviews/2026-09-10-skill9-specialization-nodes-review.md`（Round 1/2 结论：修改）
- 设计权威：`设计文档/职业设计草案_剑修.md` §3.9（:564-616）
- 参照实现：`docs/plans/2026-09-10-skill8-specialization-nodes-implementation-plan.md`、`assets/data/skills.json` 技能8（:4895-5632）
- 产品决策（用户确认）：**新增协同节点，技能9 专精树为 29 节点**（28 设计节点 + 1 个「绝影期间与御剑·回旋联动」协同节点）

---

## 1. 实施思路

按「接口冻结 → 数据/契约 → 烘焙/行为/管线 → 测试 → 文档」分层推进。

核心目标：把技能9 从「瞬时位移 + 异种反击窗口」（`PhantomFlashComponent.counter_window/triggered`）整体替换为「3 秒绝影形态 + 免死/逆脉资源循环 + 附魔窗口」体系，并清除审查点名的全部硬编码特判（ProcEngine 规则9、DamagePipeline 反击格挡、CombatSystem 瞬移反击、三处 scope 特判、触发链抑制守卫）。

数据流（沿用技能8 既有链路，不新增系统）：

```
skill_contracts_compact.json ──gen_skill_contracts.py──▶ skills.json.skill_contract
                                                              │
SkillMechanicsRegistry ──┐                                    ▼
                         ├─▶ SkillSpecializationBaker::Bake ──▶ BakedSkillProfile.delivery.trance
节点契约 ────────────────┘                                    │
                                                              ▼
                                        PhantomTrance 行为 DoCast/Update + TriggerRule 契约
```

新增机制优先复用既有系统：

| 需求 | 复用 |
| --- | --- |
| 免死锁血 | `CombatSystem::ApplyDamage` 死亡判定点（BladeFormation 353 不死先例） |
| 禁疗/上限锁定 | `RegenerationSystem` / `BloodSea::ApplyHealing` / `AilmentEngine` 三处治疗点 + 组件标记 |
| 护盾 | `CombatStats.barrier` + `BarrierComponent`（BladeWard 先例） |
| 附魔窗口 | `SkillModifierComponent.damage_modifiers` GainExtra（旧 970 先例） |
| 诅咒 | `BuffEffect` 六系伤害 PercentMult（剑阵 632 先例） |
| 剑意 | `SkillSystem::GainSwordIntent` |
| 冷却返还 | `ActiveSkillsComponent.slots[].cooldown` 直改 |
| 元素异常 | `AilmentApplier::Apply`（Chill/Freeze/Shock） |
| 死亡飞剑 | `Projectile` 实体生成（FlowingThrust 先例） |
| 触发 935 | `TriggerRuleComponent` 契约扩展（20% + 近战 + 逆脉窗口） |

---

## 2. 接口冻结（WS0，必须先落地）

### 2.1 TriggerContract / TriggerRule 扩展（N3 修复）

`src/game/foundation/data/SkillContract.hpp`：

```cpp
enum class TriggerWindow : uint8_t { None = 0, PhantomTrance = 1, DeathSeal = 2 };

struct TriggerContract {
  uint32_t trigger_skill_id = 0;
  float effectiveness = 1.0f;
  float range_mult = 1.0f;
  float internal_cooldown = 0.0f;
  bool consumes_mana = false;
  bool requires_crit = false;
  float base_chance = 1.0f;          // 新增
  bool requires_melee_hit = false;   // 新增
  TriggerWindow required_window = TriggerWindow::None; // 新增
};
```

`src/game/foundation/components/TriggerRuleComponent.hpp` 的 `TriggerRule` 增加同名字段（`#include "game/foundation/data/SkillContract.hpp"`）：

```cpp
float base_chance = 1.0f;                 // 已存在
bool requires_melee_hit = false;          // 新增
TriggerWindow required_window = TriggerWindow::None; // 新增
```

`ProcEngine::DispatchEvent` 过滤阶段新增（在 `requires_crit`/`required_skill_id` 之后）：

```cpp
if (rule.requires_melee_hit && (event.tags & Tag::Melee) == 0) continue;
if (rule.required_window != TriggerWindow::None) {
  auto* pt = registry.try_get<PhantomTranceComponent>(listener);
  if (!pt || pt->remaining <= 0.0f) continue;
  if (rule.required_window == TriggerWindow::DeathSeal && !pt->params.death_seal) continue;
}
```

`SkillSpecializationBaker::SyncTriggerRules` 与 `Bake` 契约写入同步透传三字段；`SkillRegistry.cpp` 解析 `base_chance` / `requires_melee_hit` / `requires_window`（字符串 `"None"|"PhantomTrance"|"DeathSeal"`）。

### 2.2 新组件与烘焙参数

`src/game/foundation/components/SkillDefs.hpp`：

- 删除 `PhantomFlashComponent`（:929-937），新增：

```cpp
// 技能9 绝影形态烘焙参数（由 Baker case 9 写入）
struct PhantomTranceParams {
  float duration_sec = 3.0f;
  float move_speed_pct = 20.0f;        // 902 追加
  float dodge_pct = 0.0f;              // 902
  float recovery_pct = 0.0f;           // 974 冷却缩减
  float burst_damage_mult = 1.0f;      // 976（半径同倍率）
  bool  cheat_death_hold = false;      // 977
  float rebirth_lost_pct = 0.0f;       // 978 免死触发时
  float rebirth_flat_pct = 0.0f;       // 978 未触发时
  float ward_pct = 0.0f;               // 979
  bool  void_body = false;             // 980
  float void_body_hp_cost_pct = 0.15f; // 980
  float weaken_on_pass_pct = 0.0f;     // 913
  float void_gift_mana_per_sec = 0.0f; // 914
  float void_gift_dr_pct = 0.0f;       // 914
  bool  death_seal = false;            // 981
  float death_seal_hp_cap_pct = 0.33f; // 981
  float death_seal_damage_more_pct = 33.0f; // 981
  float last_stand_crit_pct = 0.0f;    // 982 每缺失1%
  int   death_spiral_count = 0;        // 983
  float death_spiral_damage_pct = 0.0f;// 983 每柄=损失上限*该值
  float bloodthirst_pct = 0.0f;        // 984
  float atk_cast_speed_pct = 0.0f;     // 934
  bool  blink = false;                 // 985
  bool  echo_synergy = false;          // 993
  int   intent_per_sec = 0;            // 987
  float sword_step_dodge_pct = 0.0f;   // 988（形态内已处于御剑步时）
  float sword_step_drain_mult = 1.0f;  // 988
  float time_reversal_sec = 0.0f;      // 954
  float focus_mana_reduce_pct = 0.0f;  // 955
  Tag   transmuter_tag = Tag::None;    // 989 Cold / 972 Lightning
  float frost_amp_pct = 0.0f;          // 990
  float overload_speed_pct = 0.0f;     // 973
  float enchant_pen_per_intent_pct = 0.0f; // 991
  float enchant_pen_cap_pct = 40.0f;   // 991
  bool  enchant_refresh_on_kill = false;   // 992
  float cooldown_flat_reduce = 0.0f;   // 986（烘焙期直接作用于 effective_cooldown）
};
```

- `BakedDeliveryParams` 末尾增加成员：`PhantomTranceParams trance;`（唯一写入方 Baker case 9，唯一读取方 PhantomTrance 行为）。
- 新增运行时组件：

```cpp
struct PhantomTranceComponent {
  entt::entity owner = entt::null;
  uint64_t cast_id = 0;
  PhantomTranceParams params{};
  float duration = 3.0f;
  float remaining = 3.0f;
  float elapsed = 0.0f;
  bool  lethal_triggered = false;   // 免死已消耗
  float original_max_hp = 0.0f;     // 逆脉前上限
  float damage_dealt_accum = 0.0f;  // 984 统计
  float intent_tick = 0.0f;         // 987
  float spiral_tick = 0.0f;         // 983
  float pulse_tick = 0.0f;          // 989/972 冰霜/雷盾脉冲
  float weaken_tick = 0.0f;         // 913 检测
  float last_stand_buff_value = 0.0f; // 982
  float enchant_remaining = 0.0f;   // 附魔窗口
  Tag   enchant_tag = Tag::None;
  bool  ending = false;             // 正在执行结束结算
};

inline bool IsDeathSealActive(const PhantomTranceComponent& pt) {
  return pt.params.death_seal && pt.remaining > 0.0f && !pt.ending;
}
```

- Buff id 常量（`BuffIds.hpp/.cpp`）：`PhantomTranceStealth = "phantom_trance_stealth"`、`PhantomTranceDeathSeal = "phantom_trance_death_seal"`、`PhantomTranceLastStand = "phantom_trance_last_stand"`、`PhantomTranceTempo = "phantom_trance_tempo"`、`PhantomTranceEnchant = "phantom_trance_enchant"`、`PhantomTranceWeaken = "phantom_trance_weaken"`、`PhantomTranceEcho`（993 规则不需要 buff）。删除 `PhantomFlashShadowHide`。

### 2.3 29 节点与 ID 分配

保留同名节点 ID：`902 身轻如燕`、`913 灵流穿透`、`914 虚境馈赠`、`934 剑随心动`、`935 逆命反噬`、`954 时光逆流`、`955 全神贯注`、`972 疾空惊雷`、`973 过载护盾`、`974 空明心境`。
新节点用 975-993；旧独占 ID（900/901/903/910/911/912/930/931/932/933/950/951/952/953/970/971）全部删除。

| ID | 名称 | 层 | 上限 | 前置 | 契约角色 |
| --- | --- | --- | --- | --- | --- |
| 975 | 延命 | 基础 | 4 | 根 | Passive |
| 976 | 气旋爆发 | 基础 | 4 | 根 | Passive |
| 902 | 身轻如燕 | 基础 | 3 | 根 | Passive |
| 974 | 空明心境 | 基础 | 4 | 根 | Passive |
| 977 | 向死而生 | A | 1 | 975@2 | Keystone |
| 978 | 浴血重生 | A | 4 | 977@1 | Passive |
| 979 | 绝影护甲 | A | 3 | 975@4 | Passive |
| 980 | 虚灵之躯 | A | 1 | 979@2 | Keystone |
| 913 | 灵流穿透 | A | 3 | 980@1 | Passive |
| 914 | 虚境馈赠 | A | 3 | 979@1 | Passive |
| 981 | 逆脉 | B | 1 | 976@3 | Keystone |
| 982 | 孤注一掷 | B | 4 | 981@1 | Passive |
| 983 | 死亡螺旋 | B | 3 | 981@1 | Passive |
| 984 | 嗜血本能 | B | 1 | 981@1 | Passive（显式 passive） |
| 934 | 剑随心动 | B | 3 | 982@2 | Passive |
| 935 | 逆命反噬 | B | 1 | 982@3 | **Trigger** |
| 985 | 破空一闪 | C | 1 | 976@2 | Passive（显式 passive） |
| 986 | 缩地成寸 | C | 4 | 985@1 | Passive |
| 987 | 意随神行 | C | 3 | 902@2 | Passive（剑意交互） |
| 988 | 御剑化影 | C | 3 | 985@1 | Passive（剑步交互） |
| 954 | 时光逆流 | C | 1 | 987@3 | Keystone |
| 955 | 全神贯注 | C | 3 | 954@1 | Passive |
| 989 | 天山雪隐 | D | 1 | 974@2 | **Transmuter**（Cold） |
| 990 | 凛冬附魔 | D | 3 | 989@1 | Passive |
| 972 | 疾空惊雷 | D | 1 | 974@2 | **Transmuter**（Lightning） |
| 973 | 过载护盾 | D | 3 | 972@1 | Passive |
| 991 | 意念穿透 | D | 4 | 974@2 | Passive（`resist_model=TypeD_StatToPenetration`、`scope_policy=GlobalWhileBuffActive`） |
| 992 | 灵气反哺 | D | 1 | 991@3 | Passive（显式 passive） |
| 993 | 影剑回响 | 协同 | 1 | 987@1 | **Synergy** |

数据补充要求：
- 节点 `x/y` 坐标由数据工作流按 5 列（基础/A/B/C/D）布置，风格对齐技能8。
- `icon_id` 由 `scripts/sync_skill_node_icon_ids.py` 生成；新节点纹理先按语义复用被删除旧节点的 PNG（见 WS6）。
- 990 加成仅对 frozen/chilled 目标生效（结算时判定异常状态）。
- 991 前置「任意转质」无法用 AND 前置表达，采用与转质同源的 `974@2`；未点转质时效果天然空转（附魔窗口不存在），在计划风险中记录。

### 2.4 契约（compact）目标形态

```json
{
  "skill_id": 9,
  "min_nodes": 29, "max_nodes": 29,
  "max_transmuters": 1, "max_triggers": 1,
  "has_sword_intent_node": true, "has_synergy_node": true,
  "transmuter_node_ids": [989, 972],
  "synergy_node_ids": [993],
  "sword_intent_node_ids": [987, 991],
  "sword_step_node_ids": [988],
  "keystone_node_ids": [977, 980, 981, 954],
  "passive_node_ids": [984, 985, 992],
  "keystone_exclusion_groups": { "989": 2, "972": 2 },
  "resist_models": { "991": "TypeD_StatToPenetration" },
  "scope_policies": { "991": "GlobalWhileBuffActive" },
  "trigger_nodes": [
    { "node_id": 935, "trigger_skill_id": 8, "effectiveness": 1.0,
      "internal_cooldown": 0.5, "consumes_mana": false,
      "base_chance": 0.2, "requires_melee_hit": true, "requires_window": "DeathSeal" }
  ]
}
```

（删除旧包中的 `cost_affixes`。）

### 2.5 基础技能数据

```json
{
  "id": 9, "name_key": "绝影绝剑",
  "desc_key": "立即进入绝影无形，持续 3 秒；期间移速提高 20%，受到致命伤害时锁血为 1 并提前结束。结束时爆发无形剑气。",
  "mana_cost": 30.0, "cooldown": 15.0,
  "tags": ["Buff","Defensive","Movement","SwordSkill"],
  "base_damage": 50.0, "weapon_damage_mult": 3.0, "added_damage_effectiveness": 1.0,
  "icon_id": 698747719,
  "params": { "burst_radius": 160.0 }
}
```

`tags` 必须以 `TagRegistry` 实际注册名为准（`sword_skill` 修正为 `SwordSkill`；`Buff/Defensive/Movement` 若未注册则按注册表现修正，不新增引擎 tag）。

### 2.6 数值外置（`skill_mechanics.json` 技能9 块）

- 节点 `0`（技能级）：`form_duration 3.0`、`form_move_pct 20.0`、`burst_radius 160.0`、`burst_weapon_mult 1.5`、`enchant_duration 4.0`、`enchant_added_pct 0.5`、`ward_duration 2.0`、`weaken_duration 3.0`、`spiral_interval 0.5`、`echo_chance 0.4`、`echo_effectiveness 0.5`、`echo_icd 0.5`。
- 各节点每点数值键：975 `duration_per_point 0.25`；976 `burst_per_point 0.15`；902 `move_per_point 10`、`dodge_per_point 10`；974 `recovery_per_point 10`；978 `rebirth_lost_per_point 0.15`、`rebirth_flat_per_point 0.05`；979 `ward_per_point 0.10`；913 `weaken_per_point 0.06`；914 `mana_per_point 2`、`dr_per_point 0.10`；982 `crit_per_missing_per_point 4`；983 `spiral_count_per_point 1`、`spiral_damage_pct 0.08`；984 `heal_pct 0.05`；934 `speed_per_point 5`；986 `cd_per_point 1`；987 `intent_per_point 1`；988 `dodge_per_point 10`；954 `refund_sec 3`；955 `cost_per_point 0.20`；990 `amp_per_point 0.15`；973 `speed_per_point 0.10`；991 `pen_per_intent_per_point 1`、`pen_cap_pct 40`。

---

## 3. 行为与系统实现（伪代码）

### 3.1 技能993「影剑回响」定义（产品决策落地）

> 协同节点（1 点，前置 987@1）：绝影期间，你的「御剑·回旋」命中敌人时，有 40% 几率触发一次影子回响（以 50% 效果再次施放「御剑·回旋」），内置冷却 0.5 秒。

实现为行为层手写规则（注释说明契约层不建触发器，避免 `max_triggers` 结构矩阵波动）：

```cpp
// PhantomTrance::DoCast 内，if (params.echo_synergy)
TriggerRule echo;
echo.rule_id = 993;
echo.listen_event = CombatEventType::OnSkillHit;
echo.target_mode = TriggerTargetPolicy::Victim;
echo.required_skill_id = 8;      // 仅御剑·回旋命中
echo.cast_skill_id = 8;          // 影子回响=再次施放技能8
echo.base_chance = mech.GetFloat(9, 0, "echo_chance", 0.4f);
echo.use_proc_scaling = false;
echo.internal_cooldown = mech.GetFloat(9, 0, "echo_icd", 0.5f);
echo.effectiveness = mech.GetFloat(9, 0, "echo_effectiveness", 0.5f);
trig.AddRule(echo);
// 形态结束时 RemoveRule(993)
```

### 3.2 `SkillSpecializationBaker` case 9

- 基础原型：`Mobility`（主）；`secondary_archetype = None`（`ReactiveWard` 仅保留给技能4，见审查 L3）；`speed = params.dash_speed`，`duration` 不再代表反击窗口。
- 节点分支（全部写 `del.trance.*`，数值取自 `SkillMechanicsRegistry`，feature_flags 用独立 `1u<<n`）：

| 节点 | 写入 |
| --- | --- |
| 975 | `duration_sec = 3 + duration_per_point*pts` |
| 976 | `burst_damage_mult = 1 + burst_per_point*pts` |
| 902 | `move_speed_pct += move_per_point*pts; dodge_pct += dodge_per_point*pts` |
| 974 | `recovery_pct = recovery_per_point*pts` |
| 977 | `cheat_death_hold = true` |
| 978 | `rebirth_lost_pct = rebirth_lost_per_point*pts; rebirth_flat_pct = rebirth_flat_per_point*pts` |
| 979 | `ward_pct = ward_per_point*pts` |
| 980 | `void_body = true` |
| 913 | `weaken_on_pass_pct = weaken_per_point*pts` |
| 914 | `void_gift_mana_per_sec = mana_per_point*pts; void_gift_dr_pct = dr_per_point*pts` |
| 981 | `death_seal = true` |
| 982 | `last_stand_crit_pct = crit_per_missing_per_point*pts`（以小数存储：`0.04*pts`） |
| 983 | `death_spiral_count = spiral_count_per_point*pts; death_spiral_damage_pct = spiral_damage_pct` |
| 984 | `bloodthirst_pct = heal_pct` |
| 934 | `atk_cast_speed_pct = speed_per_point*pts` |
| 985 | `blink = true` |
| 986 | `cooldown_flat_reduce = cd_per_point*pts`（随后 `effective_cooldown = max(1, cd - reduce)`） |
| 987 | `intent_per_sec = intent_per_point*pts` |
| 988 | `sword_step_dodge_pct = dodge_per_point*pts; sword_step_drain_mult = 0.5` |
| 954 | `time_reversal_sec = refund_sec` |
| 955 | `focus_mana_reduce_pct = cost_per_point*pts` |
| 989 | `transmuter_tag = Tag::Cold`（互斥优先级：Cold 优先） |
| 990 | `frost_amp_pct = amp_per_point*pts` |
| 972 | `transmuter_tag = Tag::Lightning`（若 989 已选则跳过） |
| 973 | `overload_speed_pct = speed_per_point*pts` |
| 991 | `enchant_pen_per_intent_pct = pen_per_intent_per_point*pts`（存储小数：`0.01*pts`） |
| 992 | `enchant_refresh_on_kill = true` |
| 993 | `echo_synergy = true` |

- `SyncTriggerRules`：透传 `base_chance / requires_melee_hit / required_window`；移除旧 951 依赖；保留 855 式跨技能特例逻辑不变。

### 3.3 `PhantomTrance` 行为（原 PhantomFlash 重写，重命名）

文件：`src/game/systems/skill/behaviors/PhantomFlash.{hpp,cpp}` → `PhantomTrance.{hpp,cpp}`；`REGISTER_SKILL_BEHAVIOR(PhantomTrance)`。

`DoCast(registry, owner, exec)`：
1. 从 baked profile 读取 `delivery.trance`；无 profile 时用默认值 + `exec.active_nodes` 兜底（保留可测性）。
2. `blink`：将 `Position` 直接置为 `exec.target_pos`（瞬移），不创建 Mobility。
3. 生成 `PhantomTranceComponent`（`duration=params.duration_sec`，复制 params）。
4. 施加形态 Buff（`ActiveEffectsComponent`，duration=形态时长）：
   - MoveSpeed PercentAdd `move_speed_pct`；DodgeChance PercentAdd `dodge_pct`（902>0）；
   - `recovery_pct>0`：CooldownReduction PercentAdd（`BuffId::PhantomTranceTempo` 合并攻速/冷却两类）；
   - `atk_cast_speed_pct>0` 且 `death_seal`：AttackSpeed + CastSpeed PercentAdd；
   - `void_gift_dr_pct>0`：GlobalDamageReduction PercentAdd。
5. `death_seal`：`original_max_hp = stats.max_health`；`hp.max = stats.max_health * 0.33`；`hp.current = min(hp.current, hp.max)`；施加 `PhantomTranceDeathSeal` Buff（六系伤害 PercentMult +33）。
6. `void_body`：`hp.current *= (1 - 0.15)`；施加 `PhantomTranceStealth` Buff + `PhaseTag`。
7. `ward_pct>0`：`stats->barrier += stats->max_health * ward_pct`；`BarrierComponent`；`StatsDirty`；`barrier_delay = ward_duration`。
8. `intent_per_sec>0`：记录 tick；`echo_synergy`：注册规则 993。
9. 注册 `statsdirty`/VFX `BuffApply` 事件；`seven_star_shared::ConsumeLinkBuffs` 保留用于回旋联动（如无必要可移除，实施时确认技能8 依赖）。

`Update(registry, entity, pt, dt)`：
1. `elapsed += dt`；`remaining -= dt`。
2. 形态内每帧：
   - 987：`intent_tick` 累计≥1s → `GainSwordIntent(+1)`；
   - 983（`IsDeathSealActive`）每 `spiral_interval` → 向最近敌人发射 `death_spiral_count` 柄飞剑（`Projectile`，伤害=`(stats.max_health-hp.max)*death_spiral_damage_pct`，高穿透，tag=Physical）；
   - 989：每秒对半径内敌人 `AilmentApplier::Apply(Chill)`（免疫 Freeze）；
   - 972：每秒对半径内敌人 `Apply(Shock)` 并造成一次低倍闪电伤害；
   - 913（`void_body`）：检测半径内敌人 → 施加 `PhantomTranceWeaken`（六系 PercentMult 负值，`weaken_on_pass_pct`，`weaken_duration`）；
   - 982（`IsDeathSealActive`）：`missing = 1 - hp.current/hp.max`；更新 `PhantomTranceLastStand` 的 CritDamage PercentAdd = `missing*100*last_stand_crit_pct`（AddOrRefresh 后改写第一条 modifier 数值）。
3. `remaining<=0 && !ending` → `ending=true`，执行结束结算：
   a. 逆脉：`hp.max = stats.max_health`（清除锁定，恢复下一帧由 RegenerationSystem 同步）；
   b. 结束爆发：半径 `burst_radius*burst_damage_mult`；伤害走 `DamagePipeline`（tag 依 transmuter：Physical/Cold/Lightning），倍率 `burst_damage_mult`；Cold 额外 Freeze 概率、Lightning 额外 Shock；
   c. 954：其他技能槽 `cooldown = max(0, cooldown - refund_sec)`；
   d. 984：治疗 `damage_dealt_accum * bloodthirst_pct`（直接写 `hp.current`，绕过禁疗）；
   e. 978：`lethal_triggered ? 损失生命*rebirth_lost_pct : 上限*rebirth_flat_pct`（clamp 到上限）；
   f. transmuter：`enchant_remaining = enchant_duration`、`enchant_tag`；写入 `SkillModifierComponent.damage_modifiers` GainExtra（Physical→元素，`enchant_added_pct`）；
   g. 移除 993 规则、形态 Buff（临时效果按 id 移除）；若 `enchant_remaining<=0` 返回 true。
4. 附魔窗口阶段（`remaining<=0 && enchant_remaining>0`）：`enchant_remaining -= dt`；991 每帧更新元素穿透 buff（=剑意层数 × `enchant_pen_per_intent_pct`，cap `enchant_pen_cap_pct`）；到期移除 GainExtra 与穿透 buff，返回 true。

其他系统协同：
- `SkillSystem` 的形态更新分支改为调用 `PhantomTrance::Update`；退出时只发 `BuffExit`、删除组件（不再全局清除 GainExtra，由行为精确清理）。
- `TryCast`（技能9 以外、施法者处于形态且 `focus_mana_reduce_pct>0`）→ `base_cost *= (1 - focus_mana_reduce_pct)`（955）。
- 近战命中路径（SwordStep 回蓝同位置）：若攻击者处于附魔窗口且命中带 `Tag::Melee` → 概率施加 Freeze/Shock（989/972），并令 `enchant` 与 990 加成参与结算。
- 删除：N1 触发链抑制守卫（`SkillSystem.cpp`），旧 scope 特判（`StatsSystem.cpp:395-398`、`SkillSystem.cpp:2414-2417`）。

### 3.4 管线与战斗系统

- `ProcEngine.cpp`：删除规则9 自动注入/门控/执行特判（:36-57、:82-89、:160-193）；新增 `requires_melee_hit`/`required_window` 过滤（§2.1）。
- `DamagePipeline.cpp`：删除反击格挡（:1234-1255、:1729-1744）与 scope 内 `source_skill_id==9` 特判（:594-598）。
- `CombatSystem.cpp`：
  - 删除 553-594 旧瞬移反击特判；
  - `ApplyDamage` 血量扣除前插入免死：目标持有 `PhantomTranceComponent && remaining>0 && !lethal_triggered` 且本次伤害会致死 → `hp.current=1`、`lethal_triggered=true`、若 `!cheat_death_hold` 则 `remaining=0`（下一帧结算结束）、`was_prevented=true`、返回 false；
  - 伤害结算成功后：若 `attacker` 处于形态（remaining>0）→ `pt.damage_dealt_accum += healthDamageApplied`（984）；
  - `KillEnemy`：若击杀者处于附魔窗口且 992 已点、目标带对应元素异常（Cold→Freeze/Chill，Lightning→Shock）→ `enchant_remaining = enchant_duration`。
- `RegenerationSystem.hpp`：`IsDeathSealActive` → 跳过回血与 `hp.max` 同步。
- `AilmentEngine.cpp`：① 治疗路径 `IsDeathSealActive` → 跳过；② 对持有 Cold 转质形态的玩家，`Freeze` 施加请求直接忽略（免疫冻结）。
- `BloodSea.cpp`：`ApplyHealing` 加禁疗门。
- `AISystem.cpp`：目标获取跳过带 `PhantomTranceStealth` 的实体（不可选中）。
- `StatsSystem.cpp` / `SkillSystem.cpp`：删除旧 scope 特判。
- `SevenStarSlashShared.hpp`：删除 `kPhantomFlashSkillId` 反向返还（:23、:113），确认技能8 相关用例不受影响。

---

## 4. 并行工作流与文件所有权

| 工作流 | 文件（唯一所有权） | 依赖 | 说明 |
| --- | --- | --- | --- |
| WS0 接口冻结 | `SkillDefs.hpp`、`SkillContract.hpp`、`TriggerRuleComponent.hpp`、`BuffIds.hpp/.cpp` | - | 主线程先行，冻结后其余工作流才可开始 |
| WS1 数据/契约/生成 | `skills.json`、`skill_contracts_compact.json`、`skill_9_tree.json`、`skill_mechanics.json`、`scripts/gen_skill_contracts.py`、`SkillRegistry.cpp` | WS0 | 29 节点树 + 契约字段解析 + 运行生成器 |
| WS2 烘焙 | `SkillSpecializationBaker.cpp` | WS0、WS1 契约字段 | case 9 全部节点分支 + SyncTriggerRules 透传 |
| WS3 行为/技能系统 | `behaviors/PhantomTrance.*`（重命名）、`SkillBehaviorRegistry.cpp`、`SkillSystem.cpp`、`SevenStarSlashShared.hpp` | WS0 | DoCast/Update/附魔/993/955/scope 清理/触发守卫删除 |
| WS4 管线/战斗/防御 | `ProcEngine.cpp`、`DamagePipeline.cpp`、`CombatSystem.cpp`、`StatsSystem.cpp`、`RegenerationSystem.hpp`、`AilmentEngine.cpp`、`BloodSea.cpp`、`AISystem.cpp` | WS0 | 特判删除 + 免死 + 禁疗 + 触发扩展消费端 |
| WS5 测试 | `tests/**`（除 docs 外全部相关）、`GameplaySystems.cpp` | WS1-4 | 重写旧用例 + 新功能测试 |
| WS6 文档/资产 | `docs/reviews/...skill9...md`、`docs/designs/2026-09-06-...md`、`设计文档/职业职业草案_剑修.md`（状态位）、`assets/textures/skill_nodes/**`、`SkillNodeAssetRegistry.hpp`、`skill_node_prompts.json`、删除 `skills copy 2.json` | 独立 | 可在 WS1 前启动；图标复用旧节点 PNG |

执行纪律：并行期间**禁止**任何子代理执行编译/测试；所有构建由主线程在 WS1-4 汇总后统一执行，避免编译产物争用。

---

## 5. 原子任务清单

### WS0 接口冻结（主线程）
- [ ] `SkillContract.hpp`：`TriggerWindow` + `TriggerContract` 三字段
- [ ] `TriggerRuleComponent.hpp`：`TriggerRule` 三字段
- [ ] `SkillDefs.hpp`：删除 `PhantomFlashComponent`，新增 `PhantomTranceParams`、`PhantomTranceComponent`、`IsDeathSealActive`、`BakedDeliveryParams.trance`
- [ ] `BuffIds.hpp/.cpp`：新增/删除 Buff id

### WS1 数据/契约/生成
- [ ] 重写 `skills.json` 技能9 基础块（tags 修正、mana 30/cd 15）
- [ ] 重写 29 节点 `talent_tree`（ID/名称/描述/上限/前置/坐标/stat_modifiers/icon 占位）
- [ ] 重写 `skill_contracts_compact.json` 技能9 块（§2.4），删除 `cost_affixes`
- [ ] 更新 `skill_9_tree.json` 布局并合并回 `skills.json`
- [ ] `skill_mechanics.json` 增加技能9 块（§2.6）
- [ ] `gen_skill_contracts.py` 支持三触发字段；`SkillRegistry.cpp` 解析三字段
- [ ] 运行 `python scripts/gen_skill_contracts.py`，确认 `--check` 通过
- [ ] 校验前置图无环、无死锁（930 式互卡不复存在）

### WS2 烘焙
- [ ] case 9 基础原型与 975-993 全分支（§3.2）
- [ ] 986 冷却缩减写入 `effective_cooldown`
- [ ] 989/972 互斥优先级与 feature_flags
- [ ] `SyncTriggerRules` 透传三字段；移除 951 残留

### WS3 行为/技能系统
- [ ] 重命名行为文件/类/命名空间；更新注册与守卫路径
- [ ] `DoCast`（形态建立、免死预备、逆脉锁血、护盾、潜行、993 规则）
- [ ] `Update`（意图/死亡螺旋/脉冲/诅咒/孤注一掷动态/结束结算/附魔窗口）
- [ ] `SkillSystem`：更新分支、955 法耗减免、附魔命中效果、BuffExit 清理、删除 N1 守卫与 scope 特判
- [ ] `SevenStarSlashShared` 旧技能9 耦合清理

### WS4 管线/战斗/防御
- [ ] `ProcEngine`：删旧特判 + 新过滤条件
- [ ] `DamagePipeline`：删反击格挡与 scope 特判
- [ ] `CombatSystem`：删旧反击 + 免死 + 984 统计 + 992 刷新
- [ ] `RegenerationSystem`/`AilmentEngine`/`BloodSea`：禁疗 + 冻结免疫
- [ ] `AISystem`：不可选中豁免
- [ ] `StatsSystem`：scope 特判删除

### WS5 测试
- [ ] 重写 `SkillBehaviorGuardTests`（:251-276、:452-487）
- [ ] 重写/替换 `TriggerRuleTests:417-470`、`SystemMechanics:63-77`
- [ ] 重写 `EventConsistencyTests:199-259` 反击子用例（改为无特判一致性命中）
- [ ] 重写 `CombatAntiMetaLayerTests:56-84`（移除 971 词缀断言）
- [ ] 重写 `SkillSystemTests:886-929、1004-1017、1389-1390`
- [ ] 更新 `SkillContractRegistryTests`（触发节点 935、max_transmuters=1、结构计数）
- [ ] 更新 `SkillKeyNodeMatrixIntegrationTests`、`SkillKeyNodeMatrixTests`
- [ ] 更新 `SkillKeyNodeMatrixTestHelpers.hpp:115` 与 `tests/fixtures/skill_specialization_keynodes.json:44-48`
- [ ] 新增 `tests/functional/PhantomTranceNodes.cpp`（免死/向死而生/逆脉禁疗/死亡螺旋/嗜血/时光逆流/全神贯注/破空一闪/意随神行/附魔穿透/灵气反哺/935 契约/993 回响/爆发/互斥）
- [ ] `GameplaySystems.cpp:267` 行为路径更新

### WS6 文档/资产
- [ ] 追加审查报告「Round 3 修复跟进」章节（保留 Round 1/2）
- [ ] `docs/designs/2026-09-06-modular-skill-archetypes-and-specialization-design.md:362/430-435/444` 同步
- [ ] `设计文档/职业设计草案_剑修.md:1397` 状态位同步
- [ ] 删除 `assets/data/skills copy 2.json`
- [ ] 新节点图标：复用被替换旧节点 PNG → `gen_asset_registries.py` → `sync_skill_node_icon_ids.py --check` 通过；补充 `skill_node_prompts.json` 条目

### 集成验证（主线程）
- [ ] `./build.bat`（RelWithDebInfo）零警告增量
- [ ] `ctest -R "nmd.tests.(unit|integration)"` 全绿
- [ ] `ctest -L ci`（含 functional）全绿
- [ ] `python scripts/gen_skill_contracts.py --check` 通过

---

## 6. 测试与验证方法

- **单元**：组件字段、触发过滤（20%/近战/窗口）、禁疗门、免死锁血、附魔穿透计算。
- **集成**：烘焙→施法→形态状态机→结束结算；935 规则在逆脉窗口内/外的触发差异；996？无；993 回响规则注册/移除；时光逆流返还。
- **功能**：`tests/functional/PhantomTranceNodes.cpp` 按节点逐项断言（参照 `BladeBoomerangNodes.cpp` 的 `CreateTestPlayer + allocated_points + GetCast/GetHit + Bake` 模式）。
- **数据**：`gen_skill_contracts.py --check`、fixture↔compact↔contract 一致性、`GameplaySystems.cpp` 节点守卫。
- **回归**：技能4 `ReactiveWardComponent` 用例、技能8 全部用例、`EventConsistency` 批量一致性。

验收标准：审查报告 C1-C5、H1-H3、M1-M4、N1/N3/N5、R1 全部闭环；29 节点全部可点且有效果路径；无旧 `PhantomFlashComponent`/`counter_window`/`triggered` 残留；构建与全部测试通过。

## 7. 风险与近似

1. **991 前置「任意转质」无法表达**：改用 `974@2`，未点转质时效果空转；需在设计与审查跟进中记录。
2. **图标的运行时校验缺失**（审查 H3）：本轮复用旧节点 PNG 属占位，非最终美术；补 prompts 供后续批量生成。
3. **死亡螺旋弹道**：按 FlowingThrust 投射物模式实现，伤害/穿透为策划值近似；上线前按 `skill_mechanics.json` 调参。
4. **不可选中**：仅做 AI 目标获取豁免，已在战敌人仇恨掉落未强制清理（设计近似）。
5. **`skills copy 2.json` 删除**需确认无脚本引用（当前检索无引用）。

---

## 8. 实施结果（2026-09-10 完成）

WS0-WS7 全部原子任务已执行完毕，验收标准达成。详细闭环矩阵、偏差与证据见 `docs/reviews/2026-09-10-skill9-specialization-nodes-review.md` §14。

### 8.1 交付摘要

- **数据/契约**：`skills.json` 技能9 重写为 29 节点（含协同节点 993）；`skill_contracts_compact.json`、`skill_9_tree.json`、内嵌契约重新生成且 `--check` 通过；`skill_mechanics.json` 新增技能9 数值块。
- **接口**：`TriggerContract`/`TriggerRule` 扩展 `base_chance`/`requires_melee_hit`/`required_window`，生成器、`SkillRegistry`、`SyncTriggerRules`、`ProcEngine` 全链支持；`PhantomTranceComponent/Params` 取代 `PhantomFlashComponent`。
- **行为**：`PhantomTrance.*` 实现绝影形态、免死锁血、逆脉禁疗/增伤、破空一闪、附魔窗口、结束爆发与冷却返还、御剑步联动、993 协同回响、973 过载链、990 冰增幅（单目标路径）、991 元素穿透近似。
- **管线**：删除三处反击特判、三处 scope 特判与触发链抑制 guard；免死钩子落于 `CombatSystem::ApplyDamage`；技能4 `ReactiveWard` 保持。
- **测试**：重写 H2/N5 全部锁定用例，更新 fixture/helper，新增 `tests/functional/PhantomTranceNodes.cpp`（23 用例）。
- **文档/资产**：审查报告 Round 3、设计文档 §5.9 与状态位同步；图标注册表重生成；删除 `assets/data/skills copy 2.json`。

### 8.2 验证证据

- `./build.bat`（RelWithDebInfo，ALL_BUILD 含测试）：成功。
- `ctest -C RelWithDebInfo -E "nmd.tests.performance|nmd.tests.gpu"`：17/17 套件通过，1439 个 doctest 用例。
- `gen_skill_contracts.py --check --check-idempotency --check-determinism`：OK。
- `sync_skill_node_icon_ids.py --check`：0 更新 / 76 未变 / 0 缺失。

### 8.3 与计划偏差

1. 协同节点 993 为产品决策新增（29 节点）。
2. 991 以「附魔窗口内按剑意层数施加减抗 debuff」近似全局元素穿透（无施法者穿透属性）。
3. 990 未接入 `CalculateBatch`；图标为占位 PNG。
4. 数值近似集中外置在 `skill_mechanics.json`，便于后续调参。

### 8.4 第 4 轮独立复审修复

独立审查子代理复审后结论 `修改`；阻断项与建议项已全部修复：981 锁血在 `AttributePipeline` 重算中保持（同时恢复 983 螺旋伤害）、980 `PhaseTag` 生命周期纳入形态窗口、973 迁移到受击触发、982 数值单位修正、禁疗补齐三条回血入口、`TriggerCast` 位序号统一、死字段/尾附清理与文档残留清理。新增/加强功能用例 5 组；复跑构建与 `ctest` 17/17 全绿。详细记录见 `docs/reviews/2026-09-10-skill9-specialization-nodes-review.md` §15。
