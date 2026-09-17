# UMR 技能专精改造第二批（技能 4 剑气护体、技能 5 万剑归宗、技能 6 剑阵·诛仙）实施计划

- **文档状态**：已批准待实施（v2.1：按首轮 F 系列与第二轮 G 系列独立复核反馈全面修订）
- **设计依据**：[`docs/designs/2026-09-17-umr-skill-batch2-blade-ward-infinite-blades-sword-array-design.md`](file:///d:/PRJ/NoMoreDay/docs/designs/2026-09-17-umr-skill-batch2-blade-ward-infinite-blades-sword-array-design.md)
- **计划日期**：2026-09-17
- **系统代号**：`UMR-SKILL-BATCH-2` (Unified Modifier Runtime - Skill Batch 2)
- **修订记录**：
  - v1.0：初稿（19 条 Canonical 记录 + 2 个交付算子 41/42）。
  - v2.0：响应首轮独立审查反馈全面修订：
    - 补齐技能 5 lines 1197-1205 弹速消费端单源改造，闭环算子 42（对齐 F-1 Blocker）；
    - 规范节点 402 为纯正技能自身法耗折扣（`SKILL_MANA_COST_MULT`），下沉至 `effective_mana_cost`，废除 `/ 30.0f` 魔法数反算并消除装备减耗穿透漏洞（对齐 F-2 High）；
    - 显式退役节点 533 的 3 个历史遗留未引用死键（`sword_count_mult`、`size_bonus_pct`、`impact_radius`），退役键总数由 19 扩充至 22 项，使门禁 6 反向登记完全自洽（对齐 F-3 Medium）；
    - 明确 Task 3.3 中 `MIGRATION_EQUIVALENCE` 19 条规则的 relation 类型规格（`raw` / `percent` / `flat_negate`）（对齐 F-4 Medium）；
    - 明确 Baker 步骤 2 消除节点 552 对 `del.range` 的污染性覆盖（对齐 F-5 Medium）；
    - 修正 Task 2.2 注册迁移脚本的字典变量名为 `SUPPORTED_OPCODE_TO_OPERATION`（对齐 F-6 Low）；
    - 明确节点 470 反击剑气数读取接口对接细节（对齐 F-7 Low）；
    - 步骤 3 弹速合成增加下限 0 防护（对齐 B-1 最佳实践）。
  - v2.1：第二轮独立复核修订（G-1 ~ G-8）：
    - 移除失效的审查报告文件引用（对齐 G-1）；
    - 补齐 470 反击剑数落地链路：`BladeWardComponent::counter_sword_count` 缓存字段 + `SpawnBladeWardCounterSwords` 入参（对齐 G-2）；
    - 补齐 402 死字段/测试断言清理并声明「叠加→乘算」语义变更（对齐 G-3/G-4）；
    - 显式登记技能 6 施法范围（603）无消费端的既有限制（对齐 G-5）；
    - 补齐 Canonical 记录字段模板，并修正 510 索敌基准为机制表 `lock_range`（对齐 G-6/G-7）；
    - 退役键由 22 项扩充至 24 项（新增 6/610、6/611 的 `max_arrays_bonus`），新增独立死键门禁（对齐 G-8）。
- **计划范围**：
  - Phase 1：UMR 算子体系扩充（`ModifierOpCode` 41..42 + `ModifierDelta`）
  - Phase 2：离线生成与迁移脚本注册（`gen_modifier_runtime_v2.py` + `migrate_skill_spec_modifier_slice.py`）
  - Phase 3：技能 4/5/6 Canonical 记录配置（19 条）、二进制编译与离线门禁升级
  - Phase 4：`SkillSpecializationBaker` 步骤 1 基准确值化、步骤 2 清除 552 污染、步骤 3 增量合成（含下限 0）、步骤 4 Keystone 覆盖与冗余清理
  - Phase 5：消费端对接（`BladeWard.cpp`、`BeamChannelDeliverySystem.cpp`）与单一事实源退役（24 个 mechanics 键）
  - Phase 6：回归测试套件强化与全量构建验证

---

## 1. 实施思路与原理

### 1.1 数据流与架构边界
1. **纯数值外置入 UMR Canonical 数据管线**：
   - 技能 4（402 法耗折扣、471 反击 More 增伤）；
   - 技能 5（500 法耗折扣、502 陨铁增伤、510 锁定增耗、511 索敌范围、511 下落弹速、533 巨剑增伤、554 满剑意暴击、555 暴伤）；
   - 技能 6（600 持续时间、601 范围半径、602 阵内增伤、603 法耗折扣、603 施法射程、610 双生减伤、611 三才增耗、634 牢笼半径惩罚、653 随身减伤）；
   - 共计 19 条纯数值配置全部录入 [`assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json`](file:///d:/PRJ/NoMoreDay/assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json)，并通过编译器生成紧凑二进制四表。
2. **基准确值化与步骤 1 显式初始化**：
   - 乘算算子（`SKILL_RANGE_MULT`、`SKILL_SPEED_MULT`、`SKILL_AREA_MULT`）与加算算子（`SKILL_DURATION_FLAT`）不能依赖结构体默认值；
   - 在 Baker 步骤 1 显式写入技能 4（持续 10s、反击基准剑数 5）、技能 5（间隔 0.3s、弹速 1000、射程取自机制表 `5/510 lock_range` 默认 450、引导法耗覆写 20）、技能 6（半径 150、持续 5s、间隔 0.5s、射程 400）的确定性基准；技能 4/6 的施法法耗沿用 `skillData->mana_cost`（`skills.json` 单一事实源），不重复硬编码。
3. **步骤 2 污染清除与步骤 3 UMR AOT 增量合成**：
   - 清除步骤 2 中 552 对 `del.range` 的篡改（552 自身只消费 `circle_radius`，不可覆盖 510/511 的 450 索敌基准）；
   - 在步骤 3 调用 `SkillSpecModifierAdapter::EvaluateSkillDeliveryDeltas` 一次性求出 30..42 算子的增量包裹并执行确定性公式计算（弹速增加 `std::max(0.0f, ...)` 防护）；
   - 步骤 4 保持 Keystone 覆盖（533 巨剑范围保底下限 70.0f、670/672 互斥守卫）。
4. **单一事实源与消费端对齐**：
   - 移除 [`assets/data/skill_mechanics.json`](file:///d:/PRJ/NoMoreDay/assets/data/skill_mechanics.json) 中对应的 19 个同名键，外加 5 个未引用死键（节点 533 的 `sword_count_mult`、`size_bonus_pct`、`impact_radius`，以及节点 610/611 的 `max_arrays_bonus`），合计彻底退役删除 24 个键；
   - 修正 [`BeamChannelDeliverySystem.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/BeamChannelDeliverySystem.cpp)：
     - lines 1136-1146 索敌半径：单源直接读取 `profile->delivery.range`，删除 `GetSkill5Point(511)` 二次查表重算；
     - lines 1197-1205 下落弹速：单源直接读取 `profile->delivery.speed`，彻底移除 `GetSkill5Point(511)` 二次查表重算，闭环算子 42；
   - 修正 [`BladeWard.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/BladeWard.cpp)：
     - 402 法耗由 Baker 下沉到 `profile->effective_mana_cost`，自然扣费，彻底废除在 Buff 中反算 `ResourceCostReduction` 的魔法数逻辑；同步删除死字段 `BladeWardComponent::mana_cost_reduction` 并更新 `Skill4FollowupTests.cpp:160` 断言（402 折减与装备减耗由「叠加」改为「乘算」，属有意修正，需回归断言保护）；
     - 471 反击增伤直接读取 `profile->more_damage_mult`（不变量：技能 4 的 `more_damage_mult` 仅由 471 贡献）；
     - 470 反击剑气数在 `DoCast` 缓存到 `BladeWardComponent::counter_sword_count`，`SpawnBladeWardCounterSwords` 增收入参，由 `DamageInterceptors.hpp` 调用点传入（原调用点无 Profile，禁止直接取 `profile->delivery.sub_count`）。

---

## 2. 关键逻辑伪代码引导

### 2.1 算子分发与点数缩放 (ModifierEvaluator.cpp)
```cpp
// 在 ApplyOp 函数中扩充 41..42 算子分发
void ApplyOp(ModifierOpCode opcode, uint32_t paramU32, float paramF32,
             uint16_t points, float effectivePercentMult, ModifierDelta &out) {
  const float pts = static_cast<float>(points);
  switch (opcode) {
    // 30..40 既有交付算子...

    case ModifierOpCode::SKILL_DURATION_FLAT:
      // 持续时间绝对秒数累加：单点 delta = paramF32 * pts
      out.AddSkillDurationFlat(paramU32, paramF32 * pts);
      break;

    case ModifierOpCode::SKILL_SPEED_MULT:
      // 弹速/下落速度相对乘算：单点倍率 = 1.0 + paramF32 * pts
      out.AddSkillSpeedMult(paramU32, 1.0f + paramF32 * pts);
      break;
  }
}
```

### 2.2 烘焙层基准初始化与增量合成 (SkillSpecializationBaker.cpp)
```cpp
// 步骤 1: 基础属性与交付默认推导 (显式基准写入)
switch (skill_id) {
  case 4: // 剑气护体
    del.duration = 10.0f;
    del.sub_count = 5; // 反击剑气基准数量 5 (解 F-7)，DoCast 缓存到 ward.counter_sword_count
    out_profile.more_damage_mult = 1.0f;
    // 施法法耗沿用 skillData->mana_cost（skills.json=30），不重复硬编码 (G-3)
    break;
  case 5: // 万剑归宗
    del.sub_interval = 0.3f;
    del.speed = 1000.0f; // 无机制键，集中为具名常量
    del.range = data::SkillMechanicsRegistry::Get().GetFloat(5, 510, "lock_range", 450.0f); // 单一事实源 (G-7)
    out_profile.effective_mana_cost = 20.0f; // 既有引导速率覆写
    break;
  case 6: // 剑阵·诛仙
    out_profile.area_radius = 150.0f;
    del.duration = 5.0f;
    del.sub_interval = 0.5f;
    del.range = 400.0f; // 当前无消费端 (G-5)
    // 施法法耗沿用 skillData->mana_cost（skills.json=30）
    break;
}

// 步骤 2: 清理专精循环中由 UMR 接管的纯数值
// 特别清理: 移除 case 5 中 552 对 del.range 的赋值覆盖 (解 F-5)

// 步骤 3: UMR 交付 Delta 合成
// ... 既有 30..40 合成 ...
del.speed = std::max(0.0f, del.speed * specDelta.GetSkillSpeedMult(skill_id)); // 下限 0 防护 (采纳 B-1)
del.duration = std::max(0.0f, del.duration + specDelta.GetSkillDurationFlat(skill_id));

// 步骤 4: Keystone 覆盖
if (skill_id == 5) {
  if ((out_profile.delivery.feature_flags & 4096) != 0) { // 533 巨剑术
    const float giantRad =
        data::SkillMechanicsRegistry::Get().GetFloat(5, 533, "giant_radius", 70.0f);
    out_profile.area_radius = std::max(out_profile.area_radius, giantRad);
  }
}
```

### 2.3 消费端单源对齐 (BeamChannelDeliverySystem.cpp & BladeWard.cpp)
```cpp
// BeamChannelDeliverySystem.cpp: 索敌半径与下落弹速双单源读取 (解 F-1)
// 1. 索敌逻辑 (lines 1136-1146):
if (beam.aim_assist) {
  const float lock_radius = (profile && profile->delivery.range > 0.0f) 
                          ? profile->delivery.range : 450.0f;
  // 彻底移除对 GetSkill5Point(registry, entity, 511) 的二次重算分支！
}

// 2. 弹速逻辑 (lines 1197-1205):
const float finalSpeed = (profile && profile->delivery.speed > 0.0f) 
                       ? profile->delivery.speed : 1000.0f;
// 彻底移除对 GetSkill5Point(registry, entity, 511) 的二次弹速计算！

// BladeWard.cpp: 402 法耗自然消费，471 增伤单源读取，470 剑气数缓存 (解 F-2, F-7, G-2~G-4)
// 402: 施法时直接扣除 profile->effective_mana_cost（已下沉折扣），移除 ward_buff 反算；
//      同步删除 BladeWardComponent::mana_cost_reduction 死字段并更新 Skill4FollowupTests 断言。
// 471（DoCast）:
ward.counter_damage_more = profile ? (profile->more_damage_mult - 1.0f) : 0.0f;
// 470（DoCast 缓存，供后续伤害拦截点复用）:
ward.counter_sword_count = (profile && profile->delivery.sub_count > 0)
                             ? profile->delivery.sub_count : 5;
// DamageInterceptors.hpp::ResolveSkill4CounterEffects:
SpawnBladeWardCounterSwords(registry, counter_attacker, 0, ward.counter_sword_count);
```

---

## 3. 原子任务拆分

### Phase 1：UMR 算子体系扩充（OpCode 41..42 + ModifierDelta）
- [ ] 1.1 在 [`ModifierContext.hpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/modifier/ModifierContext.hpp) 的 `ModifierOpCode` 中增加 `SKILL_DURATION_FLAT = 41` 与 `SKILL_SPEED_MULT = 42`。
- [ ] 1.2 在 `CategoryOfOp` 中将 41 与 42 映射为 `ModifierOpCategory::SkillDelivery`。
- [ ] 1.3 在 [`ModifierEvaluator.hpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/modifier/ModifierEvaluator.hpp) 的 `ModifierDelta` 中新增 `skill_duration_flat`（默认 0.0f）与 `skill_speed_mult`（默认 1.0f）容器及访问器接口。
- [ ] 1.4 在 [`ModifierEvaluator.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/modifier/ModifierEvaluator.cpp) 中实现访问器、`MergeFrom` 合并逻辑与 `ApplyOp` 中的算子点数缩放分发。

### Phase 2：离线生成与迁移脚本注册
- [ ] 2.1 在 [`scripts/gen_modifier_runtime_v2.py`](file:///d:/PRJ/NoMoreDay/scripts/gen_modifier_runtime_v2.py) 的 `OPCODE_VALUES` 注册 `SKILL_DURATION_FLAT: 41` 与 `SKILL_SPEED_MULT: 42`。
- [ ] 2.2 在 [`scripts/migrate_skill_spec_modifier_slice.py`](file:///d:/PRJ/NoMoreDay/scripts/migrate_skill_spec_modifier_slice.py) 的 `SUPPORTED_OPCODE_TO_OPERATION` 中注册 41 与 42（修正字典变量名，解 F-6）。
- [ ] 2.3 在 [`scripts/validate_skill_spec_modifiers.py`](file:///d:/PRJ/NoMoreDay/scripts/validate_skill_spec_modifiers.py) 中将 `SKILL_DELIVERY_OPCODE_MAX` 上调至 `42`。

### Phase 3：Canonical 数据录入与离线门禁升级（19 条记录）
- [ ] 3.1 在 [`skill_spec_modifiers.canonical.json`](file:///d:/PRJ/NoMoreDay/assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json) 中追加录入 19 条记录（ID 范围 `2004020` ~ `2006530`）。
- [ ] 3.2 运行代码生成工具重建 `skill_spec_modifiers.json` 与二进制运行时。
- [ ] 3.3 在 [`scripts/validate_skill_spec_modifiers.py`](file:///d:/PRJ/NoMoreDay/scripts/validate_skill_spec_modifiers.py) 中注册：
  - `MIGRATION_EQUIVALENCE` 19 条规则的 relation 规格（解 F-4）：
    - `percent`: 554（`crit_chance_bonus: 100.0` -> `1.0`）；
    - `flat_negate`: 510 (`0.30` -> `-0.30`), 610 (`0.15` -> `-0.15`), 611 (`0.30` -> `-0.30`), 634 (`0.30` -> `-0.30`), 653 (`0.50` -> `-0.50`)；
    - `raw`: 402, 471, 500, 502, 511(2项), 533, 555, 600, 601, 602, 603(2项)；
  - `KEPT_MECHANICS_KEYS` 注册已迁移节点的合法保留键（5/502 `splash_radius`、5/510 `lock_range`、5/533 `giant_radius`、5/554 `intent_cost`、4/470 `counter_swords`；原方案的 6/610、6/611 `max_arrays_bonus` 改列死键，不再保留）。
  - **新增独立死键门禁（解 G-8）**：`DEAD_MECHANICS_KEYS` 覆盖 5/533 `sword_count_mult`/`size_bonus_pct`/`impact_radius` 与 6/610、6/611 的 `max_arrays_bonus`（5 项，无 canonical 记录，`check_retired_keys_absent` 无法覆盖），断言其已从机制表删除且不得回潮。
- [ ] 3.4 按设计 §4.2.0 模板补齐每条记录的 `operation`/`target`/`tags`/`conditions`/`stacks`（不变量：`stacks == param_u32 == skill_id`，`stat_path == target`）。
- [ ] 3.5 扩展或新增 `tests/python/SkillSpecBatch2GateTest.py`，断言 19 条记录全部满足 ID 解码、范围校验与门禁。

### Phase 4：`SkillSpecializationBaker` 烘焙管线与基准改造
- [ ] 4.1 在 `Bake` 步骤 1 显式初始化技能 4、5、6 的默认交付基准参数（含技能 4 `del.sub_count = 5`，解 F-7；技能 5 索敌基准改读机制表 `5/510 lock_range`，解 G-7；技能 4/6 施法法耗沿用 `skillData->mana_cost`，不再硬编码）。
- [ ] 4.2 清理 `case 5` 专精循环中节点 552 对 `del.range` 的赋值，消除对 510/511 射程基准的污染（解 F-5）。
- [ ] 4.3 在 `Bake` 步骤 3 接入 `del.speed`（加下限 0 防护，解 B-1）与 `del.duration` 的确定性合成。
- [ ] 4.4 在 `Bake` 步骤 4 确认技能 5 Keystone 533（巨剑术）的 `area_radius` 保底下限（`70.0f`）与技能 6 元素互斥守卫。
- [ ] 4.5 清理 `case 4`、`case 5`、`case 6` 中已迁移为 UMR 交付算子的冗余手写赋值分支，仅保留机制标志位（`feature_flags`）。

### Phase 5：消费端对接与单一事实源退役（24 个退役键）
- [ ] 5.1 修改 [`BladeWard.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/BladeWard.cpp)：
  - 402 法耗直接消费 `profile->effective_mana_cost`，彻底移除 `ward_buff` 中基于 `/ 30.0f` 的反算逻辑（解 F-2）；删除死字段 `BladeWardComponent::mana_cost_reduction` 并更新 `Skill4FollowupTests.cpp:160` 断言（叠加→乘算，解 G-4）；
  - 471 反击增伤统一改读 `profile->more_damage_mult`（不变量：技能 4 的 `more_damage_mult` 仅由 471 贡献，解 G-3）；
  - 471 fail-closed 定案（评审 #1）：`profile == nullptr` 时反击增伤 fail-closed 为 `0.0f`，与 470 机制表回退刻意不对称（缓存档案缺失时同名专精即时烘焙，`profile == nullptr` 等价于专精槽缺失，迁移前 `nodePoints` 本为 `0`；影子复制入口 `ShadowDuplicationHook.cpp:21-26` 的技能 4 带 `Tag::Buff` 已排除）；回归断言落在 `tests/functional/Skill4FollowupTests.cpp`；
  - ward 持续时间单源（评审 #9）：改读 `profile->delivery.duration`（未烘焙回退 `10.0f`），消除与 Baker 基准的多份硬编码副本；
  - 470 反击剑气数在 `DoCast` 缓存到新增字段 `ward.counter_sword_count`，`SpawnBladeWardCounterSwords` 增参，由 `DamageInterceptors.hpp::ResolveSkill4CounterEffects` 传入（解 F-7/G-2，禁止在原调用点直接取 Profile）。
- [ ] 5.2 修改 [`BeamChannelDeliverySystem.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/BeamChannelDeliverySystem.cpp)：
  - lines 1136-1146 索敌半径直接采用 `profile->delivery.range`，彻底删除手写查表二次计算；
  - lines 1197-1205 下落弹速直接采用 `profile->delivery.speed`，彻底删除 `GetSkill5Point(511)` 二次重算，闭环算子 42（解 F-1）；
  - 511 索敌/弹速语义门控（评审 #3/#4）：以 `feature_flags & 16` 作语义门控，取值来自 `profile->delivery.range`/`profile->delivery.speed`，未分配 511 时沿用基准 `450`/`1000`，替换 `> 0.0f` sentinel 判定。
- [ ] 5.3 从 [`skill_mechanics.json`](file:///d:/PRJ/NoMoreDay/assets/data/skill_mechanics.json) 中彻底删除 24 个退役键（19 个迁移键 + 5 个未引用死键：节点 533 的 `sword_count_mult`/`size_bonus_pct`/`impact_radius`，节点 610/611 的 `max_arrays_bonus`）（解 F-3/G-8）。
- [ ] 5.4 运行 `validate_skill_spec_modifiers.py --check`，断言退役键不回潮且反向登记 100% 吻合。

### Phase 6：回归测试与全量构建验证
- [ ] 6.1 新增单元测试 [`tests/unit/SkillBatch2DeliveryOpTests.cpp`](file:///d:/PRJ/NoMoreDay/tests/unit/SkillBatch2DeliveryOpTests.cpp)，覆盖 41/42 算子单点、多点、负数下限防护与合并行为，并登记进 CMake 测试目标（避免未被收集）。
- [ ] 6.2 扩展 [`tests/unit/SkillSpecializationBakerTests.cpp`](file:///d:/PRJ/NoMoreDay/tests/unit/SkillSpecializationBakerTests.cpp)，覆盖技能 4/5/6 专精烘焙测试用例；补充 402 与装备减耗乘算、470 双路径剑数、603 parity 断言。
  - 负输入钳制与可达点数修正（评审 #2）：`SkillSpecializationBakerTests.cpp` 增加 41/42 算子负输入（负时长/负速度）下限 0 钳制断言，以及专精可达点数（`allocated_points`）修正用例；
  - [`tests/functional/SwordArrayNodes.cpp`](file:///d:/PRJ/NoMoreDay/tests/functional/SwordArrayNodes.cpp) 增加与上述修正对应的功能回归断言。
- [ ] 6.3 为 `Skill4FollowupTests.cpp`、`InfiniteBladesNodes.cpp`、`SwordArrayNodes.cpp`、`Skill5FollowupTests.cpp` 的测试初始化函数补齐 `ReloadModifierRuntimeFromAsset()`，杜绝环境污染。
- [ ] 6.4 运行 `build.bat RelWithDebInfo`，断言编译成功退出码 0。
- [ ] 6.5 运行全量测试套件，断言 0 失败、全量门禁工具 `--check` 通过。

---

## 4. 测试方法与护栏

### 4.1 测试层级与覆盖目标
1. **OpCode 基础单元测试（Unit）**：
   - 验证 `SKILL_DURATION_FLAT`：1 点加 0.5s、4 点加 2.0s，下限保护不出现负时长；
   - 验证 `SKILL_SPEED_MULT`：1 点加 25%（1.25x）、3 点加 75%（1.75x），下限保护不出现负速度；
   - 验证 `ModifierDelta::MergeFrom`：累加与累乘行为一致性。
2. **烘焙组合测试（Baker Unit）**：
   - 技能 4：402 加点降低法耗（`30 * (1 - 0.15*N)`）、471 加点提升反击倍率（`1.0 + 0.20*N`）、470 基准剑数 5；
   - 技能 5：500 法耗折扣、502 More 增伤、510 增耗、511 射程与弹速同步缩放（450 与 1000 基准）、533 巨剑增伤且半径保底 70、554 满暴击 1.0、555 暴伤累加；
   - 技能 6：600 时长加成（`5.0 + 0.5*N`）、601 范围缩放、602 增伤、603 法耗降低（施法距离为 parity 保持、当前无消费端，解 G-5）、610 减伤惩罚、611 增耗惩罚、634 半径惩罚、653 随身光环减伤。
3. **功能回归测试（Functional）**：
   - `ctest -R Skill4FollowupTests`
   - `ctest -R InfiniteBladesNodes`
   - `ctest -R SwordArrayNodes`
   - `ctest -R Skill5FollowupTests`
4. **离线与生成器三检门禁（Python Gate）**：
   - `python scripts/validate_skill_spec_modifiers.py --check`（断言 6/6 通过，19 条迁移键 + 5 条死键拦截）
   - `python scripts/gen_skill_spec_modifier_contract.py --check`
   - `python scripts/gen_modifier_runtime_v2.py --check`
   - `python tests/python/SkillSpecBatch1GateTest.py`（及新增的 Batch 2 门禁用例）

---

## 5. 验证任务完成（退出标准与验收证据）

| 序号 | 验收项 | 退出标准 / 证据要求 |
|---|---|---|
| 1 | 编译构建 | 执行 `build.bat RelWithDebInfo`，返回码 `EXIT = 0`，0 警告 / 0 错误。 |
| 2 | 全量单元测试 | 默认顺序执行全量用例，0 failed，断言全部通过。 |
| 3 | 专项功能回归 | 技能 4、5、6 的全部功能测试（`Skill4FollowupTests`、`InfiniteBladesNodes`、`SwordArrayNodes`）100% 通过。 |
| 4 | 算子单元测试 | 新增的 `SkillBatch2DeliveryOpTests` 与扩展的 Baker 测试用例 100% 通过。 |
| 5 | 消费端单源断言 | `BeamChannelDeliverySystem.cpp` 中 lines 1136-1146 与 lines 1197-1205 均单源消费 `profile->delivery`，无 `GetSkill5Point(511)` 二次重算；`BladeWard` 471/470 亦单源（`more_damage_mult` / `ward.counter_sword_count`）。 |
| 6 | 离线三检门禁 | `validate_skill_spec_modifiers.py --check` 输出 `6/6 passed`（含 19 条新记录、19 条迁移键与 5 条独立死键门禁）。 |
| 7 | 单一事实源验证 | `rg` 确认退役的 24 个键在 `skill_mechanics.json` 中已被完全删除，且 C++ 中无对应字面量残留；保留键（5/502 `splash_radius`、5/510 `lock_range`、5/533 `giant_radius`、5/554 `intent_cost`）仍在。 |
| 8 | 生成器自洽性 | `gen_skill_spec_modifier_contract.py --check` 与 `gen_modifier_runtime_v2.py --check` 输出 `up to date`。 |
