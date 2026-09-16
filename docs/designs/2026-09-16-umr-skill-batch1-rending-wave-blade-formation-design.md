# UMR 技能专精改造第一批（技能 2 裂空斩与技能 3 灵剑决）设计说明

- **文档状态**：已实施（v2.1：按首轮实施审查反馈修订）
- **文档路径**：`docs/designs/2026-09-16-umr-skill-batch1-rending-wave-blade-formation-design.md`
- **设计日期**：2026-09-16
- **修订记录**：
  - v1：初稿（11 条 canonical 记录 + 4 个新算子）。
  - v2：补充单一事实源治理（退役 `skill_mechanics.json` 残留键与门禁）、skill 3 基准确值显式化、210 惩罚改读数据源、记录 ID 解码门禁、类别泄漏防护与 `ModifierDelta` 设计债登记。
  - v2.1：按实施审查反馈补登 3/300 `swords_per_point` 退役（canonical `2003000` 为唯一源，`duration` 保留）、补齐全部 11 条记录的迁移等价锚点（含 2/210 字面量迁移）、补「法耗下限 0」断言。
- **系统代号**：`UMR-SKILL-BATCH-1` (Unified Modifier Runtime - Skill Batch 1)
- **输入来源**：
  - `设计文档/统一修饰器运行时系统_UMR.md`
  - `docs/designs/2026-09-16-umr-skill-spec-foundation-and-skill1-slice-design.md`（上游基础切片设计）
  - `docs/reviews/2026-09-16-umr-skill-spec-foundation-and-skill1-slice-review.md`（技能 1 审查报告）
  - `docs/designs/2026-09-13-skill-abstraction-and-b2-cleanup-design.md`（单源原则与架构边界）
  - 代码实况：[`SkillSpecializationBaker.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSpecializationBaker.cpp) `case 2` 与 `case 3`、[`ModifierContext.hpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/modifier/ModifierContext.hpp)、[`ModifierEvaluator.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/modifier/ModifierEvaluator.cpp)、[`skill_spec_modifiers.canonical.json`](file:///d:/PRJ/NoMoreDay/assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json)

---

## 1. 背景与目标

### 1.1 背景与现状
在完成技能 1（流云刺）的垂直切片并轨后，UMR 的数据管线（canonical → runtime JSON → 二进制四表）、点数缩放模型（`node_points`）以及基础交付算子体系（OpCodes 30..36）已验证稳定，全仓测试与漂移门禁保持全绿通过。

然而，在 [`SkillSpecializationBaker.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSpecializationBaker.cpp) 中，技能 2（裂空斩，lines 315-422）与技能 3（灵剑决，lines 424-523）依然沉淀了约 200 行手写 `case` 分支：
1. **纯数值直接赋值与硬编码混合**：如 200（范围/射程）、201（平减蓝耗）、202（More 伤害）、210（投射物数量）、300（灵剑数量）、302（More 伤害）、310（索敌射程）、312（蓝耗折扣）、331（额外暴击）、332（额外暴伤）等数值均由 C++ 字面量或 `GetMech` 动态计算后直接写入 `out_profile` 与 `del`，缺乏统一的数据化承载。
2. **算子表达缺口**：现有 UMR OpCodes 30..36 覆盖了 More增伤、CD平加/乘算、充能数、暴击率、范围缩放、法耗折扣，但对于投射物数量增减（`projectile_count`）、绝对法耗减免（`effective_mana_cost - 1.0f`）、暴伤加成（`del.bonus_crit_damage`）以及射程缩放（`del.range`）尚无标准算子。
3. **确定性执行流与 Keystone 覆盖**：技能 3 包含无尽剑匣（311，灵剑上限翻倍）与巨剑降临（330，灵剑置 1）等赋值式 Keystone；技能 2 包含多重剑气（210，投射物增加伴随非线性递减惩罚）。这些节点需要与通用 UMR 增量确定性合成。

### 1.2 核心目标（Goals）
1. **轻量扩充 4 个标准技能交付算子（P0）**：
   - `SKILL_PROJECTILES_ADD = 37`（投射物/召唤剑数量增减，整数累加）；
   - `SKILL_MANA_COST_FLAT = 38`（技能法力消耗绝对增减，浮点累加）；
   - `SKILL_BONUS_CRIT_DAMAGE = 39`（额外暴击伤害百分点，浮点累加）；
   - `SKILL_RANGE_MULT = 40`（技能射程/索敌范围乘算偏移，浮点累乘）。
2. **技能 2（裂空斩）数值节点规范化接入（P1）**：
   - 节点 200（剑气纵横，范围+射程）、201（凝神，平减法耗）、202（锋芒，More 物理增伤）、210（多重剑气，投射物平加）迁入 canonical 数据管线；
   - 移除 [`SkillSpecializationBaker.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSpecializationBaker.cpp) 中对应节点的硬编码赋值，保留机制标志位；
   - 210 非线性伤害惩罚保留在步骤 4 后处理，但数值改为读取 `skill_mechanics.json`（`damage_penalty_pt1/2/3`），不再使用 C++ 字面量。
3. **技能 3（灵剑决）数值节点规范化接入（P1）**：
   - 节点 300（剑池充盈，灵剑数量+1）、302（锋灵，More 物理增伤）、310（索敌范围，追踪半径+20%）、312（灵力网络，维持法耗折扣-5%）、331（弱点锁定，额外暴击+5%）、332（致命锋芒，额外暴伤+25%）迁入 canonical 数据管线；
   - 移除 Baker 中对应节点的直接赋值，退役 `skill_mechanics.json` 中同名键，规范化 311/330 的后置覆盖顺序。
4. **单一事实源治理（P0）**：
   - 迁移完成后，凡已由 canonical UMR 承载的线性数值，其 `skill_mechanics.json` 同名键必须退役（删除），杜绝"改数据文件却无效果"的误导性死数据；
   - 无法用线性算子表达的非线性后处理（210 伤害惩罚）必须改为**读取** `skill_mechanics.json`，而非 C++ 字面量硬编码；
   - 新增离线一致性门禁，强制迁移前后数值等价、且退役键不再出现。
5. **验证等价性与防漂移（P0）**：
   - 既有单元与功能测试 100% 通过（包含 `RendingWaveNodes.cpp` 与 `BladeFormationNodes.cpp`）；
   - 构建系统 precheck 漂移门禁三检通过。

### 1.3 明确非目标（Non-Goals）
1. **非目标**：本批次**不将独特的机制/形态分支**迁入 UMR（如 211 分裂子剑气、230 折返行为、232 黑洞牵引、254 回响斩触发、313 攻速继承、351 剑影环身绕体近战、355 剑阵共鸣、370/372 元素转质等），此类逻辑继续由 C++ 行为层与 SpecState 承载。
2. **非目标**：本批次不触碰技能 4~12 的专精代码，保持严格的单批变更边界。
3. **非目标**：不改动已发布的数据 Schema，不破坏既有二进制文件头与内存四表紧凑寻址。
4. **非目标（显式例外登记）**：技能 3 节点 301（疾风意，`del.sub_interval`）本批次**不迁移**，继续读取 `skill_mechanics.json` 的 `haste_pct_per_point`。原因是 `sub_interval` 为"间隔/频率"语义，与现有 CD 类算子语义不同，需在后续批次单独设计 `SKILL_INTERVAL_*` 算子，避免本轮引入语义含混的复用。此例外必须在设计与测试中显式登记，不得默认为遗漏。

---

## 2. 体系架构与职责分工

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│                      SpecializedSkill (加点: allocated_points)               │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │
                 ┌─────────────────────┴─────────────────────┐
                 ▼                                           ▼
   【UMR 数值修饰执行层】 (统一数据驱动)          【SpecState / 机制形态层】 (代码驱动)
   - SKILL_MORE_DAMAGE_MULT (30)                - 元素转质 (ResolveElementalConversion)
   - SKILL_BONUS_CRIT (34)                      - 特殊位移/折返/黑洞标记 (feature_flags)
   - SKILL_AREA_MULT (35)                       - 触发契约 (TriggerRuleComponent)
   - SKILL_MANA_COST_MULT (36)                  - 灵剑环绕/巨剑形态分支 (351/330)
   - SKILL_PROJECTILES_ADD (37) [新增]          - 多重剑气非线性惩罚 (210 后处理)
   - SKILL_MANA_COST_FLAT (38)  [新增]          - 无尽剑匣翻倍覆盖 (311 后处理)
   - SKILL_BONUS_CRIT_DAMAGE (39) [新增]
   - SKILL_RANGE_MULT (40)      [新增]
                 │                                           │
                 └─────────────────────┬─────────────────────┘
                                       │ 汇流 (SkillSpecializationBaker::Bake)
                                       ▼
                     【BakedSkillProfile / BakedDeliveryParams】
                                       │
                 ┌─────────────────────┴─────────────────────┐
                 ▼                                           ▼
    【RendingWave::DoCast (剑气发射)】        【BladeFormation::DoCast (灵剑维持)】
```

### 核心分工原则：
- **通用线性交付数值归入 UMR**：凡能够归纳为“技能全局参数 × 加点线性缩放”的数值，一律以 OpCode 驱动，数据落在 canonical UMR；
- **机制与分支逻辑留在 C++**：形态改变（如 351 变近战环绕）、触发监听、特殊实体生成（分裂子剑气）、赋值式 Keystone（311/330）与非线性后处理（210 惩罚）保持在代码层；
- **单一事实源（Pillar 6）**：同一数值禁止在 C++ 字面量、canonical UMR 与 `skill_mechanics.json` 中双重/三重维护。线性数值的唯一源是 canonical UMR；非线性数值的唯一源是 `skill_mechanics.json`（代码只读不写死）。迁移完成即退役旧源，并加门禁阻止回潮。

---

## 3. 详细技术设计

### 3.1 OpCode 体系扩充（37..40）

在 [`ModifierContext.hpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/modifier/ModifierContext.hpp) 中扩充 `ModifierOpCode`：

```cpp
enum class ModifierOpCode : uint16_t {
  // 0..4: 基础属性与全局技能修饰
  // 5..28: 怪物行为与事件
  // 30..36: 既有技能交付算子
  SKILL_MORE_DAMAGE_MULT = 30,
  SKILL_COOLDOWN_FLAT    = 31,
  SKILL_COOLDOWN_MULT    = 32,
  SKILL_CHARGES_ADD      = 33,
  SKILL_BONUS_CRIT       = 34,
  SKILL_AREA_MULT        = 35,
  SKILL_MANA_COST_MULT   = 36,

  // Batch 1 新增技能交付算子 (37..40)
  SKILL_PROJECTILES_ADD   = 37, // param_u32: 技能 ID; param_f32: 每点投射物数量 (1.0 = 每点 +1)
  SKILL_MANA_COST_FLAT    = 38, // param_u32: 技能 ID; param_f32: 每点绝对法耗减免 (负值如 -1.0 = 每点 -1 蓝)
  SKILL_BONUS_CRIT_DAMAGE = 39, // param_u32: 技能 ID; param_f32: 每点额外暴伤百分点 (0.25 = 每点 +25%)
  SKILL_RANGE_MULT        = 40, // param_u32: 技能 ID; param_f32: 每点射程乘算偏移 (0.10 = 每点 +10%)
};
```

- **类别归属**：37..40 均归入 `ModifierOpCategory::SkillDelivery`（`CategoryOfOp` 扩展匹配；`IsSkillDeliveryOpCode` 委托 `CategoryOfOp`，故只有单一改动点）；
- **参数惯例**：`param_u32` 为技能 ID（容器字典键），`param_f32` 为每点幅度；
- **点数缩放公式**：与 30..36 保持一致，设有效加点为 $N$：
  - `SKILL_PROJECTILES_ADD`：累加 `static_cast<int>(op.param_f32 * N)` 到 `skill_projectiles_add`；
  - `SKILL_MANA_COST_FLAT`：累加 `op.param_f32 * N` 到 `skill_mana_cost_flat`；
  - `SKILL_BONUS_CRIT_DAMAGE`：累加 `op.param_f32 * N` 到 `skill_bonus_crit_damage`；
  - `SKILL_RANGE_MULT`：累乘 `1.0f + op.param_f32 * N` 到 `skill_range_mult`。
- **整数算子约束（37）**：`static_cast<int>` 对非整数 `param_f32` 会静默截断（`0.5 × 1` 得 `0`），且负数向零截断。因此 `SKILL_PROJECTILES_ADD` 的 `param_f32` **必须为整数值**；由 contract 生成器增加断言强制（见 §5.1 第 5 项离线门禁），本批次取值恒为 `1.0`。
- **命名说明**：节点 210（多重剑气）与 300（剑池充盈）的语义是"剑气/灵剑数量"，共用 `projectile_count` 字段。算子名保留 `SKILL_PROJECTILES_ADD` 以对齐字段名与既有 33 号算子风格；语义以"该技能交付数量"为准，按技能 ID 分键，不与其他技能串味。

### 3.2 `ModifierDelta` 容器与合并更新

在 [`ModifierEvaluator.hpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/modifier/ModifierEvaluator.hpp) 与 [`ModifierEvaluator.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/modifier/ModifierEvaluator.cpp) 中扩充：

```cpp
class ModifierDelta {
  // 既有容器...
  std::unordered_map<uint32_t, int> skill_projectiles_add;       // 缺省 0，累加
  std::unordered_map<uint32_t, float> skill_mana_cost_flat;     // 缺省 0.0f，累加
  std::unordered_map<uint32_t, float> skill_bonus_crit_damage;  // 缺省 0.0f，累加
  std::unordered_map<uint32_t, float> skill_range_mult;         // 缺省 1.0f，累乘

public:
  void AddSkillProjectiles(uint32_t skillId, int count);
  [[nodiscard]] int GetSkillProjectiles(uint32_t skillId) const;

  void AddSkillManaCostFlat(uint32_t skillId, float flat);
  [[nodiscard]] float GetSkillManaCostFlat(uint32_t skillId) const;

  void AddSkillBonusCritDamage(uint32_t skillId, float delta);
  [[nodiscard]] float GetSkillBonusCritDamage(uint32_t skillId) const;

  void AddSkillRangeMult(uint32_t skillId, float mult);
  [[nodiscard]] float GetSkillRangeMult(uint32_t skillId) const;

  void MergeFrom(const ModifierDelta &other); // 同步合并这 4 个新容器
};
```

### 3.3 烘焙合成序列规范化（The Baking Pipeline）

在 [`SkillSpecializationBaker.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSpecializationBaker.cpp) 中，`Bake()` 执行步骤固化为以下严格顺序：

```text
步骤 1: 基础属性与交付默认推导 (switch skill_id: 设置 base speed / range / duration 等)
步骤 2: 专精树 AOT 节点循环 (仅机制标志位 feature_flags、元素转质、触发契约；移除纯数值分支)
步骤 3: UMR 交付 Delta 一次性求值与合成 (按固定公式生效)
步骤 4: 确定性 Keystone 与形态覆盖 (154 覆盖、210 伤害惩罚、311 翻倍、330 巨剑置 1)
步骤 5: 装备修饰器 (ItemComponent::skill_modifiers)
步骤 6: 装备 UMR 法耗折叠 (EquipmentModifierAdapter)
```

**步骤 1 的基准显式化（防隐式耦合）**：`SKILL_RANGE_MULT` 以乘算作用于既有基准，因此每种技能的基准必须**在步骤 1 显式写入**，不得依赖 `BakedDeliveryParams` 的结构体默认值（默认值一旦被其他技能或后续改动调整，节点 310 会静默漂移，且乘算不具备幂等性）：

```cpp
case 2: // 裂空斩（既有，不变）
  del.speed = 1.0f;
  del.range = 500.0f; // 既有基准
  out_profile.area_radius = 35.0f;
  break;
case 3: // 灵剑决：基准索敌半径 200，必须显式赋值（节点 310 在其上乘算）
  del.range = 200.0f;
  out_profile.projectile_count = 3;
  break;
```

> 依据：`case 7`（心剑·无影）已有同类先例与注释——基准 range 缺省会让后续节点回落到公式内 200 基线而出现"点满反而更短"的缺陷。因此 skill 3 的 200 基准必须与 skill 2 的 500 一样显式写入，不得依赖 `del = BakedDeliveryParams{}` 的默认值。

**步骤 3 的确定性合成公式**：
```cpp
// 1. 法耗：先平减/平加，再乘专精折扣，下限 0
out_profile.effective_mana_cost = std::max(
    0.0f,
    (out_profile.effective_mana_cost + specDelta.GetSkillManaCostFlat(skill_id))
        * specDelta.GetManaCostMultiplier(skill_id));

// 2. 冷却：先绝对秒数，再乘算，下限 0
out_profile.effective_cooldown = std::max(
    0.0f,
    (out_profile.effective_cooldown + specDelta.GetSkillCooldownFlat(skill_id))
        * specDelta.GetSkillCooldownMult(skill_id));

// 3. 充能数
out_profile.effective_charges += specDelta.GetSkillCharges(skill_id);

// 4. 投射物/灵剑数量
out_profile.projectile_count += specDelta.GetSkillProjectiles(skill_id);

// 5. 更多伤害乘区
out_profile.more_damage_mult *= specDelta.GetSkillMoreDamageMult(skill_id);

// 6. 范围半径乘区
out_profile.area_radius *= specDelta.GetSkillAreaMult(skill_id);

// 7. 交付参数包扩展
del.bonus_crit += specDelta.GetSkillBonusCrit(skill_id);
del.bonus_crit_damage += specDelta.GetSkillBonusCritDamage(skill_id);
del.range *= specDelta.GetSkillRangeMult(skill_id);
```

**法耗结算顺序（跨域固化，避免语义漂移）**：本批次后，技能法耗的完整结算序为「专精 UMR 平减（flat）→ 专精 UMR 折扣（mult）→ 装备 flat → 装备 mult（步骤 6 折叠）」。当前没有同时带 flat 与 mult 的技能，故本批次不产生行为变化；但必须在设计中固化此顺序，并补一条同时含 flat 与 mult 的测试用例，防止后续新增节点时静默改变乘/加顺序。

**步骤 4 的 Keystone 后置覆盖**：
```cpp
// 技能 1: 154 孤注一掷覆盖
if (skill_id == 1) {
  if (spec->allocated_points.contains(154)) { ... }
}
// 技能 2: 210 多重剑气伤害惩罚后处理（非线性，数值从 skill_mechanics.json 读取，禁止字面量）
else if (skill_id == 2) {
  const auto it210 = spec->allocated_points.find(210);
  if (it210 != spec->allocated_points.end() && it210->second > 0) {
    const int pts = it210->second;
    const std::string key = "damage_penalty_pt" + std::to_string(std::clamp(pts, 1, 3));
    const float penalty_pct =
        data::SkillMechanicsRegistry::Get().GetFloat(2, 210, key, 0.0f);
    out_profile.more_damage_mult *= (1.0f - penalty_pct);
  }
}
// 技能 3: 330 / 311 覆盖
else if (skill_id == 3) {
  if ((out_profile.delivery.feature_flags & 2) != 0) {
    // 330 巨剑降临：固定为 1 柄，范围 80
    out_profile.projectile_count = 1;
    out_profile.area_radius = 80.0f;
  } else if ((out_profile.delivery.feature_flags & 1) != 0) {
    // 311 无尽剑匣：在已加点灵剑总数上翻倍
    out_profile.projectile_count *= 2;
  }
}
```

---

## 4. 技能 2 与技能 3 节点数据配置契约

### 4.1 记录 ID 规则与通用字段
沿用决议 1：`2,000,000 + node_id * 10 + op_index`。
- `priority`: `200`
- `profession_mask`: `1` (BladeAscendant)
- `weapon_class_mask`: `65535`
- `equip_slot_mask`: `0`
- `exclusive_group`: `0`
- `max_active`: `0`
- `debug_source`: `"skill_spec_node"`
- `min_player_level`: `1`

> **ID 解码门禁**：生成器 `--check` 须断言 `modifier_id` 解码结果等于 `node_id_whitelist[0]`（即 `(id - 2,000,000) / 10 == node_id`），唯一豁免为历史记录 `2002103`（白名单实为 node 213，属已登记例外）。该规则必须由机器校验，不得仅停留在文档描述。

### 4.2 详细记录对照表

| 记录 ID | 技能 | 节点 | OpCode | param_u32 | param_f32 | canonical stat_path | debug_name |
|---|---|---|---|---|---|---|---|
| `2002000` | 2 (裂空斩) | 200 (剑气纵横) | `SKILL_AREA_MULT` | 2 | `0.10` | `skill.area_mult` | `RendingWave_Node200_AreaMult` |
| `2002001` | 2 (裂空斩) | 200 (剑气纵横) | `SKILL_RANGE_MULT` | 2 | `0.10` | `skill.range_mult` | `RendingWave_Node200_RangeMult` |
| `2002010` | 2 (裂空斩) | 201 (凝神) | `SKILL_MANA_COST_FLAT` | 2 | `-1.0` | `skill.mana_cost_flat` | `RendingWave_Node201_ManaCostFlat` |
| `2002020` | 2 (裂空斩) | 202 (锋芒) | `SKILL_MORE_DAMAGE_MULT` | 2 | `0.10` | `skill.more_damage` | `RendingWave_Node202_MoreDamage` |
| `2002100` | 2 (裂空斩) | 210 (多重剑气) | `SKILL_PROJECTILES_ADD` | 2 | `1.0` | `skill.projectiles_add` | `RendingWave_Node210_Projectiles` |
| `2003000` | 3 (灵剑决) | 300 (剑池充盈) | `SKILL_PROJECTILES_ADD` | 3 | `1.0` | `skill.projectiles_add` | `BladeFormation_Node300_Projectiles` |
| `2003020` | 3 (灵剑决) | 302 (锋灵) | `SKILL_MORE_DAMAGE_MULT` | 3 | `0.10` | `skill.more_damage` | `BladeFormation_Node302_MoreDamage` |
| `2003100` | 3 (灵剑决) | 310 (索敌范围) | `SKILL_RANGE_MULT` | 3 | `0.20` | `skill.range_mult` | `BladeFormation_Node310_RangeMult` |
| `2003120` | 3 (灵剑决) | 312 (灵力网络) | `SKILL_MANA_COST_MULT` | 3 | `0.05` | `skill.mana_cost` | `BladeFormation_Node312_ManaCost` |
| `2003310` | 3 (灵剑决) | 331 (弱点锁定) | `SKILL_BONUS_CRIT` | 3 | `0.05` | `skill.bonus_crit` | `BladeFormation_Node331_BonusCrit` |
| `2003320` | 3 (灵剑决) | 332 (致命锋芒) | `SKILL_BONUS_CRIT_DAMAGE` | 3 | `0.25` | `skill.bonus_crit_damage` | `BladeFormation_Node332_BonusCritDamage` |

> 说明 1：既有历史记录 `2002103` 登记为特殊例外维持现状，不予覆盖。
>
> 说明 2（字段语义陷阱）：canonical 记录没有独立的 `skill_id` 字段，技能 ID 复用 `stacks` 承载（迁移脚本 `stacks = op.param_u32`，且校验 `stacks ∈ [1,99]`，见 `migrate_skill_spec_modifier_slice.py`）。因此上表"`param_u32`"列在 canonical JSON 中写作 `"stacks"`，`param_f32` 写作 `"value"`。手工录入 11 条记录时必须遵守此映射，写错会命中错误技能或触发校验失败；且技能 ID ≥ 100 时会撞上 `stacks` 上限，属已知约束。

### 4.3 数值单一事实源治理（`skill_mechanics.json` 键退役）

迁移完成后按下表处置 `skill_mechanics.json` 中的同名键，禁止保留第二份事实源：

| 技能/节点 | mechanics 键 | 现状 | 迁移后处置 |
|---|---|---|---|
| 2/200 | `area_radius_pct_per_point`, `range_pct_per_point` | 死数据（无读者） | **删除**（canonical `2002000`/`2002001` 为唯一源） |
| 2/201 | `mana_reduction_per_point` | 死数据 | **删除**（canonical `2002010`） |
| 2/202 | `phys_damage_pct_per_point` | 死数据 | **删除**（canonical `2002020`） |
| 2/210 | `damage_penalty_pt1/2/3` | 死数据 | **保留并转为唯一源**，由步骤 4 读取（非线性，无法线性外置） |
| 3/302 | `phys_damage_pct_per_point` | 活数据 | **删除**（canonical `2003020`） |
| 3/300 | `swords_per_point` | 死数据（无读者） | **删除**（canonical `2003000` 为唯一源；同节点 `duration` 保留，见 `BladeFormation.cpp` 召唤时长读取） |
| 3/310 | `range_pct_per_point` | 活数据 | **删除**（canonical `2003100`） |
| 3/312 | `cost_reduction_pct_per_point` | 活数据 | **删除**（canonical `2003120`） |
| 3/331 | `crit_chance_per_point` | 活数据 | **删除**（canonical `2003310`） |
| 3/332 | `crit_damage_per_point` | 活数据 | **删除**（canonical `2003320`） |
| 3/301 | `haste_pct_per_point` | 活数据 | **保留**（本批次显式不迁移，见 §1.3 非目标 4） |

**一致性门禁（必须实现）**：
1. 迁移前置断言：对每条新 canonical 记录，断言 `canonical.value == mechanics[key] / 100`（技能 2 的 200/201 例外见下），确保迁移瞬间数值严格等价，不产生静默平衡变化；
   - 特例：节点 201 原逻辑为**绝对平减 1.0 蓝**（`- 1.0f*points`），mechanics 键 `mana_reduction_per_point=1.0` 与 canonical `value=-1.0` 语义一致（前者是"每点减 1"，无 /100），门禁需按算子逐条给出期望关系，不能一律套用 `/100`。
2. 退役后断言：上述"删除"清单中的键不得再出现于 `skill_mechanics.json`（防止回潮）；
3. 反向登记：每个被删除的键都必须在门禁白名单中登记其 canonical 替代记录 ID，新增/遗留键一律报错。

---

## 5. 验证与测试策略

### 5.1 自动化测试矩阵
1. **OpCode 基础单元测试（`SkillSpecDeliveryOpTests` / 新增 `SkillBatch1DeliveryOpTests`）**：
   - 断言 37..40 的单点与多点缩放计算；
   - 断言 `SKILL_MANA_COST_FLAT` 负值减免与下限 0；
   - 断言 `SKILL_RANGE_MULT` 累乘行为；
   - 断言 `ModifierDelta::MergeFrom` 对 4 个新容器的合并正确性。
2. **烘焙层组合测试（`SkillSpecializationBakerTests`）**：
   - 技能 2：200（范围+射程）、201（平减蓝耗）、202（More 增伤）、210（投射物+4 与惩罚 0.85x）；
   - 技能 3：300（灵剑数量 3+4=7）、310（射程）、312（蓝耗折扣）、331（暴击+25%）、332（暴伤+100%）；
   - 技能 3 复合覆盖：300 加点 + 311 翻倍（(3+4)*2 = 14）；300 加点 + 330 巨剑置 1（数量 1，范围 80）。
3. **单一事实源与基准确值测试**：
   - 断言 skill 3 步骤 1 后 `del.range == 200.0f`（基准显式化），且节点 310 生效后 `del.range == 200.0f * (1 + 0.20 * N)`；
   - 断言节点 210 惩罚读取 mechanics（把 `damage_penalty_pt3` 改为 `0.30` 时 `more_damage_mult` 随之变化），证明不存在残留字面量；
   - 一条同时含 `SKILL_MANA_COST_FLAT` 与 `SKILL_MANA_COST_MULT` 的用例，锁定「先 flat 后 mult」的结算序。
4. **功能回归测试**：
   - 执行 `tests/functional/RendingWaveNodes.cpp` 与 `tests/functional/BladeFormationNodes.cpp`，断言实机施法与伤害行为无偏差。
5. **离线门禁校验**：
   - 跑通 `gen_skill_spec_modifier_contract.py --check` 与 `gen_modifier_runtime_v2.py --check`；
   - 新增/扩展 Python 门禁：
     - 记录 ID 解码断言（§4.1）；
     - canonical↔mechanics 迁移等价断言与退役键不回潮断言（§4.3）；
     - 领域约束断言：OpCode 30..40 只允许出现在 `debug_source == "skill_spec_node"` 的记录上，防止 SkillDelivery 类别蔓延（上批次评审 N-1 遗留）；
     - `SKILL_PROJECTILES_ADD` 的 `param_f32` 必须为整数值（§3.1 整数算子约束）。

---

## 6. 风险评估与应对措施

| 风险项 | 等级 | 应对措施 |
|---|:---:|---|
| 311 翻倍与 300 投射物加点时序颠倒 | Medium | 在设计与计划中严格固化执行步骤：步骤 3 先应用 UMR 交付增量，步骤 4 再执行 311 翻倍或 330 置 1，并编写确定性断言。 |
| **单一事实源被破坏（canonical 与 mechanics 双份承载）** | **High** | 迁移后立即退役 §4.3 清单中的 mechanics 键，并加"退役键不回潮 + 迁移等价"门禁。这是 Pillar 6 的核心约束，不得仅以文档约定代替机器校验。 |
| **`SKILL_RANGE_MULT` 隐式依赖 `BakedDeliveryParams::range` 默认值 200** | **Medium** | 步骤 1 对 skill 3 显式写入 `del.range = 200.0f`（对齐 skill 2 的 500 与 case 7 的既有先例），并加基准确值测试。 |
| 210 非线性伤害惩罚硬编码 / 丢失 | Medium | 投射物数量由 UMR 交付；惩罚保留在步骤 4，但改为读取 `skill_mechanics.json` 的 `damage_penalty_pt1/2/3`（`1 - pct`），并以"改数据即变化"的测试守护。 |
| 删除 mechanics 键影响其它消费者 | Low | 删除前以 `rg` 全仓确认无读者（本批已确认：仅 Baker 读取，且 tests 不直接读这些键）；删除后跑全量功能测试。 |
| `ModifierDelta` 持续膨胀（本批 11→15 个 `unordered_map`） | Low（性能）/ Medium（架构） | 性能上无影响（Bake 是 AOT、空 map 不分配堆）；但每交付概念新增需改 7 处，登记为设计债，建议后续批次改为按 `(skill_id, kind)` 的紧凑结构或专用 POD。 |
| SkillDelivery 类别向非技能调用点泄漏（上批 N-1） | Low | 本批新增 37..40 扩大影响面；加生成器领域约束断言，并计划将 `TalentModifierAdapter`/`MapModifierAdapter`/`SkillSpecModifierAdapter::EvaluateDamageMultiplier` 的 `Evaluate` 默认掩码收紧（可独立小改动）。 |
| 生成脚本 opcode 字典未同步 | Low | 同步更新 `gen_modifier_runtime_v2.py` 与 `migrate_skill_spec_modifier_slice.py`，由门禁 `--check` 强制阻断漂移。 |
