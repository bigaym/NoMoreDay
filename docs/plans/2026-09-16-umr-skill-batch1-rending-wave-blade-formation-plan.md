# UMR 技能专精改造第一批（技能 2 裂空斩与技能 3 灵剑决）实施计划

- **文档状态**：已实施（v2.1：按首轮实施审查反馈修订）
- **设计依据**：[`docs/designs/2026-09-16-umr-skill-batch1-rending-wave-blade-formation-design.md`](file:///d:/PRJ/NoMoreDay/docs/designs/2026-09-16-umr-skill-batch1-rending-wave-blade-formation-design.md)
- **计划日期**：2026-09-16
- **计划范围**：
  - Phase 1：UMR 算子体系扩充（`ModifierOpCode` 37..40 + `ModifierDelta`）
  - Phase 2：离线生成与迁移脚本注册（`gen_modifier_runtime_v2.py` + `migrate_skill_spec_modifier_slice.py`）
  - Phase 3：技能 2 与技能 3 Canonical 记录配置、二进制生成与离线门禁（11 条记录）
  - Phase 4：`SkillSpecializationBaker` 交付参数接入、基准确值化与单一事实源退役
  - Phase 5：回归测试与全量构建验证
- **修订记录**：
  - v1：初稿。
  - v2：修正伪代码 `default` 位置；新增 Phase 3.3 离线门禁任务、Phase 4.5 基准确值化、Phase 4.6 mechanics 键退役、Phase 5.4 Python 门禁；210 惩罚改读数据源。
  - v2.1：按实施审查反馈补登 3/300 `swords_per_point` 退役、补齐 11 条记录迁移等价锚点、补「法耗下限 0」测试。

---

## 1. 实施思路与原理

### 1.1 数据流与系统边界
1. **纯数值外置入数据管线**：
   - 技能 2 与技能 3 的 10 个纯数值节点（200 范围/射程、201 平减法耗、202 More伤害、210 投射物数量、300 灵剑数量、302 More伤害、310 索敌射程、312 法耗折扣、331 额外暴击、332 额外暴伤）全部录入 [`skill_spec_modifiers.canonical.json`](file:///d:/PRJ/NoMoreDay/assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json)；
   - 离线编译器编译为紧凑二进制四表，并在 `build.bat` precheck 门禁保护下杜绝漂移。
2. **烘焙层一次性求值并接入**：
   - [`SkillSpecializationBaker::Bake`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSpecializationBaker.cpp) 在步骤 3 调用 `EvaluateSkillDeliveryDeltas`，一次性获取包含 30..40 全部算子的增量包裹；
   - 移除 `case 2:` 与 `case 3:` 内部的冗余赋值，保留机制标志位；
   - 在步骤 4 确定性执行 210（非线性伤害惩罚）、311（灵剑上限翻倍）与 330（巨剑数量置 1、范围 80）的后置覆盖，消除遍历顺序造成的未定义行为。
3. **单源原则与非侵入性**：
   - 行为层 [`RendingWave::DoCast`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/RendingWave.cpp) 与 [`BladeFormation::DoCast`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/BladeFormation.cpp) 无需改变消费逻辑，照常读取 `BakedSkillProfile`，保证 100% 行为等价；
   - **迁移即退役**：canonical 记录一旦接手某线性数值，`skill_mechanics.json` 的同名键必须同步删除（设计 §4.3），并以门禁阻止回潮——否则"改数据无效果"的误导性死数据会破坏 Pillar 6；
   - **基准确值化**：乘算类算子（`SKILL_RANGE_MULT`）所依附的基准必须在步骤 1 显式写入，不得依赖结构体默认值（设计 §3.3）。

---

## 2. 关键逻辑伪代码引导

### 2.1 算子分发与点数缩放 (ModifierEvaluator.cpp)
```cpp
// 在共享算子辅助函数 ApplyOp 中扩展 37..40
void ApplyOp(ModifierOpCode opcode, uint32_t paramU32, float paramF32,
             uint16_t points, float effectivePercentMult, ModifierDelta &out) {
  const float pts = static_cast<float>(points);
  switch (opcode) {
    // 30..36 既有算子...
    case ModifierOpCode::SKILL_PROJECTILES_ADD:
      out.AddSkillProjectiles(paramU32, static_cast<int>(paramF32 * pts));
      break;
    case ModifierOpCode::SKILL_MANA_COST_FLAT:
      out.AddSkillManaCostFlat(paramU32, paramF32 * pts);
      break;
    case ModifierOpCode::SKILL_BONUS_CRIT_DAMAGE:
      out.AddSkillBonusCritDamage(paramU32, paramF32 * pts);
      break;
    case ModifierOpCode::SKILL_RANGE_MULT:
      out.AddSkillRangeMult(paramU32, 1.0f + paramF32 * pts);
      break;
    // 注意：不在交付 case 内新增 default（ApplyOp 已存在统一 default 分支），
    // 否则与既有 default 重复，按本伪代码照抄会导致编译失败。
  }
}
```

### 2.2 烘焙层合成与后处理覆盖 (SkillSpecializationBaker.cpp)
```cpp
// 步骤 3: UMR 交付 delta 合成（确定性公式）
out_profile.effective_mana_cost = std::max(
    0.0f, (out_profile.effective_mana_cost + specDelta.GetSkillManaCostFlat(skill_id))
              * specDelta.GetManaCostMultiplier(skill_id));
out_profile.effective_cooldown = std::max(
    0.0f, (out_profile.effective_cooldown + specDelta.GetSkillCooldownFlat(skill_id))
              * specDelta.GetSkillCooldownMult(skill_id));
out_profile.effective_charges += specDelta.GetSkillCharges(skill_id);
out_profile.projectile_count += specDelta.GetSkillProjectiles(skill_id);
out_profile.more_damage_mult *= specDelta.GetSkillMoreDamageMult(skill_id);
out_profile.area_radius *= specDelta.GetSkillAreaMult(skill_id);
del.bonus_crit += specDelta.GetSkillBonusCrit(skill_id);
del.bonus_crit_damage += specDelta.GetSkillBonusCritDamage(skill_id);
del.range *= specDelta.GetSkillRangeMult(skill_id);

// 步骤 4: 后置 Keystone 与非线性覆盖
if (skill_id == 2) {
  const auto it210 = spec->allocated_points.find(210);
  if (it210 != spec->allocated_points.end() && it210->second > 0) {
    const int pts = it210->second;
    // 非线性惩罚：数值从 skill_mechanics.json 读取（pt1=0.25/pt2=0.20/pt3=0.15），不再硬编码
    const std::string key = "damage_penalty_pt" + std::to_string(std::clamp(pts, 1, 3));
    const float penalty_pct =
        data::SkillMechanicsRegistry::Get().GetFloat(2, 210, key, 0.0f);
    out_profile.more_damage_mult *= (1.0f - penalty_pct);
  }
} else if (skill_id == 3) {
  if ((out_profile.delivery.feature_flags & 2) != 0) { // 330 巨剑降临
    out_profile.projectile_count = 1;
    out_profile.area_radius = 80.0f;
  } else if ((out_profile.delivery.feature_flags & 1) != 0) { // 311 无尽剑匣
    out_profile.projectile_count *= 2;
  }
}
```

---

## 3. 详细任务清单 (Detailed Task List)

### Phase 1：UMR 算子体系扩充与求值累积（P0）
- [ ] **Task 1.1**：扩充 `ModifierOpCode` 枚举（37..40）。
  - 文件：`[MODIFY]` [`src/game/systems/modifier/ModifierContext.hpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/modifier/ModifierContext.hpp)
  - 内容：新增 `SKILL_PROJECTILES_ADD = 37`、`SKILL_MANA_COST_FLAT = 38`、`SKILL_BONUS_CRIT_DAMAGE = 39`、`SKILL_RANGE_MULT = 40`。
- [ ] **Task 1.2**：在 `ModifierDelta` 中新增 4 个容器、存取器与 `MergeFrom` 支持。
  - 文件：`[MODIFY]` [`src/game/systems/modifier/ModifierEvaluator.hpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/modifier/ModifierEvaluator.hpp) / [`ModifierEvaluator.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/modifier/ModifierEvaluator.cpp)
  - 内容：
    - `skill_projectiles_add`（缺省 0，累加）
    - `skill_mana_cost_flat`（缺省 0.0f，累加）
    - `skill_bonus_crit_damage`（缺省 0.0f，累加）
    - `skill_range_mult`（缺省 1.0f，累乘）
    - 对应的 Getter/Add 方法与 `MergeFrom` 累积逻辑。
- [ ] **Task 1.3**：更新 `CategoryOfOp` 与共享分发函数 `ApplyOp`。
  - 文件：`[MODIFY]` [`src/game/systems/modifier/ModifierEvaluator.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/modifier/ModifierEvaluator.cpp)
  - 内容：将 37..40 映射到 `ModifierOpCategory::SkillDelivery`；在 `ApplyOp` 中接入 4 个新算子的点数缩放求值。
- [ ] **Task 1.4**：编写新算子单元测试。
  - 文件：`[NEW]` `tests/unit/SkillBatch1DeliveryOpTests.cpp`
  - 内容：单点与多点缩放计算、负值法耗下限、暴伤累加、射程累乘、`MergeFrom` 合并正确性。

### Phase 2：离线生成管线与迁移注册（P0）
- [ ] **Task 2.1**：在运行时生成器注册 37..40。
  - 文件：`[MODIFY]` [`scripts/gen_modifier_runtime_v2.py`](file:///d:/PRJ/NoMoreDay/scripts/gen_modifier_runtime_v2.py)
  - 内容：在 `OPCODE_VALUES` 添加 `SKILL_PROJECTILES_ADD: 37`, `SKILL_MANA_COST_FLAT: 38`, `SKILL_BONUS_CRIT_DAMAGE: 39`, `SKILL_RANGE_MULT: 40`。
- [ ] **Task 2.2**：在 canonical 迁移映射中注册 37..40。
  - 文件：`[MODIFY]` [`scripts/migrate_skill_spec_modifier_slice.py`](file:///d:/PRJ/NoMoreDay/scripts/migrate_skill_spec_modifier_slice.py)
  - 内容：在 `SUPPORTED_OPCODE_TO_OPERATION` 添加 `SKILL_PROJECTILES_ADD: "add"`, `SKILL_MANA_COST_FLAT: "add"`, `SKILL_BONUS_CRIT_DAMAGE: "add"`, `SKILL_RANGE_MULT: "mul"`。

### Phase 3：技能 2 与技能 3 Canonical 记录配置与生成（P1）
- [ ] **Task 3.1**：在 canonical JSON 中追加 11 条记录。
  - 文件：`[MODIFY]` [`assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json`](file:///d:/PRJ/NoMoreDay/assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json)
  - 内容：录入设计 §4.2 表所列 11 条记录（`2002000` ~ `2003320`）。
- [ ] **Task 3.2**：重生成运行时产物并验证门禁（**产物必须随变更提交**）。
  - 命令：
    ```bash
    python scripts/gen_skill_spec_modifier_contract.py
    python scripts/gen_modifier_runtime_v2.py --build
    python scripts/gen_skill_spec_modifier_contract.py --check
    python scripts/gen_modifier_runtime_v2.py --check
    ```
  - 说明：`tests/python/SkillSpecCanonicalGenerationTest.py` 断言"生成物 == 已提交物"，因此重生成的 runtime JSON 与二进制必须入库，否则 CI 必红。
- [ ] **Task 3.3**：新增/扩展离线门禁断言（设计 §4.1/§4.3/§5.1-5）。
  - 文件：`[MODIFY]` `scripts/gen_skill_spec_modifier_contract.py`（或新增 `scripts/validate_skill_spec_modifiers.py`）+ 对应 Python 测试。
  - 内容：
    1. 记录 ID 解码断言 `(id - 2000000) / 10 == node_id_whitelist[0]`，豁免白名单 `{2002103}`；
    2. `SKILL_PROJECTILES_ADD` 的 `param_f32` 必须为整数；
    3. OpCode 30..40 仅允许出现在 `debug_source == "skill_spec_node"` 的记录上（SkillDelivery 类别防蔓延）；
    4. canonical↔`skill_mechanics.json` 迁移等价断言（按算子逐条给出，非一律 `/100`）；
    5. 退役键不回潮断言（覆盖设计 §4.3 清单）。

### Phase 4：`SkillSpecializationBaker` 接入与硬编码退役（P1）
- [ ] **Task 4.1**：更新 `SkillSpecializationBaker::Bake` 步骤 3 的交付合成公式。
  - 文件：`[MODIFY]` [`src/game/systems/skill/SkillSpecializationBaker.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSpecializationBaker.cpp)
  - 内容：加入 `effective_mana_cost` flat 减免、`projectile_count` 增减、`bonus_crit_damage` 累加、`range` 乘算。
- [ ] **Task 4.2**：更新步骤 4 的 Keystone 与分支覆盖。
  - 文件：`[MODIFY]` [`src/game/systems/skill/SkillSpecializationBaker.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSpecializationBaker.cpp)
  - 内容：将 210 伤害惩罚后处理、311 翻倍、330 巨剑置 1 集中放置在步骤 4。
- [ ] **Task 4.3**：清理 `case 2:` 与 `case 3:` 的硬编码数值分支。
  - 文件：`[MODIFY]` [`src/game/systems/skill/SkillSpecializationBaker.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSpecializationBaker.cpp)
  - 内容：
    - `case 2`: 移除 200（area/range）、201（mana）、202（more）直接修改；210 仅保留机制位与步骤 4 惩罚；
    - `case 3`: 移除 300（count）、302（more）、310（range）、312（mana）、331（crit）、332（crit_damage）直接修改；保留 312/331/350 等的 feature_flags。
- [ ] **Task 4.4**：更新烘焙层测试用例。
  - 文件：`[MODIFY]` [`tests/unit/SkillSpecializationBakerTests.cpp`](file:///d:/PRJ/NoMoreDay/tests/unit/SkillSpecializationBakerTests.cpp)
  - 内容：编写技能 2 与技能 3 的 UMR 烘焙复合断言（200/201/202/210 组合；300+311 翻倍、300+330 巨剑覆盖等）。
- [ ] **Task 4.5**：步骤 1 显式写入 skill 3 的 `del.range` 基准。
  - 文件：`[MODIFY]` [`src/game/systems/skill/SkillSpecializationBaker.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSpecializationBaker.cpp)
  - 内容：在 `case 3:` 增加 `del.range = 200.0f;`（对齐 skill 2 的 `500.0f` 与 case 7 的显式基准先例）。断言：加点 310 后 `del.range == 200.0f * (1 + 0.20 * N)`。
- [ ] **Task 4.6**：退役 `skill_mechanics.json` 中已迁移的同名键。
  - 文件：`[MODIFY]` [`assets/data/skill_mechanics.json`](file:///d:/PRJ/NoMoreDay/assets/data/skill_mechanics.json)
  - 内容：
    - 删除 2/200 `area_radius_pct_per_point`/`range_pct_per_point`、2/201 `mana_reduction_per_point`、2/202 `phys_damage_pct_per_point`、3/302 `phys_damage_pct_per_point`、3/310 `range_pct_per_point`、3/312 `cost_reduction_pct_per_point`、3/331 `crit_chance_per_point`、3/332 `crit_damage_per_point`；
    - 保留 2/210 `damage_penalty_pt1/2/3`（转为唯一源）与 3/301 `haste_pct_per_point`（本批显式不迁移）；
    - 删除前以 `rg` 确认无残留读者；删除后跑全量功能测试。

### Phase 5：全量回归与验证闭环（P0）
- [ ] **Task 5.1**：全量编译构建验证。
  - 命令：`cmd /v:on /c "build.bat RelWithDebInfo & echo RC=!errorlevel!"`（确保 RC=0）。
- [ ] **Task 5.2**：针对性单元测试与功能套件执行。
  - 命令：`bin/NoMoreDayTests.exe --test-case=*RendingWave*,*BladeFormation*,*SkillBatch1*,*SkillSpecializationBaker*`。
- [ ] **Task 5.3**：全量 CTest 套件与 Python 门禁三检。
  - 命令：
    ```bash
    ctest --test-dir build -C RelWithDebInfo -L "unit|skill" --output-on-failure
    python -m unittest tests.python.SkillSpecCanonicalGenerationTest tests.python.SkillSpecModifierMigrationTest tests.python.ValidateJsonModifierCanonicalArtifactsTest tests.python.SkillSpecBatch1GateTest -v
    python scripts/gen_skill_spec_modifier_contract.py --check
    python scripts/gen_modifier_runtime_v2.py --check
    ```
  - 说明：`SkillSpecBatch1GateTest` 为 Task 3.3 新增门禁（ID 解码、整数约束、领域约束、迁移等价、退役键不回潮）。
- [ ] **Task 5.4**（可选，随本批顺手）：收紧非技能调用点的类别掩码。
  - 文件：`[MODIFY]` `src/game/systems/modifier/TalentModifierAdapter.cpp`、`MapModifierAdapter.cpp`、`SkillSpecModifierAdapter.cpp`（`EvaluateDamageMultiplier`）。
  - 内容：将默认 `ModifierOpCategory::All` 改为显式非 SkillDelivery 掩码，闭合上批次评审 N-1 的类别泄漏，防止 37..40 被非技能记录误用。

---

## 4. 测试方法与验证要求

1. **单测覆盖**：
   - `SkillBatch1DeliveryOpTests` 确保 4 个新增算子的代数正确性；
   - `SkillSpecializationBakerTests` 覆盖 300+311（(3+4)*2=14）、300+330（数量 1，半径 80）、210（数量 4，伤害 0.85x）等边界用例；
   - 新增：skill 3 基准 `del.range == 200.0f`、210 惩罚随 mechanics 数值变化、flat+mult 法耗结算序（设计 §5.1-3）。
2. **功能测试覆盖**：
   - `tests/functional/RendingWaveNodes.cpp` 全部既有用例（裂空斩发射、分裂、折返、湮灭波）PASS；
   - `tests/functional/BladeFormationNodes.cpp` 全部既有用例（灵剑召唤、巨剑、环绕、引爆）PASS。
3. **完成定义（DoD）**：
   - `build.bat RelWithDebInfo` 构建退出码 0 且无新增编译告警；
   - 全仓三方生成器 `--check` 门禁无漂移（exit 0）；
   - `SkillSpecBatch1GateTest` 全绿（ID 解码、整数约束、领域约束、迁移等价、退役键不回潮）；
   - `skill_mechanics.json` 中已迁移键全部退役、210 与 301 键保留并仍被读取；
   - 目标用例通过率 100%，无既有功能退化。
