# 技能 10~12 专精抽象化与模块化迁移设计规范（Track A-02）

- 日期：2026-09-14
- 状态：草案（待实施计划落实与用户确认）
- 依据标准：`docs/workflows/design.md`
- 前置输入：
  - `docs/designs/2026-09-14-skill-abstraction-track-a01-design.md`（Track A-01 总体架构设计）
  - `docs/plans/2026-09-14-skill-abstraction-track-a01-plan.md`（Track A-01 实施计划及验收基线）
  - `docs/reviews/2026-09-14-skill-abstraction-a01-review.md`（A-01 复审报告，特别是 H-2、L-2 遗留）
  - `docs/plans/2026-09-12-skill1-9-followup-backlog.md`（A-02 任务项与依赖拓扑）
- 涉及技能：
  - 技能 10：七星斩（`SevenStarSlash`）
  - 技能 11：天剑降临（`HeavenlySwordDescent`）
  - 技能 12：血海（`BloodSea`）

---

## 1. 目标与非目标

### 1.1 核心目标（Why & What）

1. **消除全仓手写绑定结构残留，达成真正的 D-A5 闭环**：
   - A-01 周期中，技能 1~9 均已迁移至统一的 `SpecStateTable<State>` 模板体系，但由于 `D7=b`（技能 10~12 仅回归、不重做），技能 10~12 保留了各自手写的 `PointBinding`、`FlagBinding`、`MechBinding` 结构（见 `SevenStarSlashShared.hpp:318/323`、`HeavenlySwordDescent.hpp:110/115/121`、`BloodSea.hpp:132/137/143`），造成复审报告 H-2 的“D-A5 仅在迁移范围内达标，全仓仍存手写例外”。
   - 本 Track（A-02）正式退役这 3 组手写绑定结构，实现全仓 12 技能 100% 统一由 `SpecStateTable.hpp` 泛型模板驱动。
2. **统一效果层 SpecState 纯粹性（达成 D-A1 统一标准）**：
   - 技能 11（`HeavenlySwordCastSpec`）与技能 12（`BloodSeaCastSpec`）目前存在历史架构异味：结构体内混杂了大量机制系数 `float` 字段（技能 11 含 20 个机制系数、技能 12 含 40 个机制系数），并通过 `MechBinding` 在循环中反射赋值。
   - 这违背了 A-01 确立的 `D-A1` 核心准则（“每技能唯一 POD SpecState，仅包含投入点数 `int` 与点亮标志 `bool`，无机制数值与交付参数混杂”）。
   - 本 Track 将技能 10~12 全面信封化为纯粹的 POD SpecState（直接对接生成头 `*Gen`），机制系数剥离并统一在行为层消费点按需通过 `GetMech` 读取（对齐技能 1~10 的标准模式）。
3. **退役手写解析器，收敛解析入口**：
   - 将手写的 `SevenStarSlashShared.hpp::ResolveSpecState`、`HeavenlySwordDescent.hpp::ResolveHeavenlySwordCastSpec`、`BloodSea.hpp::ResolveBloodSeaCastSpec` 统一改由 `skills::ResolveSpecState(registry, owner, skillId, table)` 驱动。
4. **闭环 A-01 复审遗留项 L-2**：
   - 将 `SpecStateTableTests.cpp` 与 `GeneratedSpecStateTests.cpp` 中技能 10~12 的测试从“对拍手写绑定表”升级为“对拍独立期望表 + 直接断言运行期 `ReadPoints`/`HasNode`”，消灭测试对已废弃手写表的依赖。

### 1.2 明确非目标（Non-Goals）

1. **不修改底层机制数值与 GDD 设计**：本次为纯架构层重构与绑定层收敛，不变更各技能节点伤害公式、持续时间、冷却时间等任何玩法数值。
2. **不重构场地核心逻辑**：`HeavenlySwordFieldComponent` 与 `BloodSeaFieldComponent` 的场地生命周期、脉冲结算与实体交互已在 B2 阶段收敛至 `PersistentFieldHeader`，本 Track 保持场地组件结构与行为逻辑不变。
3. **不改变 Baker 的 switch 架构**：`SkillSpecializationBaker.cpp` 目前仅覆盖技能 1~9，技能 10~12 无 Baker 专精写入点，本 Track 不在 Baker 盲目增加 10~12 的空桩分支。

---

## 2. 架构设计与契约收敛

### 2.1 体系分层对照

重构前后对比架构：

```text
【当前现状（A-01 后期过渡态）】
技能 1~9  ──▶ [SpecStateTable<State>] ──▶ 生成头 (*SpecState.gen.hpp) ──▶ DoCast 消费
技能 10   ──▶ [SevenStarSlashShared.hpp 手写 Point/FlagBinding] ──▶ 手写 ResolveSpecState
技能 11   ──▶ [HeavenlySwordDescent.hpp 手写 Point/Flag/MechBinding] ──▶ 手写 ResolveHeavenlySwordCastSpec
技能 12   ──▶ [BloodSea.hpp 手写 Point/Flag/MechBinding] ──▶ 手写 ResolveBloodSeaCastSpec

【Track A-02 目标架构（全仓统一终态）】
技能 1~12 ──▶ [SpecStateTable<State> (SpecStateTable.hpp)]
                 │
                 ├──▶ 生成头 (*SpecState.gen.hpp)
                 │      ├── *NodesGen (节点常量)
                 │      ├── *SpecStateGen (纯 POD：int 点数 + bool 标志)
                 │      └── k*TableGen (SpecStateTable 静态实例)
                 │
                  └──▶ 行为层 (DoCast/DoHit)
                        ├── ResolveState(registry, owner) ──▶ 调统一 ResolveSpecState
                        └── 机制数值 ──▶ 经 GetMech(skill_id, node_id, key, default) 按需读取
```

消费侧包装命名（现状与目标一致，无需额外统一）：
- 技能 1~9：行为 `.cpp` 匿名命名空间内 `ResolveState(registry, owner)` 单行调模板（如 `SwordArray.cpp:39`）；
- 技能 10：共享头内 2 参 `ResolveSpecState`（含转质覆盖，行为与功能测试共用，见 §2.2）；
- 技能 11/12：头内单行 `ResolveHeavenlySwordCastSpec` / `ResolveBloodSeaCastSpec`（见 §2.3/§2.4）。

### 2.2 技能 10（七星斩 SevenStarSlash）迁移设计

1. **Descriptor 完善（`skill_10.json`）**：
   - 现有的 `assets/data/skill_specstate/skill_10.json` 包含 17 条 point 绑定与 6 条 flag 绑定，但缺少转质节点 1021（`PoleStarOrbit`）与 1022（`Starfall`）的成员映射。
   - 规则变更：在 `skill_10.json` 中将 1021 与 1022 补充为 `kind: "flag"` 绑定（成员名 `poleStarOrbit`、`starfall`）。
   - 重新执行 `--gen-specstate`，使 `SevenStarSlashSpecStateGen` 原生具备 `bool poleStarOrbit` 与 `bool starfall` 成员，`kSevenStarSlashFlagBindingsGen` 数组由 6 项扩至 8 项。
   - 合法性依据：`scripts/gen_skill_contracts.py::_validate_specstate_model`（约 :909-917）已显式授权转质节点以 flag 绑定入表（"skill 10 在绑定循环外经 GetActiveTransmuterNode 处理，skill 12 走 flag 绑定，两种约定都合法"），本次变更无需改动生成器。
   - 节点 1014（`DipperHandleReturn`）保持非绑定：它是非点读节点（`SevenStarSlash.cpp` 经 `GetAllocatedPoints` 直读），生成结构中不产生成员，实施时不得顺手为其补绑定。
2. **转质互斥处理与解析入口**：
   - `SevenStarSlashShared.hpp` 新增 include：`#include "game/systems/skill/behaviors/generated/SevenStarSlashSpecState.gen.hpp"`（其已传递包含 `SpecStateTable.hpp`）。
   - 包装函数与现实现同名同签名（2 参），与统一模板 4 参 `ResolveSpecState<State>` 构成合法重载（参数个数不同，无二义性）：
     ```cpp
     namespace SevenStarSlashNodes = SevenStarSlashNodesGen;
     using SevenStarSlashSpecState = SevenStarSlashSpecStateGen;

     [[nodiscard]] inline SevenStarSlashSpecState ResolveSpecState(
         const entt::registry &registry, entt::entity owner) {
       auto state = ResolveSpecState(registry, owner,
           seven_star_shared::kSevenStarSlashSkillId, kSevenStarSlashTableGen);
       // 转质节点（1021/1022）的表内 HasNode 填充仅为占位，最终语义一律以
       // GetActiveTransmuterNode 的互斥单选为准，此处对两字段无条件覆盖。
       const uint32_t active = SkillSystem::GetActiveTransmuterNode(
           registry, owner, seven_star_shared::kSevenStarSlashSkillId);
       state.poleStarOrbit = (active == SevenStarSlashNodes::PoleStarOrbit);
       state.starfall = (active == SevenStarSlashNodes::Starfall);
       return state;
     }
     ```
   - 等价性论证：迁移前 1021/1022 完全不经 `HasNode` 填充，仅由 `GetActiveTransmuterNode` 判等赋值；迁移后表内填充是临时占位，随即被同样的判等赋值无条件覆盖——两字段最终值在任何「点数/激活」组合下都与迁移前一致。
   - 风险与守门：`kSevenStarSlashTableGen` 的 flag 段自此含 1021/1022 的 HasNode 占位语义。**任何绕过本包装函数、直接用该表解析出的 state 的代码，不得读取 `poleStarOrbit`/`starfall`**（其含义是「已投入点数」而非「转质激活」）；当前全仓唯一消费者即本包装函数与功能测试。
3. **退役清理**：
   - 删除 `SevenStarSlashShared.hpp` 中的 `SevenStarSlashPointBinding`、`SevenStarSlashFlagBinding`、`kSevenStarSlashPointBindings`、`kSevenStarSlashFlagBindings` 以及手写 `struct SevenStarSlashSpecState`。
   - 删除手写节点常量命名空间 `namespace SevenStarSlashNodes { ... }`（`SevenStarSlashShared.hpp:258-284`），以 `namespace SevenStarSlashNodes = SevenStarSlashNodesGen;` 别名承接，`SevenStarSlash.cpp` 等既有引用点零改动。
   - 同步更新 `src/game/systems/skill/SpecStateTable.hpp:10-12` 的约束注释：技能 10 的 1021/1022 由「循环外叠加、不入表」改为「flag 入表占位 + 包装函数以活跃转质判等覆盖」，避免架构注释与代码互相矛盾。
   - 保留 `seven_star_shared` 命名空间下的公共辅助函数（`RefundSkillCooldownPercent` 等）。

### 2.3 技能 11（天剑降临 HeavenlySwordDescent）迁移设计

1. **Descriptor 与生成物**：
   - `assets/data/skill_specstate/skill_11.json` 已有完整 16 point + 9 flag 绑定，生成的 `HeavenlySwordCastSpecGen` 已经包含全部 25 个节点的点读与点亮字段，无需重新生成。
2. **分离机制系数，恢复 POD 纯粹性**：
   - `HeavenlySwordDescent.hpp` 新增 include：`#include "game/systems/skill/behaviors/generated/HeavenlySwordDescentSpecState.gen.hpp"`。
   - `HeavenlySwordCastSpec` 改为类型别名 `using HeavenlySwordCastSpec = HeavenlySwordCastSpecGen;`。
   - 删除 `HeavenlySwordPointBinding`、`HeavenlySwordFlagBinding`、`HeavenlySwordMechBinding`。
   - 删除 `kHeavenlySwordPointBindings`、`kHeavenlySwordFlagBindings`、`kHeavenlySwordMechBindings`。
   - 删除手写节点常量命名空间 `namespace HeavenlySwordNodes { ... }`（`HeavenlySwordDescent.hpp:18-44`），以 `namespace HeavenlySwordNodes = HeavenlySwordDescentNodesGen;` 别名承接。
   - `ResolveHeavenlySwordCastSpec` 实现简化为单行：
     ```cpp
     [[nodiscard]] inline HeavenlySwordCastSpec ResolveHeavenlySwordCastSpec(
         const entt::registry &registry, const entt::entity owner) {
       return ResolveSpecState(registry, owner, kHeavenlySwordSkillId, kHeavenlySwordDescentTableGen);
     }
     ```
   - 点数语义差异收敛：手写解析对 points 做 `std::max(0, ReadPoints(...))` 防御性 clamp（`HeavenlySwordDescent.hpp:245`），统一模板无 clamp。按 §2.5 决策在统一模板补 clamp，包装函数不改语义。
3. **机制系数在消费点（`DoCast`）按需读取**：
   - 在 `HeavenlySwordDescent.cpp::DoCast` 中，此前通过 `spec.<mechCoeff>` 读取的 20 个机制参数，改为统一在函数入口处直接使用 `GetMech(kHeavenlySwordSkillId, node, key, default)` 声明为本地 `const float`（结构与 `SevenStarSlash.cpp:365-416` 一致）；`MechBinding` 表中的 default_value 字面量随迁移移入消费点，`skill_mechanics.json` 仍是数值单一来源。
   - **技能级参数回退字段同步退役**：`impactRadiusFallback`、`fieldRadiusFallback`、`fieldDurationFallback`、`tierDamageBonusFallback`、`tierRadiusBonusFallback`（`HeavenlySwordDescent.hpp:101-105`，供 `skills.json` params 缺失时 `GetParam` 兜底）不属于任何节点绑定，生成 POD 中不存在对应成员；其全部消费点位于 `DoCast`（`HeavenlySwordDescent.cpp:492-505`），迁移为 `DoCast` 内本地 `constexpr float` 具名常量，字面量与迁移前字段默认值逐项一致（如 `tierDamageBonusFallback = 0.18f`）。
   - 运行时开销：`GetMech` 内部为平坦数组查找，每施法调用仅一次，零堆分配，且完全消除了原解析循环中的指针间接赋值。

### 2.4 技能 12（血海 BloodSea）迁移设计

1. **Descriptor 与生成物**：
   - `assets/data/skill_specstate/skill_12.json` 已有完整 18 point + 7 flag 绑定，生成的 `BloodSeaCastSpecGen` 已经包含全部 25 个节点的点读与点亮字段，无需重新生成。
2. **分离机制系数，恢复 POD 纯粹性**：
   - `BloodSea.hpp` 新增 include：`#include "game/systems/skill/behaviors/generated/BloodSeaSpecState.gen.hpp"`。
   - `BloodSeaCastSpec` 改为类型别名 `using BloodSeaCastSpec = BloodSeaCastSpecGen;`。
   - 删除 `BloodSeaPointBinding`、`BloodSeaFlagBinding`、`BloodSeaMechBinding`。
   - 删除 `kBloodSeaPointBindings`、`kBloodSeaFlagBindings`、`kBloodSeaMechBindings`。
   - 删除手写节点常量命名空间 `namespace BloodSeaNodes { ... }`（`BloodSea.hpp:18-44`），以 `namespace BloodSeaNodes = BloodSeaNodesGen;` 别名承接。
   - `ResolveBloodSeaCastSpec` 实现简化为单行：
     ```cpp
     [[nodiscard]] inline BloodSeaCastSpec ResolveBloodSeaCastSpec(
         const entt::registry &registry, const entt::entity owner) {
       return ResolveSpecState(registry, owner, kBloodSeaSkillId, kBloodSeaTableGen);
     }
     ```
   - 点数语义差异收敛：手写解析对 points 做 `std::max(0, ReadPoints(...))` clamp（`BloodSea.hpp:282`），同 §2.5 由统一模板补 clamp 收敛。
3. **机制系数在消费点（`DoCast`）按需读取**：
   - 40 条 `MechBinding` 全部迁为 `BloodSea.cpp::DoCast` 内本地 `const float ... = GetMech(...)`；其中 **3 条 node=0 的技能级系数**（`lowLifeThreshold`、`fieldDurationPerBloodthirst`、`fieldRadiusPerBloodthirst`，`BloodSea.hpp:190-194`）按 `GetMech(kBloodSeaSkillId, 0, key, default)` 形式迁移；`lowLifeThreshold` 在 `BloodSea.cpp:370/410` 还被用作低血量判定的本地比较基准，迁移后同样读本地常量。
   - **技能级参数回退字段同步退役**：`fieldDurationDefault`、`fieldRadiusDefault`、`fieldTickDefault`、`bloodthirstDamageBonusDefault`、`leechRatioDefault`（`BloodSea.hpp:85-89`，供 `skills.json` params 缺失时 `GetParam` 兜底，消费点 `BloodSea.cpp:319-336`）不属于任何节点绑定，生成 POD 中不存在对应成员；迁为 `DoCast` 内本地 `constexpr float` 具名常量，字面量与迁移前字段默认值逐项一致。

### 2.5 统一解析基元语义对齐（点数 clamp 入模板）

- 现状差异：技能 11/12 的手写解析对 point 填充做 `std::max(0, ReadPoints(...))` 防御性 clamp；技能 10 的手写解析与统一模板（供技能 1~9）均直接赋值。`SpecStateTable.hpp:45` 的「与手写实现语义一致」注释对 11/12 实际不成立，本 Track 必须显式收敛，不能默认等价。
- 决策：在统一模板 `ResolveSpecState<State>` 的 point 填充处补 clamp（`state.*(binding.points) = std::max(0, ReadPoints(spec, binding.node));`，`SpecStateTable.hpp` 单点改动），使全 12 技能的语义结构化一致，而非依赖「负值不可达」的论证式等价。
- 行为影响论证：`allocated_points` 的全部写入路径均不产生负值（`SkillSystem.cpp:2335` 递增并在 `:2340` 校验 `max_points` 上限；`UISkillTalentTree.cpp:593/823` 为 UI 校验赋值；`PhantomTrance.cpp:95` 写常量 1），故 clamp 在运行期为不可达的纵深防御，对技能 1~9 无可观察行为变化；仅当存档或外部数据注入负值时，1~9 在迁移后获得与 11/12 相同的防御，属严格加强。
- 备选（否决）：① 包装函数内逐成员后置 clamp——需 17/18 个成员指针的二次遍历，复杂度高于基元单点修改；② 直接丢弃 clamp——把结构化等价退化为论证式等价，遗留负值注入时的静默行为分歧。

---

## 3. 测试与验证策略

1. **测试用例对拍升级（消除死结构引用）**：
   - `tests/unit/SpecStateTableTests.cpp`：
     - 移除旧的 `kSkill10IntMembers` / `kSkill10BoolMembers` 手写指针数组，改用直接断言 `kSevenStarSlashTableGen`、`kHeavenlySwordDescentTableGen`、`kBloodSeaTableGen` 逐字段与 `skills::ReadPoints` / `skills::HasNode` 相等（落地 L-2 修复）。
   - `tests/unit/GeneratedSpecStateTests.cpp`：
     - 将技能 10、11、12 的 `CheckGeneratedMatchesHandWritten` 重构为 `CheckGeneratedMatchesExpected<State>`（在测试内维护独立的期望表对拍），与技能 1~9 的测试模式保持一致。
     - 技能 10 用例的 flag 计数断言随 1021/1022 入表由 6 更新为 8（`GeneratedSpecStateTests.cpp:192`）；技能 11/12 用例现状只对拍 point/flag 表，不引用 `MechBinding` 表，无额外迁移项。
   - `tests/functional/SevenStarSlashNodes.cpp`、`HeavenlySwordDescentNodes.cpp`、`BloodSeaNodes.cpp`：
     - 保持现有功能测试全部通过；`SevenStarSlashNodes.cpp` 的 2 参 `skills::ResolveSpecState(registry, owner)` 调用点与转质判等断言不变（包装函数语义未变）。
     - fallback 断言改指消费点：`HeavenlySwordDescentNodes.cpp:120` 的 `spec.tierDamageBonusFallback == 0.18f` 改为断言 `DoCast` 本地 `constexpr` 回退常量；`BloodSeaNodes.cpp:117` 的 `spec.lowLifeThreshold == 0.35f` 改为断言 `GetMech(kBloodSeaSkillId, 0, "low_life_threshold", 0.35f)` 的键缺失回退行为。
   - `tests/integration/GameplaySystems.cpp`：
     - `behaviorFiles` 数组（`GameplaySystems.cpp:275`，现为 9 个生成头 + `SevenStarSlashShared.hpp`，容量 10）扩为容量 12：移除 `SevenStarSlashShared.hpp` 条目，新增 `SevenStarSlashSpecState.gen.hpp`、`HeavenlySwordDescentSpecState.gen.hpp`、`BloodSeaSpecState.gen.hpp` 三项，完成全 12 技能生成头校验闭环。
2. **自动化校验脚本门禁**：
   - `python scripts/gen_skill_contracts.py --gen-specstate --check --check-idempotency --check-determinism`
   - `python scripts/gen_skill_mechanics_schema.py --check`
   - `python scripts/sync_skill_node_icon_ids.py --check`

---

## 4. 退出标准与可观察证据（DoD）

1. **编译与告警**：`build.bat RelWithDebInfo` EXIT=0，0 错误、0 新增警告。
2. **D-A5 真正闭环**：全仓检索 `struct SevenStarSlashPointBinding|struct HeavenlySwordPointBinding|struct BloodSeaPointBinding` 命中数为 **0**；全仓手写 `*PointBinding`/`*FlagBinding` 结构清零。
3. **D-A1 全覆盖**：全 12 技能的 SpecState 均为无机制浮点混杂的 POD 结构体（`sizeof` 紧凑，只含 `int` 与 `bool`）；全仓检索 11/12 的机制与回退字段名（`impactRadiusFallback|fieldRadiusFallback|fieldDurationFallback|tierDamageBonusFallback|tierRadiusBonusFallback|fieldDurationDefault|fieldRadiusDefault|fieldTickDefault|bloodthirstDamageBonusDefault|leechRatioDefault`）命中数为 **0**。
4. **测试门禁**：
   - `ctest -C RelWithDebInfo -L ci` 100% 通过；
   - `ctest -C RelWithDebInfo -L skill` 100% 通过；
   - `ctest -C RelWithDebInfo -L integration` 100% 通过；
   - `NoMoreDayTests --test-case="*[Functional]*"` 全部通过。
5. **代码复审**：输出 Track A-02 审查报告，结论为「提交」。
