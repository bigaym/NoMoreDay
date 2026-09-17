# UMR 技能专精改造第二批（技能 4 剑气护体、技能 5 万剑归宗、技能 6 剑阵·诛仙）设计说明

- **文档状态**：已批准待实施（v2.1：按首轮 F 系列与第二轮 G 系列独立复核反馈全面修订）
- **文档路径**：`docs/designs/2026-09-17-umr-skill-batch2-blade-ward-infinite-blades-sword-array-design.md`
- **设计日期**：2026-09-17
- **系统代号**：`UMR-SKILL-BATCH-2` (Unified Modifier Runtime - Skill Batch 2)
- **修订记录**：
  - v1.0：初稿（19 条 Canonical 记录 + 2 个新交付算子 41/42）。
  - v2.0：响应独立审查报告（F-1 ~ F-7）：
    - 补齐技能 5 lines 1197-1205 弹速消费端单源改造，闭环算子 42（解 F-1 Blocker）；
    - 规范节点 402 为纯正技能自身法耗折扣（`SKILL_MANA_COST_MULT`），废除 `/ 30.0f` 魔法数反算并消除装备减耗穿透漏洞（解 F-2 High）；
    - 显式退役节点 533 的 3 个历史遗留未引用死键（`sword_count_mult`、`size_bonus_pct`、`impact_radius`），使门禁 6 反向登记完全自洽（解 F-3 Medium）；
    - 消除节点 552 在 Baker 步骤 2 中对 `del.range` 的污染性覆盖，保全 510/511 的 450 射程基准（解 F-5 Medium）；
    - 澄清节点 470 反击剑气数读取接口对接（解 F-7 Low）；
    - 步骤 3 弹速合成增加下限 0 防护（采纳最佳实践 B-1）。
  - v2.1：第二轮独立复核修订（G-1 ~ G-8，基于代码实况核验）：
    - 移除指向不存在审查报告文件的失效引用（G-1）；
    - 补齐 470 反击剑数的 Profile 落地链路（`BladeWardComponent` 缓存字段 + `SpawnBladeWardCounterSwords` 入参），修正原方案「调用点无 Profile」的断链（G-2）；
    - 补齐 402 退役引发的死字段与测试断言清理（`BladeWardComponent::mana_cost_reduction`、`Skill4FollowupTests`），并声明减耗「叠加→乘算」的语义变更（G-3/G-4）；
    - 显式登记技能 6 施法范围（603）当前无消费端的既有限制，取消其等价性夸大表述（G-5）；
    - 补齐 Canonical 记录完整字段模板（`operation`/`target`/`tags`/`conditions`/`stacks`），并修正 510 索敌基准来源为机制表 `lock_range`（消除魔法数 450）（G-6/G-7）；
    - 退役键由 22 项扩充至 24 项（新增 `6/610`、`6/611` 的未引用死键 `max_arrays_bonus`），并新增独立死键防回潮门禁（G-8）。
- **输入来源**：
  - `设计文档/统一修饰器运行时系统_UMR.md`
  - `设计文档/职业设计草案_剑修.md`（§3.4 剑气护体、§3.5 万剑归宗、§3.6 剑阵·诛仙）
  - `docs/designs/2026-09-16-umr-skill-batch1-rending-wave-blade-formation-design.md`（Batch 1 批次设计基线）
  - 首轮方案内部独立复核结论（F-1 ~ F-7）与第二轮复核结论（G-1 ~ G-8）：已内联记录于本文件「修订记录」，不单独产出报告文件。
  - 代码实况：
    - [`src/game/systems/skill/SkillSpecializationBaker.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSpecializationBaker.cpp)
    - [`src/game/systems/modifier/ModifierContext.hpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/modifier/ModifierContext.hpp) 与 [`ModifierEvaluator.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/modifier/ModifierEvaluator.cpp)
    - [`src/game/systems/skill/behaviors/BladeWard.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/BladeWard.cpp)
    - [`src/game/systems/skill/BeamChannelDeliverySystem.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/BeamChannelDeliverySystem.cpp)
    - [`src/game/systems/skill/behaviors/SwordArray.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/SwordArray.cpp)
    - [`assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json`](file:///d:/PRJ/NoMoreDay/assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json)
    - [`scripts/validate_skill_spec_modifiers.py`](file:///d:/PRJ/NoMoreDay/scripts/validate_skill_spec_modifiers.py)

---

## 1. 背景与目标

### 1.1 背景与现状
在完成技能 1（流云刺）垂直切片以及第一批次技能 2（裂空斩）和技能 3（灵剑决）的 UMR 并轨后，技能交付算子体系（OpCodes 30..40）已建立成熟的离线生成、门禁三检与运行时烘焙规范。

然而，在现存代码基线中，技能 4（剑气护体）、技能 5（万剑归宗）与技能 6（剑阵·诛仙）的专精烘焙与消费链路依然存在显著的割裂与设计债：
1. **技能 4（剑气护体）烘焙层空心化与消费层双重求值**：
   - [`SkillSpecializationBaker.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSpecializationBaker.cpp) 的 `case 4` 仅写入部分布尔标志位（`feature_flags`），几乎没有交付数值计算；
   - 行为层 [`BladeWard.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/BladeWard.cpp) 在 `DoCast` 中跳过 `BakedSkillProfile`，直接调用 `ResolveState` 读取专精点数并查阅 `skill_mechanics.json` 计算持久法耗折扣（402）与反击增伤（471），违背了“Baker AOT 统一烘焙、行为层只读 Profile”的架构分层；
   - 反击剑气数量（470）在 Baker 中硬编码 `del.sub_count = 5`，但在生成反击实体时又重新读取机制表 `counter_swords: 5.0`，形成双重事实源。
2. **技能 5（万剑归宗）混合硬编码与重算漂移**：
   - Baker `case 5` 沉淀了约 100 行硬编码计算（500 引导法耗、502 陨铁增伤、510 锁定增耗、511 锁定半径/弹速、533 巨剑增伤、554 满剑意暴击、555 暴伤等）；
   - 511（无处遁形）的索敌半径在 Baker 中写入 `del.range`，下落弹速在 Baker 中写入 `del.speed`，但 [`BeamChannelDeliverySystem.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/BeamChannelDeliverySystem.cpp)（line 1142 与 line 1201）在运行帧又通过 `GetSkill5Point` 重新读取点数并查机制表计算，导致 Baker 的计算在运行期完全被旁路且空转；
   - 缺少标准弹速算子，导致 511 下落速度直接在 `del.speed` 乘算字面量。
3. **技能 6（剑阵·诛仙）数值与惩罚节点混合**：
   - Baker `case 6` 中 600（持续时间）、601（范围半径）、602（阵内增伤）、603（法耗降低+施法距离）、610（双生减伤）、611（三才增耗）、634（牢笼范围缩小）、653（随身剑垒伤害减半）均由 C++ 手写直接改写 `out_profile` 与 `del`；
   - 持续时间（`del.duration`）作为核心交付参数，目前缺乏标准 UMR 交付算子支持；
   - 施法范围（603）依赖隐式基准 `400.0f`，若未前置明确写入易导致计算歧义。

### 1.2 核心目标（Goals）
1. **轻量扩充 2 个标准技能交付算子（P0）**：
   - `SKILL_DURATION_FLAT = 41`（技能持续时间绝对秒数增减，浮点累加到 `del.duration`）；
   - `SKILL_SPEED_MULT = 42`（技能投射物/下落速度相对倍率乘算，浮点累乘到 `del.speed`，下限 0）。
2. **规范化接入 19 条 Canonical 记录（P0）**：
   - **技能 4（2 条）**：402（持久，自身法耗折扣）、471（以眼还眼，反击 More 增伤）；
   - **技能 5（8 条）**：500（剑雨绵绵，法耗折扣）、502（陨铁，More 物理增伤）、510（神识锁定，法耗增加）、511（无处遁形，索敌范围倍率）、511（无处遁形，下落速度倍率）、533（巨剑术，More 物理增伤）、554（意气爆发，额外暴击率）、555（意念合一，额外暴击伤害）；
   - **技能 6（9 条）**：600（灵气流转，持续时间秒数）、601（虚空法网，范围半径倍率）、602（极刑，More 物理增伤）、603（阵基稳固，法耗折扣）、603（阵基稳固，施法范围倍率）、610（双生剑阵，伤害惩罚）、611（三才阵，法耗增加）、634（剑阵牢笼，范围惩罚）、653（随身剑垒，伤害惩罚）。
3. **消除双重事实源与彻底退役死数据（P0）**：
    - 退役上述 19 条数值在 [`assets/data/skill_mechanics.json`](file:///d:/PRJ/NoMoreDay/assets/data/skill_mechanics.json) 中的对应键，外加 5 个未引用死键（节点 533 的 `sword_count_mult`、`size_bonus_pct`、`impact_radius`，以及节点 610/611 的 `max_arrays_bonus`），合计彻底退役删除 24 个键；
   - 修正 [`src/game/systems/skill/BeamChannelDeliverySystem.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/BeamChannelDeliverySystem.cpp) 索敌半径（line 1142）与下落弹速（line 1201），统一单源消费 `profile->delivery.range` 与 `profile->delivery.speed`，根除运行帧二次查表重算；
   - 修正 [`src/game/systems/skill/behaviors/BladeWard.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/BladeWard.cpp) 与 [`src/game/systems/combat/damage/DamageInterceptors.hpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/combat/damage/DamageInterceptors.hpp) 反击增伤读取，单源消费 `profile->more_damage_mult`。
4. **基准显式化与确定性烘焙（P0）**：
    - 在 Baker 步骤 1 显式写入技能 4、5、6 的默认基准参数（`del.duration`、`del.speed`、`del.range`、`out_profile.area_radius`），杜绝乘算算子对未定义/默认结构体状态的隐式依赖；技能法耗基准沿用 `skillData->mana_cost`（`skills.json` 单一事实源），仅技能 5 引导速率保留既有 20/s 覆写并加注释；技能 5 索敌基准改为从机制表 `5/510 lock_range` 读取，消除魔法数 `450.0f`；
   - 清理步骤 2 中节点 552 对 `del.range` 的污染性覆盖（552 自身仅消费 `circle_radius` 机制键，不能覆盖索敌基准）；
   - 固化步骤 4 的 Keystone 覆盖逻辑（如 533 巨剑半径保底下限 `giant_radius = 70.0f`）。
5. **离线门禁与全量测试闭环（P0）**：
   - 扩展 [`scripts/validate_skill_spec_modifiers.py`](file:///d:/PRJ/NoMoreDay/scripts/validate_skill_spec_modifiers.py) 的 6 项门禁，覆盖全量 19 条新记录的 ID 解码、等价断言（精确区分 `raw`/`percent`/`flat_negate`）与退役键不回潮；
   - 既有单元测试与功能测试 100% 保持全绿，防止任何平衡性或运行期漂移。

### 1.3 明确非目标（Non-Goals）
1. **非目标**：不将纯分支形态、机制标志与触发规则迁入 UMR（如 452 瞬身反打触发、472/474 转质、512 命印、513 天诛触发、534 天剑召唤、552 环形落剑、613 连线、614 位移引爆、633 绝命处决、635 阵斩回响触发、670/672 阵法元素转质等），这些继续由 C++ 机制层承载；
2. **非目标**：本批次不推进技能 7~12 的改造，维持严格的 3 技能批次边界；
3. **非目标**：不修改二进制运行时的底层内存对齐与 Schema 头结构，保持零堆分配的高性能寻址；
4. **非目标（显式例外）**：技能 5 的 501 动态频率加速（引导过程中随时间 ramp）保持在交付系统内部基于 Baker 基准 `sub_interval` 计算，不转化为静态 UMR 算子。

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
    - SKILL_MORE_DAMAGE_MULT (30)               - 元素转质 (ResolveElementalConversion: 472/474/570/572/670/672)
    - SKILL_BONUS_CRIT (34)                     - 触发契约 (TriggerRule: 452/513/635)
    - SKILL_AREA_MULT (35)                      - 机制位掩码 (feature_flags: 470/501/534/613/634 等)
    - SKILL_MANA_COST_MULT (36)                 - 引导动态 Ramp (501 提频)
    - SKILL_BONUS_CRIT_DAMAGE (39)              - 阵地状态与领域管理 (SwordArrayComponent 实体生成与挪阵)
    - SKILL_RANGE_MULT (40)                     - 巨剑术范围保底覆盖 (533 giant_radius 步骤 4 后处理)
    - SKILL_DURATION_FLAT (41) [新增]           - 命印/处决/破甲等复杂业务状态
    - SKILL_SPEED_MULT (42)    [新增]
                 │                                           │
                 └─────────────────────┬─────────────────────┘
                                       │ 汇流 (SkillSpecializationBaker::Bake)
                                       ▼
                     【BakedSkillProfile / BakedDeliveryParams】
                                       │
       ┌───────────────────────────────┼───────────────────────────────┐
       ▼                               ▼                               ▼
【BladeWard::DoCast】        【BeamChannelDeliverySystem】     【SwordArray::DoCast】
- 读 effective_mana_cost     - 读 profile->delivery.range      - 读 delivery.duration
- 读 more_damage_mult (471)  - 读 profile->delivery.speed      - 读 profile->area_radius
- 读 delivery.sub_count(470) - 读 profile->more_damage_mult    - 读 profile->more_damage_mult
                             - 读 profile->effective_mana_cost - 读 delivery.range
```

---

## 3. 详细技术设计

### 3.1 OpCode 体系扩充（41..42）

在 [`src/game/systems/modifier/ModifierContext.hpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/modifier/ModifierContext.hpp) 中扩充 `ModifierOpCode`：

```cpp
enum class ModifierOpCode : uint16_t {
  // 30..40 既有交付算子
  SKILL_MORE_DAMAGE_MULT  = 30,
  SKILL_COOLDOWN_FLAT     = 31,
  SKILL_COOLDOWN_MULT     = 32,
  SKILL_CHARGES_ADD       = 33,
  SKILL_BONUS_CRIT        = 34,
  SKILL_AREA_MULT         = 35,
  SKILL_MANA_COST_MULT    = 36,
  SKILL_PROJECTILES_ADD   = 37,
  SKILL_MANA_COST_FLAT    = 38,
  SKILL_BONUS_CRIT_DAMAGE = 39,
  SKILL_RANGE_MULT        = 40,

  // Batch 2 新增技能交付算子 (41..42)
  SKILL_DURATION_FLAT     = 41, // param_u32: 技能 ID; param_f32: 每点持续时间增减秒数 (+0.5 = 每点 +0.5s)
  SKILL_SPEED_MULT        = 42, // param_u32: 技能 ID; param_f32: 每点弹速/下落速度相对乘算偏移 (0.25 = 每点 +25%)
};
```

- **类别映射**：在 `CategoryOfOp` 中将 41 与 42 归入 `ModifierOpCategory::SkillDelivery`；
- **点数缩放公式**：设有效专精点数为 $N$：
  - `SKILL_DURATION_FLAT`：累加 `op.param_f32 * N` 到 `skill_duration_flat`；
  - `SKILL_SPEED_MULT`：累乘 `1.0f + op.param_f32 * N` 到 `skill_speed_mult`。

### 3.2 `ModifierDelta` 容器与合并扩展

在 [`src/game/systems/modifier/ModifierEvaluator.hpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/modifier/ModifierEvaluator.hpp) 与 [`ModifierEvaluator.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/modifier/ModifierEvaluator.cpp) 中新增容器与无堆分配访问器：

```cpp
class ModifierDelta {
  // 既有容器...
  std::unordered_map<uint32_t, float> skill_duration_flat; // 默认 0.0f，加性累加
  std::unordered_map<uint32_t, float> skill_speed_mult;    // 默认 1.0f，乘性累乘

public:
  void AddSkillDurationFlat(uint32_t skillId, float delta);
  [[nodiscard]] float GetSkillDurationFlat(uint32_t skillId) const;

  void AddSkillSpeedMult(uint32_t skillId, float mult);
  [[nodiscard]] float GetSkillSpeedMult(uint32_t skillId) const;

  void MergeFrom(const ModifierDelta &other); // 扩展合并这两个新容器
};
```

### 3.3 烘焙合成序列（Baking Pipeline）与基准显式化

在 [`SkillSpecializationBaker.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSpecializationBaker.cpp) 中强化六步执行流：

#### 步骤 1：基础参数显式初始化（消除隐式依赖）
乘算算子（`SKILL_RANGE_MULT`、`SKILL_SPEED_MULT`、`SKILL_AREA_MULT`）与加算算子（`SKILL_DURATION_FLAT`）严格依赖确定性的基准初值。步骤 1 必须显式赋予技能 4、5、6 的基准值：

```cpp
case 4: // 剑气护体 (Blade Ward)
  del.duration = 10.0f;                   // 基准持续时间 10s
  del.sub_count = 5;                      // 反击剑气基准数量 5 (解 F-7)，DoCast 缓存到 ward.counter_sword_count
  out_profile.more_damage_mult = 1.0f;
  // 法耗基准沿用 skillData->mana_cost（skills.json=30），不重复硬编码 (G-3)
  break;

case 5: // 万剑归宗 (Infinite Blades)
  del.sub_interval = 0.3f;                // 发射基准间隔 0.3s
  del.speed = 1000.0f;                    // 下落基准弹速 1000（无机制键，集中为具名常量）
  del.range = data::SkillMechanicsRegistry::Get().GetFloat(5, 510, "lock_range", 450.0f);
                                          // 索敌基准取自机制表单一事实源，消除魔法数 (G-7)
  out_profile.effective_mana_cost = 20.0f;// 引导基准每秒法耗 20（既有覆写，非 skills.json 施法法耗）
  break;

case 6: // 剑阵·诛仙 (Sword Array)
  out_profile.area_radius = 150.0f;       // 阵法基准半径 150
  del.duration = 5.0f;                    // 阵法基准持续 5.0s
  del.sub_interval = 0.5f;                // 判定基准间隔 0.5s
  del.range = 400.0f;                     // 施法基准射程 400（当前无消费端，见 G-5）
  // 法耗基准沿用 skillData->mana_cost（skills.json=30），不重复硬编码 (G-3)
  break;
```

#### 步骤 2：专精节点循环清理（解 F-5）
清理已转由 UMR 承载的纯数值分支：
- 移除 case 5 中节点 552 对 `del.range` 的赋值（`del.range = circle_radius` 会污染 510/511 的 450 基准，552 仅保留 `del.feature_flags |= 131072`）；
- 移除 case 4、5、6 中所有已由 UMR 代替的直接乘/加赋值。

#### 步骤 3：UMR 交付增量一次性合成
```cpp
// 既有 30..40 合成...
out_profile.effective_mana_cost = std::max(
    0.0f,
    (out_profile.effective_mana_cost + specDelta.GetSkillManaCostFlat(skill_id))
        * specDelta.GetManaCostMultiplier(skill_id));
out_profile.more_damage_mult *= specDelta.GetSkillMoreDamageMult(skill_id);
out_profile.area_radius *= specDelta.GetSkillAreaMult(skill_id);
del.bonus_crit += specDelta.GetSkillBonusCrit(skill_id);
del.bonus_crit_damage += specDelta.GetSkillBonusCritDamage(skill_id);
del.range *= specDelta.GetSkillRangeMult(skill_id);

// Batch 2 新增算子合成（加下限 0 防护，采纳 B-1）
del.speed = std::max(0.0f, del.speed * specDelta.GetSkillSpeedMult(skill_id));
del.duration = std::max(0.0f, del.duration + specDelta.GetSkillDurationFlat(skill_id));
```

#### 步骤 4：确定性 Keystone 与非线性覆盖
```cpp
if (skill_id == 5) {
  // 533 巨剑术: 范围保底下限 (giant_radius = 70.0f)
  if ((out_profile.delivery.feature_flags & 4096) != 0) {
    const float giantRad =
        data::SkillMechanicsRegistry::Get().GetFloat(5, 533, "giant_radius", 70.0f);
    out_profile.area_radius = std::max(out_profile.area_radius, giantRad);
  }
} else if (skill_id == 6) {
  // 670 焚天烈焰阵 与 672 九幽雷池 互斥守卫保持在步骤 4
  if ((out_profile.delivery.feature_flags & 262144) &&
      (out_profile.delivery.feature_flags & 1048576)) {
    out_profile.delivery.feature_flags &= ~1048576;
    out_profile.effective_tags =
        (out_profile.effective_tags & ~Tag::Lightning) | Tag::Fire;
    out_profile.delivery.sub_interval = 0.5f;
  }
}
```

---

## 4. 节点数据配置契约与单一事实源治理

### 4.1 记录 ID 规则与通用头约束
严格沿用 UMR 规范：`modifier_id = 2,000,000 + node_id * 10 + op_index`。
- `priority`: `200`
- `profession_mask`: `1` (BladeAscendant)
- `weapon_class_mask`: `65535`
- `equip_slot_mask`: `0`
- `exclusive_group`: `0`
- `max_active`: `0`
- `debug_source`: `"skill_spec_node"`
- `min_player_level`: `1`

### 4.2 详细 Canonical 记录对照表（19 条）

#### 4.2.0 Canonical 记录字段模板（必填完整，解 G-6）

新增记录必须补全 `operation` 与 `runtime.target`（原表仅列 `stat_path` 不足以生成合法记录）。不变量：`record.stacks == runtime.param_u32 == skill_id`，`record.stat_path == runtime.target`。

| OpCode | `record.operation` | `record.stat_path` / `runtime.target` |
|---|---|---|
| `SKILL_MORE_DAMAGE_MULT` | `mul` | `skill.more_damage` |
| `SKILL_AREA_MULT` | `mul` | `skill.area_mult` |
| `SKILL_RANGE_MULT` | `mul` | `skill.range_mult` |
| `SKILL_MANA_COST_MULT` | `mul` | `skill.mana_cost` |
| `SKILL_BONUS_CRIT` | `add` | `skill.bonus_crit` |
| `SKILL_BONUS_CRIT_DAMAGE` | `add` | `skill.bonus_crit_damage` |
| `SKILL_DURATION_FLAT` | `add` | `skill.duration_flat` |
| `SKILL_SPEED_MULT` | `mul` | `skill.speed_mult` |

```json
{
  "record": {
    "schema_version": 1,
    "record_type": "modifier",
    "modifier_id": 2004020,
    "domain": "skill_spec",
    "operation": "mul",
    "stat_path": "skill.mana_cost",
    "value": 0.15,
    "stacks": 4,
    "tags": ["skill"],
    "conditions": { "all_skill_ids": [4], "min_player_level": 1 }
  },
  "runtime": {
    "priority": 200,
    "profession_mask": 1,
    "skill_id_whitelist": [4],
    "required_skill_tags_all": 0,
    "forbidden_skill_tags_any": 0,
    "weapon_class_mask": 65535,
    "equip_slot_mask": 0,
    "node_id_whitelist": [402],
    "exclusive_group": 0,
    "max_active": 0,
    "opcode": "SKILL_MANA_COST_MULT",
    "target": "skill.mana_cost",
    "param_u32": 4,
    "param_f32": 0.15,
    "debug_name": "BladeWard_Node402_ManaCost",
    "debug_source": "skill_spec_node"
  }
}
```

| 记录 ID | 技能 | 节点 | OpCode | param_u32 (`stacks`) | param_f32 (`value`) | canonical stat_path | debug_name |
|---|---|---|---|---|---|---|---|
| `2004020` | 4 (剑气护体) | 402 (持久) | `SKILL_MANA_COST_MULT` | 4 | `0.15` | `skill.mana_cost` | `BladeWard_Node402_ManaCost` |
| `2004710` | 4 (剑气护体) | 471 (以眼还眼) | `SKILL_MORE_DAMAGE_MULT` | 4 | `0.20` | `skill.more_damage` | `BladeWard_Node471_MoreDamage` |
| `2005000` | 5 (万剑归宗) | 500 (剑雨绵绵) | `SKILL_MANA_COST_MULT` | 5 | `0.10` | `skill.mana_cost` | `InfiniteBlades_Node500_ManaCost` |
| `2005020` | 5 (万剑归宗) | 502 (陨铁) | `SKILL_MORE_DAMAGE_MULT` | 5 | `0.10` | `skill.more_damage` | `InfiniteBlades_Node502_MoreDamage` |
| `2005100` | 5 (万剑归宗) | 510 (神识锁定) | `SKILL_MANA_COST_MULT` | 5 | `-0.30` | `skill.mana_cost` | `InfiniteBlades_Node510_ManaCostIncrease` |
| `2005110` | 5 (万剑归宗) | 511 (无处遁形) | `SKILL_RANGE_MULT` | 5 | `0.15` | `skill.range_mult` | `InfiniteBlades_Node511_RangeMult` |
| `2005111` | 5 (万剑归宗) | 511 (无处遁形) | `SKILL_SPEED_MULT` | 5 | `0.25` | `skill.speed_mult` | `InfiniteBlades_Node511_SpeedMult` |
| `2005330` | 5 (万剑归宗) | 533 (巨剑术) | `SKILL_MORE_DAMAGE_MULT` | 5 | `1.50` | `skill.more_damage` | `InfiniteBlades_Node533_MoreDamage` |
| `2005540` | 5 (万剑归宗) | 554 (意气爆发) | `SKILL_BONUS_CRIT` | 5 | `1.00` | `skill.bonus_crit` | `InfiniteBlades_Node554_BonusCrit` |
| `2005550` | 5 (万剑归宗) | 555 (意念合一) | `SKILL_BONUS_CRIT_DAMAGE` | 5 | `0.20` | `skill.bonus_crit_damage` | `InfiniteBlades_Node555_BonusCritDamage` |
| `2006000` | 6 (剑阵·诛仙) | 600 (灵气流转) | `SKILL_DURATION_FLAT` | 6 | `0.50` | `skill.duration_flat` | `SwordArray_Node600_DurationFlat` |
| `2006010` | 6 (剑阵·诛仙) | 601 (虚空法网) | `SKILL_AREA_MULT` | 6 | `0.15` | `skill.area_mult` | `SwordArray_Node601_AreaMult` |
| `2006020` | 6 (剑阵·诛仙) | 602 (极刑) | `SKILL_MORE_DAMAGE_MULT` | 6 | `0.10` | `skill.more_damage` | `SwordArray_Node602_MoreDamage` |
| `2006030` | 6 (剑阵·诛仙) | 603 (阵基稳固) | `SKILL_MANA_COST_MULT` | 6 | `0.05` | `skill.mana_cost` | `SwordArray_Node603_ManaCost` |
| `2006031` | 6 (剑阵·诛仙) | 603 (阵基稳固) | `SKILL_RANGE_MULT` | 6 | `0.10` | `skill.range_mult` | `SwordArray_Node603_RangeMult` |
| `2006100` | 6 (剑阵·诛仙) | 610 (双生剑阵) | `SKILL_MORE_DAMAGE_MULT` | 6 | `-0.15` | `skill.more_damage` | `SwordArray_Node610_MoreDamage` |
| `2006110` | 6 (剑阵·诛仙) | 611 (三才阵) | `SKILL_MANA_COST_MULT` | 6 | `-0.30` | `skill.mana_cost` | `SwordArray_Node611_ManaCostIncrease` |
| `2006340` | 6 (剑阵·诛仙) | 634 (剑阵牢笼) | `SKILL_AREA_MULT` | 6 | `-0.30` | `skill.area_mult` | `SwordArray_Node634_AreaPenalty` |
| `2006530` | 6 (剑阵·诛仙) | 653 (随身剑垒) | `SKILL_MORE_DAMAGE_MULT` | 6 | `-0.50` | `skill.more_damage` | `SwordArray_Node653_MoreDamage` |

### 4.3 机制表（`skill_mechanics.json`）退役清单（24 项全量闭环，解 F-3/G-6）

| 技能/节点 | mechanics 键名 | 迁移前用途 | 迁移后处置 |
|---|---|---|---|
| 4/402 | `mana_cost_reduction_per_point` | Baker/DoCast 直接读取 | **删除**（唯一源：canonical `2004020`，法耗统一下沉至 `effective_mana_cost`） |
| 4/471 | `counter_more_damage_per_point` | DoCast 直接读取 | **删除**（唯一源：canonical `2004710`） |
| 5/500 | `mana_reduction_pct_per_point` | Baker case 5 直接赋值 | **删除**（唯一源：canonical `2005000`） |
| 5/502 | `phys_damage_pct_per_point` | Baker case 5 直接赋值 | **删除**（唯一源：canonical `2005020`；`splash_radius` 保留） |
| 5/510 | `mana_cost_increase_pct` | Baker case 5 直接赋值 | **删除**（唯一源：canonical `2005100`） |
| 5/510 | `lock_range` | Baker 步骤 1 索敌基准 | **保留**（作为索敌基准单一事实源，转入 `KEPT_MECHANICS_KEYS`，解 G-7） |
| 5/511 | `lock_radius_pct_per_point` | Baker 与 BeamChannel 重复计算 | **删除**（唯一源：canonical `2005110`） |
| 5/511 | `fall_speed_mult_per_point` | Baker 与 BeamChannel 重复计算 | **删除**（唯一源：canonical `2005111`） |
| 5/533 | `damage_more_pct` | Baker case 5 直接赋值 | **删除**（唯一源：canonical `2005330`） |
| 5/533 | `sword_count_mult` | **未引用死键** (schema 标注) | **删除退役**（清理历史死数据，解 F-3） |
| 5/533 | `size_bonus_pct` | **未引用死键** (schema 标注) | **删除退役**（清理历史死数据，解 F-3） |
| 5/533 | `impact_radius` | **未引用死键** (schema 标注) | **删除退役**（清理历史死数据，解 F-3，保留唯一活键 `giant_radius`） |
| 5/554 | `crit_chance_bonus` | Baker case 5 直接赋值 | **删除**（唯一源：canonical `2005540`；`intent_cost` 保留） |
| 5/555 | `crit_damage_pct_per_point` | Baker case 5 直接赋值 | **删除**（唯一源：canonical `2005550`） |
| 6/600 | `duration_per_point` | Baker case 6 直接赋值 | **删除**（唯一源：canonical `2006000`） |
| 6/601 | `radius_pct_per_point` | Baker case 6 直接赋值 | **删除**（唯一源：canonical `2006010`） |
| 6/602 | `phys_damage_pct_per_point` | Baker case 6 直接赋值 | **删除**（唯一源：canonical `2006020`） |
| 6/603 | `mana_reduction_pct_per_point` | Baker case 6 直接赋值 | **删除**（唯一源：canonical `2006030`） |
| 6/603 | `cast_range_pct_per_point` | Baker case 6 直接赋值 | **删除**（唯一源：canonical `2006031`） |
| 6/610 | `damage_reduction_pct` | Baker case 6 直接赋值 | **删除**（唯一源：canonical `2006100`） |
| 6/610 | `max_arrays_bonus` | **未引用死键**（`SwordArray.cpp:64-69` 由 feature flag 硬编码 2/3 阵上限） | **删除退役**（清理历史死数据，解 G-8） |
| 6/611 | `mana_increase_pct` | Baker case 6 直接赋值 | **删除**（唯一源：canonical `2006110`） |
| 6/611 | `max_arrays_bonus` | **未引用死键**（同上） | **删除退役**（清理历史死数据，解 G-8） |
| 6/634 | `radius_penalty_pct` | Baker case 6 直接赋值 | **删除**（唯一源：canonical `2006340`） |
| 6/653 | `damage_reduction_pct` | Baker case 6 直接赋值 | **删除**（唯一源：canonical `2006530`） |

---

## 5. 消费端对接与行为层单源修正

1. **技能 4 行为层（[`BladeWard.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/BladeWard.cpp) 与 [`DamageInterceptors.hpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/combat/damage/DamageInterceptors.hpp)）（解 F-2, F-7, G-2, G-3, G-4）**：
   - 节点 402：理清语义为标准技能施法消耗降低（`SKILL_MANA_COST_MULT`），其数值折减由 Baker 步骤 3 烘焙进 `profile->effective_mana_cost`（加点后法耗从 30 降至 25.5..16.5）；在 `BladeWard.cpp` 中彻底移除向 `ward_buff` 追加 `ResourceCostReduction` 的逻辑，废除所有反算与魔法数 `30.0f`；
   - **语义变更声明（解 G-4）**：原实现把 402 折减以 `ResourceCostReduction` 平铺进属性结算，会与装备减耗**叠加**（可穿透至超额减耗）；新实现折减烘焙进 `effective_mana_cost`，与装备减耗**乘算**（乘法交换律下与既有单源顺序等价），属有意修正。同步删除已无读者的 `BladeWardComponent::mana_cost_reduction` 字段，并把 `Skill4FollowupTests.cpp:160` 的断言改为对 `profile->effective_mana_cost` 的等价断言；
   - 节点 471：反击增伤统一改为单源读取 `ward.counter_damage_more = profile ? (profile->more_damage_mult - 1.0f) : 0.0f;`。
     **不变量（解 G-3）**：技能 4 的 `more_damage_mult` 当前仅由 471 贡献（无其他节点/装备写入），故 `-1.0f` 提取反击加成成立；若未来新增技能 4 的 More 增伤来源，必须改用独立字段而非差值提取；
    - **471 fail-closed 有意不对称（评审 #1 定案）**：技能 4 的 `471` 在 `profile == nullptr` 时 fail-closed 为 `0.0f`，与 470 保留机制表回退的做法刻意不对称。理由：`ResolveBakedProfile`（`src/game/systems/skill/SkillProfileResolve.cpp:15-36`）在缓存档案缺失时会用 `specialized_slots` 同 ID 专精即时烘焙，故 `profile == nullptr` 当且仅当专精槽同样缺失；此时 `specState` 与 `exec.active_nodes` 均为空，迁移前 `nodePoints` 回退本就为 `0`。唯一「有 `active_nodes` 无专精槽」的入口是影子复制（`ShadowDuplicationHook.cpp:21-26`），而技能 4 带 `Tag::Buff` 已被排除（技能 5 带 `Channeled` 同样排除）。470 的回退源于其调用点只持有组件（`DamageInterceptors.hpp:73`），属结构性特例，不能类推到 471；
    - **技能 4 持续时间单源（评审 #9）**：BladeWard 的 ward 持续时间改读 `profile->delivery.duration`（未烘焙回退 `10.0f`），消除行为层与 Baker 基准（步骤 1 `del.duration = 10.0f`）之间的多份硬编码副本；
    - 节点 470（反击剑气数）：在 `BladeWard::DoCast` 中把 `profile->delivery.sub_count`（=5，无 Profile 时回退机制表 `counter_swords`）缓存到 `BladeWardComponent` 新增字段 `counter_sword_count`（`uint8_t`）；`SpawnBladeWardCounterSwords` 增加 `count` 入参，由 [`DamageInterceptors.hpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/combat/damage/DamageInterceptors.hpp) 的 `ResolveSkill4CounterEffects` 传入 `ward.counter_sword_count`。机制表 `counter_swords` 仅保留在 `KEPT_MECHANICS_KEYS` 作为安全回退（解 G-2：原调用点只有 `BladeWardComponent`，无法直接取得 `BakedSkillProfile`）。
2. **技能 5 交付系统（[`BeamChannelDeliverySystem.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/BeamChannelDeliverySystem.cpp)）（解 F-1, F-5）**：
   - 节点 511 索敌逻辑（lines 1136-1146）：移除针对 `GetSkill5Point(511)` 的重复查表乘算，直接采用 `profile->delivery.range` 作为真实锁敌半径；
   - 节点 511 下落弹速（lines 1197-1205）：**彻底移除 `GetSkill5Point(511)` 及查机制表求值逻辑**，统一改为单源读取 `profile->delivery.speed`，闭环算子 42（`SKILL_SPEED_MULT`）；
   - **锁定半径与弹速的语义门控（评审 #3/#4）**：`BeamChannelDeliverySystem` 对技能 5 的锁定半径与下落弹速改以 511 的 `feature_flags & 16` 作语义门控（是否分配 511 决定是否采用专精覆盖），取值一律来自烘焙交付档案 `profile->delivery.range` / `profile->delivery.speed`；未分配 511 时沿用基准 `450` / `1000`（与烘焙基准等价），不再使用 `> 0.0f` 的数值 sentinel 判定；
   - 节点 552：Baker 步骤 2 清除对 `del.range` 的篡改，`BeamChannelDeliverySystem.cpp:1128` 维持从机制表读取 `circle_radius` 作为落剑环形半径，互不干扰。
3. **技能 6 行为层（[`SwordArray.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/SwordArray.cpp)）**：
   - 技能 6 消费 `profile->delivery.duration`（`SwordArray.cpp:132`）、`profile->area_radius`（`:133`）与 `profile->delivery.sub_interval`（`:134`），UMR 接管后 Baker 生成的数据自然流入行为层，无缝闭环；
   - **既有限制登记（解 G-5）**：`profile->delivery.range` 在技能 6 路径上当前无消费端（全仓库仅技能 1/2/7 读取；技能 6 施法距离由输入层/`Stats::cast_range` 处理）。因此节点 603 的「施法范围」迁移属**等价保持**而非生效增益，本批次按 parity 处理，并登记后续独立任务「将 603 射程接入 `SwordArray::DoCast` 落点夹取」，不得在验收中计为已闭环收益。

---

## 6. 验证与测试策略

### 6.1 单元测试与基础算子验证
1. **新增 `SkillBatch2DeliveryOpTests.cpp`**（需同时登记进单元测试 CMake 目标，避免测试未被编译收集）：
   - 验证 `SKILL_DURATION_FLAT`（41）的单点/多点加算与下限 0 断言；
   - 验证 `SKILL_SPEED_MULT`（42）的单点/多点乘算断言与下限 0 断言（`AddSkillSpeedMult` 为累乘，最终下限保护在 Baker 步骤 3）；
   - 验证 `ModifierDelta::MergeFrom` 正确合并新容器。
2. **扩展 `SkillSpecializationBakerTests.cpp`**：
   - 技能 4：402 法耗折扣下沉（`30 * (1 - 0.15*N)`）、471 反击增伤（`1.0 + 0.20*N`）；
   - 技能 5：500 法耗折扣、502 增伤、510 增耗、511 索敌与弹速（`450*(1+0.15*N)` 与 `1000*(1+0.25*N)`）、533 巨剑增伤与半径保底 70、554 暴击、555 暴伤；
   - 技能 6：600 持续时间（`5.0 + 0.5*N`）、601 半径、602 增伤、603 法耗与射程、610 减伤、611 增耗、634 半径惩罚、653 随身减伤。

### 6.2 功能回归与环境防污染
- 在 `Skill4FollowupTests.cpp`、`InfiniteBladesNodes.cpp`、`SwordArrayNodes.cpp` 以及 `Skill5FollowupTests.cpp` 的测试初始化（`EnsureSkillMechanics`）中补齐 `ReloadModifierRuntimeFromAsset()`，防止前置测试注入合成 blob 导致的全局运行时污染。
- 更新既有断言：`Skill4FollowupTests.cpp:160` 的 `ward->mana_cost_reduction == 0.15f` 改为对 402 烘焙结果 `profile->effective_mana_cost` 的等价断言；`SwordArrayNodes.cpp:135` 等无关断言保持不变。
- 新增回归断言：402 技能减耗与装备减耗为**乘算**（在 `SkillManaCostSettlementTests.cpp` 增加装备 × 专精的复合用例）；470 反击剑数在带/不带 Profile 两条路径下均为 5；603 施法范围在迁移前后数值一致但无行为差异（parity 守护）。

### 6.3 离线门禁断言闭环
- 更新 [`scripts/validate_skill_spec_modifiers.py`](file:///d:/PRJ/NoMoreDay/scripts/validate_skill_spec_modifiers.py)：
  - `SKILL_DELIVERY_OPCODE_MAX` 上调至 `42`（并确保 `gen_modifier_runtime_v2.py` 的 `OPCODE_VALUES` 已注册 41/42，否则算子名集合不完整）；
  - 注册 19 条新记录的迁移等价断言（`MIGRATION_EQUIVALENCE`，按设计明确 relation）；
  - **新增独立死键门禁（解 G-8）**：现有 `check_retired_keys_absent` 仅遍历 `MIGRATION_EQUIVALENCE`，无法拦截不带 canonical 记录的退役键。新增 `DEAD_MECHANICS_KEYS` 集合与对应「不得回潮」断言，覆盖 5/533 `sword_count_mult`/`size_bonus_pct`/`impact_radius`、6/610 `max_arrays_bonus`、6/611 `max_arrays_bonus`（合计 5 项）；
  - 注册已迁移节点保留键（`KEPT_MECHANICS_KEYS`）：5/502 `splash_radius`、5/510 `lock_range`、5/533 `giant_radius`、5/554 `intent_cost`、4/470 `counter_swords`。
- 确保 `gen_skill_spec_modifier_contract.py --check` 与 `gen_modifier_runtime_v2.py --check` 门禁 100% 通过。

---

## 7. 风险评估与应对措施

| 风险项 | 等级 | 应对措施 |
|---|:---:|---|
| **单一事实源破坏与死数据残留** | **High** | 严格执行迁移与退役同步完成，24 个键彻底删除，离线门禁脚本（含独立 `DEAD_MECHANICS_KEYS` 死键门禁）强制拦截回潮。 |
| **511 索敌半径与弹速二次计算** | **High** | 在 `BeamChannelDeliverySystem.cpp` 同步剔除两处 `GetSkill5Point(511)` 手写分支，改读 `profile->delivery.range` 与 `profile->delivery.speed`。 |
| **测试执行顺序依赖（跨用例运行时污染）** | **Medium** | 在所有涉及技能 4、5、6 的测试夹具初始化处显式调用 `ReloadModifierRuntimeFromAsset()`。 |
| **乘算负值惩罚与公式下限** | **Medium** | 610/653 等负值加点乘数（-0.15/-0.50）在 `ApplyOp` 中公式为 `1.0f + param * pts`，单测验证结果为预期折扣值，且步骤 3 增加下限 0 保护。 |
| **基准确值化缺失导致漂移** | **Medium** | 在 Baker 步骤 1 显式初始化技能 4、5、6 的全部基准值，并在步骤 2 消除 552 对 `del.range` 的多余覆盖。 |
| **代码生成器字典未同步** | **Low** | 同步在 `gen_modifier_runtime_v2.py` 与 `migrate_skill_spec_modifier_slice.py`（`SUPPORTED_OPCODE_TO_OPERATION`，该字典名已正确、无需改名）注册 41/42 算子。 |
| **470 反击剑数 Profile 落地链路缺失** | **High** | 在 `BladeWardComponent` 新增 `counter_sword_count` 缓存字段，`DoCast` 写入，`SpawnBladeWardCounterSwords` 增参，`DamageInterceptors` 调用点传入；机制表仅作回退。 |
| **402 减耗由「叠加」改为「乘算」的平衡性漂移** | **Medium** | 在设计与提交说明中声明为有意修正；补充 `SkillManaCostSettlementTests` 的装备 × 专精乘算断言；删除死字段并更新 `Skill4FollowupTests` 断言。 |
| **603 施法范围无消费端（等价保持假象）** | **Medium** | 显式登记为既有路径限制，不宣称已生效；新增后续接线任务，避免验收误判。 |
| **死键无独立门禁可拦截回潮** | **Medium** | 新增 `DEAD_MECHANICS_KEYS` 断言，覆盖 5 个无 canonical 的退役键。 |
| **471 单源提取依赖隐含不变量** | **Low** | 在代码注释与设计登记「技能 4 `more_damage_mult` 仅 471 贡献」；未来扩展时改用独立字段而非差值提取。 |
