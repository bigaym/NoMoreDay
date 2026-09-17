# UMR 技能专精改造第四批（技能 10 七星斩、技能 11 天剑降临、技能 12 血海）实施计划

- **文档状态**：待评审（v1.2：二轮代码实况复核闭环）
- **设计依据**：[`docs/designs/2026-09-17-umr-skill-batch4-seven-stars-heavenly-sword-blood-sea-design.md`](file:///d:/PRJ/NoMoreDay/docs/designs/2026-09-17-umr-skill-batch4-seven-stars-heavenly-sword-blood-sea-design.md)
- **计划日期**：2026-09-17
- **系统代号**：`UMR-SKILL-BATCH-4` (Unified Modifier Runtime - Skill Batch 4)
- **修订记录**：
  - v1.0：初稿（9 条 Canonical 记录 + 8 迁移键退役 + 6 处全局属性清理 + Baker 步骤 1 基准补齐 + 3 招牌行为层单源消费重构 + 7 阶段原子任务）。
  - v1.1：首轮审查缺陷闭环（修复 2011190 节点 ID 笔误 1109->1119、清空 1207 damage_modifiers 并防范双重计算、改用 ResolveBakedProfile 标准基元、校准技能 11 sub_interval 为 0.5f、补齐 1107/1207 门禁死键防回潮、补全 Phase 5 行为层物理清理清单）。
  - v1.2：二轮代码实况复核闭环（删除无消费点的 `del.range`/`del.sub_interval` 死写、新增 `ResolveBakedProfile` 空值守卫与逐技能回退基准、删除 `HeavenlySwordFieldComponent::bonus_damage_mult` 与技能 10/11 的无效 More 乘区、补齐 `BloodSea.cpp:482-485` 使用点删除与 `:484` 乘算位置约束、新增既有测试断言同步改造 Task 3.3、1207 描述文本修正 Task 3.4、迁移等价性快照 Task 6.3、Phase 7 全量回归要求）。
- **计划范围**：
  - Phase 1：技能 10/11/12 Canonical 记录配置（9 条）与二进制管线生成
  - Phase 2：离线门禁体系扩展（`validate_skill_spec_modifiers.py`）与独立 Python 门禁测试（`SkillSpecBatch4GateTest.py`）
  - Phase 3：机制表单一事实源退役（8 个迁移键）、`mastery_skill_trees.json` 属性及修饰器清理（6 处 stat_modifiers + 1 处 damage_modifiers）、既有断言的同步改造与节点 1207 描述文本修正
  - Phase 4：`SkillSpecializationBaker` 步骤 1 基准补齐（case 10..12）与步骤 2 边界确权
  - Phase 5：招牌技能行为层消费重构（`SevenStarSlash.cpp`, `HeavenlySwordDescent.cpp`, `BloodSea.cpp`）单源消费 Profile，物理清理旧读取与旧分支
  - Phase 6：C++ 单元测试套件实现（`SkillBatch4DeliveryOpTests.cpp`）、迁移等价性快照与测试分层隔离保障
  - Phase 7：编译验证、门禁三检、全量回归测试与收尾交付

---

## 1. 实施思路与原理

### 1.1 数据流与架构边界

1. **纯数值全量外置入 UMR Canonical 数据管线（0 新增算子）**：
   - **技能 10（七星斩）**：
     - 1001 锋芒毕露：`SKILL_BONUS_CRIT`（基础暴击率 +2%/点，param_f32: 0.02）；
     - 1015 踏虚：`SKILL_DURATION_FLAT`（无敌帧时长 +0.03s/点，param_f32: 0.03）。
   - **技能 11（天剑降临）**：
     - 1101 天域增幅：`SKILL_AREA_MULT`（领域半径 +8%/点，param_f32: 0.08）；
     - 1107 天穹贯星：`SKILL_AREA_MULT`（Keystone 领域半径惩罚 -30%，param_f32: -0.30）；
     - 1119 久驻天域：`SKILL_DURATION_FLAT`（领域持续时间 +0.5s/点，param_f32: 0.50）。
   - **技能 12（血海）**：
     - 1201 血压潮升：`SKILL_MORE_DAMAGE_MULT`（持续伤害 More +6%/点，param_f32: 0.06）；
     - 1207 无间血狱（增伤）：`SKILL_MORE_DAMAGE_MULT`（Keystone More 增伤 +10%，param_f32: 0.10）；
     - 1207 无间血狱（缩圈）：`SKILL_AREA_MULT`（Keystone 范围缩小 -40%，param_f32: -0.40）；
     - 1219 久驻血雾：`SKILL_DURATION_FLAT`（血海持续时间 +0.6s/点，param_f32: 0.60）。
   - 共计 9 条纯数值记录录入 [`assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json`](file:///d:/PRJ/NoMoreDay/assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json)，复用既有 OpCodes 30, 34, 35, 41，不产生底层 Schema 变更。

2. **Baker 步骤 1 基准最小化补齐**：
   - 彻底解决技能 10~12 长期命中 Baker `default:` 空分支导致的“半空心化”问题：
     - 技能 10：`out_profile.area_radius = skillData->GetParam("radius", 96.0f)`、`del.duration = skillData->GetParam("invulnerable_duration", 0.5f)`；
     - 技能 11：`out_profile.area_radius = skillData->GetParam("field_radius", 140.0f)`、`del.duration = skillData->GetParam("field_duration", 5.0f)`；
     - 技能 12：`out_profile.area_radius = skillData->GetParam("field_radius", 120.0f)`、`del.duration = skillData->GetParam("field_duration", 4.8f)`；
   - **只写步骤 3 会二次合成的字段**（`area_radius` 参与 `*= GetSkillAreaMult`、`del.duration` 参与 `+= GetSkillDurationFlat`）。`del.range` 与 `del.sub_interval` 本批**不写**：两者既无专精算子，也无任何消费点（三个行为层不读 `profile->delivery.range`；`profile->delivery.sub_interval` 仅被技能 3/5/6 消费），写入即为死写，违反本设计“杜绝死写”的分层原则；
   - 使步骤 3 的 UMR 交付增量合成有据可依，杜绝乘算算子因隐式 `1.0f`/`200.0f` 默认值导致严重数值漂移。

3. **专精树全局属性清理与单一事实源归位**：
   - 清理 `mastery_skill_trees.json` 中 1001、1101、1107、1119、1201、1207 的 6 处 `stat_modifiers: []`，防止误经 `StatsSystem`（`StatsSystem.cpp:455-495`，技能作用域）污染玩家全局属性；清理前须按 `Stats.hpp:224-284` 的 `StatType` 逐条核对语义（现登记的 `type 17/25/39/13/25` 实为 AttackSpeed/ResistLightning/HealthRegen/PoisonDamage/ResistLightning，与节点描述完全不符，属历史串写）；
   - **清空节点 1207 的 `damage_modifiers: []`**，并在行为层物理删除 `bottomless_damage_mult` 旧乘算，由 UMR `2012070` 单源驱动 10% More 增伤，杜绝 1.43x 伤害膨胀；同时修正节点 1207 的 `desc_key` 文本（当前明文仍为 30%）；
   - 节点 1015 残留的 `stat_modifiers: [{"type":35,"mode":1,"value":20.0}]`（DodgeChance +20%）与本次退役键无关，**本批明确保留并登记为已知遗留**，不纳入清理；
   - 机制表 `skill_mechanics.json` 彻底退役删除 8 个已迁移键，并在门禁脚本追加 `DEAD_MECHANICS_KEYS` 严格守护死键（覆盖 literal 迁移键）。

4. **行为层单源消费 Profile 重构（含空值守卫）**：
   - 重构 `SevenStarSlash.cpp`、`HeavenlySwordDescent.cpp`、`BloodSea.cpp` 中的 `DoCast`，采用统一标准公共基元 [`SkillProfileResolve.hpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillProfileResolve.hpp) 的 `ResolveBakedProfile`；
   - **`ResolveBakedProfile` 在“无缓存档案且无同 ID 专精槽”时返回 `nullptr`，不会自动兜底烘焙**，故全部消费点必须写成 `profile ? profile->X : <原基准>` 三元回退（对齐 Batch 1~3 既有惯例），严禁无条件解引用；
   - 彻底物理删除函数内部残留的 `GetMech` 本地查表加算、乘算变量与分支，按技能单源消费：技能 10 → `delivery.bonus_crit` / `delivery.duration`；技能 11 → `area_radius` / `delivery.duration`；技能 12 → `area_radius` / `delivery.duration` / `more_damage_mult`。

---

## 2. 关键逻辑伪代码引导

### 2.1 烘焙层基准初始化 (`SkillSpecializationBaker.cpp`)

```cpp
// ==================== 步骤 1: 基础属性与交付默认推导 ====================
// 写入原则：只写步骤 3 会二次合成的字段（area_radius / del.duration）。
// del.range 与 del.sub_interval 无算子、无消费点，写入即死写，本批不写。
switch (skill_id) {
  // ...既有 case 1..9 保持不变...

  case 10: // 七星斩 (Sword Saint Mastery)
    out_profile.area_radius = skillData->GetParam("radius", 96.0f);
    del.duration = skillData->GetParam("invulnerable_duration", 0.5f);
    break;

  case 11: // 天剑降临 (Heavenly Sword Mastery)
    out_profile.area_radius = skillData->GetParam("field_radius", 140.0f);
    del.duration = skillData->GetParam("field_duration", 5.0f);
    break;

  case 12: // 血海 (Demon Blade Mastery)
    out_profile.area_radius = skillData->GetParam("field_radius", 120.0f);
    del.duration = skillData->GetParam("field_duration", 4.8f);
    break;
}

// ==================== 步骤 2: ApplyNodeModifiersToProfile 边界确权 ====================
// 技能 10/11/12 不新增分支，继续落入既有 default: break; 直接返回。
// 纯数值交付交由步骤 3 UMR 自动合成；非 UMR 状态由各行为层 SpecState 驱动，
// 因此无需（也不应）新增行为等价于 default 的空分支。
switch (skill_id) {
  // ...既有 case 1..9 保持不变...
}
```

### 2.2 行为层单源消费重构伪代码（复用既有标准基元 `ResolveBakedProfile`）

> **通用不变量**：`ResolveBakedProfile` 仅在“命中缓存档案”或“命中同 ID 专精槽并即时烘焙”时返回非空；两者都不满足时返回 `nullptr`（且**不会**自动兜底烘焙）。因此每个消费点都必须显式回退，回退值等于迁移前的基准。
> **哨兵守卫**：`skillData` 缺失时 Baker 会早退并写入 `duration = 1.0f` / `area_radius = 1.0f` 的**哨兵档案**（非空、默认值为正，无法用数值判据区分），此时 `skill` 亦为空指针。故凡有字段级回退的消费点一律写 `(profile && skill != nullptr) ? profile->X : (skill ? skill->GetParam(...) : <默认基准>)`；回退分支的 `skill->GetParam` 必须包在 `skill ?` 内以免空解引用。

#### 1. 七星斩 (`SevenStarSlash.cpp`)
```cpp
#include "game/systems/skill/SkillProfileResolve.hpp"

void SevenStarSlash::DoCast(entt::registry &registry, entt::entity owner, SkillExecution &exec) {
  BakedSkillProfile localProfile;
  const auto *profile = ResolveBakedProfile(registry, owner, kSkillId, localProfile);

  // 单源消费 UMR 交付参数（物理删除 critChancePerPoint(line 368) / voidTreadDurationPerPoint(line 380)）
  float invulnerableDuration =
      profile ? profile->delivery.duration
              : skillData->GetParam("invulnerable_duration", 0.5f);
  float critChanceBonus = profile ? profile->delivery.bonus_crit : 0.0f;
  // 注意：技能 10 无 SKILL_MORE_DAMAGE_MULT 记录，且本文件不存在 baseDamageMultiplier
  //      （斩击基础伤害变量为 baseSlashDamage），故不引入 more_damage_mult 乘算。
  // ... 其余斩击循环逻辑保持不变 ...
}
```

#### 2. 天剑降临 (`HeavenlySwordDescent.cpp`)
```cpp
#include "game/systems/skill/SkillProfileResolve.hpp"

void HeavenlySwordDescent::DoCast(entt::registry &registry, entt::entity owner, SkillExecution &exec) {
  BakedSkillProfile localProfile;
  const auto *profile = ResolveBakedProfile(registry, owner, kSkillId, localProfile);

  // 1. 领域半径单源消费（已包含 1101 与 1107 惩罚，物理删除 celestial_domain_per_point(line 505) 与 sky_piercing_range_mult(line 508)）
  const float areaMult =
      (skill != nullptr && profile && base_field_radius > 0.0f)
          ? (profile->area_radius / base_field_radius) : 1.0f;
  field.header.radius = (base_field_radius + static_cast<float>(spent_tiers) * tier_radius_bonus) * areaMult;

  // 2. 领域持续时间单源消费（已包含 1119 久驻天域，物理删除 enduring_heaven_per_point(line 517)）
  field.header.duration = (profile && skill != nullptr)
                              ? profile->delivery.duration
                              : (skill ? skill->GetParam("field_duration", kFieldDurationFallback)
                                       : kFieldDurationFallback);

  // 3. 本技能不存在 More 增伤 UMR 记录，且 HeavenlySwordFieldComponent 无 bonus_damage_mult 成员，
  //    故不写任何 more_damage_mult 乘算（1107 的 impact_damage_bonus 仍走机制层）
  // ... 其余领域生成与脉冲逻辑保持不变 ...
}
```

#### 3. 血海 (`BloodSea.cpp`)
```cpp
#include "game/systems/skill/SkillProfileResolve.hpp"

void BloodSea::DoCast(entt::registry &registry, entt::entity owner, SkillExecution &exec) {
  BakedSkillProfile localProfile;
  const auto *profile = ResolveBakedProfile(registry, owner, kSkillId, localProfile);

  // 1. 持续时间单源消费（基础时长 + 1219 加算由 Profile 承接；
  //    物理删除 duration_per_point(line 372) 及其使用点 line 482-483）
  field.header.duration =
      ((profile && skill != nullptr)
           ? profile->delivery.duration
           : (skill ? skill->GetParam("field_duration", kFieldDurationDefault) : kFieldDurationDefault)) +
      field_duration_per_bloodthirst * static_cast<float>(effective_consumed);

  // 2. 领域半径单源消费（基础半径由 Profile 承接，含 1207 -40% 缩圈；
  //    随后保留 line 480-481 的 1200 加算与 line 540 的环形形态乘算）
  field.header.radius =
      ((profile && skill != nullptr)
           ? profile->area_radius
           : (skill ? skill->GetParam("field_radius", kFieldRadiusDefault) : kFieldRadiusDefault)) +
      static_cast<float>(effective_consumed) * field_radius_per_bloodthirst +
      static_cast<float>(spec.bloodCurtainOpeningPoints) * radius_per_point;

  // 3. 持续伤害 More 增伤单源消费（含 1201 +6%/点 与 1207 +10%）
  //    插入位置固定在 line 484-485（原 1201 乘算处），并物理删除：
  //      - damage_per_point(line 313) 及其使用点 line 484-485
  //      - bottomless_damage_mult(line 330) 及其使用点 line 544-546
  //    顺序约束：line 486-488 的 1202 是“+=”加算项，故把 1201 的乘算提前会引入
  //    0.1*K*ring 偏差（K=1202 加算项）；该偏差已被设计 R-05 显式接受，由新增的
  //    R-05 快照测试 tests/unit/SkillBatch4BloodSeaOrderingTests.cpp（“1202 = 0”与
  //    “1202 > 0”两组用例，精确锁定 final = baseline*more + K 与偏差 K*(more-1)）覆盖。
  field.bonus_damage_mult *= (profile ? profile->more_damage_mult : 1.0f);
  // ... 其余血海场生成逻辑保持不变 ...
}
```

---

## 3. 原子任务拆分

### Phase 1: Canonical 记录配置与生成链
- [ ] **Task 1.1**: 在 [`assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json`](file:///d:/PRJ/NoMoreDay/assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json) 中追加 9 条记录（`2010010`, `2010150`, `2011010`, `2011070`, `2011190`, `2012010`, `2012070`, `2012071`, `2012190`）。
- [ ] **Task 1.2**: 运行 `python scripts/gen_skill_spec_modifier_contract.py` 同步契约 JSON（`assets/data/modifier_v2/skill_spec_modifiers.json`）。
- [ ] **Task 1.3**: 运行 `python scripts/gen_modifier_runtime_v2.py --build` 编译生成紧凑二进制（`assets/generated/modifier_runtime_v2.bin`）。

### Phase 2: 离线门禁体系扩展
- [ ] **Task 2.1**: 在 [`scripts/validate_skill_spec_modifiers.py`](file:///d:/PRJ/NoMoreDay/scripts/validate_skill_spec_modifiers.py) 的 `MIGRATION_EQUIVALENCE` 中登记 9 条等价断言（确保 `2011190` 正确映射至节点 `1119`），在 `DEAD_MECHANICS_KEYS` 中明确追加 `(11, 1107, "field_radius_mult")` 与 `(12, 1207, "damage_mult")` 防回潮死键，在 `KEPT_MECHANICS_KEYS` 中维护保留键。
- [ ] **Task 2.2**: 新建独立 Python 门禁测试 [`tests/python/SkillSpecBatch4GateTest.py`](file:///d:/PRJ/NoMoreDay/tests/python/SkillSpecBatch4GateTest.py)，覆盖 ID 编解码、契约同步校验与等价性断言，同步断言死键缺席。

### Phase 3: 单一事实源治理（清理退役键与混淆属性）
- [ ] **Task 3.1**: 从 [`assets/data/skill_mechanics.json`](file:///d:/PRJ/NoMoreDay/assets/data/skill_mechanics.json) 中彻底删除 8 个已迁移键：
  - `10/1001/crit_chance_per_point`
  - `10/1015/invulnerable_duration_per_point`
  - `11/1101/field_radius_range_per_point`
  - `11/1107/field_radius_mult`
  - `11/1119/duration_per_point`
  - `12/1201/damage_per_point`
  - `12/1207/damage_mult`
  - `12/1219/duration_per_point`
- [ ] **Task 3.2**: 从 [`assets/data/mastery_skill_trees.json`](file:///d:/PRJ/NoMoreDay/assets/data/mastery_skill_trees.json) 中清理 6 处错位的 `stat_modifiers` 为 `[]`（1001, 1101, 1107, 1119, 1201, 1207），**并彻底清空节点 1207 的 `damage_modifiers` 为 `[]`**，消灭多重增伤源头。
  - 前置动作：按 `src/game/foundation/components/Stats.hpp:224-284` 的 `StatType` 逐节点核对被清理条目与节点 `desc_key` 的一致性，并在提交说明中记录核对结论（现登记值语义为 AttackSpeed / ResistLightning / HealthRegen / PoisonDamage / ResistLightning，与节点描述均不符）。若发现某条实为节点描述所承诺的真实收益，须暂停清理并回到设计评审。
  - 明确不动：节点 `10/1015` 的 `stat_modifiers: [{"type": 35, "mode": 1, "value": 20.0}]`（DodgeChance +20%）属于已知遗留（设计 R-04），本批禁止顺手删除。
- [ ] **Task 3.3**: 同步改造既有断言（与 Task 3.1 同一原子提交内完成，否则回归必红）：
  - [`tests/unit/SkillMechanicsRegistryTests.cpp`](file:///d:/PRJ/NoMoreDay/tests/unit/SkillMechanicsRegistryTests.cpp) `:81-85` 移除/改写 `HasNode(10, 1001)` 与 `GetFloat(10, 1001, "crit_chance_per_point", ...)` 断言（注意：删键后节点可能不再存在，`HasNode` 断言也须一并处理）；
  - [`tests/functional/BloodSeaNodes.cpp`](file:///d:/PRJ/NoMoreDay/tests/functional/BloodSeaNodes.cpp) `:128`（1201 `damage_per_point`）、`:140`（1207 `damage_mult`）、`:178`（1219 `duration_per_point`）改为断言 UMR 交付产物或删除该断言（`:180`/`:200` 的 1220/1222 `damage_mult` 为保留键，不得误删）；
  - [`tests/functional/HeavenlySwordDescentNodes.cpp`](file:///d:/PRJ/NoMoreDay/tests/functional/HeavenlySwordDescentNodes.cpp) `:128`（1101 `field_radius_range_per_point`）、`:178`（1119 `duration_per_point`）同上处理；
  - 逐个确认改造后的用例仍表达"迁移后该数值由 UMR 单源提供"这一真实意图，不得简化为删除断言。
- [ ] **Task 3.4**: 修正 [`assets/data/mastery_skill_trees.json`](file:///d:/PRJ/NoMoreDay/assets/data/mastery_skill_trees.json) 中节点 1207 的 `desc_key` 明文，由「血海范围缩小 40%，但伤害总增 (More) 30%。」改为「血海范围缩小 40%，但伤害总增 (More) 10%。」，与 UMR `2012070` 冻结值及 `bottomless_damage_mult = 1.1f` 运行时基准对齐。

### Phase 4: `SkillSpecializationBaker` 基准初始化与分工确权
- [ ] **Task 4.1**: 修改 [`src/game/systems/skill/SkillSpecializationBaker.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSpecializationBaker.cpp) `Bake()` 步骤 1，补齐 `case 10:`, `case 11:`, `case 12:` 显式基准写入，**每例仅写 `out_profile.area_radius` 与 `del.duration` 两行**（见 §2.1）。禁止写入 `del.range` / `del.sub_interval`：本批无对应算子与消费点，写入即为死写。
- [ ] **Task 4.2**: `ApplyNodeModifiersToProfile()` **不新增** `case 10/11/12`：三者继续落入既有 `default: break;` 直接返回。理由：纯数值交付已在步骤 3 由 UMR 自动合成、非 UMR 状态由行为层 SpecState 驱动，显式空分支与 `default` 行为完全等价，且违反同一 switch 内「不留空分支」的既有惯例（如 `case 5` 节点 500 的注释），迁移意图已在步骤 1 的注释中留存。

### Phase 5: 招牌技能行为层消费重构与旧代码物理清理
- [ ] **Task 5.1**: 重构 [`src/game/systems/skill/behaviors/SevenStarSlash.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/SevenStarSlash.cpp)：
  - 引入并调用公共基元 `ResolveBakedProfile`，并加 `profile != nullptr` 守卫；
  - 物理删除旧变量定义与读取：`critChancePerPoint`（声明行 368）、`voidTreadDurationPerPoint`（声明行 380）及其局部乘加；
  - 单源消费（统一用赋值 `=`，不得用 `+=`）：`invulnerableDuration = profile ? profile->delivery.duration : skillData->GetParam("invulnerable_duration", 0.5f);`、`critChanceBonus = profile ? profile->delivery.bonus_crit : 0.0f;`；
  - **不引入 `more_damage_mult` 乘算**：技能 10 无 More 记录，且本文件不存在 `baseDamageMultiplier`（基础伤害变量为 `baseSlashDamage`，见 `:475`）。
- [ ] **Task 5.2**: 重构 [`src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp)：
  - 引入并调用公共基元 `ResolveBakedProfile`，并加 `profile != nullptr` 守卫；
  - 物理删除旧变量定义与读取：`celestial_domain_per_point`（声明行 505）、`sky_piercing_range_mult`（声明行 508）、`enduring_heaven_per_point`（声明行 517）及其局部计算；
  - 单源消费：`field.header.radius` 缩放按 `(skill != nullptr && profile && base_field_radius > 0.0f) ? profile->area_radius / base_field_radius : 1.0f` 计算 `areaMult`；`field.header.duration = (profile && skill != nullptr) ? profile->delivery.duration : (skill ? skill->GetParam("field_duration", kFieldDurationFallback) : kFieldDurationFallback);`（哨兵守卫见 §2.2 通则）；
  - **禁止写 `field.bonus_damage_mult *= profile->more_damage_mult;`**：`HeavenlySwordFieldComponent` 无 `bonus_damage_mult` 成员（`PersistentFieldComponents.hpp:6-21` 仅 `impact_damage_mult` / `field_damage_mult`），该行无法编译；且技能 11 无 More 记录。
- [ ] **Task 5.3**: 重构 [`src/game/systems/skill/behaviors/BloodSea.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/BloodSea.cpp)：
  - 引入并调用公共基元 `ResolveBakedProfile`，并加 `profile != nullptr` 守卫；
  - 物理删除旧变量定义：`damage_per_point`（声明行 313）、`bottomless_damage_mult`（声明行 330）、`duration_per_point`（声明行 372）；
  - **物理删除全部使用点（仅删定义不算完成，会编译失败）**：
    - `:482-483` `field.header.duration += spec.lingeringBloodMistPoints * duration_per_point;`
    - `:484-485` `field.bonus_damage_mult *= 1.0f + spec.pressureTideRisePoints * damage_per_point;`
    - `:544-546` `if (spec.bottomlessPurgatory) { field.bonus_damage_mult *= bottomless_damage_mult; }`
  - 明确保留：`:480-481`（1200 `radius_per_point`）、`:486-488`（1202 `damage_per_point_per_bloodthirst`）、`:540-542`（环形形态 `ring_radius_mult` / `ring_damage_mult`）、`:547-550`（脉冲提频）；
  - 单源消费：`field.header.duration` 基础部分（`(profile && skill != nullptr) ? profile->delivery.duration : (skill ? skill->GetParam("field_duration", kFieldDurationDefault) : kFieldDurationDefault)`）、`field.header.radius` 基础部分（同式，键为 `field_radius` / `kFieldRadiusDefault`）、**`field.bonus_damage_mult *= (profile ? profile->more_damage_mult : 1.0f);` 固定在 `:484` 原位置**（`more_damage_mult` 在任何分支下都是有效数值，哨兵情形下 `1.0f` 与回退基准相同，故无需 `skill` 门；顺序约束见 §2.2 第 3 条与设计 R-05）。

### Phase 6: C++ 单元测试套件实现
- [ ] **Task 6.1**: 新建 [`tests/unit/SkillBatch4DeliveryOpTests.cpp`](file:///d:/PRJ/NoMoreDay/tests/unit/SkillBatch4DeliveryOpTests.cpp)：
  - 专注使用 `ModifierEvaluator::Evaluate` 测试纯算子对技能 10/11/12 各 OpCode 的线性与复合缩放行为；
  - 覆盖单点与多点加点、0 点回落单位元、多条记录乘算复合等行为；
  - 若包含 Baker 综合测试，严格配备 `TestSetupScope` 与 `ReloadModifierRuntimeFromAsset()` 保障测试单例隔离，或将 Baker 综合断言同步写入 [`SkillSpecializationBakerTests.cpp`](file:///d:/PRJ/NoMoreDay/tests/unit/SkillSpecializationBakerTests.cpp)。
- [ ] **Task 6.2**: 将新测试注册入 `CMakeLists.txt`。
- [ ] **Task 6.3**: 在 [`tests/unit/SkillSpecializationBakerTests.cpp`](file:///d:/PRJ/NoMoreDay/tests/unit/SkillSpecializationBakerTests.cpp) 中新增技能 10/11/12 的**迁移等价性快照断言**（零点 / 单点 / 点满三档，浮点用 `doctest::Approx`，并配 `TestSetupScope` + `ReloadModifierRuntimeFromAsset()`）：
  - 技能 10：`area_radius == 96.0f`；`delivery.duration` 0 点 `0.5f`、1015 点满（3 点）`0.59f`；`delivery.bonus_crit == 0.02 * points(1001)`；`more_damage_mult == 1.0f`；
  - 技能 11：0 点 `area_radius == 140.0f` / `delivery.duration == 5.0f`；1101 点满（4 点）`area_radius == 184.8f`；1107 点亮（1 点）`area_radius == 98.0f`；1119 点满（3 点）`delivery.duration == 6.5f`；
  - 技能 12：0 点 `area_radius == 120.0f` / `delivery.duration == 4.8f` / `more_damage_mult == 1.0f`；1201 点满（4 点）`more_damage_mult == 1.24f`；1207 点亮 `more_damage_mult == 1.10f` 且 `area_radius == 72.0f`；1219 点满（3 点）`delivery.duration == 6.6f`。
- [ ] **Task 6.4**: 新增**空档案守卫回归用例**：构造无专精槽 / 无缓存档案的 owner 调用三个行为层 `DoCast`，断言不崩溃且消费到设计约定的回退基准（技能 10 无敌帧 `0.5f`、技能 11 领域半径 `140.0f` 与时长 `5.0f`、技能 12 半径 `120.0f` 与时长 `4.8f`、More `1.0f`）。该用例是设计 R-01 的验收证据。

### Phase 7: 验证闭环与收尾交付
- [ ] **Task 7.1**: 编译工程：`build.bat RelWithDebInfo`，确保 0 errors / 0 warnings。
- [ ] **Task 7.2**: 执行门禁脚本与测试：
  - `python scripts/validate_skill_spec_modifiers.py`（6/6 gates 通过）
  - `python tests/python/SkillSpecBatch4GateTest.py`
  - `python scripts/gen_skill_spec_modifier_contract.py --check`
  - `python scripts/gen_modifier_runtime_v2.py --check`
- [ ] **Task 7.3**: 执行 C++ 测试：先跑 `bin/NoMoreDayTests.exe -tc="*SkillBatch4DeliveryOp*"`，再跑**全量 doctest 回归**（含被改写的 `SkillMechanicsRegistryTests`、`BloodSeaNodes`、`HeavenlySwordDescentNodes`、`SkillSpecializationBakerTests`），最后 `ctest --test-dir build -C RelWithDebInfo -L unit`。禁止仅凭新增过滤用例通过即宣告完成。
- [ ] **Task 7.4**: 确认 `git status` 中不存在除 Task 3.1/3.2/3.4 声明外的数据文件改动（防误删/误改），并还原运行测试过程中被自动更新的根目录 `settings.json`。

---

## 4. 测试方法与验证要求

### 4.1 测试分层与指令

1. **构建验证**：
   ```powershell
   cmd /c build.bat RelWithDebInfo
   ```
   标准：退出码 0，无编译警告，无链接错误。

2. **离线门禁验证**：
   ```powershell
   python scripts/validate_skill_spec_modifiers.py
   python tests/python/SkillSpecBatch4GateTest.py
   python scripts/gen_skill_spec_modifier_contract.py --check
   python scripts/gen_modifier_runtime_v2.py --check
   ```
   标准：全量门禁检查通过，退出码均为 0。

3. **C++ 单元测试回归**：
   ```powershell
   bin/NoMoreDayTests.exe -tc="*SkillBatch4DeliveryOp*"
   ctest --test-dir build -C RelWithDebInfo -L unit
   ```
   标准：所有测试用例 100% 通过，0 failed assertions。

### 4.2 验证任务完成定义（Exit Criteria）

- [ ] **设计收敛**：`*-design.md` 与 `*-plan.md` 齐备，职责边界与数据事实源唯一确定。
- [ ] **构建无误**：`build.bat RelWithDebInfo` 0 错误 0 警告。
- [ ] **数据门禁**：`validate_skill_spec_modifiers.py` 6/6 通过，退役键无残留、无回潮。
- [ ] **迁移等价**：Task 6.3 的技能 10/11/12 零点/单点/点满快照断言全绿；Task 6.4 的空档案守卫用例全绿（无崩溃且回退基准正确）。
- [ ] **既有断言同步**：Task 3.3 涉及的 `SkillMechanicsRegistryTests` / `BloodSeaNodes` / `HeavenlySwordDescentNodes` 全部改造完毕并通过。
- [ ] **文本一致**：节点 1207 `desc_key` 已改为 10%，与 UMR `2012070` 冻结值一致。
- [ ] **单测全绿**：Batch 4 交付算子测试与既有 Batch 1/2/3 测试全绿（全量 doctest + `ctest -L unit`，而非仅跑新增过滤用例）。
- [ ] **环境清洁**：`git status` 仅含声明内的数据/代码改动；`settings.json` 还原，无根目录临时文件遗留。
