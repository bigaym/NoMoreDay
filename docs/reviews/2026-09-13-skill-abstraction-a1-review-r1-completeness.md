# Skill 抽象与 B2 清理 · A1 批次审查报告（第一轮·完成性）

## 审查目标

只读审查计划 `docs/plans/2026-09-13-skill-abstraction-and-b2-cleanup-plan.md` 的 **A1 批次（A1-0 ~ A1-7）**（工作区未提交变更，baseline = `HEAD`）相对设计 `docs/designs/2026-09-13-skill-abstraction-and-b2-cleanup-design.md` §4.7 DoD#1~#9 的达成情况。重点：

1. DoD 逐条完成性 + A1-0~A1-7 任务真实性（对照 `git status` / `git diff` / 计划复选框）。
2. 测试质量（硬性否决项）：是否弱化/删除断言而非等价迁移；新增测试是否平凡/空洞；概率性测试是否可靠。
3. 未验证/延后项是否如实登记。
4. 文档（`docs/designs/2026-09-13-skill-baker-consumer-map.md`）与代码一致性。
5. 数据/契约（`assets/data/`、`gen_skill_contracts.py --check`）一致性。

## 结论

**修改**

理由：存在多项 Medium 级别的「计划/DoD 过度声明」与「测试名副其实性不足」，均落在本次硬性否决的测试质量与完成性判定范围内：

- **DoD#8（读点 API 单一）** 未达成却被 A1-1 勾选：全仓仍有 **62 处** `allocated_points.(find|contains|count)` 直查（基线 75），计划 §2.1 明确要求迁移的 UI 直查点（含 `GameUiSnapshotBuilder.cpp:423`）**未迁移**。
- **DoD#3（节点判定单源）** 未达成却被 A1-5 勾选：`BeamChannelDeliverySystem.cpp:165-166` 的 `Has7`（flags ∨ `GetSkill7Point`）正是本 DoD 点名的双源，且 B2-20 映射表 §7.4 自述该双判定「属 A1-1 helper 迁移范围」——但 A1 未收敛。
- **`tests/unit/SkillBakerFlagConsumerTests.cpp` 不名副其实**：计划 T6.2 目标是「每个 Baker flag 至少一个消费点（防写而不读回归）」，实际测试只断言「新位必须登记」；`consumer` 列仅做非空检查，4 个死位（bit24/26/27/28）登记为「无直接读取方」仍通过。
- **`tests/unit/SpecStateMappingTests.cpp`** 为自述「手写镜像」，物理上无法检出生产漂移，DoD#7 证据不足。

无 Blocker（无安全/数据丢失/契约破坏）；核心拆解逻辑（Channeling 拆除、字段删除、数据归位）实现正确且大体等价迁移。

## 审查轮次

第一轮 · 完成性（不涉及运行期性能实测与运行期行为验证）

## 输入

- 计划：`docs/plans/2026-09-13-skill-abstraction-and-b2-cleanup-plan.md`（659 行）
- 设计：`docs/designs/2026-09-13-skill-abstraction-and-b2-cleanup-design.md`（DoD §4.7 / §5.3）
- 映射表：`docs/designs/2026-09-13-skill-baker-consumer-map.md`（511 行）
- 性能基线：`docs/performance/2026-09-13-skill-abstraction-baseline.md`
- 工作区 diff：`git diff HEAD -- tests/ assets/ docs/ src/`（baseline `HEAD=dbda488d`）
- 独立复核命令：`rg`、`git diff --stat`、`python scripts/gen_skill_contracts.py --check --check-idempotency --check-determinism`

## 变更文件边界

工作区未提交变更：源码 28 文件（+551/-725）、测试 14 文件（+382/-245）、数据 2 文件、计划/文档若干；新增未跟踪文件 3 个测试 + 3 个文档（design / consumer-map / plan）。范围与「A1 批次」一致，未见越界改动（无编译产物、无临时文件入库）。

- 未跟踪新测试：`tests/unit/SpecStateMappingTests.cpp`、`tests/unit/SkillBakerFlagConsumerTests.cpp`、`tests/unit/SkillMechanicsRegistryTests.cpp`。
- 新增文档：设计、映射表、计划、性能基线。

## 范围对齐

- A1-0~A1-7 任务项均已勾选，但 **A1-3 的 3 条完成标准复选框仍为 `[ ]`**（DoD#5 / 编译+`rg`=0+`ctest` / 基线复跑）——属如实未勾选。
- A2-1~A2-4（含 DoD#4）明确 `[ ]` 延后，**不属 A1 范围**。
- 未发现「A1 范围外代码被改动」。

## 质量与风险评估

### 正面的点

- **Channeling → BeamChannel 迁移忠实**：`ChannelingComponent` 全仓 `rg` 归零；`BeamChannelComponent` 承接 `channel_timer/conversion_tag/bonus_crit_chance/bonus_armor_pen`（`DeliveryArchetypes.hpp:170-173`）；A1-3 6 个测试文件的断言均由 beam 等价承接，且 `SkillKeyNodeMatrixIntegrationTests.cpp:132-134` 额外新增 `mode == BarrageChannel::BarrageEmitter` 断言（**加强**）。
- **OrbitingSentinel 拦截用例删除合法**：拦截分支被物理删除（`OrbitingSentinelDeliverySystem.cpp` 删除 `s_intercepted_projectiles`/dice-roll 段，`OrbitingSentinelComponent.interception_chance` 删除），测试随功能删除而删除，非削弱。
- **数据单源**：`assets/data/skill_mechanics.json:319-336` 补齐 `ward_per_block_per_point=10.0`、`intent_chance_per_point=0.15`、`dodge_speed_per_point=1.0`，与 `SkillSystem.cpp:1119-1135` 的 `GetMech(...,默认值)` 数值等价；`skill_4_tree.json` 29 节点 `max_points` 与 `skills.json` 由单测 `SkillSpecializationBakerTests`→`SkillPrerequisiteRequiredPointsTests.cpp:918-937` 校验。
- **契约校验通过**：独立复跑 `python scripts/gen_skill_contracts.py --check --check-idempotency --check-determinism` → `[OK] skill_contract blocks are up to date.` exit 0。
- **DoD#2 达成**：`rg 'id\.find' / 'strcmp' / '==' 字符串比较` 在 `behaviors/` 与 `DamageConditions.cpp` 均为 0；`DamageConditions.cpp:11-16` 改用 `effect.kind == BuffKind::Slow`。
- **未验证项登记诚实**：性能基线文档标题即标注「**未验证·延后**」并说明无可用运行环境；计划 `:251` 明确「运行时『观察一拍』因无运行环境标注未验证」。
- **映射表抽查与代码一致**：`DamageMitigationService.cpp:141/169/318`、`BladeFormation.cpp:111-158`、`FlowingThrust.cpp` 等消费点行号经复核一致。

### 风险

- **完成性判定依赖自述**：本轮禁止编译，测试「全绿」为计划自述（`:336`），未独立复现；`SkillSystemTests.cpp:1269` 曾出现一次跨用例污染型偶发失败（计划自述后续 8 次复跑全绿），残留抖动风险未闭环。
- **DoD 全局目标与 A1 局部达成混淆**：DoD#1/#2/#3/#7/#8 为「全仓」目标，A1 只做了部分文件；计划 §5.3 将 #8 挂 A1-1、#3 挂 A1-5 且未标「部分」，易被误读为已达成。

## 发现项

> 标记：🔴 Blocker / 🟡 Medium / 💭 Nit

### 🟡 M1 — DoD#8 过度声明：全仓仍有 62 处 `allocated_points` 直查，UI 点未迁移

- path: `docs/plans/2026-09-13-skill-abstraction-and-b2-cleanup-plan.md:188`（A1-1 勾选 DoD#8）、`:52`（§2.1 要求 UI 迁移）、`:546`（DoD#8 全文）
- 证据（独立 `rg`，行数 = 62，基线 = 75）：
  - `src/game/application/ui/GameUiSnapshotBuilder.cpp:423`
  - `src/game/application/ui/UISkillSpecRenderer.cpp:58,497,509,550`
  - `src/game/application/ui/UISkillTalentTree.cpp:107,412,736`
  - `src/game/systems/combat/AilmentEngine.cpp:57`、`DamageMitigationService.cpp:323`、`DamagePipeline.cpp:561`
  - `src/game/systems/skill/behaviors/BladeFormation.cpp:85-102`（18 处）、`BladeBoomerang.cpp:152,304`、`FlowingThrust.cpp:190,256,366,994`、`MindBlade.cpp:54-55`、`BladeWard.cpp:69`、`HeavenlySwordDescent.cpp:153`、`BloodSea.cpp:77`、`SevenStarSlash.cpp:168`
- 问题：A1-1 复选框将 DoD#8 记为达成（未标「部分」），但计划 §2.1 明确点名待迁移的 UI 文件仍在直查，且行为文件仍有大量直查。
- 建议：将 A1-1 的 DoD#8 复选框改标「部分」，或在 A1/A2 中补 UI + 行为文件迁移任务；在 §5.3 明确标注为跨 wave 目标。

### 🟡 M2 — DoD#3 过度声明：`Has7` 双通道与 profile/active_nodes 双源仍存

- path: `src/game/systems/skill/BeamChannelDeliverySystem.cpp:165-166`；`docs/plans/...plan.md:376`（A1-5 勾选 DoD#3）
- 代码：
  ```cpp
  auto Has7 = [&](uint32_t bit, uint32_t node) {
    return (flags & bit) != 0u || GetSkill7Point(registry, caster, node) > 0;
  };
  ```
- 问题：这正是 DoD#3「同一节点同时用 flags 与 allocated_points 两处判定」的点名对象；B2-20 映射表 §7.4 自述「其收敛属 A1-1 helper 迁移范围」，但 A1 未收敛。此外 `BladeFormation.cpp:111-158` 存在 `profile→feature_flags` 与 `exec.active_nodes.test(...)`/`allocated_points` 的双判定，`:85-102` 仍直读 `allocated_points`。
- 建议：A1-5 的 DoD#3 改标为未达成/部分，或将其收敛明确排入 A2-1；同步修订映射表 §7.4 的归属说明。

### 🟡 M3 — `SkillBakerFlagConsumerTests.cpp` 不名副其实（消费点存在性未断言）

- path: `tests/unit/SkillBakerFlagConsumerTests.cpp:4-13`（自述「防止『写而不读』回归」）、`:155-172`（4 个死位 consumer 标注）、`:197-215`（`AllProducedBitsRegistered` 仅查登记）
- 问题：计划 T6.2（`plan.md:386`）目标为「每个 Baker flag **至少一个消费点**」，但测试只保证「新位必须登记」；`FlagSpec.consumer` 仅在 `:328` 做 `!= nullptr` 检查，4 个全死位（`16777216/67108864/134217728/268435456`，即 bit24/26/27/28）以「无直接 feature_flags 读取方」登记仍通过。映射表 §7.3 自己建议「为每个 Baker flag 位断言消费列非空」——测试未落实。
- 建议：二选一：(a) 增加消费点存在性校验（由 `rg` 生成的消费清单或逐位显式期望消费点）使死位显式失败/白名单；(b) 将测试更名并降级为「Baker flag 登记/掩码守护」，修正计划 T6.2 的表述。作为硬性否决项，至少需明确「消费点守护」尚未实现。

### 🟡 M4 — `SpecStateMappingTests.cpp` 为手写镜像，无法检出生产漂移（DoD#7 证据不足）

- path: `tests/unit/SpecStateMappingTests.cpp:1-20`（自述局限）、`:103-150`（镜像实现）、`:265-531`（用例）
- 问题：测试内 `ResolveSpecStateBaseline` 是生产 `SevenStarSlash.cpp` 匿名命名空间逻辑的**副本**，断言对象是副本自身；即使生产映射改变，测试仍绿。作为 DoD#7「每技能 allocated_points→SpecState 字段映射有表驱动单测」的落地，只能证明「意图映射表内部自洽」，无法守护生产。
- 建议：DoD#7 改标「有条件达成」；A2-1 必须把 `ResolveSpecState` 与节点常量暴露为可测符号/生成物后，改为「镜像 == 生产」等价断言。过渡期可在文件中显式声明「非生产漂移守卫」，避免误导。

### 🟡 M5 — missing key 策略与设计冲突：设计要求加载期断言，实现为运行期静默回退

- path: `docs/designs/2026-09-13-skill-abstraction-and-b2-cleanup-design.md:178`（"missing key 策略：加载期断言失败…运行期不静默返 0"）；`src/game/foundation/data/SkillMechanicsRegistry.cpp`（`GetFloatImpl` 缺失 key 直接 `return default_value`）；`tests/unit/SkillMechanicsRegistryTests.cpp:84-97`（`Missing Entry Returns Default` 固化该回退）
- 问题：实现把缺失键的兜底放在运行期返回值，正是设计禁止的「静默」行为（`GetMech(4,470,"base_damage",35.0f)` 若键缺失会静默取 35.0，掩盖调平错误）。加载期校验只覆盖「必需技能 id 缺失/叶子非数字」，未覆盖具体机制键。
- 建议：要么在加载期对「被 `GetMech` 消费的机制键」做必填 schema 校验（键缺失即 `LoadFromFile` 失败），要么修订设计 §4.4/计划 T1.3 表述为「缺失键回退调用方默认值」，并在评审记录中留痕。

### 🟡 M6 — 计划自相矛盾：`--gen-specstate` 生成器挂在 A1-1 但无对应任务，且脚本未实现

- path: `docs/plans/...plan.md:612`（SpecState 行："A1-1 生成器改造"）、`:136-151`（A1-1 任务 T1.1~T1.6，无生成器任务）
- 证据：`rg "gen-specstate|specstate|SpecState" scripts/gen_skill_contracts.py` 无命中——生成器未实现；T6.5 因此退化为手写镜像（见 M4）。
- 建议：在 §8 待确认或 A2 任务中显式登记「`--gen-specstate` 生成器未产出」为延后项，并修正 §612 的归属，避免「A1-1 已含生成器改造且已完成」的误读。

### 💭 L1 — `AddTalentPoint` 新增 `tree->nodes.at(pre_id)` 可能抛异常

- path: `src/game/systems/skill/SkillSystem.cpp:2169`
- 问题：对损坏数据（前置节点不在树中）`std::map::at` 会抛 `std::out_of_range`；虽然当前数据经 `SkillPrerequisiteRequiredPointsTests.cpp:883-885` 校验，但运行期健壮性欠佳。
- 建议：用 `find` 判定后跳过/告警，避免异常穿透。

### 💭 L2 — 计划引用不存在的符号 `SkillSystem::AllocateSkillNode`

- path: `docs/plans/...plan.md:350`
- 问题：M8 诊断实际落在 `SkillSystem::AddTalentPoint`（`SkillSystem.cpp:2116` 起，`:2165-2184`）；`rg "AllocateSkillNode" src` 无命中。
- 建议：修正计划行号为真实函数/位置。

### 💭 L3 — 435 概率测试只断言「发生过」，掩盖触发率回归

- path: `tests/integration/SkillSystemTests.cpp:1339-1349`
- 评估：128 次重复派发在 45% 命中下 P(全不触发)=0.55^128≈1e-33，**存在性判定可靠**；但若触发率回归到 ~5%，P(通过)≈0.95^128≈99.86%，测试仍绿——无法守住概率量级。RNG 未固定种子。
- 建议：改为可注入/固定种子的 RNG 后断言期望命中区间，或在测试中直接断言 `intent_chance_per_point` 数值（单源校验）+ 少量确定性触发。

### 💭 L4 — 旧 backlog 的 B2 项已完成但仍未勾选

- path: `docs/plans/2026-09-12-skill1-9-followup-backlog.md:107-117`（B2-05/B2-06/B2-07/B2-15 等仍 `[ ]`）
- 问题：A1 已实现 B2-05/06/07/10/13/14/15/17/18/19/20/21/24 等，backlog 未同步（新增计划另行追踪，存在双账风险）。
- 建议：在新计划销项或 backlog 备注「由 A1 承接」。

### 💭 L5 — 新测试依赖传递包含 `nlohmann::json`

- path: `tests/unit/SkillPrerequisiteRequiredPointsTests.cpp:923-926`（使用 `nlohmann::json`），文件未直接 `#include <nlohmann/json.hpp>`
- 建议：显式包含，避免头文件依赖变化导致编译脆弱。

### 💭 L6 — 少数测试判定条件被收窄

- path: `tests/functional/SwordArrayNodes.cpp:408`（弃用 `managed_ailment || id.find("Slow"/"slow")`）；`tests/functional/BladeFormationNodes.cpp:808`（弃用 `id.find("shock")`）
- 评估：生产侧已统一用 `BuffKind`/`BuffType`，收窄后仍等价；但若存在仅靠 legacy id 命中的路径会漏检，建议随映射表复核确认。

## 被削弱/删除测试清单（path:line + 是否有等价承接）

| # | 位置 | 删除内容 | 承接 | 判定 |
|---|---|---|---|---|
| 1 | `tests/unit/SkillSpecializationBakerTests.cpp`（原 `OrbitingSentinelDeliverySystem - Interception Dice Roll & Cap`，5 个子用例 ~141 行） | 整个拦截测试 | 无（功能物理删除） | 合理删除 |
| 2 | 同上（原 :44/:70/:221） | 3 处 `primary_archetype` CHECK | 2 处改 `(feature_flags & 1)`；技能1 Mobility 无承接（字段删除） | 合理（字段删除） |
| 3 | `tests/functional/MindBladeNodes.cpp`（用例1/2/5 等多处） | `chan->skill_id/channel_timer/tick_interval/target_pos.x/conversion_tag` 与 `ChannelingComponent` 空判 | beam 等价断言（`tick_interval` 保留于 :165；target_pos.x/y、conversion_tag、channel_timer 均有 beam 断言） | 等价迁移 |
| 4 | `tests/functional/InfiniteBladesNodes.cpp`（H1/H2/H4/C7） | `chan->*` 断言 | beam 等价（channel_timer/tick_interval/is_empowered/bonus_damage_mult/bonus_crit_chance） | 等价迁移 |
| 5 | `tests/functional/SkillBehaviors.cpp`（:1019/:1035） | `ChannelingComponent` 引用 | 改 `BeamChannelComponent` | 等价迁移 |
| 6 | `tests/unit/SkillBehaviorGuardTests.cpp`（:262/:451/:2354/:2370） | `ChannelingComponent` 引用 | 改 `BeamChannelComponent` | 等价迁移 |
| 7 | `tests/integration/SkillKeyNodeMatrixIntegrationTests.cpp`（技能5/7） | `ChannelingComponent` 断言 | Beam 等价 + **新增 mode 断言** | 等价/加强 |
| 8 | `tests/integration/SkillSystemTests.cpp`（:374/:838/:1404/:1448） | `Channeling`/`ReactiveWard` 断言 | Beam 等价；ReactiveWard 无承接（组件删除） | 等价/合理删除 |
| 9 | `tests/integration/DeliveryArchetypesTests.cpp`（ReactiveWard 生命周期子用例） | ReactiveWard 用例 | 无（组件删除） | 合理删除 |
| 10 | `tests/functional/BladeFormationNodes.cpp:764/808` | `id.find("ignite"/"shock")` 分句 | `BuffKind::Ignite` / `BuffType::Shock` | 等价（收窄，见 L6） |
| 11 | `tests/functional/SwordArrayNodes.cpp:408` | `managed_ailment || id.find("Slow"/"slow")` | `kind == BuffKind::Slow` | 等价（收窄，见 L6） |

**结论：未发现以「删除/弱化」替代等价迁移的硬性违规**；唯一整用例删除有明确的功能删除依据。

## 平凡测试评估

| 测试 | 类型 | 评估 |
|---|---|---|
| `tests/unit/SpecStateMappingTests.cpp` | 自述手写镜像 | **实质平凡**：断言对象是测试内副本，无法检出生产漂移；仅证明映射表内部自洽。有文档价值，但作为 DoD#7 证据不足（M4）。 |
| `tests/unit/SkillBakerFlagConsumerTests.cpp` | 登记守护 + 全树 bake | **部分名副其实**：全树逐节点 bake 的掩码/代表节点断言有真实约束力；但「每 flag 有消费点」未断言，consumer 列形同注释（M3）。 |
| `tests/unit/SkillMechanicsRegistryTests.cpp` | 生产加载/校验测试 | **名副其实**：直接驱动 `LoadFromFile`，覆盖必需技能缺失、非数字键、非对象节等，有效。 |
| `tests/unit/SkillPrerequisiteRequiredPointsTests.cpp`（新增 3 用例） | 生产分配器 + 数据单源 | **名副其实**：M8 不可达前置拒绝、技能4 可达性/无环、`skill_4_tree.json` 与 `skills.json` max_points 一致，均有真实约束力。 |
| `tests/functional/BladeFormationNodes.cpp`（373 空间网格等价） | 线性 vs 网格双跑对比 | **强**：两路径命中集合逐项对比，非空洞。 |
| `tests/functional/FlowingThrustNodes.cpp`（172/173）、`RendingWaveNodes.cpp`（235） | 生产行为断言 | **有效**：覆盖 kind/source_skill_id/契约上限等真实语义。 |

## 未验证项登记情况

| 未验证项 | 是否登记 | 位置 / 说明 |
|---|---|---|
| T0.2 性能基线 | ✅ 如实登记 | `docs/performance/2026-09-13-skill-abstraction-baseline.md` 标题「未验证·延后」，计划 A1-0 注记；仅静态观测值（直查 75 等）。 |
| T3.1 运行期「观察一拍」 | ✅ 如实登记 | 计划 `:251` 明确「因无运行环境标注未验证」，A1-3 DoD 复选框留空。 |
| T6.5 SpecState 生成器未产出 | ⚠️ 部分登记 | 计划 `:389` 记「若生成器已产出」+ 已知限制；但 §612 仍把生成器改造挂在 A1-1（无任务、脚本未实现），缺显式延后登记（M6）。 |
| A1-3 DoD 基线复跑 | ✅ 未勾选 | `plan.md:294` 保持 `[ ]`。 |
| T5.6 符号名 `AllocateSkillNode` | ❌ 未登记 | 计划引用不存在符号（L2）。 |

## 最佳实践建议

1. **DoD 勾选需附可复核命令与当前数值**：如 DoD#8 应附 `rg -c` 前后值（75→62），复选框标注「部分」，避免全局目标被局部达成覆盖。
2. **「消费点守护」测试应可失败**：对映射表登记的每个 flag 构造「期望消费点集合」，缺失即失败；死位走显式白名单，杜绝「登记即通过」。
3. **镜像测试须绑定生产符号**：无法直调时应暴露最小可测接口（如把 `ResolveSpecState` / 节点常量放入命名头），否则应在测试名与注释中显式声明「非生产漂移守卫」。
4. **概率测试固定种子**：以可注入 RNG 或固定种子 + 期望区间替代「重复到必然发生」，既稳定又能守住量级。
5. **计划与 backlog 单账**：已完成的 B2 项在 backlog 同步销项或注明承接计划，避免双账漂移。
6. **加载期校验与运行期兜底语义统一**：将「设计禁止静默回退」与「实现默认值兜底」的取舍写入设计 §4.4 或评审记录。

## 剩余风险

- 未编译、未运行测试：所有「全绿」结论依赖计划自述；`SkillSystemTests.cpp:1269` 曾偶发跨用例污染，未在本轮复核。
- 运行期行为（引导松手/计时结束、元素转换、反击数值）未实测；仅静态等价论证。
- `Has7` 双通道与 UI 直查点在 A2 收敛前，DoD#3/#8 的守护缺位。
- 消费点映射表为手工快照，缺自动化守护，随时间可能漂移。

## 下一步

1. 主控裁决 M1~M6：优先修正计划复选框与 DoD 表述（M1/M2/M6），使其与真实达成状态一致；此为「修改」结论的核心闭环项。
2. 补强测试（M3/M4）：为 `SkillBakerFlagConsumerTests.cpp` 增加消费点断言（或降级更名），为 `SpecStateMappingTests.cpp` 在 A2-1 前标注非生产守卫。
3. 明确 missing key 策略（M5）：实现加载期必填键校验，或修订设计并留痕。
4. 修正 L1/L2/L3/L4/L5/L6 低危项。
5. 修正后进入第二轮（运行期/性能）审查；A1-3 基线复跑与「观察一拍」需在具备运行环境时补做。
