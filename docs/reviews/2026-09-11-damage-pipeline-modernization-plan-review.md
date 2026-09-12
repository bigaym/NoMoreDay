# 伤害管线现代化重构实施计划审查报告（R1）

- 审查对象：`docs/plans/2026-09-11-damage-pipeline-modernization-plan.md`（R1 稿）
- 配套设计：`docs/designs/2026-09-11-damage-pipeline-modernization-design.md`
- 基线 commit：`a803623f`
- 日期：2026-09-11
- 审查方式：两名独立子代理只读审查（架构完整性 / C++ 技术可行性），要求每条问题附代码证据；未运行构建与测试
- 结论：**修改**（R2 修订已执行，见 §5 处置映射）

---

## 1. 阻断问题

| # | 问题 | 证据 | 处置 |
|---|------|------|------|
| B1 | `skill_id=0` 的合成伤害在删桩后被武器污染：回退技能 `kDefaultBasicAttack{base_damage=0, weapon_damage_mult=1.0}`，缺 key 默认 `mult=1.0`，`skills.json` 无 id 0。删桩后荆棘、DoT tick、危险区、词缀、投射物 fallback、冰爆等会在 `base_pool` 之外再叠加武器伤害并进入完整乘区 | `SkillRegistry.cpp:752-773,709-712`；`Assets/data/skills.json`（id 1-12）；`CombatSystem.cpp:40-50,266-273,349-357,455-462`；`AilmentEngine.cpp:711-722`；`HazardSystem.cpp:419-426`；`MonsterAffixSystem.hpp:1016-1044`；`ProjectileSystem.cpp:128-134,199-210`；`RendingWave.cpp:605-617` | 计划 P0 改为「先分类审计 → 修武器唯一来源 → 再删桩」原子序列；设计 §4.3 注 2 固化请求分类 |
| B2 | `calculateBatch` 钩子结果被丢弃（返回空 vector），违反 D1/§4.7「钩子必须真实返回结果」 | `DamagePipeline.cpp:1902-1931`（调用段 1917-1924） | 新增 P1-1 任务；`DamagePipelineUnifiedEntryTests` 断言钩子返回非空且与单目标一致 |
| B3 | P1 声称「行为保持」，但单/批并轨必然改变批量事件字段（批量无 `enrichEvent`，`parent_skill_cd/trigger_depth` 恒 0）；且批量经 `CreateSnapshot` 预置了攻方乘区快照，P2 若改用程序会与快照双算，快照边界未定义 | `DamagePipeline.cpp:297-399` vs `:1808-1877`；`:1404-1431`；`:1452-1454` | P1-0 黄金表同时固化单/批基线；P1 退出改为「差异全部登记于 P1-delta」；P2-4 显式决策「程序化后删除 CreateSnapshot 攻方模拟」 |
| B4 | 黄金表无法锁定批量路径：批量返回 void、`ThreadSafeRandom` 无 seed 注入、taskflow 并行下 thread_local RNG 不可复现；单/批暴击单位相差 100 倍（批量 `snap.crit_chance/100.0f` vs 单目标归一化） | `ThreadSafeRandom.hpp:32-39`；`DamagePipeline.cpp:1422,1625,1711-1720`；`StatsSystem.cpp:215-217`；`DamagePipeline.cpp:1181-1204` | P1-0 夹具强制 crit/dodge/block=0 + 批量观测手段（HP 差或测试钩子）；P2-5 修复单位分歧并加 parity 测试，P2-8 以带暴击场景复跑黄金表 |

## 2. 严重问题

| # | 问题 | 证据 | 处置 |
|---|------|------|------|
| S1 | 行为变化的平衡复核与发布说明无交付物、无门禁挂点 | 计划 R1/R2 仅写「另行处理」 | 新增 P0-5（`P0-balance-impact.md` + 发布说明），外部门禁挂 P4-6 |
| S2 | 缺集中「回退与降级」章节（planning.md 强制）；兼容开关取舍未决定 | 计划零散提及 | 新增 §1.6；采用 `damage_legacy_conversion` 迁移期开关（默认新语义，P4-2 删除），降级路径全部日志+一次性告警+测试 |
| S3 | 程序缓存未定义并发/所有权/键/淘汰：批量 taskflow 并行段、按值返回整块拷贝、键与设计不一致、LRU 无任务 | `DamagePipeline.cpp:1437,1711-1720`；设计 §4.8 | 设计 §4.8 键改为 `(attacker, skill_id, source_entity, version_hash)`；S0-2/S0-3 冻结单写者模型（编译在派发前完成、执行段只读 const 引用）；新增缓存失效/淘汰测试 |
| S4 | 编译器 v1 只物化 `GetStatWithTags` 不够：另有 6 条直读通道（GlobalMore 999-1007、节点 More pow 1043-1050、cost affix 1052-1063、SkillModifier More 1068-1079、per-skill 特判 926-943/945-979/1098-1171/1252-1260、TriggerEff/endgame 589-604+664-668）；dual-run 无场景矩阵 | 同上（`DamagePipeline.cpp`） | 新增 P2-0 读取通道清单（file:line）；P2-3 场景矩阵；P2-7 技能2 回归门禁；必要时把技能2 一致性修复提前到 P2 |
| S5 | 版本戳接入面不足：transmuter 写入 4 处、`SkillModifierComponent` 就地增删、SoulEater/Avenger 栈、Channeling/Trance、endgame 加载、`StatsSystem` 自身缓存无版本；「无稳定变更点」退化策略循环定义 | `SkillSystem.cpp:2109,2112,2321,2353`；`SaveManager.cpp:253,257`；`BeamChannelDeliverySystem.cpp:696,1245,1493`；`BladeBoomerang.cpp:234`；`BladeFormation.cpp:272-273,320-321`；`PhantomTrance.cpp:549,608`；`RendingWave.cpp:248`；`EndgameModifierContract.hpp:91`；`StatsSystem.cpp:120-128` | S0-3 交付 file:line 接入点表 + 变更路径矩阵；复用 `StatsDirty`；无稳定点统一 bump 全局 epoch，禁止时间戳 |
| S6 | 测试锚定面低估：桩 ×1.05 期望还散落在 5+ 文件 | `DefenseMitigationChainTests.cpp:65-90`（期望 105）；`SkillSpecializationBakerTests.cpp:1238,1300-1303,1313-1315`；`MonsterAffixTests.cpp:350,643,651,683,784`；`BladeMasteryGameTests.cpp:81-89,159`；`BladeMasteryTests.cpp:439` | P0-4 扩展为全仓锚点清单（限定搜索模式），设计 §8.1 的「保留」分类同步修正 |
| S7 | 批量事件字段差异（enrichEvent/parent_skill_cd/trigger_depth）未登记 | 同 B3 | P1-3/P1-5 将差异写入 P1-delta；事件一致率度量从 P1 起纳入门禁 |
| S8 | 转换效率真实落点在 `BladeFormation` 召唤元素比例（`conv_efficiency_pct_per_point=10`、`convRatio=min(1,0.5+convBonus)`），不在 kernel 转换链；只迁 kernel 会漏节点真实效果 | `SkillDefs.hpp:1121`；`SkillSpecializationBaker.cpp:439-441`；`BladeFormation.cpp:233-236` | P3-4 明确包含 BladeFormation 召唤侧迁移，避免两处重复实现 |
| S9 | 快照落点未定义：DoT 若在每 tick 处取快照则「施加时快照」失效；黏爆在引爆时才计算武器伤害 | `AilmentEngine.cpp:580-600,711-722`；`ProjectileSystem.cpp:122-127,199-210` | P2-6：DoT 在效果创建处编译存快照；投射物/黏爆在生成时把快照存入组件，禁止引爆重算 |

## 3. 一般与建议问题

| # | 问题 | 处置 |
|---|------|------|
| G1 | P0 审计顺序应为「先审计后删桩」（已并入 B1-B2 处置） | P0-2/P0-3 |
| G2 | Q3 可延后与 P3 门禁冲突，需条件退出定义 | P3-10 条件退出 |
| G3 | UMR 迁移顺序/兼容窗口缺失；`build.bat` precheck 实际不含 modifier 校验；缺 `--check-determinism` 与 `check_monster_behavior_dispatch.py` | P3-1、§6.1 命令修正 |
| G4 | 性能门禁未覆盖帧时间（`CombatReleaseGateBenchmark` / `build.bat combat-gate`）；基线场景/命令/归档未定义 | P0-1、P4-4、§6.1 |
| G5 | 报告归档应与仓库惯例一致（评审报告 `docs/reviews/`，阶段证据 `docs/reports/<topic>/phase-PN/`） | §6.5、§9 |
| G6 | 新增测试命名可能落不进门禁：combat 套件按 `[Unit]*Combat*`/`[Integration]*Combat*` 过滤，现有伤害测试名不含 Combat | §6.3 增加命名/标签列 |
| G7 | 任务粒度与文件所有权不足以判定并行（混合任务、WS 级文件集） | §4 逐任务文件集；拆分 P1-8/P3-2/P3-7 |
| G8 | WS0 草图矛盾：`OpStage` 8 值 vs `stage_begin[7]`；ops 与物化数组双重表示 | 设计 §4.2 已修订（单数组 + `kOpStageCount` + 物化数组为派生） |
| G9 | 一致性小项：容差措辞、P4-4 依赖补 P4-3、模块清单补 `CombatConstants.hpp`、来源表补「地图/怪物词缀守方侧不变」、事件一致率提前、golden 数据放测试夹具、`ci` 标签描述 | 已在 R2 计划中逐条修正 |
| G10 | 0 分配断言范围（单目标 Execute）、`TryCompile` 溢出信号、`DamageProgram` const 引用传递、`sizeof(DamageOp)` 预算 | 设计 §4.2 已修订；P2-2 落地 |
| G11 | P4 删除 combat_v2 时需同步删除 CMake 过滤词条（`DamageKernelV2*`/`DamageKernelParity*`），避免空过滤假绿 | P4-1/P4-2 |
| G12 | 转换通道元数据缺失：`DamageModifier` 无 scope/source；节点 Convert 不乘 pts、More 乘 `pow(1+v,pts)` 的不对称必须保留 | P2-0/P2-1 锁定 |

## 4. 未经验证的技术假设（实施期需核实）

1. `skill_id=0` 全部调用点可经「回退技能 mult=0 或合成语义」修复，且无依赖技能 0 武器均值的空 base_pool 调用（需全仓扫描）。
2. DoT 是否完全不吃暴击/增伤乘区（计划原判定为「确认」，实为待核）。
3. 批量 crit÷100 是历史缺陷还是调用方约定（修复方向影响批量平衡）。
4. `kMaxDamageOps=64` 的容量覆盖（[PLACEHOLDER]）。
5. 浮点重排后 1e-3 等价容差对节点 `pow`、cost affix、TriggerEff 顺序的敏感性。
6. `EndgameModifierRuntimeComponent` 生产写入点当前不存在（仅测试），endgame 版本戳暂无实际失效源。

## 5. 处置映射（R1 → R2）

| 审查项 | 修订落点 |
|--------|----------|
| B1/B2 | 计划 P0-2/P0-3、P1-1；设计 §4.3 注 2 |
| B3/B4/S7 | 计划 P1-0/P1-5/P1-9、P2-4/P2-5；设计 §8.1 |
| S1/S2 | 计划 P0-5、§1.6、P4-2 兼容开关删除 |
| S3/S4/S5 | 设计 §4.2/§4.8；计划 S0-2/S0-3、P2-0/P2-3 |
| S8/S9/G12 | 计划 P2-0/P2-6、P3-4 |
| S6/G6/G7 | 计划 P0-4、§4、§6.3 |
| G2/G3/G4 | 计划 P3-1/P3-10、§6.1、P4-4 |
| G1/G5/G9/G10/G11 | 计划 §1.6/§6.5/§9、P2-2、P4-1 |

**后续**：R2 计划（`docs/plans/2026-09-11-damage-pipeline-modernization-plan.md`）已按本报告修订；设计文档同步修订 8 处（§4.2/§4.3/§4.8/§7/§8.1/§8.2/§9.2）。Q1–Q7 默认假设按 R2 §1.5 执行，若用户/主程调整，先改设计再改计划。
