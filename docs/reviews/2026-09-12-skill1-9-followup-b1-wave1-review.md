# 复审报告：技能1~9 收尾 B1 第一波

- 日期：2026-09-12
- 审查者：C++ 复审子代理
- 依据流程：`docs/workflows/review.md`

## 审查目标

对「技能1~9 收尾 B1 第一波」实施包做最终复审，确认：

1. 计划 `docs/plans/2026-09-12-skill1-9-followup-B1-wave1-plan.md` 中 B1-08 ~ B1-18 的实现是否与设计/计划/代码规范一致。
2. 硬否决项是否成立：热路径字符串比较是否彻底移除、`BuffKind` 扩展是否破坏既有索引/序列化/伤害条件、`EffectSystem` 唯一 owner 迁移的帧序后果、测试是否为真实语义推导且未削弱覆盖、是否重复造轮子或越界改契约。
3. 未验证项与剩余风险是否被如实登记。

## 结论

**修改**

整体实现方向正确、边界基本受控，T1.4 唯一 owner 迁移、T2.3 `BuffKind` 热路径替换、T3.x 测试补强均达到计划要求，构建与既有专项证据齐全。但存在 1 项未在计划中声明的核心玩法行为变更（发现项 F1），必须先处置；另有 1 项旧存档 `kind` 迁移缺口（F2）与 1 项帧序 1 帧延迟（F3）需明确修复或登记接受。上述处置完成并通过回归后，可转为 `提交`。

> 后续轮次见文末。第 2 轮曾因 F7 判 `修改`；第 3 轮 F7 已闭环，**最终结论：`提交`**（F3/F4/F5 与 B1-21/22/23 等列为剩余风险）。

## 审查轮次

第 1 轮。

## 输入

- 流程/规则：`docs/workflows/review.md`、`conductor/code_standard.md`、`conductor/tech-stack.md`
- 计划：`docs/plans/2026-09-12-skill1-9-followup-B1-wave1-plan.md`
- 销项：`docs/plans/2026-09-12-skill1-9-followup-backlog.md`（§2.1/§2.2/§8）
- 变更差异：`git status --short`、`git diff`（18 个修改文件，+294/-35）
- 代码走查：`StatsSystem.cpp`、`EffectSystem.cpp`、`Buff.hpp`、`DamageConditions.hpp/.cpp`、`AilmentEngine.cpp`、`HazardSystem.cpp`、`AreaFieldDeliverySystem.cpp`、`FlowingThrust.cpp`、`InfiniteBlades.cpp`、`SwordArray.cpp`、`HeavenlySwordDescent.cpp`、`GameplayState.cpp`
- 测试走查：`DamagePipelineP1Tests.cpp`、`SkillBehaviorGuardTests.cpp`、`SkillCastConstraintServiceTests.cpp`、`SkillContractRegistryTests.cpp`、`MindBladeNodes.cpp`
- 交付证据（由实施方提供，本复审未重复跑全量）：`build.bat` RelWithDebInfo 成功；`ctest -L ci` = 100%（1/1）；`ctest -L skill` = 2/2；专项用例全过；`python scripts/gen_skill_contracts.py --check --check-idempotency --check-determinism` = PASS

## 变更文件边界

`git diff --stat` 与任务给定清单完全一致，无越界文件：

- `assets/data/skills.json`
- `docs/reviews/2026-09-08-skill2-rending-wave-specialization-review.md`
- `src/game/contracts/impl/StatsSystem.cpp`
- `src/game/foundation/components/Buff.hpp`
- `src/game/systems/combat/AilmentEngine.cpp`
- `src/game/systems/combat/EffectSystem.cpp`
- `src/game/systems/combat/HazardSystem.cpp`
- `src/game/systems/combat/damage/DamageConditions.hpp`
- `src/game/systems/skill/AreaFieldDeliverySystem.cpp`
- `src/game/systems/skill/behaviors/FlowingThrust.cpp`
- `src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp`
- `src/game/systems/skill/behaviors/InfiniteBlades.cpp`
- `src/game/systems/skill/behaviors/SwordArray.cpp`
- `tests/functional/MindBladeNodes.cpp`
- `tests/integration/SkillContractRegistryTests.cpp`
- `tests/unit/DamagePipelineP1Tests.cpp`
- `tests/unit/SkillBehaviorGuardTests.cpp`
- `tests/unit/SkillCastConstraintServiceTests.cpp`

新增未跟踪文档（本包预期产物）：`docs/plans/2026-09-12-skill1-9-followup-B1-wave1-plan.md`、`docs/plans/2026-09-12-skill1-9-followup-backlog.md`、`docs/reviews/2026-09-12-n10-generator-side-effect-investigation.md`。未发现根目录临时文件，符合 `AGENTS.md`。

## 范围对齐

| 事项 | 计划/销项 | 实现 | 对齐 |
| --- | --- | --- | --- |
| B1-08 990 冰增幅口径 | T1.1~T1.3 + 生产链路守卫 | `DamagePipelineP1Tests.cpp:254` 新增单目标 vs `ResolveDamageBatch` 一致性用例 | ✅ |
| B1-09 唯一 owner | T1.4/T1.5/T1.6 | `StatsSystem.cpp:543` 仅保留视觉；`EffectSystem.cpp:74-88` 承担衰减 + `StatsDirty`；`SkillBehaviorGuardTests.cpp:1817` 锁定单次衰减 | ✅（帧序见 F3） |
| B1-10 sword_skill | T2.1 | `skills.json` 3 处 `sword_skill`→`SwordSkill` | ✅ |
| B1-11 GetByKind | T2.3/T2.4/T2.6 | `FlowingThrust.cpp:381-385` 改整型查找；`BuffKind` 追加 4..8、`kBuffKindCount=9`；各创建点补 `.kind` | ✅（spread id 见 F1；存档见 F2） |
| B1-12 N10 调查 | T2.6 | 新增 `2026-09-12-n10-generator-side-effect-investigation.md`，结论「一致」 | ✅ |
| B1-13 转质守卫 | T3.1 | `SkillCastConstraintServiceTests.cpp:57/:81` 新增技能3 与 4~7/9 | ✅ |
| B1-14 契约/批量 | T3.2/T3.3 | `SkillContractRegistryTests.cpp` Keystone 精确集合 + 计数；P1 批量一致性 | ✅ |
| B1-16 MindBlade | T3.4 | `MindBladeNodes.cpp:831` 新增 772×730 Lightning 压制 | ✅（与既有 :796 Cold 不同口径，非重复） |
| B1-17 非转质 Keystone | T3.5 | `SkillBehaviorGuardTests.cpp:323` 新增 SUBCASE | ✅ |
| B1-18 crit_chance 口径 | 文档 | `2026-09-08-...review.md` 备注修正 | ✅ |
| B1-21/22/23 运行验证 | 阻塞 | 无交互环境，未登记/未验证 | ⚠ 见剩余风险 |

未发现新建设计范围或触碰他人模块权限/状态/归属边界。

## 质量与风险评估

### 硬否决项核查

1. **热路径字符串比较（§7.2）——通过。** `FlowingThrust.cpp` 已无 `b.id.find("Bleed"/"Ignite"/"Cold"/"Chill"/"Slow")`；`rg "find\("` 命中的仅为 `unordered_map::find` 与 `std::find`（实体/点数容器），非字符串热路径分支。`GetByKind` 为既有 API（`Buff.hpp:272`，此前已被 `SkillSystem.cpp:862`、`InfiniteBlades.cpp:312` 使用），本波在元素异常上复用，无重复造轮子。
2. **`BuffKind` 扩展安全性——基本通过，存档存在缺口。** 枚举仅在末尾追加 `Bleed=4..Slow=8`，`None=0/QiBrand=1/FateMark=2/FreeCast=3` 数值未变（`Buff.hpp:59-72`）；`kBuffKindCount` 4→9 与枚举一致（`DamageConditions.hpp:52-53`），`TargetConditionState::stacks[]` 由 `{0,0,0,0}` 改为 `{}` 全零初始化（`DamageConditions.hpp:57`）；全仓检索确认 `kBuffKindCount`/`stacks` 无其它按 kind 硬编码 4 的数组，FateMark/QiBrand 条件位与 `StackCountFor` 越界保护未受影响。唯一缺口是旧存档缺 `kind` 时无法通过刷新升级（F2）。
3. **`EffectSystem` 唯一 owner 与帧序——通过但存在 1 帧延迟。** `StatsSystem::UpdateBuffs` 已删除 `effects.Update(...)` 与 `swordStepDrainMult` 块（`StatsSystem.cpp:543-604` 仅剩粒子视觉）；`EffectSystem.cpp:74-88` 复刻绝影形态倍率并以 size 变化标记 `StatsDirty`，正确反映唯一 owner。`GameplayState.cpp` 中 `UpdateBuffs:356 → StatsSystem::update:357 → SkillSystem:370 → CombatSystem:706 → EffectSystem:717`，`EffectSystem` 之后本帧无属性消费者，故非「同帧更早消费」缺陷；但到期撤销由旧代码的 :356（战斗前）变为 :717（战斗后），属性重算落到下一帧 :357，产生 1 帧延迟（F3）。
4. **T1.5 期望值——真实语义推导，未削弱覆盖。** `git diff` 显示该处为**纯新增**断言（无删除）：旧实现同帧双重衰减 0.25−0.21−0.21<0；唯一 owner 修复后应 0.25−0.21=0.04，断言即锁定「本帧只扣减一次」。新增 SUBCASE（:323）与 T1.5 断言均为追加，未改动既有断言。
5. **测试真实性——通过。** 变更测试文件中无 `REQUIRE(true)`/`CHECK(true)`/`DISABLED`/`#if 0`/注释旁路。T3.4（:831，772 Lightning×730）与既有 :558、:796（770 Cold×730）在提供元素与转质路径上不同，属补齐组合，非重复。
6. **枚举/字段越界——通过。** `BuffKind` 为追加；`BuffEffect.kind` 字段本就存在；新增代码均为热路径整型查找，未改契约结构。

### 代码规范符合性

- 无新增 raw `new/delete`、无 UB/UAF/悬垂；`GetByKind` 返回指针仅在 `ActiveEffectsComponent` 未被修改的作用域内使用，`FlowingThrust` 用法安全。
- 注释为中文且解释语义（如唯一 owner、单次衰减），未夹带任务执行过程，符合 `AGENTS.md`。
- 无新增热路径堆分配/字符串临时对象。

## 发现项

按严重度排序。

### High

**F1｜未声明的核心玩法行为变更：175 余韵传染减速 id 由 `FrostSlow` 改为 `spreadSlow`**

- 位置：`src/game/systems/skill/behaviors/FlowingThrust.cpp:740-741`
- 观察：`git diff` 明确显示该创建点由 `.id = "FrostSlow", .name = "Frost Slow"` 改为 `.id = "spreadSlow", .name = "Spread Slow"`。172 主减速仍为 `FrostSlow`（:538）。改前，余韵传染若落到一个已被 172 直击、身上有 `FrostSlow` 的敌人时，`AddOrRefresh` 按 id 命中并**刷新同一减速**；改后两者成为独立 buff，两个 `StatType::MoveSpeed / ModifierMode::PercentAdd` 修饰符同时生效，减速强于单一 -30%。
- 违规：计划 B1-11（T2.3/T2.4）的定位是「把热路径 id 字符串子串匹配替换为 `BuffKind` 整型查找」，属**行为保持**重构，计划与 `docs/plans/...-backlog.md` 均未授权修改 175 的 buff 身份语义，也无设计文档或测试覆盖该独立叠加语义。触发 `review.md` 的「未声明行为变更 / 范围越界 ⇒ 修改」。
- 修复建议（二选一，均需在本波内完成）：
  1. 回退为 `.id = "FrostSlow", .name = "Frost Slow"`，保持与改前一致的刷新语义；或
  2. 若确需「独立减速」语义，则登记为明确设计决策（补 `设计文档/` 说明或计划变更记录），并在 `SkillBehaviorGuardTests.cpp` / `SkillSpecializationBakerTests.cpp` 增加「172 直击 + 175 传染命中同一目标 → 独立 buff 且位移减速符合预期」的回归用例，同时补充序列化 id 变更说明。

### Medium

**F2｜旧存档 `kind` 无法经 `AddOrRefresh` 升级，`GetByKind` 热路径会静默漏检**

- 位置：`src/game/foundation/components/Buff.hpp:185-211`（刷新字段未含 `kind`/`type`；`from_json` :168-169 旧存档默认 `kind=None`）
- 观察：旧存档中的 `FrostSlow`/`frozen_chill` 等 buff 读入后 `kind=None`。之后命中走 `AddOrRefresh` 只刷新 duration/metadata/`resist_cap_*`/stacks，**不更新 `kind`**，因此这些 buff 在持续被刷新期间永远保持 `None`，`FlowingThrust.cpp:381-385` 的 `GetByKind(Chill/Freeze/Slow/Bleed/Ignite)` 会漏检，节点 170/173/175 的衍生判定对旧存档目标不触发。与之相对，`AilmentEngine.cpp:699-702` 的原地刷新注释明确承诺「upgrades old saves to the fast path on their next refresh」，直接 `BuffEffect` 路径与之一致性缺失。
- 违规：计划 B1-11 要求确认存档兼容；`Buff.hpp:64-66` 注释亦以「保持既有已存档 kind 数值稳定」为约束。当前实现对旧存档存在静默降级。
- 修复建议：在 `AddOrRefresh` 命中分支同步 `effect.kind = new_effect.kind;`（并建议顺手同步 `effect.type = new_effect.type;`，避免同类字段漂移）；或在 `from_json` 中按 `id`/`type` 推断 `kind` 兜底。补充「旧存档无 kind → 命中刷新后 kind 升级」的单测。

**F3｜`EffectSystem` 唯一 owner 迁移引入到期撤销 1 帧延迟**

- 位置：`src/game/systems/combat/EffectSystem.cpp:82-88`，配合 `src/game/application/states/GameplayState.cpp:356-357,706,717`
- 观察：到期删除由旧代码 `StatsSystem::UpdateBuffs`（:356，战斗前）移到 `EffectSystem::update`（:717，战斗后）。`StatsDirty` 在 :717 标记，属性重算最早在下一帧 `StatsSystem::update`（:357）。结果是「本帧到期」的 buff/debuff 修饰符仍参与本帧战斗（:706）一次，之后才被移除并重算；旧实现下该修饰符在本帧战斗前已失效。
- 违规/影响：这是计划 B1-09 明确要求核对的帧序副作用。已确认 `EffectSystem` 之后本帧无属性消费者，故不构成悬垂/同帧错读缺陷；但属可观测的玩法偏差（增益/减益多持续约 1 帧，约 16ms@60fps），需明确处置而非默默接受。
- 修复建议：二选一并在报告中留痕——(a) 接受该 1 帧偏差，在残留风险/计划中登记（当前任务正是要求登记）；(b) 若要求同帧严格到期，将 `ActiveEffects` 生命周期衰减（或其 `StatsDirty` 重算触发）前移至战斗前，或在 `EffectSystem::update` 后对受影响实体补跑一次 `StatsSystem::update`。建议优先 (a)，避免破坏唯一 owner 结构。

### Low

**F4｜残留热路径字符串比较（既有文件，未在本波边界内）**

- 位置：`src/game/systems/combat/damage/DamageConditions.cpp:19`（`IsControllingEffect` 仍 `effect.id.find("Slow")`）；`src/game/systems/skill/behaviors/BladeFormation.cpp:483`（`b.id.find("ignite")`）
- 观察：均为既有实现，不在本波变更文件清单内，故不判为本波阻塞。但本波已为 `Slow/Chill/Freeze/Ignite` 建立 `BuffKind`，这两处恰好可直接迁移。
- 修复建议：登记为后续销项（如沿用 B1-11 思路），分别用 `kind`/`GetByKind` 替代字符串子串比较。

**F5｜测试内共享树定义被 `const_cast` 原地修改，缺少 RAII 恢复**

- 位置：`tests/unit/SkillBehaviorGuardTests.cpp:334-355`
- 观察：`const_cast<SkillTreeDefinition*>` 修改 `213/214` 的 `add_tags` 后手工恢复。若中途断言 abort，恢复被跳过，污染同一进程内后续 SUBCASE。与既有 :358 用例同风格，属测试代码卫生问题，不影响生产。
- 修复建议：用 RAII guard（析构恢复）或在 SUBCASE 内深拷贝树副本再修改。

## 最佳实践建议

**F6｜`kBuffKindCount` 与 `BuffKind` 的同步缺少编译期保护**

- 位置：`src/game/systems/combat/damage/DamageConditions.hpp:52-53`
- 建议：在枚举末尾加哨兵 `Count`（`BuffKind::Count`），并令 `constexpr uint32_t kBuffKindCount = static_cast<uint32_t>(BuffKind::Count);`；或加 `static_assert(static_cast<uint32_t>(BuffKind::Slow) + 1 == kBuffKindCount, "...")`。当前仅靠注释约束，未来追加枚举值若忘记同步会静默越界/丢弃叠层，是本波硬核查 #2 关注的核心风险点，应以编译期断言固化。

其余建议：

- `EffectSystem.cpp:86` 的 `if (size != before)` 只覆盖「到期删除」，建议补一行注释说明「叠层/修饰符变更由各施加点自行 `StatsDirty`」，避免读者误以为此处覆盖全部集合变化（当前语义正确，仅可读性）。
- `AilmentEngine.cpp:225-226` 将 `AilmentType::Slow` 映射为 `BuffKind::Slow`，但 `Slow` 无注册契约（`FlowingThrust.cpp:533` 注释亦确认），该分支当前为不可达的向前兼容代码；建议注释标注，或与设计确认 172 减速是否应正式注册 `Slow` 契约。

## 剩余风险

1. **B1-21/22/23 未验证**：无交互运行环境，运行时行为（Frost amp 实机表现、唯一 owner 实机、元素异常衍生）未做端到端验证，仅静态走查 + 单测覆盖。必须在计划/销项中登记为阻塞项，不得在复审中记为「已通过」。
2. **F1 未决期间**：技能1 175 传染减速的实际强度口径与设计是否存在偏差无法从测试判定。
3. **F2 旧存档兼容**：若线上存在旧版本存档，受影响的直击类减速/异常在刷新期间存在静默漏检窗口。
4. **F3 1 帧延迟**：若后续将战斗结算前移或新增 `EffectSystem` 之后的属性消费者，该偏差可能被放大，需回归。
5. **构建基础设施**：主程序首次并行链接出现瞬时 `LNK1136`，单独重链成功；不排除并行构建竞态，建议在收尾时记录并观察是否复现。
6. **告警**：`C4834/C4002` 位于本波未触及的既有文件（`PhantomTrance.cpp:257`、`SkillTreeController.cpp`、`Game.cpp:259`、`DamageElementIndexTests.cpp:92`、`DamagePipelineP3bTests.cpp:269`、`SwordArrayNodes.cpp:40`），不归本波，但计划「0 告警」退出条件需区分「本波新增为 0」与「全量历史告警仍然存在」。

## 下一步动作

1. （必须）处置 F1：回退 `spreadSlow` 改名，或补设计决策 + 回归测试；完成后重跑 `ctest -L ci`、`ctest -L skill` 与相关专项。
2. （必须）处置 F2：`AddOrRefresh` 同步 `kind`（建议连带 `type`）或 `from_json` 推断，并加单测。
3. （必须）F3 明确选择：接受并登记残留风险，或调整更新时序。
4. （建议）F6 加编译期同步断言；F4 登记后续销项。
5. 将 B1-21/22/23 以「阻塞/未验证」状态写入计划与销项；确认后由审查者进行第 2 轮，若上述项闭环则改判 `提交`。

---

## 跟进审查（第 2 轮）

- 日期：2026-09-12
- 触发：第 1 轮结论 `修改`，实施方就 F1/F2/F6 修复后提请复查
- 复查方式：直接检视修复后代码 + `git diff` 边界核对（未修改任何源码）

### 本轮变更内容（`git diff` 核对）

- `src/game/systems/skill/behaviors/FlowingThrust.cpp:739-746`：175 余韵传染减速 `id`/`name` 回退为 `"FrostSlow"` / `"Frost Slow"`，并保留 `.kind = BuffKind::Slow`；原 `.type = BuffType::SpeedDown` 一行被 `.kind` **替换**（未同时保留）。
- `src/game/foundation/components/Buff.hpp:196-199`：`AddOrRefresh` 命中同 id 时新增 `effect.type = new_effect.type; effect.kind = new_effect.kind;`（含中文注释）。
- `src/game/systems/combat/damage/DamageConditions.hpp:55-58`：新增 `static_assert(static_cast<uint32_t>(BuffKind::Slow) + 1 == kBuffKindCount, ...)`。
- 变更边界仍为原 18 个修改文件，无新增越界文件。

### 复查范围与证据

- `FlowingThrust.cpp:733-754`：确认 id 已回退、kind 保留；同时发现 `.type` 缺失（见 F7）。
- `Buff.hpp:185-213`：确认 type/kind 同步逻辑；据此逐一核查同 id 多创建点，确认 `QiBrand`（`RendingWave.cpp:492-510`）、`FateMark`（`InfiniteBlades.cpp:295-306`）、`FreeCast`（`BoomerangDeliverySystem.cpp:229-237`）、`Frozen`/`Chill`/`Ignite`（`InfiniteBlades.cpp:471-551`、`FlowingThrust.cpp:562-590`）、`Root`（`MonsterAffixSystem.hpp:979-988`）、`Stun`（`ElementPathSystem.cpp:115-126`）均在每次刷新时显式带上一致的 `type`/`kind`，未受 type/kind 同步影响；唯一不一致点为 `FrostSlow`（F7）。
- `DamageConditions.hpp`：确认 `kBuffKindCount=9`、`{}` 零初始化、`static_assert` 编译期保护成立。
- 交付证据（实施方提供）：`build.bat` = 0；`ctest -L ci` = 100%（1/1，27.60s）；专项 `[Unit] FlowingThrust - 175 Residual Elements spread respects 1s ICD`(7/7)、`[Unit] BuffSystem - Duration and Stacking`(8/8)、`[Unit] DamagePipeline P1 - Frost amp...`(7/7)、`[Unit] SkillBehaviorGuard - Transmuter mutex and scope policy`(44/44) 全过。

### 上轮发现项处置

| 上轮项 | 严重度 | 状态 | 说明 |
| --- | --- | --- | --- |
| F1 未声明行为变更（spreadSlow 改名） | High | **已解决** | `FlowingThrust.cpp:740-741` 已回退 `FrostSlow`/`Frost Slow`，与 172 主减速恢复同 id 刷新语义，叠加问题消除 |
| F2 旧存档 `kind` 刷新缺口 | Medium | **已解决** | `Buff.hpp:198-199` 刷新时同步 `type`/`kind`；与 `AilmentEngine.cpp:699-702` 的旧存档升级承诺一致 |
| F3 到期撤销 1 帧延迟 | Medium | **接受为剩余风险** | 帧序后移 1 帧，`EffectSystem` 之后本帧无属性消费者；按实施方决定登记接受 |
| F4 残留热路径字符串比较 | Low | **保留（边界外）** | `DamageConditions.cpp:19`、`BladeFormation.cpp:483` 属本波未触及既有文件，登记后续销项 |
| F5 测试内 `const_cast` 无 RAII | Low | **保留（测试卫生）** | `SkillBehaviorGuardTests.cpp:334-355`，mutation 后均为非中止 CHECK，实施方判定低危不修 |
| F6 `kBuffKindCount` 缺编译期保护 | Best Practice | **已解决** | `DamageConditions.hpp:57-58` 新增 `static_assert` |

### 新发现项

**F7（High，回归）｜175 传染减速丢失 `BuffType::SpeedDown`，经 type 同步反向污染共享的 172 减速**

- 位置：`src/game/systems/skill/behaviors/FlowingThrust.cpp:739-746`（配合 `Buff.hpp:198`）
- 观察：本轮为修复 F1 将传染减速 `id` 改回 `"FrostSlow"` 时，原 `.type = BuffType::SpeedDown` 被 `.kind = BuffKind::Slow` 替换，聚合初始化里不再设置 `type`，故 `BuffEffect.type` 取默认 `BuffType::None`。由于 `id` 现与 172 直击减速（`FlowingThrust.cpp:538-544`，`type = BuffType::SpeedDown`）相同，且本轮新加的 `AddOrRefresh` 会执行 `effect.type = new_effect.type`（`Buff.hpp:198`），一次余韵传染刷新就会把目标身上共享的 `FrostSlow` 的 `type` **清为 None**；即便目标此前无该 buff，传染生成的减速本身也是 `type=None`。代码注释 `FlowingThrust.cpp:734`「沿用 172 凛风的 legacy SpeedDown 方式」与实际字段不一致。
- 影响（type 消费者，均可复现）：
  - `src/game/systems/skill/behaviors/RendingWave.cpp:708`（275 异常扩散）：仅按 `type == SpeedDown || Freeze` 识别寒冷家族，`type=None` 的传染减速不再被判定为 Chill，扩散失效。
  - `src/game/systems/skill/BoomerangDeliverySystem.cpp:257-261`（835 回旋游步净化）：仅按 `type == SpeedDown/Freeze/Root` 清除，`type=None` 的传染减速无法被净化。
  - `src/game/foundation/data/BuffRegistry.cpp:45-51`：`type=None` 取不到 `SpeedDown` 视觉数据，回落 `default_data`（未知 buff 文案/图标），UI 退化。
  - 直击 172 减速被传染刷新后 `type` 同样被清空，直到下一次直击才恢复，形成抖动。
  - `DamageConditions.cpp:14-20` 受控判定因 `id.find("Slow")`（`FrostSlow` 命中）不受影响，`kind=Slow` 亦保证技能1 元素检测正确，故影响面集中在 `type` 消费方。
- 违规：本轮 F1 的修复目标是「恢复 172 刷新语义、不改变既有行为」，但同一 `id` 的两个创建点身份字段（`type`）不一致，叠加本轮新增的 `AddOrRefresh` type 同步后，对既有刷新路径产生行为回退（含跨技能 275/835 与 UI），F1 修复不完整。触发 `review.md`「行为回归 ⇒ 修改」。
- 修复建议（单行）：在 `FlowingThrust.cpp:739-746` 的 `spreadSlow` 聚合初始化中补回 `.type = BuffType::SpeedDown,`，与 172 直击一致；建议同时在 `Buff.hpp:196-199` 注释中约定「同一 id 的多个创建点必须保持 `type`/`kind` 一致」，并补一条回归用例（余韵传染刷新后断言共享 `FrostSlow` 的 `type` 仍为 `SpeedDown`，且 `kind == Slow`）。

### 跟进结论

**修改**

F1/F2/F6 已按上轮建议闭环，边界与构建/测试证据可信；但本轮修复引入新的行为回归 F7（共享 `FrostSlow` 的 `type` 被清空，影响 275 扩散、835 净化与 UI）。该问题为单行可修，修复并重跑 `ctest -L ci` / 相关专项后可转 `提交`。本轮不删除第 1 轮任何内容。

### 剩余风险（截至第 2 轮）

1. **F7 未决期间**：技能1 传染减速在 275/835 与 UI 中表现为非减速；直击减速的 `type` 存在清空抖动。
2. **F3 已接受**：到期撤销帧序后移 1 帧；若后续新增 `EffectSystem` 之后的属性消费者需重新评估。
3. **F4 边界外**：`DamageConditions.cpp:19`、`BladeFormation.cpp:483` 仍存在热路径字符串子串比较，需后续销项。
4. **F5 测试卫生**：`SkillBehaviorGuardTests.cpp:334-355` 的 `const_cast` 树修改无 RAII，断言中止时会污染兄弟 SUBCASE；低危。
5. **B1-21/22/23 未验证**：无交互运行环境，运行时项仍未端到端验证，须继续登记为阻塞/未验证，不得记为通过。
6. **并行链接瞬态**：主程序首次并行链接出现瞬时 `LNK1136`，单独重链成功；疑并行构建竞态，需观察是否复现。
7. **历史告警**：`C4834/C4002` 位于本波未触及的既有文件，计划「0 告警」退出条件需区分「本波新增为 0」与「历史告警仍在」。

### 下一步动作

1. （必须）修复 F7：`FlowingThrust.cpp:739-746` 补回 `.type = BuffType::SpeedDown`，并加共享 id 的 type 一致性注释与回归用例。
2. 重跑 `build.bat`、`ctest -L ci` 与 `[Unit] FlowingThrust - 175 ...` 等专项；确认 `BuffSystem - Duration and Stacking` 仍通过。
3. 将 F4/F5 与 B1-21/22/23 的阻塞状态写入计划/销项；F3 已在报告中登记接受。
4. 上述闭环后由审查者进行第 3 轮，预期可改判 `提交`。

---

## 跟进审查（第 3 轮）

- 日期：2026-09-12
- 触发：第 2 轮结论 `修改`（F7），实施方修复 F7 并补回归用例后提请复查
- 复查方式：`git diff` 边界核对 + 直接检视修复代码与新增用例（未修改任何源码）

### 本轮变更内容（`git diff` 核对）

- `src/game/systems/skill/behaviors/FlowingThrust.cpp:739-747`：传染减速 `spreadSlow` 现同时设置 `.id = "FrostSlow"`、`.name = "Frost Slow"`、`.type = BuffType::SpeedDown`、`.kind = BuffKind::Slow`，与 172 直击减速 `:538-544` 的 id/type/kind 完全一致，F7 回归路径消除。
- `tests/unit/SkillSpecializationBakerTests.cpp:1399-1460`：新增 `TEST_CASE("[Unit] FlowingThrust - 175 spread slow keeps SpeedDown type on refresh")`（+64 行）。
- 变更边界：19 个修改文件（+365/-33），无新增越界文件；新增用例与既有 `[Unit] FlowingThrust - 175 Residual Elements spread respects 1s ICD`（同文件 `:1333`）相邻，位置合理。

### 复查范围与证据

- **F7 修复正确性**：`git diff` 确认 `FlowingThrust.cpp:742` 已补回 `.type = BuffType::SpeedDown`，与 `:541` 172 直击点一致；同 id 两创建点身份字段现已相同，`Buff.hpp:198` 的 `effect.type = new_effect.type` 不再造成污染。
- **无残留热路径字符串比较**：`FlowingThrust.cpp:372-386` 的破阵流前置探测已完全改为 `GetByKind(BuffKind::...)`，`git diff` 显示原 `b.id.find("Bleed"/"Ignite"/"Cold"/"Chill"/"Slow")` 循环整体删除，热路径无残留子串匹配。
- **新用例有效性（是否覆盖「已存在同 id buff 的刷新」且修复前会失败）**：逐一核对——
  - 传染分支在 `FlowingThrust.cpp:753` 对最近目标**无条件**调用 `AddOrRefresh(spreadSlow)`，并非「仅目标无该 buff 时新建」；用例在 `targetA` 预置同一 `FrostSlow`，正好命中刷新路径。
  - 用例断言 `fxA->GetByKind(BuffKind::Slow)` 非空且 `id=="FrostSlow"`、`type==SpeedDown`、`kind==Slow`。修复前 `spreadSlow.type` 取默认 `BuffType::None`，经 `Buff.hpp:198` 会把 `targetA` 既有 buff 的 `type` 覆盖为 None，`CHECK(spread->type == BuffType::SpeedDown)` 必然失败；修复后通过。故该用例是**有效回归用例**，非空断言，非 `REQUIRE(true)`。
  - 用例首击确实触发传染：与既有 ICD 用例同构（`active.specialized_slots[0].allocated_points[175]=5` → 100% 几率），后者 `:1367-1375` 首击即成功传染 A 并已验证通过；`victim` 预置 `kind=Slow` 使 `victimHasCold=true`（`FlowingThrust.cpp:381-384`），`spreadTarget` 唯一（`targetA` @50 码，半径 200）。
- 交付证据（实施方提供）：`build.bat` 成功（BUILD3_EXIT=0）；新用例 6/6 通过；`ctest --test-dir build -C RelWithDebInfo -L ci` = 100%（1/1，11.52s），含既有 1544 用例。

### 发现项处置

| 发现项 | 严重度 | 状态 |
| --- | --- | --- |
| F7 传染减速丢失 `SpeedDown`、经 type 同步污染共享 `FrostSlow` | High | **已解决**（`FlowingThrust.cpp:742` + 新回归用例） |
| F1 spreadSlow 未授权改名 | High | 已解决（第 2 轮） |
| F2 旧存档 `kind` 刷新缺口 | Medium | 已解决（`Buff.hpp:196-199`，第 2 轮） |
| F6 `kBuffKindCount` 缺编译期保护 | Best Practice | 已解决（`DamageConditions.hpp:57-58`，第 2 轮） |
| F3 到期撤销帧序后移 1 帧 | Medium | 仍开启（接受为剩余风险） |
| F4 边界外热路径字符串比较 | Low | 仍开启（`DamageConditions.cpp:19`、`BladeFormation.cpp:483`） |
| F5 测试内 `const_cast` 无 RAII | Low | 仍开启（`SkillBehaviorGuardTests.cpp:334-355`） |

### 本轮新观察（非阻塞，Best Practice）

- **O3-1**：新用例 `targetA` 预置 `remaining = 2.5f` 与新配置 `spread_slow_duration = 2.5f` 相同，用例唯一的复位信号是 `type`；建议将预置 `remaining` 设为不同值（或断言刷新后 `remaining == spreadSlowDur`），使用例同时证明「刷新确已发生」，对配置变更更鲁棒。
- **O3-2**：用例置于 `SkillSpecializationBakerTests.cpp` 与既有 175 ICD 用例同文件，可接受；后续 FlowingThrust 用例增多时可考虑归并到专属测试文件。

### 跟进结论

**提交**

F7 已按第 2 轮建议完整闭环，修复代码与 172 直击点身份字段一致；新增用例真实覆盖「同 id 刷新」路径且在修复前会失败，无 `REQUIRE(true)`/注释旁路；变更边界未越界，构建与 `ctest -L ci` 证据完整。无剩余 Blocker/High。F3/F4/F5 及运行时项为经确认的低风险/边界外/环境受限项，列入剩余风险后不影响本包提交。

### 剩余风险（截至第 3 轮）

1. **F3（Medium，已接受）**：到期撤销/属性重算在 `EffectSystem` 后移 1 帧；若后续在 `EffectSystem` 之后新增属性消费者需重评。
2. **F4（Low，边界外）**：`DamageConditions.cpp:19`、`BladeFormation.cpp:483` 仍为热路径字符串子串比较，待后续销项（§7.2 长期收敛项，非本波引入）。
3. **F5（Low，测试卫生）**：`SkillBehaviorGuardTests.cpp:334-355` 的 `const_cast` 树修改无 RAII，断言中止时会污染兄弟 SUBCASE。
4. **B1-21/22/23（未验证）**：无交互运行环境，运行时项未端到端验证，继续登记为阻塞/未验证，不得记为通过。
5. **并行链接瞬态 `LNK1136`**：主程序首次并行链接偶发，单独重链成功；疑并行构建竞态，需观察是否复现。
6. **历史告警基线**：`C4834/C4002` 位于本波未触及既有文件，计划「0 告警」退出条件应区分「本波新增为 0」与「历史告警仍在」。

### 下一步动作

1. 本包按 `提交` 处理，可进入 B1 第二波。
2. 将 F4/F5 与 B1-21/22/23 的阻塞状态写入计划与销项清单；F3 已在报告登记接受。
3. 迭代后续轮次不删除本报告早期轮次，仅追加。
