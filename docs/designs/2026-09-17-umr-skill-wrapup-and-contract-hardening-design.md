# UMR 技能专精收尾与契约加固设计规范

- **文档状态**：已按首轮独立审查意见（v1.1）与二轮代码实证复审（v1.2，2026-09-17）修订
- **设计日期**：2026-09-17
- **系统代号**：`UMR-SKILL-WRAPUP-HARDENING`
- **输入来源**：
  - 2026-09-17 用户拍板 11 项决策记录（记忆哈希 `40f0450086cd8448ea86b76a337abce6883ea9b71692b22f33c67dd734282e3a`）
  - 首轮架构审查报告：`docs/reviews/2026-09-17-umr-skill-wrapup-and-contract-hardening-review.md`
  - Batch 1~4 实施审查报告（`docs/reviews/2026-09-16-umr-skill-batch1-*` ~ `...-batch4-*`）
  - 技能收尾总账（`docs/plans/2026-09-12-skill1-9-followup-backlog.md`）
  - 策划设计文档（`设计文档/职业设计草案_剑修.md`、`设计文档/职业被动和技能设置.md`）

---

## 1. 背景与目标

### 1.1 背景
在完成 Batch 1（技能 2/3）、Batch 2（技能 4/5/6）、Batch 3（技能 7/8/9）、Batch 4（技能 10/11/12）四批 UMR 交付算子（OpCode 30..42）迁移后，剑修 12 技能的纯交付层数值缩放已全部收敛至统一修饰器管线。
但在多轮对抗性审查中，发现若干结构性潜在缺陷与未收口边界：
1. **哨兵档案守卫失效 (F-05 / F-13)**：`BakedSkillProfile` 的 `area_radius` 默认值为 `1.0f`，导致 `profile->area_radius > 0.0f` 守卫对哨兵档案恒真放行，存在半径严重坍缩隐患。
2. **行为层单源消费脱节**：技能 10 判定半径未消费 `profile->area_radius`；技能 6 节点 603 烘焙的 `del.range` 无行为层读者；技能 7 节点 703 范围缩放未连入渲染指示圈。
3. **节点语义与数据错配（原表述"全局属性污染"经实证修正）**：技能 10 节点 1015 的 `stat_modifiers` 为 `{type: 35 = DodgeChance, mode: 1 = PercentAdd, value: 20.0}`（`mastery_skill_trees.json:578-584`），而该节点 `desc_key` 描述的是"延长无敌帧前后的容错窗口，并降低释放期间 20%/40%/60% 的减速和击退影响"（同文件 `:563`）——修饰符类型与节点文案语义不一致，属误映射。
   **实证附注（推翻 v1.1 的"全局"判断）**：节点 1015 **不在** skill 10 的 `skill_contract.nodes[]` 中；`NodeContractData::scope_policy` 默认 `ScopePolicy::SkillOnly`（`src/game/foundation/data/SkillContract.hpp:64`），而 `StatsSystem.cpp:393` 的 `can_apply_scope(SkillOnly, src)` 要求查询方 `skill_id != 0 && skill_id == src`；且全仓 `StatType::DodgeChance` 从未经 `GetStatWithTags` 查询（仅 `StatsSystem.cpp:271` 基础值、`AttributePipeline.cpp:765-766` 终值、`AffixMapping.hpp:86` 显示名、`PhantomTrance.cpp:182` Buff）。因此该修饰符**极可能不生效**，须由 Task 3.0 证伪门实测确认。另注：`StatsSystem.cpp:487` 按点数缩放，故即便生效，3 点亦为 `+60`（PercentAdd）而非 `+20`。
4. **设计未闭环节点**：技能 9 节点 930（993）协同语义、技能 4 节点 473/474 异常强度标准未正式定稿。
5. **CI 预检防线脱节**：门禁脚本未接入日常构建（~~"既有 9 处 precheck 调用均无失败中止判断"——已于 §2.7 修订：实证显示该 9 处**本就带有** `if errorlevel 1 exit /b 1`，真正的缺口仅是两项门禁未接入构建~~）。测试运行导致 `settings.json` 自动污染。

### 1.2 设计目标
1. **哨兵解析统一收敛（P0）**：在 `BakedSkillProfile` 引入显式烘焙成功标记 `is_baked`，并在 `ResolveBakedProfile` 统一拒绝未烘焙哨兵，使**经该出口解析**的行为层只需处理"空指针"一种失败语义。
   - **范围澄清（实证修正）**：`SkillSystem::GetBakedSkillProfile`（`SkillSystem.cpp:2766-2779`）为缓存直读口，**不参与** `is_baked` 过滤，现有约 30 处直连调用（含 `BeamChannelDeliverySystem.cpp:153`、`PlayerHUD.cpp:144`、`GameUiSnapshotBuilder.cpp:406`、`SkillSystem.cpp:925/1805/2004`）必须保留空指针与数值守卫。本轮不修改该接口语义（避免破坏 `RebakeSkillProfiles` 幂等性与其余消费者），仅登记为已知残余。
2. **行为层单源消费闭环（P0）**：
   - 技能 10 判定半径接入 `profile->area_radius` 作为基准派生，严格保留原有形态分支逻辑。**注**：技能 10 当前无 `SKILL_AREA_MULT` 记录，`area_radius` 恰等于 `GetParam("radius", 96.0f)`，故本项为**零行为变化的纯重构**，其价值在于消除未来引入范围算子时的双源漂移。
   - 技能 6 施法距离将选点落点限制在 `del.range` 范围内（超距外沿向量钳制，含 `Position` 空指针守卫与 mobile-aura 分支保序）。
   - 技能 7 渲染圈与 `BeamChannelDeliverySystem` 的 `maxRange` 由**同一共享函数**产出，杜绝双源漂移。
3. **节点 1015 语义与数据对齐（P0，口径修正；已裁决方案甲）**：先以证伪门确认该修饰符实际作用域，再清空误映射的 `stat_modifiers`；**不新增闪避 Buff**，节点保留 `SKILL_DURATION_FLAT`(`2010150`) 的无敌时长效果。文案所述"降低减速与击退影响"属既有未实现缺口，登记为跟催项 F-01。自检条件为"节点文案、数据、代码在闪避维度上三者一致（均无闪避）"。
4. **未定义机制规范定稿（P1）**：
   - 技能 9 节点 930/993 定稿为“绝影姿态下位移/剑技暴击生成残影斩击”协同机制。
   - 技能 4 节点 473/474 明确挂靠 `AilmentEngine` 统一基准单层强度（1.0f），保留既有安全的抗 NaN 转换逻辑。
   - 天剑降临（技能 11）多元素属性合流（炎阳/霜华）完成完整设计规格说明。
5. **门禁加固与环境整洁（P1）**：门禁接入 `build.bat` prechecks（采用标准 `:run_quiet_step` 过程包装，且须排在 `gen_modifier_runtime_v2.py` 之前）；统一全部 precheck 的失败中止语义；在**测试夹具层**（`TestSetupScope`）对 `settings.json` 做条件保存/还原，使本地与未来 CI 环境均不再被污染。
   - **范围澄清（实证修正）**：仓库当前不存在 `.github` 目录，无 GitHub Actions 环境，故"仅在 CI 下 `git checkout`"属死代码，不采纳。

### 1.3 明确非目标（Non-Goals）
1. **不修改装备范围折叠逻辑**：按用户决策 5，装备 `area_radius_mult` 折叠逻辑保持现状，暂不引入额外的外部折叠。
2. **不开拓其他职业**：按用户决策 10，保持对剑修 12 技能的纵深专注，其余 5 职业维持占位。
3. **不强推行为层行数拆分**：按用户决策 9，废除僵化的 `<=100` 行指标，以职责内聚和无跨系统 hack 为标准。
4. **不对 `BladeWard.cpp` 反击剑数转换引入 `std::clamp`**：现有逻辑已经过抗 NaN 特别加固，不作多余修改以避免引入 UB。

---

## 2. 系统设计与技术规格

### 2.1 哨兵档案生命周期治理 (Sentinel Lifecycle & Resolution)

#### 2.1.1 数据结构扩充
在 [`src/game/foundation/components/SkillDefs.hpp`](file:///d:/PRJ/NoMoreDay/src/game/foundation/components/SkillDefs.hpp) 中为 `BakedSkillProfile` 增加显式状态标志：

```cpp
struct BakedSkillProfile {
  uint32_t skill_id = 0;
  bool is_baked = false; // 显式标记该档案是否由 Baker 完整成功烘焙
  float effective_mana_cost = 0.0f;
  float effective_cooldown = 0.0f;
  // ... 其他既有字段保持不变
};
```

#### 2.1.2 烘焙端置位
在 [`src/game/systems/skill/SkillSpecializationBaker.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSpecializationBaker.cpp) 的 `Bake(...)` 函数出口处：
```cpp
// Bake 方法签名返回类型为 void，向 out_profile 写入结果
// 步骤 4/5 结算完毕后，在函数末尾置位：
out_profile.is_baked = true;
```
当 `skillData == nullptr` 触发提前返回时，`out_profile.is_baked` 保持为默认 `false`。

#### 2.1.3 底座解析统一过滤（保留完整三步走流程）
在 [`src/game/systems/skill/SkillProfileResolve.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillProfileResolve.cpp) 的 `ResolveBakedProfile` 统一把关，严格维持三步走路径：

```cpp
const BakedSkillProfile *ResolveBakedProfile(
    entt::registry &registry, entt::entity owner, uint32_t skillId,
    BakedSkillProfile &scratch,
    const SpecializedSkill *fallbackSpec) {
  // 1) 缓存命中：必须同时满足非空且已成功烘焙
  if (const auto *profile = SkillSystem::GetBakedSkillProfile(registry, owner, skillId)) {
    if (profile->is_baked) {
      return profile;
    }
  }

  // 2) 专属合成专精即时烘焙（技能 9 现场动态烘焙核心依赖）
  if (fallbackSpec != nullptr) {
    SkillSpecializationBaker::Bake(registry, owner, skillId, fallbackSpec, scratch, nullptr);
    return scratch.is_baked ? &scratch : nullptr;
  }

  // 3) 槽位扫描回退即时烘焙
  if (const auto *active = registry.try_get<ActiveSkillsComponent>(owner)) {
    for (const auto &spec : active->specialized_slots) {
      if (spec.skill_id == skillId) {
        SkillSpecializationBaker::Bake(registry, owner, skillId, &spec, scratch, nullptr);
        return scratch.is_baked ? &scratch : nullptr;
      }
    }
  }

  return nullptr;
}
```

**对既有单测的保护**：手写构造 `BakedSkillProfile` 缓存的单元测试（如 `tests/unit/SkillProfileResolveTests.cpp`）需同步显式设置 `is_baked = true`，以准确表达“有效缓存命中”语义。

**收益**：`ResolveBakedProfile` 的消费方（如 `BloodSea.cpp:306`、`HeavenlySwordDescent.cpp:490`、`SevenStarSlash.cpp:368`）可将**哨兵专属**守卫退化为空指针判断，即 `profile ? profile->X : fallback`。

**守卫策略（v1.2 澄清，避免两种错误做法）**：
- **应删除**：哨兵专属的合取项（如 `(profile && skill != nullptr)` 中的 `skill != nullptr`）。
- **应保留**：数值有效性与除零守卫（如 `area_radius > 0.0f`、`base_field_radius > 0.0f`），以及 `SkillSystem::GetBakedSkillProfile` 直连点的全部空指针守卫。
- **不成立**：v1.1 的“彻底卸下行为层防御负担”——`is_baked` 只覆盖 `ResolveBakedProfile` 出口；`GetBakedSkillProfile`（约 30 处直连，含 `BeamChannelDeliverySystem.cpp:153`）不过滤，属已登记残余。

---

### 2.2 行为层单源消费闭环

#### 2.2.1 技能 10（七星斩）判定半径接入与形态分支保护
- **消费点**：[`SevenStarSlash.cpp:417`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/SevenStarSlash.cpp#L417)。
- **规范**：**仅修改** 417 行的 `baseRadius` 数据源获取，将其接入 UMR 档案：
  ```cpp
  const float baseRadius = profile
      ? profile->area_radius
      : (skillData ? skillData->GetParam("radius", 96.0f) : 96.0f);
  ```
  此处不再保留 `area_radius > 0.0f`：`is_baked == true` 已保证该值来自 `Baker` 且被钳制在 `>= 1.0f`（`SkillSpecializationBaker.cpp:51-54`）。
- **等价性声明（实测）**：技能 10 在 `assets/data/modifier_v2/skill_spec_modifiers.json` 中仅有 `2010010`（BonusCrit）与 `2010150`（SKILL_DURATION_FLAT，node 1015），**无 `SKILL_AREA_MULT` 记录**；`Baker` case 10 取 `GetParam("radius", 96.0f)`。因此 `profile->area_radius` 与 `skillData->GetParam("radius", 96.0f)` 恒等，本项为**零行为变化的纯重构**。
- **可证伪性要求**：由于真实管线无法构造差异，验证必须以"人工构造 `is_baked = true`、`area_radius = 200.0f` 的档案直接驱动技能 10"的方式完成，并断言三形态命中半径为 `200 × 0.28 / 0.34 / 0.50 = 56.0f / 68.0f / 100.0f`（该测例在改动前必然失败）。
- **形态分支绝不修改**：456-468 行关于专精形态分支的半径派生保持原样不变：
  - 基础形态：`baseRadius * 0.28f`
  - 极星轨道形态（节点 1021）：`baseRadius * 0.34f`
  - 星落形态（节点 1022）：`baseRadius * 0.50f`

#### 2.2.2 技能 6（剑阵·诛仙）施法距离限制与安全钳制
- **消费点**：[`SwordArray.cpp:125-145`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/SwordArray.cpp) `DoCast`。
- **机制规则**：
  - 基础施法半径常量：与 `SkillSpecializationBaker.cpp` case 6 的 `400.0f` 统一为具名常量，禁止两处字面量各写一份。
  - 烘焙距离：`del.range`（点满节点 603 时为 `400.0f * 1.40f = 560.0f`）。
  - 若鼠标点击目标点与施法者距离超出 `del.range`，将目标点沿施法方向钳制在半径 `del.range` 处。
- **实现约束（v1.2 修正）**：**整体替换** `SwordArray.cpp:119-126`，不得新增 `spawn_pos` 声明（原片段重复声明会编译失败），且 `is_mobile_aura` 必须保持"钳制后覆盖"的次序，使 owner 缺少 `Position` 时行为与现状一致：
  ```cpp
  const bool is_mobile_aura = nodeActive(SwordArrayNodes::MobileAura, specState.mobileAura);
  Vector2 spawn_pos = exec.target_pos;
  if (const auto *ownerPos = registry.try_get<Position>(owner)) {
    const Vector2 casterPos{ownerPos->x, ownerPos->y};
    if (!is_mobile_aura) {
      const Vector2 diff = Vector2Subtract(exec.target_pos, casterPos);
      const float maxRange = (profile && profile->delivery.range > 0.0f)
                                 ? profile->delivery.range
                                 : kSwordArrayBaseCastRange;
      const float distSq = diff.x * diff.x + diff.y * diff.y;
      if (distSq > maxRange * maxRange) {          // 常规路径不做开方
        const float dist = std::sqrt(distSq);
        if (dist > 0.001f) {
          spawn_pos = Vector2Add(casterPos, Vector2Scale(diff, maxRange / dist));
        }
      }
    } else {
      spawn_pos = casterPos;                        // mobile aura 落点恒为施法者
    }
  }
  ```
- **落点单源（v1.3 修正）**：落点解析最终上提至 `DoCast` 开头，产出唯一的 `cast_target`，由**新建剑阵**与**节点 675「重按挪阵」**两条路径共用。旧实现只有新建路径钳制，挪阵分支直接把原始 `exec.target_pos` 写入既有剑阵的 `Position`，构成射程绕过；上提后钳制与 mobile-aura 覆盖对两条路径一致生效。
- **风险与守卫**：`del.range` 由 UMR 节点 603 提供，缺失时回退常量；旧写法未钳制，属行为增强，须由新测例覆盖"超距钳制""owner 无 `Position` 不崩溃""mobile aura 落点仍为施法者""675 挪阵超距钳制"四种情形。
  - 关于 mobile-aura 语义扩展到挪阵路径：随身剑垒逐帧跟随施法者，挪阵后落点本就会被下一帧拉回施法者身边，故运行时净影响可忽略；此处统一是为了契约一致——射程钳制属于该技能的施法距离规则，不应存在不设防的写入路径。

#### 2.2.3 技能 7（心剑·无影）指示器渲染同步（单源共享函数）
- **消费点**：[`GameplayState.cpp:877`](file:///d:/PRJ/NoMoreDay/src/game/application/states/GameplayState.cpp#L877)。
- **规则**：指示圈半径必须与 `BeamChannelDeliverySystem.cpp:209-213` 的 `maxRange` **由同一函数产出**，禁止各自复算（两处独立推导必然漂移）：
  ```cpp
  // 实际落于 src/game/systems/skill/behaviors/BeamChannelShared.hpp，命名空间 NoMoreDay。
  // 形参名避开 skillId，避免被离线 schema 扫描器误解析（见 F-08）。
  [[nodiscard]] inline float ResolveBeamChannelMaxRange(const BakedSkillProfile *profile,
                                                       uint32_t beamSkillId);
  ```
- **取值语义**：`profile->delivery.range`（基础 350.0f 乘以 703 的 `SKILL_RANGE_MULT`）；`profile == nullptr`、哨兵（`!is_baked`）或 `delivery.range <= 0.0f` 时回退 `GetMech(beamSkillId, 0, "base_range", kBeamChannelBaseRangeDefault)`（`kBeamChannelBaseRangeDefault = 350.0f`，定义于同头文件）。
- **实现约束**：渲染路径只允许调用只读的 `SkillSystem::GetBakedSkillProfile`（**禁止** `ResolveBakedProfile`，避免 cache miss 时逐帧烘焙）；遍历 `BeamChannelComponent` 时先判 `owner` 与 `ActiveSkillsComponent` 存在性。

#### 2.2.4 哨兵守卫加固（v1.1 判断已证伪）
- **`BeamChannelDeliverySystem.cpp:211,224`**：v1.1 曾判定 `(profile && profile->area_radius > 1.0f)` 为"历史死分支"并要求删除。**实证推翻**：
  - 同文件 `:153` 使用 `SkillSystem::GetBakedSkillProfile`，返回**可空**且**不参与** `is_baked` 过滤；
  - 哨兵档案 `area_radius == 1.0f`（`SkillSpecializationBaker.cpp:26-31`），故 `> 1.0f` 是**唯一存活守卫**；
  - 删除后将导致空指针解引用，且哨兵路径半径由 60.0f 坍缩至 1.0f。
- **正确处置**：**加固而非删除**——`(profile != nullptr && profile->is_baked && profile->area_radius > 1.0f) ? profile->area_radius : mech.GetFloat(7u, 0u, "base_radius", 60.0f)`，并在代码注释中保留该证伪结论，防止后续再次被当作死代码清理。
- **`BladeWard.cpp:137-144`**：保留现有抗 NaN 保证的安全收敛代码，不作破坏性修改。

---

### 2.3 节点 1015 语义对齐（v1.2 重写）

> **v1.1 表述作废**：原文为"全局闪避污染清理与局部 Buff 化，替换为 `Flat +20.0f`"。该表述在**作用域**、**数值口径**、**语义**三方面均与代码/数据不符，现重写如下。

#### 2.3.1 先证伪，再动手（强制前置门）
以单测断言确认当前实际作用域，禁止以未经验证的"全局污染"为前提施工：
1. 分配 1015 点数的角色，`StatsSystem::GetStatWithTags(reg, caster, StatType::DodgeChance, Tag::None, skill_id = 0, entt::null)` 应与未分配时**完全一致**（证明无全局污染）；
2. 同一查询以 `skill_id = 10` 调用时应存在差异（证明 `SkillOnly` 作用域生效）。
若第 1 条失败，则本节点确为全局生效，须停止本阶段并转入独立缺陷流程。

> **实施期实证修订（2026-09-17）**：第 2 条预期被证伪。节点 1015 的 `StatModifier` 未声明 `required_tags`，JSON 缺省即 `Tag::None`（`Stats.hpp:399-406`），而 `StatsSystem.cpp:318` 的 `is_baked = (mod.required_tags == Tag::None)` 快路径会在**所有作用域**跳过它；`AttributePipeline` 亦不折叠专精节点修饰符。故它是双重惰性修饰符，第 2 条改为"与基准一致"的同向断言。门禁结论仍由第 1 条承担，并新增灵敏度对照（基线 `dodge_chance=0.15` 必须读回 15.0）以排除探针假阴性；第 1 条改为 `REQUIRE` 以匹配"失败即停止"语义。系统性影响面另立跟催项 F-06（同类未声明 `required_tags` 的专精节点修饰符共约 42 条）。

#### 2.3.2 误映射定性
- 数据：`{type: 35 (DodgeChance), mode: 1 (PercentAdd), value: 20.0}`，且 `StatsSystem.cpp:487` 按点数缩放 → 3 点时为 `base × (1 + 0.6)`。
- 文案：`desc_key` 为"延长七星斩无敌帧前后的容错窗口，并降低释放期间 20%/40%/60% 的减速和击退影响"。
- 结论：**修饰符类型与节点文案语义无关**，属误映射；数值 20 与文案 20%/40%/60% 数目巧合一致。
- 附带说明：若采用闪避语义，其生效窗口恰为**无敌帧**，此期间闪避无实际收益——进一步说明"闪避"不是该节点的正确语义。

#### 2.3.3 处置方案（已裁决：方案甲）
**用户于本轮裁决采用方案甲，方案乙否决。**

- **方案甲（已采纳并锁定）**：本次仅将 `stat_modifiers` 置为 `[]`，**不新增闪避 Buff**；把节点字面语义（减速/击退影响降低）登记为跟催项 F-01（需先确认是否存在对应的抗性 `StatType`，当前 `Stats.hpp:224-284` 枚举无此项）。
  自检：节点文案、数据、代码在闪避维度上三者一致（均不含闪避）；节点仍保留 `2010150` 无敌时长增益，**非死节点**。理由：
  1. 节点生效窗口即无敌帧，期间闪避无实际收益；
  2. `desc_key` 与闪避无关，补丁式闪避 Buff 会引入第三种语义；
  3. 旧 `mode: 1`(PercentAdd) 与任何 `Flat` 数值都不等价（`PercentAdd` 对闪避本就无效），只能凭空造数。
  - `BuffIds.hpp`、`SevenStarSlash.cpp` 行为层、`skill_mechanics.json` 均**不修改**（半径单源见 §2.2.1 除外）。
- ~~**方案乙（保留用户决策 4 的局部 Buff 形态）**：~~ **已否决，不再作为备选。** 若后续恢复该需求，须重新走设计流程并附策划强度确认；备选要点原文保留如下以便追溯：
  1. 强度由点数推导——`value = 20.0f * points`（`ModifierMode::Flat`，单位百分点），与 `desc_key` 的 20/40/60 对齐；
  2. 明确登记这是**强度上调**（`PercentAdd base × (1+0.6)` → `Flat +60` 百分点），需策划确认；
  3. 受 `Scaling::DODGE_MAX_CHANCE` 钳制；
  4. 窗口起点早于无敌帧（对应"无敌帧**前**的容错窗口"），并使用 `ActiveEffectsComponent::AddOrRefresh`（`Buff.hpp:192`）实现"刷新而非叠层"，沿用 `GrantSwordStepMirage`（`SevenStarSlash.cpp:265-283`）既有模式；
  5. 若 Buff 方案落地，`BuffIds.hpp` 追加 `SevenStarVoidTread` 与 `"seven_star_void_tread"`（追加在 `Count` 之前，不改既有字面量）。

#### 2.3.4 文案同步
方案甲下 `desc_key` **本轮不修改**：清理后文案与数据在闪避维度上不再背离；文案中尚未实现的"降低减速与击退影响"属既有缺口，随 F-01 定稿时与实现一并处理。必须复核 `tests/unit/GeneratedSpecStateTests.cpp:157`、`tests/unit/SkillBatch4DeliveryOpTests.cpp:121-131`、`tests/functional/SevenStarSlashNodes.cpp:127`、`tests/python/SkillSpecBatch4GateTest.py:24,37` 的既有断言。

---

### 2.4 技能 9 节点 930/993 协同机制规范

- **节点定位**：`Synergy` 节点。
- **协同机制规范（用户决策 6-A）**：
  - **触发条件**：处于“绝影姿态（Phantom Trance）”持续期间。
  - **交互源**：施法者施放任何带有 `[Movement]` 标签的剑技（如技能 1 流云刺）或剑气技能（技能 2 裂空斩）并造成**暴击**时。
  - **效果**：在暴击发生位置立刻生成一道虚影残像，同步执行一次无消耗的剑气斩击，造成相当于该次攻击 40% 的独立物理伤害。
  - **内置冷却 (ICD)**：`0.5s`。

---

### 2.5 技能 4 节点 473/474 异常强度规范

- **用户决策 7-B**：节点 473（感电反击）与节点 474（冰冻反击）反击发射时，施加的感电与冰缓状态强度恒定为标准 1 层基础强度（`magnitude = 1.0f`，对应 `AilmentEngine` 的基准状态）。
- 行为层中移除任何潜在的点数强度缩放预期，在代码与 GDD 注释中固定此设计语义。

---

### 2.6 天剑降临（技能 11）属性合流设计规格 (Design Specification)

为天剑降临后续的三系元素合流确立标准数据契约与架构规范（本阶段仅落地设计与数据规范，代码实装留待后续独立深化包）：

```text
┌─────────────────────────────────────────────────────────────────────────┐
│                        天剑降临 (Heavenly Sword Descent)                 │
└────────────────────────────────────┬────────────────────────────────────┘
                                     │ 专精转质节点选择 (互斥)
        ┌────────────────────────────┼────────────────────────────┐
        ▼                            ▼                            ▼
  【基础/雷霆天剑】             【炎阳天剑 (Fire)】          【霜华天剑 (Cold)】
  - 标签: [Lightning, Area]     - 标签: [Fire, Area]         - 标签: [Cold, Area]
  - 核心机制: 高频神雷轰击      - 核心机制: 熔岩地火海       - 核心机制: 绝对零度极寒
  - 区域附着: 0.5s 脉冲感电     - 区域附着: 持续点燃 Burn DoT- 区域附着: 持续极寒冰缓+冻结
  - 伤害折算: 物理→闪电 100%    - 伤害折算: 物理→火焰 100%   - 伤害折算: 物理→冰霜 100%
```

1. **转质契约**：
   - 节点 1121（炎阳天剑）：`max_transmuters = 1`，赋予 `Tag::Fire`，移除 `Tag::Physical`。
   - 节点 1122（霜华天剑）：`max_transmuters = 1`，赋予 `Tag::Cold`，移除 `Tag::Physical`。
2. **场域派生契约**：
   - `PersistentFieldComponent` 支持 `element_tag` 属性分支。雷霆生成连环电弧，炎阳生成熔岩地火（带地面火焰粒子并施加 Burn），霜华生成暴风雪（带冰晶粒子并高频累积 Freeze 计数）。

---

### 2.7 门禁与环境卫生加固（v1.2 修正）

1. **Prechecks 接入与次序**：
   在 [`build.bat`](file:///d:/PRJ/NoMoreDay/build.bat) 预检段接入，且**必须排在 `gen_modifier_runtime_v2.py`(`:313`) 之前**——否则坏数据会先被烘焙进生成物，门禁失去意义：
   ```bat
   call :run_quiet_step "Validating skill spec modifiers" "Skill spec modifier validation failed! Aborting." "python scripts\validate_skill_spec_modifiers.py --check"
   call :run_quiet_step "Checking skill mechanics schema" "Skill mechanics schema check failed! Aborting." "python scripts\gen_skill_mechanics_schema.py --check"
   ```
   - **保留 `if errorlevel 1 exit /b 1`（实施期修正）**：v1.2 原拟删除该判断，理由是 `:run_quiet_step`(`build.bat:704-720`) 内部已是 `endlocal & exit /b %STEP_EXIT%`。实证显示 `:288-313` 既有 **9 处** precheck 调用**本就带有**该判断，删除反而制造不一致，故本轮保留，新增两项沿用同一模式（现共 11 处一致）。该判断确为冗余防御性代码，属可选清理项。
   - **基线证据**：两者在 HEAD 上已验证 EXIT=0（前者输出 `skill_spec modifier offline gates passed: 6/6 passed`；后者输出 `skill_mechanics_schema.json up to date (437 entries, 62 unreferenced)`）。注：前者实际执行 7 项检查，汇总仅计入 6 项（`dead_keys_absent` 未计入），文案沿用脚本自报口径即可。
2. **统一 precheck 中止语义（实施期修正：原前提不成立）**：
   v1.2 原文称 `:288-313` 的 9 处 precheck "均无失败中止判断"，**经实证证伪**——它们均已带 `if errorlevel 1 exit /b 1`。故本轮无缺口需补齐，实际动作仅为让新增 2 项沿用同一模式；"故意破坏数据 → 构建中止"的负向验证证据已补齐：篡改 canonical `param_f32` 为 `1.5` 后运行 `build.bat`，门禁报 `5/6 passed` 并打印 `Skill spec modifier validation failed! Aborting.`，进程退出码为 **1**，日志止于预检段（未进入 CMake 配置）；数据随后还原、哈希一致。
3. **测试环境防污染（改为夹具级，与 CI 无关）**：
   - **v1.1 的 `if defined GITHUB_ACTIONS (git checkout -- settings.json >nul 2>&1)` 不采纳**：仓库无 `.github` 目录，该判断为死代码；且构建脚本不应产生仓库写副作用。
   - 实际污染源：测试内 `qualityManager.Initialize("settings.json")` 会写回 `benchmarkScore`/`updatedAtUtc`（`tests/integration/MaterialLightingIntegrationTest.cpp:49,78` 等）。**写入面计数（复检修订）**：测试侧显式字面量调用 **7 处 / 5 文件**（`MaterialLightingBenchmark.cpp:119`、`GPUABIBindingTierIntegrationTest.cpp:23`、`RenderSystemPhaseDToggleSmokeTest.cpp:63`、`MaterialLightingIntegrationTest.cpp:55`+`:86`、`VFXSequencerTest.cpp:152`+`:464`）+ **无参默认实参路径 1 处**（`JFAPassUpsampleMaskTest.cpp:217`；`QualityTierManager.hpp:100` 默认实参即仓库 `settings.json`，无参调用同样写回）+ **生产路径间接触达 1 例**（`RenderSystemInitializeFailureTest.cpp` 两例经 `RenderSystem::Initialize()` → `RenderSystem.cpp:969`，且写入发生在能力门禁之前）。本轮已将这 7 个文件全部接入 `TestSetupScope`。
   - **正确做法**：在 `tests/TestCommon.hpp` 的 `TestSetupScope` 构造函数中（若 `settings.json` 存在）读入原有字节，析构时**仅在内容被修改时**写回。该方案本地与未来 CI 一致有效，且不影响开发者既有配置。
4. **测试夹具单例防线**：
   - 不在 `TestSetupScope` 析构中重复读盘重载 `ReloadModifierRuntimeFromAsset()`，仅在注入了合成 blob 的特化测试用例中按需重载真实资产，保持全量单测性能与用例隔离。

---

## 3. 验收标准与验证方案

1. **构建与门禁**：
   - `build.bat RelWithDebInfo` 退出码 0，零编译警告（以既有基线日志比对新增 `warning` 计数为 0）。
   - `python scripts\validate_skill_spec_modifiers.py --check` 退出码 0（脚本自报 `6/6 passed`）。
   - `python scripts\gen_skill_mechanics_schema.py --check` 退出码 0。
   - 门禁失败必须中止构建（以"故意破坏数据 → 构建中断"为证据）。
2. **单元测试与回归**：
   - 哨兵拦截：经 `SkillSystem::RebakeSkillProfiles`（`skillData` 缺失）写入缓存哨兵后，`ResolveBakedProfile` 严格返回 `nullptr`；改动前该断言应失败。
   - 幂等：`RebakeSkillProfiles` 连续两次调用，第二次因 `operator==`（新增字段一并参与）不写入。
   - 既有 `SkillProfileResolveTests.cpp`、`SkillBatch4NullProfileFallbackTests.cpp`、`SkillBatch4BloodSeaOrderingTests.cpp` 全绿。
   - 技能 10 半径单源：以人工构造 `is_baked = true, area_radius = 200.0f` 的档案驱动，断言基础/极星/星落形态命中半径为 `56.0f / 68.0f / 100.0f`（改动前必然失败，构成可证伪证据）。
   - 技能 6 钳制：超距选点被限制在 `del.range` 圆周；owner 无 `Position` 不崩溃；mobile aura 落点仍为施法者。
   - 共享函数 `ResolveBeamChannelMaxRange` 三分支（已烘焙 / 哨兵 / 空指针）。
   - 节点 1015：作用域双断言（`skill_id = 0` 不变化、`skill_id = 10` 按实证修订为**同样不变化**，并附灵敏度对照 `dodge_chance=0.15 → 读回 15.0`）。
   - `ctest --test-dir build -C RelWithDebInfo -L unit` 与 `bin\NoMoreDayTests.exe` 100% 通过。
3. **环境卫生**：
   - 运行全量测试后 `git status` 中 `settings.json` 无改动。
   - **覆盖门限（必须知晓）**：`TestSetupScope` 是**进程级**且**仅保护显式声明作用域的用例**。同一进程内未被覆盖的写入点不会被还原，其写入还可能被后续某个作用域在构造时当作"原始内容"快照固化，从而使局部修复在测试顺序变化时失效。因此写入点必须逐一穷举接入，口径为 (a) 显式字面量 `Initialize("settings.json")` /(b) 无参默认实参调用 /(c) 经 `RenderSystem::Initialize` 等入口的间接调用（计数见 §2.7）。
   - **同进程比对（复检强化）**：函数集必须在**同一进程**内跑完后比对 `settings.json` 内容哈希不变（仅 mtime 变化可接受）。可复现命令：
     ```powershell
     $h = (Get-FileHash settings.json).Hash
     bin\NoMoreDayTests.exe --test-case="*VFXSequencer*,*JFAPass*,*GPU ABI*,*Material Lighting*,*RenderSystem*"
     (Get-FileHash settings.json).Hash -eq $h
     ```
