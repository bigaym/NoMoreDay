# UMR 技能专精承载底座扩充与技能 1 数值切片实施计划

- **文档状态**：已按评审意见修订（2026-09-16）/ 待复审
- **设计依据**：`docs/designs/2026-09-16-umr-skill-spec-foundation-and-skill1-slice-design.md`
- **计划日期**：2026-09-16
- **计划范围**：
  - Phase 1：点数分配模型与上下文扩展（`ModifierEvalContext` + `ModifierEvaluator`）
  - Phase 2：技能交付参数算子体系（`ModifierOpCode` 30..36 + `ModifierDelta`）
  - Phase 3：装备槽位/职业/武器过滤闭环（`EquipmentModifierAdapter`）
  - Phase 4：生成链注册与 canonical 拆记录策略（`gen_modifier_runtime_v2.py` + `migrate_skill_spec_modifier_slice.py`）
  - Phase 5：技能 1（流云刺）数值节点配置与适配器对接（canonical JSON + `skill_spec_modifiers.json` + `SkillSpecModifierAdapter`）
  - Phase 6：`SkillSpecializationBaker` 接入与原硬编码数值分支退役
  - Phase 7：回归测试与全量构建验证

---

## 1. 实施思路与原理

### 1.1 数据流与系统协作
1. **点数数据流**：
   - 玩家加点保存在 `SpecializedSkill.allocated_points`；
   - 烘焙期由 `SkillSpecModifierAdapter` 提取为有序的 `vector<NodePointEntry>` 注入 `ModifierEvalContext`（同时填充 `active_node_ids`）；
   - `ModifierEvaluator` 在匹配到 `skill_spec` 记录时检索该节点分配点数 $N$ 并按线性公式缩放计算；未提供点数上下文的既有调用点回退为单次生效（细节见设计 §3.1.2）。
2. **交付参数数据流**：
   - 数值字面量只允许出现在 canonical 层：`assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json` → `scripts/gen_skill_spec_modifier_contract.py` → `assets/data/modifier_v2/skill_spec_modifiers.json` → `scripts/gen_modifier_runtime_v2.py` → `assets/generated/modifier_runtime_v2.bin`；
   - `SkillSpecModifierAdapter::EvaluateSkillDeliveryDeltas` 执行求值，返回携带各项倍率和累加值的 `ModifierDelta`；
   - `SkillSpecializationBaker::Bake` 按固定顺序（普通节点 delta → 154 覆盖 → 装备段）将 delta 应用至 `BakedSkillProfile` 与 `BakedDeliveryParams`；
   - 行为层 `FlowingThrust::DoCast` 照常读取 `BakedSkillProfile`，保证行为等价。

### 1.2 架构边界与裁决原则
- **单源原则（Pillar 6）**：数值不再由 C++ 硬编码字面量维护，而是归入 canonical 数据管线；多 op 节点拆为多条单 op 记录（canonical 契约不变）。
- **兼容回退**：`ctx.node_points` 为空 `effective_points = 1`，保证只填 `active_node_ids` 的既有调用点（`EvaluateDamageMultiplier`、`SkillDisplayPreviewService`）行为不变。
- **确定性顺序**：`effective_cooldown = max(0, (cd + flat) * mult)`；154 在普通节点之后做最终覆盖（旧实现依赖 `unordered_map` 迭代顺序，属未定义行为）。
- **非侵入性**：机制形态分支（流云刺变瞬移、残影同步、元素转质、Keystone 架构特例）依然保持在 C++ 与 `SpecStateGen` 中，不生硬制造复杂 DSL。

---

## 2. 关键逻辑伪代码引导

### 2.1 上下文点数提取与求值缩放
```cpp
// ModifierContext.hpp
struct NodePointEntry {
  uint32_t node_id = 0;
  uint16_t points = 0;
};

// ModifierEvaluator.cpp
uint16_t ResolveEffectivePoints(std::span<const uint32_t> nodeWhitelist,
                                const ModifierEvalContext& ctx) {
  // 非专精域（白名单为空）：单次生效
  if (nodeWhitelist.empty()) return 1;
  // 兼容回退：只填 active_node_ids 的既有调用点
  if (ctx.node_points.empty()) return 1;
  // 取第一个已加点节点；全部为 0 点则跳过该记录
  for (uint32_t nodeId : nodeWhitelist) {
    const uint16_t pts = ctx.GetPointsForNode(nodeId);
    if (pts > 0) return pts;
  }
  return 0;
}

// 算子缩放处理（param_u32 = 技能 ID，每点幅度在 param_f32）
void ApplySkillDeliveryOp(const ModifierRuntimeOp& op, uint16_t points, ModifierDelta& out) {
  const float pts = static_cast<float>(points);
  switch (static_cast<ModifierOpCode>(op.opcode)) {
    case ModifierOpCode::SKILL_MORE_DAMAGE_MULT:
      out.AddSkillMoreDamageMult(op.param_u32, 1.0f + op.param_f32 * pts); break;
    case ModifierOpCode::SKILL_COOLDOWN_FLAT:
      out.AddSkillCooldownFlat(op.param_u32, op.param_f32 * pts); break;
    case ModifierOpCode::SKILL_COOLDOWN_MULT:
      out.AddSkillCooldownMult(op.param_u32, 1.0f + op.param_f32 * pts); break;
    case ModifierOpCode::SKILL_CHARGES_ADD:
      out.AddSkillCharges(op.param_u32, static_cast<int>(op.param_f32 * pts)); break;
    case ModifierOpCode::SKILL_BONUS_CRIT:
      out.AddSkillBonusCrit(op.param_u32, op.param_f32 * pts); break;
    case ModifierOpCode::SKILL_AREA_MULT:
      out.AddSkillAreaMult(op.param_u32, 1.0f + op.param_f32 * pts); break;
    case ModifierOpCode::SKILL_MANA_COST_MULT:
      out.AddManaCostMultiplier(op.param_u32, std::max(0.0f, 1.0f - op.param_f32 * pts)); break;
    default: break;
  }
}
```
> `ModifierEvaluator.cpp` 的运行时记录路径与静态 `span<const ModifierRecord>` 路径共用同一个算子应用辅助函数，避免两处 switch 漂移。

### 2.2 烘焙接入
```cpp
// SkillSpecializationBaker.cpp::Bake() 数值应用顺序：
// (1) 基础值 + 交付默认 switch
// (2) AOT 节点循环：仅机制位/条件暴击/触发契约
// (3) UMR 交付 delta 应用
if (spec != nullptr) {
  const ModifierDelta deliveryDeltas = SkillSpecModifierAdapter::EvaluateSkillDeliveryDeltas(
      out_profile.skill_id, out_profile.effective_tags, *spec);

  out_profile.effective_mana_cost *= deliveryDeltas.GetManaCostMultiplier(out_profile.skill_id);
  out_profile.effective_cooldown = std::max(
      0.0f,
      (out_profile.effective_cooldown + deliveryDeltas.GetSkillCooldownFlat(out_profile.skill_id))
          * deliveryDeltas.GetSkillCooldownMult(out_profile.skill_id));
  out_profile.effective_charges += deliveryDeltas.GetSkillCharges(out_profile.skill_id);
  out_profile.more_damage_mult *= deliveryDeltas.GetSkillMoreDamageMult(out_profile.skill_id);
  del.bonus_crit += deliveryDeltas.GetSkillBonusCrit(out_profile.skill_id);
  del.area_radius *= deliveryDeltas.GetSkillAreaMult(out_profile.skill_id);
}
// (4) 154 覆盖：若 allocated_points[154] > 0 → charges=1、cd=8.0f、mana*=2.0f、more*=2.0f、crit+=1.0f
// (5) 装备 skill_modifiers 段（既有逻辑）
// (6) 装备 UMR 法耗折叠（既有逻辑）
```

---

## 3. 详细任务清单与文件变更拆分 (Detailed Task List)

### Phase 1：点数分配模型扩展（P0）
- [ ] **Task 1.1**：新增 `NodePointEntry` 与 `ModifierEvalContext::node_points`。
  - 文件：`[MODIFY]` `src/game/systems/modifier/ModifierContext.hpp`
  - 内容：
    ```cpp
    struct NodePointEntry {
      uint32_t node_id = 0;
      uint16_t points = 0;
    };
    ```
    为 `ModifierEvalContext` 增加 `std::vector<NodePointEntry> node_points`（按 `node_id` 升序约定）与内联辅助 `[[nodiscard]] uint16_t GetPointsForNode(uint32_t nodeId) const`（二分查找，未分配返回 0）。
- [ ] **Task 1.2**：实现记录级点数解析并贯通两条求值路径。
  - 文件：`[MODIFY]` `src/game/systems/modifier/ModifierEvaluator.cpp`
  - 内容：按设计 §3.1.2 实现 `ResolveEffectivePoints`（白名单空 → 1；`ctx.node_points` 空 → 1 legacy 回退；0 点 → 跳过记录），并把 `effective_points` 传入算子应用辅助函数；同步更新运行时记录路径与静态记录路径。不做上限截断。
- [ ] **Task 1.3**：编写点数缩放单元测试。
  - 文件：`[NEW]` `tests/unit/ModifierPointsScalingTests.cpp`
  - 内容：
    - 0 点：记录被跳过，delta 不产生条目；
    - 1 点：基准值；
    - 3 点 / 5 点：线性累乘（More/Area/CooldownMult）与累加（Flat/Crit/Mana）正确；
    - legacy 回退：`node_points` 为空但 `active_node_ids` 命中时记录仍生效一次；
    - 白名单恰好 1 节点的防御行为（多节点白名单取第一个已加点节点）。

### Phase 2：技能交付参数算子体系扩充（P0）
- [ ] **Task 2.1**：扩充 `ModifierOpCode` 枚举（30..36）。
  - 文件：`[MODIFY]` `src/game/systems/modifier/ModifierContext.hpp`
  - 内容：`SKILL_MORE_DAMAGE_MULT = 30`、`SKILL_COOLDOWN_FLAT = 31`、`SKILL_COOLDOWN_MULT = 32`、`SKILL_CHARGES_ADD = 33`、`SKILL_BONUS_CRIT = 34`、`SKILL_AREA_MULT = 35`、`SKILL_MANA_COST_MULT = 36`（新增，不复用 `MANA_COST_MULT`）。
- [ ] **Task 2.2**：扩充类别掩码 `ModifierOpCategory`。
  - 文件：`[MODIFY]` `src/game/systems/modifier/ModifierContext.hpp`
  - 内容：新增 `SkillDelivery = 1u << 3`，`All` 更新为 `(1u << 0) | (1u << 1) | (1u << 2) | (1u << 3)`。
- [ ] **Task 2.3**：在 `ModifierDelta` 中新增技能交付参数累积容器与合并能力。
  - 文件：`[MODIFY]` `src/game/systems/modifier/ModifierEvaluator.hpp` / `ModifierEvaluator.cpp`
  - 内容：
    - `std::unordered_map<uint32_t, float> skill_more_damage_mult;`（缺省 1.0f，累乘）
    - `std::unordered_map<uint32_t, float> skill_cooldown_flat;`（缺省 0.0f，累加）
    - `std::unordered_map<uint32_t, float> skill_cooldown_mult;`（缺省 1.0f，累乘）
    - `std::unordered_map<uint32_t, int> skill_charges_add;`（缺省 0，累加）
    - `std::unordered_map<uint32_t, float> skill_bonus_crit;`（缺省 0.0f，累加）
    - `std::unordered_map<uint32_t, float> skill_area_mult;`（缺省 1.0f，累乘）
    - 法耗复用现有 `mana_cost_mult` 容器与 `GetManaCostMultiplier`；
    - 对应 Getter 与 Add 方法（`AddSkillMoreDamageMult` / `GetSkillMoreDamageMult` 等，Get 缺省值按容器语义返回 1.0f / 0.0f / 0）；
    - 新增 `MergeFrom(const ModifierDelta&)`：覆盖全部容器（含怪物集合），供装备按槽分组求值合并使用。
- [ ] **Task 2.4**：支持 `SkillDelivery` 类别算子的分发与累积。
  - 文件：`[MODIFY]` `src/game/systems/modifier/ModifierEvaluator.cpp`
  - 内容：`CategoryOfOp` 将 30..36 映射为 `SkillDelivery`；`ApplyRuntimeOp` 与静态记录路径新增/复用统一算子应用辅助函数处理 30..36。
- [ ] **Task 2.5**：编写交付参数算子单元测试。
  - 文件：`[NEW]` `tests/unit/SkillSpecDeliveryOpTests.cpp`
  - 内容：断言 7 个算子的数学正确性、正负系数（110 的 `-0.15`、`-1.0`）、多记录叠加、`SKILL_CHARGES_ADD` 取整语义、`SKILL_MANA_COST_MULT` 下限 0、`MergeFrom` 合并语义。

### Phase 3：装备槽位与武器过滤闭环（P0，闭合残留风险 1）
- [ ] **Task 3.1**：重构记录收集逻辑携带来源槽位。
  - 文件：`[MODIFY]` `src/game/systems/modifier/EquipmentModifierAdapter.cpp` / `.hpp`
  - 内容：新增 `struct EquippedRecordRef { uint32_t record_id; uint32_t slot_bit; }`；`slot_bit = 1u << static_cast<uint32_t>(slot)`；按 `(slot_bit, record_id)` 去重并排序后返回。公开 API `CollectEquippedRecordIds` 若变更签名，同步更新 `tests/unit/EquipmentModifierAdapterTests.cpp:152,178`。
- [ ] **Task 3.2**：按槽分组求值并合并 delta。
  - 文件：`[MODIFY]` `src/game/systems/modifier/EquipmentModifierAdapter.cpp`
  - 内容：对每个槽位分组复制上下文并置 `ctx.equip_slot_mask = slot_bit`，调用一次求值，用 `ModifierDelta::MergeFrom` 合并进总结果；不得逐记录切换掩码（现 API 只接受单一 ctx）。改造两个消费方：`ApplyEquippedSkillLevelBonuses`、`GetEquippedManaCostMultiplier`。
- [ ] **Task 3.3**：在 `BuildContextFromCharacter` 中补充职业与武器掩码。
  - 文件：`[MODIFY]` `src/game/systems/modifier/EquipmentModifierAdapter.cpp`
  - 内容：
    - 职业：读 `AstrolabeComponent::mainProfession`，`>= 0` 时填入 `ctx.profession_id`，未誓约维持 0（保持现状）；
    - 武器：主/副手已装备武器的 `1u << static_cast<uint32_t>(WeaponSubtype)` 按位或填入 `ctx.weapon_class_mask`，徒手为 0；
    - `MatchesFilters` 两处重复实现同步支持通配：`filter.weapon_class_mask == 0 || == 0xFFFFFFFFu` 视为不过滤，保证既有 `65535` 掩码记录对徒手角色继续生效。
- [ ] **Task 3.4**：编写装备槽位过滤单元测试。
  - 文件：`[NEW]` `tests/unit/EquipmentSlotFilterTests.cpp`
  - 内容：头盔词缀安装在胸甲槽位不生效、安装在头盔槽位生效；武器类型限定过滤（剑生效/斧不生效）；未誓约职业通配；徒手 + `65535` 掩码仍生效。

### Phase 4：离线构建链与 canonical 拆记录策略（P0）
- [ ] **Task 4.1**：更新二进制生成脚本 opcode 映射。
  - 文件：`[MODIFY]` `scripts/gen_modifier_runtime_v2.py`
  - 内容：在 `OPCODE_VALUES` 注册 7 个新算子（30..36）。参数浮点/负数校验与 `(priority, id)` 稳定排序已具备，无需额外改动。
- [ ] **Task 4.2**：更新 canonical 迁移脚本 opcode 映射。
  - 文件：`[MODIFY]` `scripts/migrate_skill_spec_modifier_slice.py`
  - 内容：在 `SUPPORTED_OPCODE_TO_OPERATION` 注册 7 个新算子映射：`SKILL_COOLDOWN_FLAT` / `SKILL_CHARGES_ADD` / `SKILL_BONUS_CRIT` → `add`，`SKILL_MORE_DAMAGE_MULT` / `SKILL_COOLDOWN_MULT` / `SKILL_AREA_MULT` / `SKILL_MANA_COST_MULT` → `mul`。
- [ ] **Task 4.3**：确认 canonical 契约不变并说明拆记录策略。
  - 说明：`assets/data/modifier_v2/canonical/skill_spec_modifier_record.schema.json` 无 opcode 字段，**无需修改**；`assets/data/modifier_v2/modifier_catalog.json` 已注册 `skill_spec` 域，无需改动；canonical 记录保持「一记录一 op」，110/111/134 拆为 2 条记录（见 Phase 5.1）；既有迁移 fixtures（`tests/fixtures/schema/modifier/skill_spec_runtime.supported.json` / `unsupported.json`）不受新 opcode 注册影响，无需改动。
- [ ] **Task 4.4**：重生成 runtime JSON 并跑通三方门禁。
  - 命令：`python scripts/gen_skill_spec_modifier_contract.py`（重生成）、`python scripts/gen_skill_spec_modifier_contract.py --check`、`python -m unittest tests.python.SkillSpecCanonicalGenerationTest tests.python.SkillSpecModifierMigrationTest tests.python.ValidateJsonModifierCanonicalArtifactsTest -v`。

### Phase 5：技能 1（流云刺）数值专精切片配置与适配器实现（P1）
- [ ] **Task 5.1**：在 canonical JSON 中新增 9 条单 op 记录（含 `record` 与 `runtime` 两个块）。
  - 文件：`[MODIFY]` `assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json`
  - ID 规则：`2,000,000 + node_id * 10 + op_index`；公共字段：`priority: 200`、`profession_mask: 1`、`skill_id_whitelist: [1]`、`required_skill_tags_all: 0`、`forbidden_skill_tags_any: 0`、`weapon_class_mask: 65535`、`equip_slot_mask: 0`、`exclusive_group: 0`、`max_active: 0`、`debug_source: "skill_spec_node"`、`min_player_level: 1`、`param_u32: 1`（技能 ID）。

    | 记录 ID | 节点 | opcode | param_f32 | target（canonical stat_path） | debug_name |
    |---|---|---|---|---|---|
    | `2001010` | 101 气聚 | `SKILL_MANA_COST_MULT` | `0.15` | `skill.mana_cost` | `FlowingThrust_Node101_ManaCost` |
    | `2001020` | 102 剑心洞明 | `SKILL_BONUS_CRIT` | `0.02` | `skill.bonus_crit` | `FlowingThrust_Node102_BonusCrit` |
    | `2001030` | 103 流云劲 | `SKILL_MORE_DAMAGE_MULT` | `0.10` | `skill.more_damage` | `FlowingThrust_Node103_MoreDamage` |
    | `2001100` | 110 贯日 | `SKILL_COOLDOWN_FLAT` | `-1.0` | `skill.cooldown_flat` | `FlowingThrust_Node110_CooldownFlat` |
    | `2001101` | 110 贯日 | `SKILL_MORE_DAMAGE_MULT` | `-0.15` | `skill.more_damage` | `FlowingThrust_Node110_MoreDamage` |
    | `2001110` | 111 连环 | `SKILL_CHARGES_ADD` | `1.0` | `skill.charges_add` | `FlowingThrust_Node111_Charges` |
    | `2001111` | 111 连环 | `SKILL_COOLDOWN_MULT` | `0.15` | `skill.cooldown_mult` | `FlowingThrust_Node111_CooldownMult` |
    | `2001340` | 134 瞬狱影爆 | `SKILL_MORE_DAMAGE_MULT` | `0.25` | `skill.more_damage` | `FlowingThrust_Node134_MoreDamage` |
    | `2001341` | 134 瞬狱影爆 | `SKILL_AREA_MULT` | `0.20` | `skill.area_mult` | `FlowingThrust_Node134_AreaMult` |

  - 每条记录 `node_id_whitelist` 为对应单节点；`record.stacks` = `param_u32` = 1（迁移契约要求 `[1, 99]`），`record.value` = `param_f32`（允许负值），`record.operation` 按 Task 4.2 映射，`record.tags` 由 stat_path 首段推导为 `skill`。
- [ ] **Task 5.2**：实现 `SkillSpecModifierAdapter` 技能交付求值接口。
  - 文件：`[MODIFY]` `src/game/systems/modifier/SkillSpecModifierAdapter.hpp` / `SkillSpecModifierAdapter.cpp`
  - 内容：
    ```cpp
    [[nodiscard]] static ModifierDelta EvaluateSkillDeliveryDeltas(
        uint32_t skillId, Tag skillTags, const SpecializedSkill &activeSkillSlot);
    ```
    - 沿用单例 `ModifierRuntimeRegistry::Get()`（与既有方法风格一致，不新增 registry 参数）；
    - 新增点数提取辅助（如 `CollectNodeAssignments`，同时输出排序后的 `node_points` 与 `active_node_ids`）；
    - 记录收集规则：`node_id_whitelist` 与已加点节点有交集 **且** op 列表包含任一 `SkillDelivery` opcode（30..36）；独立于只认 `ADD_STAT_PERCENT_MULT` + `PhysicalDamage` 的 `ResolveDamageRecordIds`；
    - 调用 `ModifierEvaluator::Evaluate(registry, recordIds, ctx)`（ctx 含 `skill_id`、`skill_tags`、`node_points`、`active_node_ids`）；
    - 既有 `EvaluateDamageMultiplier` 调用点（`SkillSystem.cpp:2140` 附近、`SkillDisplayPreviewService.cpp:138` 附近）依赖 Task 1.2 的 legacy 回退，行为不变，本轮不改签名。
- [ ] **Task 5.3**：生成运行时 JSON 与二进制并验证。
  - 命令：`python scripts/gen_skill_spec_modifier_contract.py` → 更新 `assets/data/modifier_v2/skill_spec_modifiers.json`；`python scripts/gen_modifier_runtime_v2.py --build` → 更新 `assets/generated/modifier_runtime_v2.bin`；随后 `--check` 双查。

### Phase 6：`SkillSpecializationBaker` 接入与硬编码退役（P1）
- [ ] **Task 6.1**：在 `SkillSpecializationBaker::Bake` 中挂接专精数值求值结果。
  - 文件：`[MODIFY]` `src/game/systems/skill/SkillSpecializationBaker.cpp`
  - 内容：在 AOT 节点循环**之后**、装备段之前，调用 `EvaluateSkillDeliveryDeltas` 并按设计 §3.4 的固定顺序与公式应用（法耗/冷却/充能/More/Crit/Area）；`spec == nullptr` 时跳过。
- [ ] **Task 6.2**：清理 `SkillSpecializationBaker.cpp:234-310` 中 `case 1:` 的对应数值分支。
  - 文件：`[MODIFY]` `src/game/systems/skill/SkillSpecializationBaker.cpp`
  - 内容：删除节点 101、102、103、110、111、134 的直接赋值代码，仅保留机制特性位与未搬迁节点（112/113/114/115/130/132/133/150/155/170/172）。
- [ ] **Task 6.3**：重排 154 孤注一掷为最终覆盖。
  - 文件：`[MODIFY]` `src/game/systems/skill/SkillSpecializationBaker.cpp`
  - 内容：将 154 的赋值式效果（`charges=1`、`cd=8.0f`、`mana*=2.0f`、`more*=2.0f`、`crit+=1.0f`）移动到普通节点 delta 应用之后、装备段之前；补充组合用例：110+111、110+154、111+154、101+103、102+134（`SkillSpecializationBakerTests.cpp` 或 `FlowingThrustNodes.cpp`）。

### Phase 7：全量回归与验证闭环
- [ ] **Task 7.1**：全量编译构建验证。
  - 命令：`cmd /v:on /c "build.bat RelWithDebInfo & echo RC=!errorlevel!"`，确保构建 Exit Code 0。
- [ ] **Task 7.2**：定向用例回归。
  - 命令：`bin/NoMoreDayTests.exe --test-case=*ModifierPoints*,*SkillSpecDelivery*,*EquipmentSlotFilter*,*Flowing*,*SkillManaCost*,*SkillSpecializationBaker*,*EquipmentModifier*`
  - 注意：测试二进制位于 `bin/`（CMake `RUNTIME_OUTPUT_DIRECTORY` 指向 `bin`，非 `bin/RelWithDebInfo`）；doctest 2.5.3 不识别位置参数作为过滤器，必须使用 `--test-case=` 形式。
- [ ] **Task 7.3**：全量 CTest 套件执行。
  - 命令：`ctest --test-dir build -C RelWithDebInfo -L "unit|skill" --output-on-failure`，确保 100% 通过。
- [ ] **Task 7.4**：生成器漂移门禁三检。
  - 命令：`python scripts/gen_skill_spec_modifier_contract.py --check`、`python scripts/gen_modifier_runtime_v2.py --check`、`python scripts/gen_map_monster_modifier_v2.py --check`。
- [ ] **Task 7.5**：Python 契约测试。
  - 命令：`python -m unittest tests.python.SkillSpecCanonicalGenerationTest tests.python.SkillSpecModifierMigrationTest tests.python.ValidateJsonModifierCanonicalArtifactsTest -v`。

---

## 4. 测试方法与验证命令

### 4.1 自动化测试层级
1. **单元测试（Unit）**：
   - `ModifierPointsScalingTests`：点数解析、缩放、legacy 回退、0 点跳过；
   - `SkillSpecDeliveryOpTests`：7 个交付算子数学运算与 `MergeFrom`；
   - `EquipmentSlotFilterTests`：槽位/武器/职业隔离；
2. **功能测试（Functional）**：
   - `FlowingThrustNodes`：流云刺全部专精节点在各种加点配置下的数值行为；
3. **集成测试（Integration）**：
   - `SkillManaCostSettlementTests`：法力消耗折叠仍生效；
   - `SkillSpecializationBakerTests`：烘焙档案输出与既有预期一致（含 110+111、110+154、111+154 组合）；
4. **契约测试（Contract）**：
   - Python 三套 unittest + 三个生成器 `--check`。

### 4.2 验证命令
```powershell
# 1. 构建二进制
cmd /v:on /c "build.bat RelWithDebInfo & echo RC=!errorlevel!"

# 2. 运行定向测试（doctest 过滤器必须用 --test-case=）
bin/NoMoreDayTests.exe --test-case=*ModifierPoints*,*SkillSpecDelivery*,*EquipmentSlotFilter*,*Flowing*,*SkillManaCost*,*SkillSpecializationBaker*,*EquipmentModifier*

# 3. 运行全量单元与技能测试套件
ctest --test-dir build -C RelWithDebInfo -L "unit|skill" --output-on-failure

# 4. 生成器与 canonical 契约检查
python scripts/gen_skill_spec_modifier_contract.py --check
python scripts/gen_modifier_runtime_v2.py --check
python scripts/gen_map_monster_modifier_v2.py --check

# 5. Python 契约测试
python -m unittest tests.python.SkillSpecCanonicalGenerationTest tests.python.SkillSpecModifierMigrationTest tests.python.ValidateJsonModifierCanonicalArtifactsTest -v
```

---

## 5. 验收标准与完成定义（DoD）

1. **DoD 1（点数能力就绪）**：`ModifierEvaluator` 能依据 `ModifierEvalContext::node_points` 为多点节点输出按比例缩放的数值；0 点记录被跳过；未填点数上下文时行为与现状一致（legacy 回退有测试锁定）；
2. **DoD 2（交付算子闭环）**：7 种技能交付 OpCode（30..36）能由二进制加载并求值进入 `ModifierDelta`，含正负系数与取整语义；
3. **DoD 3（装备过滤生效）**：`EquipmentModifierAdapter` 能区分装备所在槽位并过滤非匹配词缀，同时填充职业与武器掩码；徒手 + `65535` 掩码保持通配；
4. **DoD 4（技能 1 专精数值数据化）**：流云刺节点 101、102、103、110、111、134 的数值完全由 canonical → runtime JSON 链路驱动，`SkillSpecializationBaker.cpp` 中对应硬编码计算已完全移除；
5. **DoD 5（确定性与行为等价）**：154 覆盖语义与冷却合成顺序确定；流云刺所有既有单测与功能测试 100% 通过，组合用例（110+111、110+154、111+154）有断言锁定；
6. **DoD 6（契约与门禁全绿）**：`gen_skill_spec_modifier_contract.py --check`、`gen_modifier_runtime_v2.py --check`、`gen_map_monster_modifier_v2.py --check` 与三套 Python 契约测试全部通过；
7. **DoD 7（构建与全量回归）**：`build.bat RelWithDebInfo` 退出码 0，CTest `unit|skill` 100% 通过。
