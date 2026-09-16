# UMR 技能专精承载底座扩充与技能 1 数值切片设计说明

- **文档状态**：已按评审意见修订（2026-09-16）/ 待复审
- **文档路径**：`docs/designs/2026-09-16-umr-skill-spec-foundation-and-skill1-slice-design.md`
- **设计日期**：2026-09-16
- **系统代号**：`UMR-SPEC-P0/P1` (Unified Modifier Runtime - Skill Spec Extension)
- **输入来源**：
  - `设计文档/统一修饰器运行时系统_UMR.md`
  - `docs/reviews/2026-09-16-equipment-mana-cost-bake-fold-review.md`（特别是残留风险 1：槽位/职业/武器过滤）
  - `docs/designs/2026-09-13-skill-abstraction-and-b2-cleanup-design.md`（Pillar 6 与单源契约）
  - 代码实况：`src/game/systems/modifier/*`、`src/game/systems/skill/SkillSpecializationBaker.cpp`、`scripts/gen_skill_spec_modifier_contract.py`、`scripts/migrate_skill_spec_modifier_slice.py`、`assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json`

---

## 0. 评审决议记录（2026-09-16）

| # | 争议点 | 决议 |
|---|---|---|
| 1 | `skill_spec` 记录 ID 方案 | 采用「域基址 + 本地编号」：`2,000,000 + node_id * 10 + op_index`（每节点预留 10 个记录位，支撑「一记录一 op」拆分）。既有 `2002103` 登记为历史例外，不迁移。 |
| 2 | 未选主职业时的职业过滤 | 视为通配、保持现状：仅当 `mainProfession >= 0` 时填充 `ctx.profession_id`，否则维持 0。 |
| 3 | 154 孤注一掷与 110/111 的组合优先级 | 154 最终覆盖：不论同时投入哪些节点，结果为 CD 8s、1 充能、必暴、More ×2。 |
| 4 | canonical 承载多 op 的方式 | canonical 契约不变，多 op 节点拆为多条单 op 记录；只在迁移脚本注册新 opcode 映射。 |

> 决议 1 是对「域基址 + 节点号」的细化：在 node_id 之后追加 1 位 op 序号，避免同一节点 2 条记录 ID 冲突，并为未来第 3 个 op 预留空间。

---

## 1. 背景与目标界定

### 1.1 背景与现状
在 2026-09-16 完成的 UMR 加固与装备降蓝耗折叠批次（commit `d56d61a9`）中，UMR 确立了在装备法力修正、地图/怪物词缀并轨、二进制生成门禁与持久化上限防御上的稳定底座，全仓 1683 个测试全量通过。

然而，在面对全仓 12 技能、331 个技能专精节点的统一数值执行诉求时，当前 UMR 框架存在三个关键的能力断层：

1. **点数上下文与缩放机制缺失**：331 个专精节点中，有 217 个节点（65.6%）需要 2~5 点多级投入。当前 `ModifierEvalContext` 仅有 `std::vector<uint32_t> active_node_ids`，求值逻辑仅靠布尔包含匹配，离线编译产物中只有固定标量 `param_f32`，无法表达 `value * points` 的点数成长。
2. **技能交付参数 OpCode 缺失**：专精节点除了全局基础属性外，大量修饰的是技能冷却（`cooldown`）、充能上限（`charges`）、技能专属更多伤害乘算（`more_damage_mult`）、基础暴击率（`bonus_crit`）、技能作用半径（`area_radius`）等交付参数。现有 `ModifierOpCode` 仅有 5 个基础属性算子与怪物事件/行为算子，技能交付算子覆盖率为 0。
3. **装备过滤上下文残留**：`EquipmentModifierAdapter` 在构建求值上下文时未填充 `equip_slot_mask`、职业与武器掩码，装备词缀自带的部位与武器限定当前等效于全通配。

### 1.2 目标（Goals）
1. **点数分配模型标准化（P0）**：在 `ModifierEvalContext` 中引入节点点数映射（`node_points`），并在 `ModifierEvaluator` 中建立点数感知的算子缩放求值路径；未提供点数上下文的既有调用点必须保持原行为（回退为单次生效）。
2. **技能交付参数算子体系（P0）**：
   - 扩充 `ModifierOpCode`，新增 7 个标准技能交付参数算子（30..36，含独立的 `SKILL_MANA_COST_MULT`）；
   - 在 `ModifierDelta` 中建立收集通道，在 `SkillSpecModifierAdapter` 中提供高内聚求值接口。
3. **装备词缀槽位/职业/武器过滤闭环（P0）**：按装备槽分组求值并填充 `equip_slot_mask`，同时补齐职业与武器掩码，闭合残留风险 1。
4. **技能 1（流云刺）专精数值垂直切片接入（P1）**：
   - 将流云刺（Skill 1）的 6 个核心数值型专精节点（气聚 101、剑心洞明 102、流云劲 103、贯日 110、连环 111、瞬狱影爆 134）全量迁入 `skill_spec` 数据管线（canonical → runtime JSON → 二进制）；
   - 在 `SkillSpecializationBaker::Bake` 中接入 UMR 求值结果，退役原 `case 1:` 中的对应硬编码数值分支；
   - 保证流云刺的所有既有单元测试与功能测试行为 100% 等价。

### 1.3 明确非目标（Non-Goals）
1. **非目标**：本阶段**不推进**技能 1 的机制形态/行为特性向 UMR 迁移（如留影 130 残影生成、风行者 113 疾风状态、移形换位 133 传送分支、元素转质 170/172 等），此类机制继续由 `FlowingThrustSpecStateGen` 与 C++ 行为层承接。
2. **非目标**：本阶段**不盲目推进**技能 2~12 的全量迁移，待技能 1 垂直切片建立稳定基线后在下一阶段批量铺开；技能 2 的 HeavyMomentum `2002103`（已知遗留 M6 语义问题）不在本切片修正范围。
3. **非目标**：不重构二进制运行时四表（RecordTable / FilterTable / OpTable / IndexTable）的内存布局与寻址模式，保持 64 字节文件头与确定性 CRC 校验不变。
4. **非目标**：不修改 canonical schema 与 `gen_skill_spec_modifier_contract.py` 的单 op 输出契约；多 op 节点通过拆记录表达（见决议 4）。

---

## 2. 体系架构与系统边界

### 2.1 系统职责与分工边界

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│                          玩家加点与专精分配 (SpecializedSkill)                 │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │ allocated_points
                 ┌─────────────────────┴─────────────────────┐
                 ▼                                           ▼
   【UMR 数值修饰执行层】 (统一数据驱动)          【SpecState 行为形态层】 (代码驱动)
   - 伤害 More 倍率 (SKILL_MORE_DAMAGE_MULT)    - 机制开关标志 (bool windwalker / swap)
   - 暴击率加成 (SKILL_BONUS_CRIT)              - 残影生成与协同 (afterimage)
   - 冷却时间减免 (SKILL_COOLDOWN_FLAT/MULT)    - 元素转质 (ResolveElementalConversion)
   - 充能次数增减 (SKILL_CHARGES_ADD)           - 孤注一掷 Keystone 架构分支
   - 范围半径缩放 (SKILL_AREA_MULT)             - 真实位移动态测距加成 (势如破竹)
   - 法耗折扣 (SKILL_MANA_COST_MULT)            - 条件暴击 (御风而行 114)
                 │                                           │
                 └─────────────────────┬─────────────────────┘
                                       │ 汇流
                                       ▼
                       【BakedSkillProfile / BakedDeliveryParams】
                                       │
                                       ▼
                         【战斗执行层 FlowingThrust::DoCast】
```

- **原则**：**通用的交付层数值与倍率统一走 UMR；独特的机制逻辑、实体生成与形态变更保持在行为层**。杜绝在 UMR 中生硬设计全能 DSL。
- **数据链路**：`canonical/skill_spec_modifiers.canonical.json` → `scripts/gen_skill_spec_modifier_contract.py` → `assets/data/modifier_v2/skill_spec_modifiers.json` → `scripts/gen_modifier_runtime_v2.py` → `assets/generated/modifier_runtime_v2.bin`。数值字面量只允许出现在 canonical 层。

---

## 3. 详细技术方案

### 3.1 点数分配模型（Points Scaling）

#### 3.1.1 上下文扩充
在 [`ModifierContext.hpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/modifier/ModifierContext.hpp) 中定义紧凑节点分配结构，并扩充 `ModifierEvalContext`：

```cpp
struct NodePointEntry {
  uint32_t node_id = 0;
  uint16_t points = 0;
};

struct ModifierEvalContext {
  uint32_t profession_id = 0;
  uint32_t skill_id = 0;
  Tag skill_tags = Tag::None;
  uint32_t weapon_class_mask = 0xFFFFFFFFu;
  uint32_t equip_slot_mask = 0xFFFFFFFFu;
  std::vector<uint32_t> active_node_ids;      // 过滤匹配的高速索引，保持既有语义
  std::vector<NodePointEntry> node_points;    // 按 node_id 升序排列（调用方保证；适配器负责排序）

  [[nodiscard]] uint16_t GetPointsForNode(uint32_t nodeId) const;  // 未分配返回 0
};
```

- `node_points` 不替代 `active_node_ids`：过滤器 `ContainsAnyNode` 仍走 `active_node_ids` 快速索引，适配器从同一份加点数据同时填充两者。
- `GetPointsForNode` 实现为有序表的二分查找；若未来出现非排序输入，按线性扫描兜底。

#### 3.1.2 记录级点数解析
在 `ModifierEvaluator.cpp` 求值记录时，解析规则如下：

1. 记录 `node_whitelist` 为空 → `effective_points = 1`（装备/天赋/地图/怪物等非专精域记录保持单次生效）。
2. `ctx.node_points` 为空 → `effective_points = 1`（**兼容回退**：只填 `active_node_ids` 的既有调用点，如 `EvaluateDamageMultiplier`、`SkillDisplayPreviewService`，行为不变）。
3. 否则遍历 `node_whitelist`，取第一个在 `ctx.node_points` 中 `points > 0` 的节点作为 `effective_points`。
   - **契约**：`skill_spec` 域记录的白名单必须恰好 1 个节点（由离线管线校验；求值层保留遍历写法作防御）。
4. `effective_points == 0` → 跳过该记录（不产生的任何 delta）。
5. `effective_points > 0` → 作为实参传给算子执行器，按 `每点数值 × N` 缩放。

**不做上限截断**：节点最大点数的校验归加点/存档校验层；求值层按 `value × N` 线性外推（当前 `NodeContractData` 无最大点数数据源，求值层截断既无依据也会掩盖加点侧的错误）。测试覆盖 1/3/5 点线性即可。

### 3.2 技能交付 OpCode 与求值通道

#### 3.2.1 OpCode 扩充
在 `ModifierOpCode` 中分配专属区间（30..36）：

```cpp
enum class ModifierOpCode : uint16_t {
  // 既有玩家属性 (0..4)
  ADD_STAT_FLAT = 0,
  ADD_STAT_PERCENT_ADD = 1,
  ADD_STAT_PERCENT_MULT = 2,
  ADD_SKILL_LEVEL = 3,
  MANA_COST_MULT = 4,

  // 既有怪物事件/行为 (5..28)
  ...

  // 新增技能交付参数算子 (30..36)
  SKILL_MORE_DAMAGE_MULT = 30,   // param_u32: 技能 ID；param_f32: 每点 More 加成（0.10 = 每点 +10%，负值合法）
  SKILL_COOLDOWN_FLAT    = 31,   // param_u32: 技能 ID；param_f32: 每点绝对秒数（负值 = 减冷却）
  SKILL_COOLDOWN_MULT    = 32,   // param_u32: 技能 ID；param_f32: 每点乘算偏移（0.15 = 每点 +15% CD）
  SKILL_CHARGES_ADD      = 33,   // param_u32: 技能 ID；param_f32: 每点平加充能数（1.0 = 每点 +1 充能）
  SKILL_BONUS_CRIT       = 34,   // param_u32: 技能 ID；param_f32: 每点平加暴击率（0.02 = 每点 +2%）
  SKILL_AREA_MULT        = 35,   // param_u32: 技能 ID；param_f32: 每点范围乘算偏移（0.20 = 每点 +20%）
  SKILL_MANA_COST_MULT   = 36,   // param_u32: 技能 ID；param_f32: 每点法耗折扣（0.15 = 每点 −15%，下限 0）
};
```

**参数约定（与既有 `MANA_COST_MULT` 惯例一致）**：`param_u32` 恒为技能 ID（delta 容器键），每点幅度放 `param_f32`。
- 不新增独立的「每点充能数」字段：`SKILL_CHARGES_ADD` 的每点幅度同样放 `param_f32`，应用时取整（`static_cast<int>(param_f32 * N)`）。
- `param_u32` 兼作 canonical `stacks` 字段，迁移契约要求其落在 `[1, 99]`，技能 ID 天然满足。

**为什么不复用 `MANA_COST_MULT`**：装备域 `MANA_COST_MULT` 的 `param_f32` 是「最终乘数」（如 `0.9`），且装备路径已按记录折叠为单次乘算；专精域需要「每点折扣」（`1 − 0.15N` 且下限 0）。两种语义混用会导致装备折叠契约被点数缩放污染，故拆出 36。

#### 3.2.2 算子执行语义
对于 `effective_points = N`：

- **`SKILL_MORE_DAMAGE_MULT`**：乘算累积 `out.AddSkillMoreDamageMult(skillId, 1.0f + op.param_f32 * N)`（负系数如 110 即 `1.0f - 0.15f * N`）。
- **`SKILL_COOLDOWN_FLAT`**：累加 `out.AddSkillCooldownFlat(skillId, op.param_f32 * N)`。
- **`SKILL_COOLDOWN_MULT`**：乘算累积 `out.AddSkillCooldownMult(skillId, 1.0f + op.param_f32 * N)`。
- **`SKILL_CHARGES_ADD`**：整数累加 `out.AddSkillCharges(skillId, static_cast<int>(op.param_f32 * N))`。
- **`SKILL_BONUS_CRIT`**：累加 `out.AddSkillBonusCrit(skillId, op.param_f32 * N)`。
- **`SKILL_AREA_MULT`**：乘算累积 `out.AddSkillAreaMult(skillId, 1.0f + op.param_f32 * N)`。
- **`SKILL_MANA_COST_MULT`**：写入既有 `mana_cost_mult` 容器 `out.AddManaCostMultiplier(skillId, std::max(0.0f, 1.0f - op.param_f32 * N))`，烘焙侧复用 `GetManaCostMultiplier(skillId)` 读取。

Bake 侧合成公式（固定顺序，确定性）：
- `effective_cooldown = max(0, (effective_cooldown + flat) * mult)`；
- `more_damage_mult *= moreDelta`；`area_radius *= areaDelta`；
- `effective_charges += chargesDelta`；`bonus_crit += critDelta`；
- `effective_mana_cost *= manaDelta`。

#### 3.2.3 类别掩码支持
在 `ModifierOpCategory` 中扩充 `SkillDelivery = 1u << 3`，`All` 同步更新为 `(1u << 0) | (1u << 1) | (1u << 2) | (1u << 3)`。

- `CategoryOfOp` 将 30..36 归入 `SkillDelivery`；
- 注意 `ModifierEvaluator.cpp` 存在两条求值路径（运行时记录路径与静态 `span<const ModifierRecord>` 路径），两条路径的算子 switch 必须同步；建议抽取共享的算子应用辅助函数，避免双路径漂移。

### 3.3 装备槽位/职业/武器过滤上下文补齐（闭环残留风险 1）

在 `EquipmentModifierAdapter.cpp` 中：

1. **记录收集携带槽位**：收集结构改为 `struct EquippedRecordRef { uint32_t record_id; uint32_t slot_bit; }`，`slot_bit = 1u << static_cast<uint32_t>(slot)`（`EquipmentComponent::slots` 下标即 `EquipmentSlot` 值，与既有数据 `equip_slot_mask = 2 (MainHand)` 的约定一致）；按 `(slot_bit, record_id)` 去重并排序，保证求值确定性。
2. **按槽分组求值**：既有 `Evaluate(registry, span<recordIds>, ctx)` 只接受单一 `ctx`，因此**必须按槽分组调用**（不得逐记录切换掩码）：每组复制上下文并置 `ctx.equip_slot_mask = slot_bit`，调用一次求值后把结果合并进总 delta。
   - 需为 `ModifierDelta` 增加 `MergeFrom`（或等价累加接口），覆盖全部容器（flat / percent_add / percent_mult / skill_levels / mana_cost_mult / 6 个技能交付容器 / 怪物集合），否则无法合并多次求值结果。
3. **消费方统一改造**：`ApplyEquippedSkillLevelBonuses` 与 `GetEquippedManaCostMultiplier` 均切换为按槽分组求值；公开 API `CollectEquippedRecordIds` 若变更签名，需同步 `tests/unit/EquipmentModifierAdapterTests.cpp:152,178` 的调用。
4. **职业填充**：从 `AstrolabeComponent::mainProfession` 读取（`Progression.hpp`）；`>= 0` 时填充 `ctx.profession_id`，未誓约（`< 0`）维持 0（保持现状，与当前数据掩码 63/1 的行为一致）。
5. **武器填充**：`ctx.weapon_class_mask = Σ(1u << static_cast<uint32_t>(WeaponSubtype))`，对已装备的主手/副手武器取按位或；无真实武器（徒手）时置 `WeaponSubtype::None`（bit 0）而非 0。
   - 过滤侧通配集合为 `{0, 0xFFFF, 0xFFFFFFFF}`，故既有 `weapon_class_mask = 65535` 记录对徒手与持械角色均继续生效（`ModifierEvaluator.cpp` 存在两处重复实现，需同步修改）。
   - 由此徒手上下文置位 bit 0，`filter.weapon_class_mask = 1` 即表示「仅徒手」。

### 3.4 技能 1（流云刺）数值节点搬迁对照

将原 `SkillSpecializationBaker.cpp:234-310` 中流云刺的纯数值分支搬迁至 UMR。记录 ID 规则：`2,000,000 + node_id * 10 + op_index`；每条记录只含 1 个 op：

| 记录 ID | 节点 | 名称 | 最大点数 | 原硬编码逻辑 | UMR OpCode 配置（param_u32 = 技能 1） |
|---|---|---|---|---|---|
| `2001010` | 101 | 气聚 | 3 | `effective_mana_cost *= (1 - 0.15*N)`，下限 0 | `SKILL_MANA_COST_MULT`, `param_f32: 0.15` |
| `2001020` | 102 | 剑心洞明 | 5 | `del.bonus_crit += 0.02*N` | `SKILL_BONUS_CRIT`, `param_f32: 0.02` |
| `2001030` | 103 | 流云劲 | 4 | `more_damage_mult *= (1 + 0.10*N)` | `SKILL_MORE_DAMAGE_MULT`, `param_f32: 0.10` |
| `2001100` | 110 | 贯日（冷却） | 1 | `effective_cooldown -= 1.0*N` | `SKILL_COOLDOWN_FLAT`, `param_f32: -1.0` |
| `2001101` | 110 | 贯日（增伤） | 1 | `more_damage_mult *= (1 - 0.15*N)` | `SKILL_MORE_DAMAGE_MULT`, `param_f32: -0.15` |
| `2001110` | 111 | 连环（充能） | 2 | `effective_charges += N` | `SKILL_CHARGES_ADD`, `param_f32: 1.0` |
| `2001111` | 111 | 连环（冷却） | 2 | `effective_cooldown *= (1 + 0.15*N)` | `SKILL_COOLDOWN_MULT`, `param_f32: 0.15` |
| `2001340` | 134 | 瞬狱影爆（增伤） | 3 | `more_damage_mult *= (1 + 0.25*N)` | `SKILL_MORE_DAMAGE_MULT`, `param_f32: 0.25` |
| `2001341` | 134 | 瞬狱影爆（范围） | 3 | `area_radius *= (1 + 0.20*N)` | `SKILL_AREA_MULT`, `param_f32: 0.20` |

- 全部记录：`priority = 200`、`profession_mask = 1`、`skill_id_whitelist = [1]`、`required_skill_tags_all = 0`、`weapon_class_mask = 65535`、`equip_slot_mask = 0`、`node_id_whitelist` 为对应单节点。
- 既有 `2002103` 沿用其原 ID（历史例外），不参与本轮命名规则。

#### 保持在代码层（不搬迁）的节点
- **112 (势如破竹)**：`feature_flags |= 256`（动态按真实位移距离结算 More 增伤）；
- **113 (风行者)**：`feature_flags |= 4`（御剑步激活）；
- **114 (御风而行)**：`riding_wind_bonus_crit += 0.08f * N`（御剑步条件暴击）；
- **115 (无止境)**：`feature_flags |= 16`（击杀几率回充能）；
- **130 / 132 (留影 / 影之突袭)**：`feature_flags |= 2/32`，生成残影实体与同步施法；
- **133 (移形换位)**：`feature_flags |= 8`，标签转为 `Teleport`，施法变瞬移；
- **150 / 155 (要害感知 / 斩断因果)**：特性位判定；
- **154 (孤注一掷)**：Keystone 覆盖，仍在代码层，但应用顺序调整为全节点求值之后（见下）；
- **170 / 172 (劫火 / 凛风)**：元素转质。

#### 烘焙集成点与顺序
`SkillSpecializationBaker::Bake` 内数值应用顺序固定为：

```cpp
// (1) 基础值 + 交付默认 switch
// (2) AOT 节点循环：仅机制位/条件暴击/触发契约（112/113/114/115/130/132/133/150/155/170/172）
// (3) UMR 交付 delta 应用（101/102/103/110/111/134）
if (activeSkillSlot != nullptr) {
  const ModifierDelta specDelta = SkillSpecModifierAdapter::EvaluateSkillDeliveryDeltas(
      out_profile.skill_id, out_profile.effective_tags, *activeSkillSlot);

  out_profile.effective_mana_cost *= specDelta.GetManaCostMultiplier(out_profile.skill_id);
  out_profile.effective_cooldown = std::max(
      0.0f,
      (out_profile.effective_cooldown + specDelta.GetSkillCooldownFlat(out_profile.skill_id))
          * specDelta.GetSkillCooldownMult(out_profile.skill_id));
  out_profile.effective_charges += specDelta.GetSkillCharges(out_profile.skill_id);
  out_profile.more_damage_mult *= specDelta.GetSkillMoreDamageMult(out_profile.skill_id);
  del.bonus_crit += specDelta.GetSkillBonusCrit(out_profile.skill_id);
  del.area_radius *= specDelta.GetSkillAreaMult(out_profile.skill_id);  // area_radius 属于 BakedDeliveryParams
}
// (4) 154 覆盖（若 allocated_points 中 154 的 points > 0）：
//     effective_charges = 1; effective_cooldown = 8.0f; effective_mana_cost *= 2.0f;
//     more_damage_mult *= 2.0f; del.bonus_crit += 1.0f;
// (5) 装备 skill_modifiers 段（cd = max(0, cd + flat)、mana = max(0, mana + delta)、area *=）
// (6) 装备 UMR 法耗折叠（effective_mana_cost *= GetEquippedManaCostMultiplier(...)）
```

- **154 最终覆盖的理由**：旧实现中 154 与 110/111 的组合语义依赖 `unordered_map` 迭代顺序（未定义行为）；决议 3 明确 Keystone 的赋值式语义优先，顺序固定为「先普通节点、后 154」，装备段仍在最后。
- **乘法可交换性**：101 的法耗乘算、103/110/134 的 More 乘算与装备乘算均为乘法累积，移位不改变单节点场景结果；只有 154 的赋值语义需要顺序保证。

---

## 4. 验证策略与契约

1. **确定性生成**：
   - `python scripts/gen_skill_spec_modifier_contract.py --check`（canonical → runtime JSON 无漂移）；
   - `python scripts/gen_modifier_runtime_v2.py --check` 与 `python scripts/gen_map_monster_modifier_v2.py --check` 必须 Exit 0；
   - 字节产物 `modifier_runtime_v2.bin` 生成具有严格确定性（记录按 `(priority, id)` 稳定排序，源码已支持）。
2. **单测覆盖**：
   - 新增 `ModifierPointsScalingTests.cpp`：覆盖 0 点跳过、1 点基准、3/5 点线性累加与累乘、未填 `node_points` 的 legacy 回退（记录仍生效一次）；
   - 新增 `SkillSpecDeliveryOpTests.cpp`：覆盖 7 个交付算子的数学正确性、正负系数、多条记录叠加、取整语义；
   - 新增 `EquipmentSlotFilterTests.cpp`：断言头盔词缀在胸甲槽位不生效、在头盔槽位生效，并覆盖武器类型过滤、未誓约职业通配、徒手 + `65535` 掩码仍生效；
   - 新增 `ModifierDelta::MergeFrom` 的合并语义测试（或并入交付算子测试）。
3. **功能回归**：
   - 运行既有 `FlowingThrustNodes.cpp`、`SkillSpecializationBakerTests.cpp`、`SkillManaCostSettlementTests.cpp`，断言行为等价；
   - 补充组合用例：110+111、110+154、111+154、101+103、102+134，锁定 3.4 的确定性顺序与 154 覆盖语义。
4. **全量构建**：
   - `build.bat RelWithDebInfo` 退出码 0，CTest `unit|skill` 全量 100% 通过；
   - Python 侧：`python -m unittest tests.python.SkillSpecCanonicalGenerationTest tests.python.SkillSpecModifierMigrationTest tests.python.ValidateJsonModifierCanonicalArtifactsTest -v` 全绿。
