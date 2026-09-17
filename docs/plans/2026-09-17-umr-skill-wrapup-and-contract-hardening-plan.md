# UMR 技能专精收尾与契约加固实施计划

- **系统代号**：`UMR-SKILL-WRAPUP-HARDENING`
- **计划日期**：2026-09-17
- **文档版本**：v1.2（已按二轮代码实证复审修订）
- **设计依据**：`docs/designs/2026-09-17-umr-skill-wrapup-and-contract-hardening-design.md` (v1.2)
- **目标分支**：`main`
- **标准遵循**：`conductor/code_standard.md` (V2.1)、`docs/workflows/implementation.md`

---

## 0. v1.1 → v1.2 修订说明（代码实证）

v1.1 的若干论断与当前代码不符。下表为逐条实证结论与本次修订动作，保留审计线索：

| 编号 | v1.1 论断 | 代码实证 | v1.2 动作 |
|---|---|---|---|
| C-1 | `BeamChannelDeliverySystem.cpp:224` 为历史死分支，可删 | 同文件 `:153` 使用 `SkillSystem::GetBakedSkillProfile`（**可空**且**不过滤**哨兵）；哨兵 `area_radius == 1.0f`（`SkillSpecializationBaker.cpp:26-31`），故 `> 1.0f` 为**唯一存活守卫** | 撤销"删除"，改为 Task 2.3「守卫加固」 |
| C-2 | 节点 1015 使角色常驻获得 +20% **全局**闪避 | 1015 **不在** skill 10 的 `mastery_skill_trees.json` `skill_contract.nodes[]` 中（整字面量 `"1015"` 仅出现于 talent_tree 显示节点）；`SkillContract.hpp:64` 无契约节点默认 `ScopePolicy::SkillOnly`；`StatsSystem.cpp:393` `SkillOnly` 要求查询方 `skill_id != 0 && == source_skill_id`；全仓 `StatType::DodgeChance` 不经 `GetStatWithTags` 查询（仅 `:271` base、`AttributePipeline.cpp:765-766` 终值、`AffixMapping.hpp:86` 显示、`PhantomTrance.cpp:182` Buff） | 判定前提**未获证实**；新增 Task 3.0 前置证伪门 |
| C-3 | 局部 `Flat 20.0f` 闪避 Buff 为等价替换 | 旧数据 `mode:1`(PercentAdd) 且 `StatsSystem.cpp:487` 按点数缩放 → 3 点 = base × (1+0.6)；`PhantomTrance.cpp:187` 注释确认 PercentAdd 对闪避无效；节点 `desc_key`（`mastery_skill_trees.json:563`）描述的是"降低 20%/40%/60% 减速与击退影响"，与闪避无关 | 用户裁决**方案甲**：仅清空误映射修饰符，不新增 Buff；字面语义登记为 F-01 |
| C-4 | "彻底卸下行为层防御负担" | `is_baked` 仅在 `ResolveBakedProfile` 生效；`SkillSystem::GetBakedSkillProfile`(`:2766-2779`) 直连点约 30 处（含 `BeamChannelDeliverySystem.cpp:153`、`PlayerHUD.cpp:144`、`GameUiSnapshotBuilder.cpp:406`、`SkillSystem.cpp:925/1805/2004`）仍可拿到哨兵 | 收敛断言为"统一解析出口" |
| C-5 | 技能 10 判定半径接入 `profile->area_radius` 会被测试区分 | 技能 10 在 `assets/data/modifier_v2/skill_spec_modifiers.json` 中**无** `SKILL_AREA_MULT` 记录，`area_radius` 恰等于 `GetParam("radius", 96.0f)` | 明示"零行为变化的纯重构"，并改写测试策略（Task 2.1/Task 6.3） |
| C-6 | §2.2.2 钳制代码片段可直接套用 | `SwordArray.cpp:119-126` 已声明 `spawn_pos` 并处理 `is_mobile_aura`；片段重复声明且改变分支归属 | §2.2.2 片段改为"替换 119-126 的整体补丁" |
| C-7 | 文件清单仅列 `SwordArray.cpp` | `tests/functional/SwordArrayNodes.cpp:277-278` 注释断言"无行为层读者…不附带任何行为差异断言"将失效 | 补入清单 |
| C-8 | 以 `:run_quiet_step` "追加"两个门禁 | 本行的"既有 9 处 precheck 调用均无中止判断"**已于实施/复检阶段证伪**（实际 9 处均已带 `if errorlevel 1 exit /b 1`，见 Task 5.2 与 design §2.7）；`build.bat:704-720` 的 `if errorlevel 1 exit /b 1` 为冗余防御，予以保留 | 拆为 Task 5.1（次序）与 Task 5.2（**已修订为：无缺口需补，新增 2 项沿用同一模式共 11 处一致**） |
| C-9 | `if defined GITHUB_ACTIONS (git checkout -- settings.json)` | `.github` 目录不存在，无 CI 环境；污染源为测试内 `qualityManager.Initialize("settings.json")`（`MaterialQualityManager` 写回 `benchmarkScore`/`updatedAtUtc`） | 改为 Task 5.4 夹具级还原 |
| C-10 | 渲染圈直接读取 `profile->delivery.range` | 与 `BeamChannelDeliverySystem.cpp:209-213` 的 `maxRange` 双源必漂移；且逐帧解析存在 cache miss 时逐帧烘焙的性能风险 | 抽出共享 helper（Task 2.4），渲染侧复用 |
| C-11 | §2.4/§2.5/§2.6 列为 §1.2 设计目标 | 计划中无对应任务与 DoD | 显式移入 §4「本次不实施项」，并回写总账 |
| C-12 | 测试"构造未成功烘焙的空 profile" | 真实哨兵入口为 `SkillSystem::RebakeSkillProfiles`(`:2737-2741`)：`skillData` 缺失时向缓存写入哨兵 | Task 1.6 改为经该入口构造 |
| C-13 | `ctest -L unit` | 缺少 `--test-dir build` | 修正命令 |
| C-14 | 测试清单仅列 `SkillProfileResolveTests.cpp` | `SkillBatch4NullProfileFallbackTests.cpp`、`SkillBatch4BloodSeaOrderingTests.cpp` 直接承载本次语义 | 补入清单为回归证据 |

**背景**：Batch 1~4 已完成剑修 12 技能交付算子的 UMR 迁移。本轮为收尾与契约加固，不新增玩法内容。

---

## 1. 计划目标与验收准则 (DoD)

1. **零编译报错与警告**：`build.bat RelWithDebInfo` 退出码 0；构建日志中新增 `warning` 计数为 0（以既有基线日志比对，命令见 §5）。
2. **哨兵解析收敛**：`BakedSkillProfile::is_baked` 生效，`ResolveBakedProfile` 三步走统一拒签未烘焙哨兵；行为层仅简化**哨兵专属**守卫，数值有效性守卫保持不变；`tests/unit/SkillProfileResolveTests.cpp` 与 `SkillBatch4NullProfileFallbackTests.cpp` 全绿。
3. **单源消费闭环（须可证伪）**：
   - 技能 10 判定半径消费 `profile->area_radius`，形态分支（0.28 / 0.34 / 0.50）严格不变；因技能 10 无 `SKILL_AREA_MULT` 记录，本项为**零行为变化的纯重构**，须以"人工构造已烘焙档案"的单测锁定。
   - 技能 6 选点超出 `del.range` 时钳制到外圆周（含 `Position` 判空与 mobile-aura 分支保序）。
   - 技能 7 指示圈与 `BeamChannelDeliverySystem` 的 `maxRange` 由**同一 helper** 产出。
4. **节点 1015 处置（已裁决：方案甲）**：先执行 Task 3.0 证伪门确认实际作用域；确认后清空该节点误映射的 `stat_modifiers`，**不新增闪避 Buff**；节点保留 `SKILL_DURATION_FLAT`(`2010150`) 的无敌时长效果；文案所述"降低减速与击退影响"作为跟催项 F-01 登记，本计划不实现。
5. **门禁与环境规范**：
   - `validate_skill_spec_modifiers.py --check` 与 `gen_skill_mechanics_schema.py --check` 接入 `build.bat`，且位于 `gen_modifier_runtime_v2.py` **之前**；两者在 HEAD 上已验证 EXIT=0（前者自报 `6/6 passed`，实际执行 7 项检查，汇总少计 `dead_keys_absent`）。
   - `build.bat` 全部 precheck 统一中止语义（或明确登记为告警），不得只加固其中两个。
   - 测试不再污染仓库根 `settings.json`（夹具级还原，不依赖 CI 判定）。
   - `bin\NoMoreDayTests.exe` 与 `ctest --test-dir build -C RelWithDebInfo -L unit` 全绿。

---

## 2. 变更文件清单 (File Manifest)

| 模块 | 文件路径 | 变更性质 | 说明 |
|---|---|---|---|
| 底座结构 | `src/game/foundation/components/SkillDefs.hpp` | 修改 | `BakedSkillProfile` 增加 `bool is_baked = false;`，并注释哨兵契约 |
| 底座数据 | `src/game/foundation/data/BuffIds.hpp` | **不修改** | 方案甲不引入 Buff，无需新增 `SevenStarVoidTread` |
| 烘焙层 | `src/game/systems/skill/SkillSpecializationBaker.cpp` | 修改 | `Bake(...)` 唯一成功出口置 `out_profile.is_baked = true;` |
| 底座解析 | `src/game/systems/skill/SkillProfileResolve.cpp` | 修改 | 三步走统一校验 `is_baked`；未烘焙返回 `nullptr` |
| 行为层 | `src/game/systems/skill/behaviors/BloodSea.cpp` | 修改 | 简化哨兵专属守卫（保留除零/数值守卫） |
| 行为层 | `src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp` | 修改 | 同上；`areaMult` 的除以 `base_field_radius` 守卫保留 |
| 行为层 | `src/game/systems/skill/behaviors/SevenStarSlash.cpp` | 修改 | 仅半径单源（Task 2.1）；1015 不新增 Buff |
| 行为层 | `src/game/systems/skill/behaviors/SwordArray.cpp` | 修改 | 整体替换 119-126：`is_mobile_aura` 保序 + `del.range` 钳制 |
| 行为层 | `src/game/systems/skill/BeamChannelDeliverySystem.cpp` | 修改 | **加固** `:211`/`:224` 哨兵守卫（非删除） |
| 共享助手 | `src/game/systems/skill/behaviors/BeamChannelShared.hpp`（或既有共享头） | 新增 | `NoMoreDay::ResolveBeamChannelMaxRange(profile, beamSkillId)` 单源函数 |
| 共享助手 | `src/game/systems/skill/behaviors/SwordArrayShared.hpp` | 新增 | `kSwordArrayBaseCastRange` 常量单源，Baker case6 与行为层共享 |
| 数据资产 | `assets/data/mastery_skill_trees.json` | 修改 | 清空节点 1015 的 `stat_modifiers`（Task 3.2） |
| 数据资产 | `assets/data/skill_mechanics.json` | **不修改** | 方案甲不新增机制键，无需同步 schema |
| 数据资产 | `assets/data/skill_mechanics_schema.json` | 修改（生成物） | 共享 helper 新增 `base_range` 读取元组，`dynamic_keys` 同步增补 |
| 渲染/UI | `src/game/application/states/GameplayState.cpp` | 修改 | 技能 7 指示圈改调共享 helper |
| 构建脚本 | `build.bat` | 修改 | precheck 次序修正 + 统一中止语义 |
| 测试夹具 | `tests/TestCommon.hpp` | 修改 | `TestSetupScope` 对 `settings.json` 做条件保存/还原 |
| 现有测试 | `tests/unit/SkillProfileResolveTests.cpp` | 修改 | 手写 mock 处补 `is_baked = true` |
| 现有测试 | `tests/unit/SkillSpecializationBakerTests.cpp` | 修改（+2） | 技能 7 射程字面量期望补注释，说明其独立于生产常量（复检 L-R2-4） |
| 现有测试 | `tests/unit/SkillBatch4NullProfileFallbackTests.cpp` | **未修改** | 仅作哨兵/空档案回退语义回归证据（`git diff` 为空） |
| 现有测试 | `tests/unit/SkillBatch4BloodSeaOrderingTests.cpp` | **未修改** | 同上（`git diff` 为空） |
| 现有测试 | `tests/functional/SwordArrayNodes.cpp` | 修改 | 更新 `:277-278` 注释并补行为断言 |
| 现有测试 | `tests/functional/SevenStarSlashNodes.cpp` | 修改 | 1015 相关断言随 §2.3 裁决调整 |
| 现有测试 | `tests/unit/VFXSequencerTest.cpp` | 修改 | 2 处 `QualityTierManager::Initialize("settings.json")` 接入 `TestSetupScope`（实施期范围扩展） |
| 现有测试 | `tests/performance/MaterialLightingBenchmark.cpp` | 修改 | 同上（实施期范围扩展） |
| 现有测试 | `tests/integration/GPUABIBindingTierIntegrationTest.cpp` | 修改 | 同上（实施期范围扩展） |
| 现有测试 | `tests/integration/MaterialLightingIntegrationTest.cpp` | 修改 | 同上，2 处（实施期范围扩展） |
| 现有测试 | `tests/integration/RenderSystemPhaseDToggleSmokeTest.cpp` | 修改 | 同上（实施期范围扩展） |
| 现有测试 | `tests/integration/JFAPassUpsampleMaskTest.cpp` | 修改 | 无参 `Initialize()` 经默认实参写回仓库 `settings.json`，同补夹具（复检 High 整改） |
| 现有测试 | `tests/unit/RenderSystemInitializeFailureTest.cpp` | 修改 | `RenderSystem::Initialize` 间接路径（`RenderSystem.cpp:969`），2 个失败用例同补夹具（复检 High 整改） |
| 单元测试 | `tests/unit/SkillProfileResolveSentinelTests.cpp` | 新增 | 经 `RebakeSkillProfiles` 构造真实哨兵并断言拦截 |
| 单元测试 | `tests/unit/SkillWrapupHardeningTests.cpp` | 新增 | 半径单源、剑阵钳制、指示圈 helper、1015 证伪门 |
| 共享常量 | `src/game/systems/skill/behaviors/SevenStarSlashConstants.hpp` | 新增 | 承载 `kSevenStarSlashBaseRadius = 96.0f` 单源常量（review.md #5）；零依赖头，供烘焙层与行为层共用 |
| 共享助手 | `src/game/systems/skill/behaviors/SevenStarSlashShared.hpp` | 修改 | 改为包含 `SevenStarSlashConstants.hpp`，不再重复定义半径常量（第五轮 F6：避免烘焙层为一个浮点常量拖入行为层重型头） |
| 行为层 | `src/game/systems/skill/behaviors/PhantomTrance.cpp` | 修改 | `ResolveParams` 消费侧补空指针守卫（review.md #7）：`ResolveBakedProfile` 出口契约变更后仍须保持"恒返回有效引用" |
| 总账 | `docs/plans/2026-09-12-skill1-9-followup-backlog.md` | 修改 | 回写 C-2/C-3/C-11 结论与跟催项；§1.5 登记 review.md 11 项整改 |
| 文档（**非本包**） | `设计文档/职业被动和技能设置.md` | 工作树内既存用户改动 | 本包实施前已存在的未提交改动，不属本包变更边界，不纳入审查/回滚范围 |

> **v1.3 整改登记（2026-09-17）**：第三轮交付后由独立子代理对全部变更文件做静态扫描，提出 11 项发现（仓库根 `review.md`），已全部采纳修复，明细见 `docs/plans/2026-09-12-skill1-9-followup-backlog.md` §1.5。其中 3 项为**计划外**衍生修复：`PhantomTrance.cpp` 空指针守卫（Task 1.3 出口契约变更的调用方）、`SwordArray.cpp` 675 挪阵分支同样受射程钳制约束（Task 2.2 的路径遗漏）、`TestCommon.hpp` 快照完整性/产物清理加固（Task 5.4 的健壮性缺口）。均已补回归用例。
>
> **v1.4 整改登记（2026-09-17，第五轮复检回应）**：独立复检子代理对上述整改做只读复核，结论 `修改`，8/11 判定为已真正修复，3 项判定为部分修复（夹具数据丢失、哨兵消费点漏改、重复断言），均已按建议闭合：`TestCommon.hpp` 引入 `m_settingsSnapshotAbandoned` 三态、清理失败补告警；`BeamChannelDeliverySystem.cpp` 的 `ResolveSkill7Element` 同补 `is_baked` 过滤；603 钳制断言去重；mobile-aura 作用于挪阵分支的语义补记入 design §2.2.2；半径常量析出为 `SevenStarSlashConstants.hpp`；`build.bat` 冗余注释精简。复检报告：`docs/reviews/2026-09-17-umr-skill-wrapup-and-contract-hardening-review-round5.md`。

---

## 3. 原子任务拆分 (Atomic Tasks)

### 阶段 1：哨兵解析收敛（Task 1）
- **Task 1.1**：`SkillDefs.hpp` 为 `BakedSkillProfile` 增加 `bool is_baked = false;`，并在结构体注释中写明：`is_baked == false` 即"不可消费"，`area_radius == 1.0f` 仅为结构体默认值、不再作为判定依据。确认 `static_assert`（`SkillDefs.hpp:697-698`、`ItemSkillModifierTests.cpp:19`）与 `operator==`(`:695`) 仍成立。
- **Task 1.2**：`SkillSpecializationBaker.cpp` 的 `Bake(...)` 末尾置位 `out_profile.is_baked = true;`。契约：函数内存在任何提前返回（当前仅 `:26-31` 的 `!skillData`）即不得置位；新增提前返回必须保持该约束。
- **Task 1.3**：`SkillProfileResolve.cpp` 三步走（缓存命中 / `fallbackSpec` 即时烘焙 / 槽位扫描即时烘焙）统一以 `is_baked` 过滤，未烘焙返回 `nullptr`。
- **Task 1.4**：`tests/unit/SkillProfileResolveTests.cpp` 手写赋值处（`:51-52`、`:143-144`、`:148-149`）补 `is_baked = true`。已核实其余夹具均经 `SkillSpecializationBaker::Bake` 后 `active.baked_profiles[0] = profile;` 写入（`Skill3/4/5FollowupTests.cpp`、`InfiniteBladesNodes.cpp`、`BladeFormationNodes.cpp` 等），标志自动传播，无需改动。
- **Task 1.5**：统一守卫策略并落地到 `BloodSea.cpp:430-445` 与 `HeavenlySwordDescent.cpp:601-616,661-664`：删除**哨兵专属**的 `skill != nullptr` 合取项（前提：二者 profile 均来自 `ResolveBakedProfile`，已核实 `BloodSea.cpp:306`、`HeavenlySwordDescent.cpp:490`），保留数值/除零守卫（如 `base_field_radius > 0.0f`）。**不采用** v1.1 的"一刀切恢复 `profile ? ...`"。
- **Task 1.6**：新增 `SkillProfileResolveSentinelTests.cpp`，经真实入口构造：
  1. 无 `ActiveSkillsComponent` → `nullptr`；
  2. 用未在 `SkillRegistry` 注册的 `skill_id` 触发 `RebakeSkillProfiles`(`SkillSystem.cpp:2737-2741`) 写入哨兵 → 断言 `ResolveBakedProfile` 返回 `nullptr`（**改前应失败**）；
  3. 已烘焙档案 → 返回缓存指针且 `is_baked == true`；
  4. `RebakeSkillProfiles` 连续两次调用，第二次因 `operator==`（已含 `is_baked`）不写入，断言幂等。
- **Task 1.7**：在计划证据中登记 `SkillBatch4NullProfileFallbackTests.cpp`、`SkillBatch4BloodSeaOrderingTests.cpp` 全绿输出，作为 Task 1.5 的回归证据。

### 阶段 2：单源消费闭环（Task 2）
- **Task 2.1**：`SevenStarSlash.cpp:417` 改为 `const float baseRadius = profile ? profile->area_radius : (skillData ? skillData->GetParam("radius", 96.0f) : 96.0f);`，`456-468` 形态分支不动。**明示**：技能 10 无 `SKILL_AREA_MULT`，本项零行为变化；同时在计划中登记"96.0f 与 `Baker` case10 默认值同源，若后续引入 `SKILL_AREA_MULT` 必须补端到端测试"。
- **Task 2.2**：整体替换 `SwordArray.cpp:119-126`（不新增声明）：
  - 先算钳制、后判 `is_mobile_aura`，保证 owner 无 `Position` 时行为与现状一致；
  - 仅在 `distSq > maxRange²` 时执行一次 `std::sqrt`；
  - `maxRange` 取 `(profile && profile->delivery.range > 0.0f) ? profile->delivery.range : kSwordArrayBaseCastRange`，常量与 `Baker` case6 的 `400.0f` 统一为具名常量。
- **Task 2.3**：`BeamChannelDeliverySystem.cpp` **加固**而非删除：`:211` 与 `:224` 改为 `profile != nullptr && profile->is_baked && <原数值守卫>`。在代码注释中记录 v1.1 的"死分支"判断已被证伪。**不引入** `ResolveBakedProfile`（避免逐帧烘焙风险），并保持空指针兜底。
- **Task 2.4**：新增 `NoMoreDay::ResolveBeamChannelMaxRange(const BakedSkillProfile *profile, uint32_t beamSkillId)`（定义于 `src/game/systems/skill/behaviors/BeamChannelShared.hpp`，命名空间为 `NoMoreDay`，非 `skills`；形参须避开 `skillId` 以免被 schema 扫描器误解析，见 F-08）（返回 `profile->delivery.range` 或 `GetMech(beamSkillId,0,"base_range", kBeamChannelBaseRangeDefault)`），`BeamChannelDeliverySystem` 与 `GameplayState` 指示圈**共同调用**。指示圈使用只读的 `SkillSystem::GetBakedSkillProfile`（不触发烘焙）；遍历 `BeamChannelComponent` 时对 owner 缺失或 `ActiveSkillsComponent` 缺失做跳过处理。

### 阶段 3：节点 1015 处置（Task 3）
- **Task 3.0（前置证伪门，必须先执行）**：在 `SkillWrapupHardeningTests.cpp` 中，于分配 1015 点数的角色上断言：
  1. `StatsSystem::GetStatWithTags(reg, caster, StatType::DodgeChance, Tag::None, /*skill_id=*/0, entt::null)` 与未分配时**完全一致**（证明无全局污染）；
  2. `... /*skill_id=*/10 ...` 与未分配时存在差异（证明作用域生效）。
  - 结果写入计划证据。**若第 1 条断言失败**（即确为全局生效），停止 Task 3.2/3.3 并上报，改按"紧急数据修复"独立立项。
  - **实施期实证修订（2026-09-17）**：第 2 条预期被证伪。节点 1015 的 `StatModifier` 未声明 `required_tags`，`Stats.hpp` 解析缺省为 `Tag::None`，而 `StatsSystem.cpp:318` 的 `is_baked = (required_tags == Tag::None)` 快路径会在**所有作用域**跳过它；`AttributePipeline` 亦不折叠专精节点修饰符。故该修饰符为双重惰性，第 2 条改为"与基准一致"的同向断言。门禁结论仍由第 1 条承担，并新增灵敏度对照（基线 `dodge_chance=0.15` 必须读回 15.0）以排除探针假阴性；第 1 条改用 `REQUIRE` 以匹配前置阻断语义。系统性影响面另立 F-06。
  - 证据：`SkillWrapupHardeningTests.cpp` 的 `[Unit] SkillWrapup - Node 1015 stat scope gate (Task 3.0)`。
- **Task 3.1**：**已取消**（方案甲不引入 Buff，`BuffIds.hpp` 不修改）。
- **Task 3.2**：将 `mastery_skill_trees.json` 节点 1015 的 `stat_modifiers` 置为 `[]`（依 Task 3.0 结论）。清理后该节点仍保留 `SKILL_DURATION_FLAT`(`2010150`) 的无敌时长效果，**非死节点**。
- **Task 3.3**：**已裁决：方案甲**——仅清理误映射修饰符，不新增闪避 Buff。理由：
  1. 节点生效窗口即无敌帧，期间闪避无实际收益；
  2. `desc_key` 语义（降低减速与击退影响）与闪避无关，补丁式闪避 Buff 会引入第三种语义；
  3. 旧 `mode: 1` 为 `PercentAdd`，任何 `Flat` 数值都无法与之等价，只能凭空造数。
  - 方案乙（局部 `Flat 20 × points`）**已否决**，不再作为备选；若后续恢复该需求，须重新走设计流程并附策划强度确认。
- **Task 3.4**：`desc_key` **本轮不修改**——清理后"文案无闪避、数据无闪避、代码无闪避"三者一致；文案中尚未实现的"降低减速与击退影响"属既有缺口，登记为跟催项 F-01（需确认是否存在对应 `StatType`，当前 `Stats.hpp:224-284` 无减速/击退抗性枚举）。
  - 必须复核的既有断言（清理 `stat_modifiers` 可能影响数据快照类用例）：`tests/unit/GeneratedSpecStateTests.cpp:157`、`tests/unit/SkillBatch4DeliveryOpTests.cpp:121-131`、`tests/functional/SevenStarSlashNodes.cpp:127`、`tests/python/SkillSpecBatch4GateTest.py:24,37`。

### 阶段 4：技能 7 指示圈同源（Task 4）
- **Task 4.1**：`GameplayState.cpp` 的技能 7 指示圈改调 Task 2.4 的共享 helper，删除本地 `skills::GetMech(7, 0, "base_range", 350.0f)` 与相关 TODO 注释。
- **Task 4.2**：为 helper 补单测（渲染循环本身不可单测）：断言"已烘焙档案 → 返回 `delivery.range`；`nullptr`/哨兵 → 返回 350"。

### 阶段 5：门禁与环境卫生（Task 5）
- **Task 5.1**：将 `validate_skill_spec_modifiers.py --check` 与 `gen_skill_mechanics_schema.py --check` 插入 `build.bat` precheck 段，且必须位于 `gen_modifier_runtime_v2.py`(`:313`) **之前**（避免坏数据先被烘焙进生成物）。
- **Task 5.2**：统一 precheck 中止语义——为 `:288-313` 全部 precheck 调用（含新增两项）补齐失败中止，或统一登记为告警并在计划中写明理由。以"临时篡改一个数据文件使门禁失败、确认构建中止"作为验证证据。
  - **实施期实证修订（2026-09-17）**：前提描述有误——`:288-313` 既有 9 处 precheck **均已**带 `if errorlevel 1 exit /b 1`，并非缺口。因此本轮无需补齐，实际动作仅为让新增 2 项沿用同一模式（现共 11 处一致）。
  - 负向证据（2026-09-17 实测）：将 `assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json` 中记录 `2002100`(`RendingWave_Node210_Projectiles`) 的 `param_f32` 由 `1.0` 篡改为 `1.5`，直接运行 `build.bat`：输出 `[FAIL] projectile_integer: ... must be an integer` → `[FAIL] skill_spec modifier offline gates detected violations (5/6 passed).` → `[Build] Skill spec modifier validation failed! Aborting.`，进程退出码 **1**，且日志在预检段即终止（未进入 CMake 配置）。事后文件已还原，SHA256 与改动前一致。
- **Task 5.3**：删除 v1.1 的 `if defined GITHUB_ACTIONS (git checkout -- settings.json)`（`.github` 不存在，属死代码；且构建脚本不应产生仓库写副作用）。
- **Task 5.4**：在 `tests/TestCommon.hpp` 的 `TestSetupScope` 中实现 `settings.json` 条件保存/还原（构造时若文件存在则读入内存，析构时仅在内容变化时写回）。禁止无条件写回；保留"提交前 `git checkout -- settings.json`"作为人工兜底并写入验证清单。
  - **覆盖面声明（复检补充）**：该夹具是**进程级**且仅保护**已显式声明 `TestSetupScope` 的用例**；同一进程内未接入的写入点既不会被还原，还可能被后续用例的快照当作"原始内容"固化。因此必须逐一穷举接入，口径为：(a) 显式字面量传参 `Initialize("settings.json")`、(b) 无参默认实参调用 `Initialize()`（默认实参即仓库文件，见 `QualityTierManager.hpp:100`）、(c) 经 `RenderSystem::Initialize` 等入口的间接调用。当前 7 个文件的计数见 design §2.7 与总账 F-07；新增调用方需按同一口径补夹具。

### 阶段 6：综合验证与全量回归（Task 6）
- **Task 6.1**：运行离线门禁并留存输出（预期 EXIT=0）。
- **Task 6.2**：`build.bat RelWithDebInfo` EXIT 0，并比对构建日志 `warning` 计数无新增。
- **Task 6.3**：运行 `tests/unit/SkillWrapupHardeningTests.cpp`：
  - 人工构造 `is_baked = true` 且 `area_radius = 200.0f` 的档案，直接驱动技能 10，断言基础/极星/星落三形态命中半径分别为 `56.0f / 68.0f / 100.0f`（**改前该测例因读取 `skillData` 而失败**，构成可证伪证据）；
  - 技能 6 超距选点钳制、owner 无 `Position` 时不崩溃；
  - `ResolveBeamChannelMaxRange` 三分支；
  - Task 3.0 的两条作用域断言；另断言清理后节点 1015 仍提供 `SKILL_DURATION_FLAT`(`2010150`) 无敌时长增益（3 点 = 0.5 + 0.09 = 0.59s，与 `SkillSpecializationBakerTests.cpp:2352-2356` 一致），证明清理未误删。
  - 方案甲回归面：七星斩行为层不得出现任何闪避相关代码路径（可用 `rg` 源码断言替代）。
- **Task 6.4**：`bin\NoMoreDayTests.exe` 与 `ctest --test-dir build -C RelWithDebInfo -L unit` 全绿。
- **Task 6.5**：确认 `git status` 中 `settings.json` 无改动；更新总账 `docs/plans/2026-09-12-skill1-9-followup-backlog.md`。

---

## 4. 本次不实施项（显式移出范围）

以下项在 v1.1 中列为设计目标但无实施任务，现明确移出本计划，登记为跟催：

| 设计条目 | 内容 | 处置 |
|---|---|---|
| 设计 §2.4 | 技能 9 节点 930/993 协同（残影斩击，0.5s ICD，40% 伤害） | 仅设计定稿；实施另立计划 |
| 设计 §2.5 | 技能 4 节点 473/474 异常强度固定 `magnitude = 1.0f` | 复核结论：`BladeWard.cpp:137-144` 现无点数缩放，无需代码改动；仅登记语义 |
| 设计 §2.6 | 天剑降临三系元素合流数据契约 | 仅设计定稿；实施另立计划 |
| 设计 §1.3 | 装备 `area_radius_mult` 折叠、其他职业、`BladeWard.cpp` NaN 逻辑 | 维持非目标不变 |

---

## 5. 风险登记与回滚

| 风险 | 影响 | 缓解 |
|---|---|---|
| Task 3.0 证伪门失败（1015 确为全局生效） | 需紧急数据修复，本计划阶段 3 失效 | 立即上报并按独立缺陷流程处理；阶段 1/2/4/5 不受影响，可独立交付 |
| 数据清理影响快照类测试 | 测试红灯 | Task 3.4 逐项复核已登记断言文件（含 `tests/python/SkillSpecBatch4GateTest.py`） |
| 指示圈逐帧解析引入性能回归 | 帧时上升 | 强制使用只读 `GetBakedSkillProfile`，禁止在渲染路径触发烘焙；helper 无分配 |
| `is_baked` 与 `operator==` 耦合导致烘焙幂等性回归 | 每帧重复烘焙 | Task 1.6 第 4 条幂等断言；`RebakeSkillProfiles` 现有跳过逻辑保持不变 |

**回滚**：全部改动可按文件粒度 `git checkout` 回退；`is_baked` 不影响存档（`ActiveSkillsComponent::to_json` 不序列化 `baked_profiles`），无存档兼容性风险。

---

## 6. 交付证据清单

1. 离线门禁输出：`python scripts\validate_skill_spec_modifiers.py --check`、`python scripts\gen_skill_mechanics_schema.py --check`（基线 EXIT=0 已确认）。
2. `build.bat RelWithDebInfo` 日志（EXIT 0 + 无新增 warning）。
3. 新旧测试对照输出（Task 6.3 的"改前失败/改后通过"证据）。
4. Task 3.0 的作用域断言输出（含失败时的上报记录）。
5. `ctest --test-dir build -C RelWithDebInfo -L unit` 与 `bin\NoMoreDayTests.exe` 输出。
6. `git status` 快照（确认 `settings.json` 未被污染）。
