# 技能 8（御剑·回旋）专精节点修复实施计划

- 依据：`docs/reviews/2026-09-10-skill8-specialization-nodes-review.md`
- 设计依据：`设计文档/职业设计草案_剑修.md` §3.8
- 目标：修复 C1-C3 / H1-H7 / M1-M5 / L1-L5 / N1-N6 全部问题，29 个专精节点语义、契约、运行时行为与设计一致，并通过构建与测试门禁。

## 一、核心架构决策

1. **节点语义单一来源**：技能 8 运行时行为以 `SkillSpecializationBaker` case 8 烘焙出的 `BakedSkillProfile` 为准；行为层只读 profile（feature_flags、delivery 新字段、effective_tags），不再依赖 `exec.active_nodes` 百分比取模的旧回退路径。
2. **回旋状态机唯一权威 = `BoomerangDeliverySystem`**：从 `ProjectileSystem` 删除回旋专属相位分支（Outward/HoverApex/Returning），保留通用投射物移动、碰撞、寿命销毁；技能 2 回归必须保持（`RendingWaveNodes` + 全量 ci 验证）。
3. **新增 BakedDeliveryParams 字段**（避免死写/复用语义不清）：`return_damage_mult`(803)、`bleed_chance`(811)、`crit_mult_vs_bleeding`(812)、`side_angle_mult`(831)、`catch_mana`(832)、`combo_attack_speed`(833)、`step_extend_sec`(834)、`pull_radius_mult`(851)、`intent_gain_chance`(852)、`intent_scaling`(853)、`giant_armor_scale`(854)、`heal_bleed_pct`(815)、`path_width_mult`/`path_duration_mult`(874)、`path_pen`(875)、`path_amp`(871)、`arc_freq_mult`(873)、`element_shield_pct`(876)。
4. **元素路径独立系统**：新增 `ElementPathComponent` + `ElementPathSystem`（新文件 `src/game/foundation/components/ElementPath.hpp`、`src/game/systems/skill/ElementPathSystem.{hpp,cpp}`）。飞剑飞行沿途铺设火/雷路径段（3s，874 缩放），火焰路径周期灼烧，雷电路径周期自动索敌电弧；871 附加易伤/减抗，875 供 `DamageMitigationService` 查询条件穿透，876 站立回盾。路径伤害带 `Tag::SecondaryHit` 防触发链放大。
5. **流血统一走 `AilmentEngine`**：811 按 25%/点几率施加 `AilmentType::Bleed`（叠层+持续伤害）；814 在 `FlowingThrust` 命中滞空区域敌人时结算全部流血层数（真实物理伤害）；815 以本次飞行累计施加的流血伤害预期值 100% 转治疗。
6. **855 触发口径**：触发 `trigger_skill_id=3`（灵剑决），`requires_crit=true`，icd 3s，effectiveness 0.5（沿用旧值，风险记录）；运行时要求施法者已专精技能 3 的巨剑节点 330（巨剑降临，设计文案“巨剑术”按数据口径落实为 330）。`TriggerRuleComponent` 新增 `required_source_node_id` 字段。
7. **互斥**：870/872 通过 compact `keystone_exclusion_groups {870:1, 872:1}` 生成节点契约 `keystone_exclusion_group`，由运行时既有排除逻辑生效；`transmuter_node_ids=[870,872]`、`max_transmuters=1`。

## 二、Phase 1：数据与契约（主线程）

`assets/data/skills.json` 技能 8：
- 基础值：`mana_cost=8.0`、`cooldown=0.0`；`desc_key` 补全穿透/折返双段伤害文案；`tags` 增加 `Return`/`Dexterity`，`sword_skill`→`SwordSkill`。
- 18 处 `max_points` 修正：800=4、801=4、803=4、810=1、811=4、812=4、813=3、831=3、832=3、833=3、851=4、852=3、870=1、871=3、872=1、874=3、875=4、876=1。
- `skill_contract` 不手改，由生成器重建。

`assets/data/skill_contracts_compact.json` 技能 8 重写：
- `max_transmuters:1`；`transmuter_node_ids:[870,872]`；`synergy_node_ids:[814]`；`sword_intent_node_ids:[852]`；`sword_step_node_ids:[834]`；`keystone_node_ids:[810,815,854]`；`keystone_exclusion_groups:{870:1,872:1}`；`resist_models:{875:TypeA_Penetration}`；`scope_policies:{875:SkillOnly}`；`trigger_nodes:[{855→3, 0.5, icd3.0, requires_crit:true}]`。
- 运行 `python scripts/gen_skill_contracts.py` 重建 `skills.json` + `mastery_skill_trees.json` 内嵌契约，随后 `--check` 必须通过。
- `assets/data/skill_8_tree.json` 仅布局（x/y/prereq），本次无布局变更，无需改；同步核对 `skills copy 2.json` 不动。

测试夹具与契约测试（主线程）：
- `tests/fixtures/skill_specialization_keynodes.json` 技能 8 key_nodes 更新为新语义节点集。
- `tests/SkillKeyNodeMatrixTestHelpers.hpp` 技能 8 期望节点集同步。
- `tests/unit/SkillCastConstraintServiceTests.cpp`：双转质折叠改为 870/872 二选一。
- 新增全树可达性回归（所有技能）：每个节点 `required_points <= 前置节点 max_points`。

## 三、Phase 2-4 并行工作流（文件所有权互斥）

| 工作流 | 文件所有权 | 内容 |
|---|---|---|
| WS-A 烘焙与行为 | `SkillSpecializationBaker.cpp`(case8)、`behaviors/BladeBoomerang.{hpp,cpp}`、`skill_mechanics.json`(技能8条目)、`DamagePipeline.cpp`(812 条件暴伤)、`tests/functional/BladeBoomerangNodes.cpp`(新建)、`tests/functional/SkillBehaviors.cpp`(技能8用例)、`tests/unit/SkillSpecializationBakerTests.cpp`(技能8用例) | 29 节点烘焙；DoCast/DoHit 重写；811/812/813/815/852/853/854/855 命中与拾取效果；元素路径段生成调用；555→3 触发 |
| WS-B 交付管线 | `BoomerangDeliverySystem.{hpp,cpp}`、`ProjectileSystem.cpp`(删除回旋分支)、`tests/integration/SkillSystemTests.cpp`(技能8用例)、`tests/integration/SkillKeyNodeMatrixIntegrationTests.cpp` | 单状态机；810 滞空切割；832 接剑回蓝；833/834/835 拾取窗口；803 折返增伤；850/851 折返牵引；854 巨剑；815 结算；阈值统一 32 |
| WS-C 元素路径 | 新建 `foundation/components/ElementPath.hpp`、`systems/skill/ElementPathSystem.{hpp,cpp}`；`DamageMitigationService.{hpp,cpp}`(875/876)；`GameplayState.cpp`(注册 Update)；`tests/functional/BladeBoomerangElementPathTests.cpp`(新建) | 路径段生成 API、火焰灼烧、雷电电弧、871/873/874/875/876 |
| WS-D 跨技能联动 | `behaviors/FlowingThrust.cpp`(814)、`ProcEngine.cpp`(855 条件)、`SkillSystem.{hpp,cpp}`(巨剑节点查询/御剑步辅助)、`tests/functional/FlowingThrustNodes.cpp`、`tests/unit/SkillBehaviorGuardTests.cpp` | 814 流血引爆；855 巨剑术门控；834 御剑步延长对接 |

接口冻结（主线程先行提交）：
- `BakedDeliveryParams` 新字段（见决策 3）。
- `BoomerangComponent`：新增 `bleed_damage_pool`(815 累计)、`path_spawn_timer`、`free_cast_pending` 等，注释按技能 8 语义改写。
- `TriggerRuleComponent.required_source_node_id`。
- `ElementPath` API：`SpawnSegment(...)`、`IsInsidePath(...)`。
- Baker `SyncTriggerRules` 对 855 写入 `requires_crit` 与 `required_source_node_id=330`。

## 四、Phase 4：验证

- `build.bat`（RelWithDebInfo）全量构建零警告错误。
- `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` 全绿，技能模块门禁 `nmd.tests.skill.*` 全绿。
- 专项：技能 2 回归（`RendingWaveNodes`）、技能 8 新功能测试、814 联动、855 触发、元素路径、全树可达性。
- 文档/证据：更新 review 结论与 plan 状态，报告修改文件与验证结果。

## 五、风险与记录

- 855 effectiveness=0.5 为沿用旧值，待设计确认；巨剑节点按数据口径用 330。
- 815 采用“累计施加流血伤害预期值”近似真实流血伤害（AilmentEngine 无按来源归属的 tick 统计），在代码注释中说明。
- ProjectileSystem 删除回旋分支影响技能 2，需以既有回归测试证据护航。
- 元素路径为新增系统，若与既有 `AreaFieldDeliverySystem` 生命周期冲突以新增回归测试暴露并修正。
