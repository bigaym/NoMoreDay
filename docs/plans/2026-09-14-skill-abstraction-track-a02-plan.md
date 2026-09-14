# 技能 10~12 专精抽象化实施计划（Track A-02）

- 日期：2026-09-14
- 依据设计：`docs/designs/2026-09-14-skill-abstraction-track-a02-design.md`
- 依据标准：`docs/workflows/planning.md`
- 基线 commit：`5e6a2b7f`（A-01 交付物，包含技能 1~9 抽象化与生成器支持）
- 目标：将技能 10（七星斩）、技能 11（天剑降临）、技能 12（血海）迁移到 `SpecStateTable` 统一信封体系，退役全部手写绑定结构，实现全仓 D-A5 闭环与 D-A1 纯粹性。

---

## 1. 实施思路与原理

1. **生成驱动，单源保障**：
   - 技能 10 的 descriptor `assets/data/skill_specstate/skill_10.json` 补齐转质节点 1021 与 1022 为 `flag` 绑定。
   - 重新执行 `gen_skill_contracts.py --gen-specstate` 自动生成包含全部点位与标志的 `SevenStarSlashSpecStateGen` 及对应绑定表 `kSevenStarSlashTableGen`。
   - 技能 11 与 12 的 descriptor 早已完备，可直接作为权威生成源。
2. **渐进替换，零语义破坏**：
   - **技能 10**：`SevenStarSlashShared.hpp` 引入生成头，将 `SevenStarSlashSpecState` 指向 `SevenStarSlashSpecStateGen`，`ResolveSpecState` 内部转调 `skills::ResolveSpecState` 模板，同时保留 `GetActiveTransmuterNode` 互斥保护。删除旧的手写结构体与数组。
   - **技能 11**：`HeavenlySwordDescent.hpp` 引入生成头，将 `HeavenlySwordCastSpec` 指向 `HeavenlySwordCastSpecGen`（恢复纯 POD）；将原 `spec` 中的机制浮点字段剥离，在 `HeavenlySwordDescent.cpp::DoCast` 中通过 `GetMech` 局部读取。删除旧的手写绑定与反射填充循环。
   - **技能 12**：`BloodSea.hpp` 引入生成头，将 `BloodSeaCastSpec` 指向 `BloodSeaCastSpecGen`；机制浮点字段剥离并在 `BloodSea.cpp::DoCast` 中通过 `GetMech` 局部读取。删除旧的手写绑定与反射填充循环。
3. **测试护栏升级（关闭 L-2 遗留）**：
   - 更新 `GeneratedSpecStateTests.cpp`：将技能 10~12 从“对拍手写绑定表”升级为“对拍测试侧独立期望表”。
   - 更新 `SpecStateTableTests.cpp`：直接断言技能 10~12 的生成表逐字段等于运行期 `skills::ReadPoints` / `skills::HasNode`。
   - 更新 `GameplaySystems.cpp`：扩展 `behaviorFiles` 为全 12 个生成的 `.gen.hpp`。

---

## 2. 伪代码引导与接口骨架

### 2.1 技能 10（`SevenStarSlashShared.hpp`）

```cpp
#include "game/systems/skill/behaviors/generated/SevenStarSlashSpecState.gen.hpp"

namespace NoMoreDay::skills {

namespace SevenStarSlashNodes = SevenStarSlashNodesGen;
using SevenStarSlashSpecState = SevenStarSlashSpecStateGen;

[[nodiscard]] inline SevenStarSlashSpecState
ResolveSpecState(const entt::registry &registry, entt::entity owner) {
  auto state = ResolveSpecState(registry, owner, seven_star_shared::kSevenStarSlashSkillId, kSevenStarSlashTableGen);
  // 转质节点互斥守卫
  const uint32_t active = SkillSystem::GetActiveTransmuterNode(
      registry, owner, seven_star_shared::kSevenStarSlashSkillId);
  state.poleStarOrbit = (active == SevenStarSlashNodesGen::PoleStarOrbit);
  state.starfall = (active == SevenStarSlashNodesGen::Starfall);
  return state;
}

// 删除: SevenStarSlashPointBinding, SevenStarSlashFlagBinding
// 删除: kSevenStarSlashPointBindings, kSevenStarSlashFlagBindings
// 删除: 原手写 struct SevenStarSlashSpecState

} // namespace NoMoreDay::skills
```

### 2.2 技能 11（`HeavenlySwordDescent.hpp` & `.cpp`）

```cpp
// HeavenlySwordDescent.hpp
#include "game/systems/skill/behaviors/generated/HeavenlySwordDescentSpecState.gen.hpp"

namespace NoMoreDay::skills {

namespace HeavenlySwordNodes = HeavenlySwordDescentNodesGen;
using HeavenlySwordCastSpec = HeavenlySwordCastSpecGen;

[[nodiscard]] inline HeavenlySwordCastSpec
ResolveHeavenlySwordCastSpec(const entt::registry &registry, const entt::entity owner) {
  return ResolveSpecState(registry, owner, kHeavenlySwordSkillId, kHeavenlySwordDescentTableGen);
}

// 删除: HeavenlySwordPointBinding, HeavenlySwordFlagBinding, HeavenlySwordMechBinding
// 删除: kHeavenlySwordPointBindings, kHeavenlySwordFlagBindings, kHeavenlySwordMechBindings
// 删除: 原手写 struct HeavenlySwordCastSpec

} // namespace NoMoreDay::skills

// HeavenlySwordDescent.cpp::DoCast
void HeavenlySwordDescent::DoCast(entt::registry &registry, entt::entity owner, SkillExecution &exec) {
  const HeavenlySwordCastSpec spec = ResolveHeavenlySwordCastSpec(registry, owner);

  // 机制系数由消费点直接读取（对齐技能 1~10 规范）
  const float swordCoreCalibrationPerPoint = GetMech(kSkillId, HeavenlySwordNodes::SwordCoreCalibration, "impact_stability_per_point", 0.10f);
  const float worldsplitCorePerPoint = GetMech(kSkillId, HeavenlySwordNodes::WorldsplitCore, "center_damage_per_point", 0.10f);
  // ... 其他机制浮点字段按需读取，替换原 spec.<field>
}
```

### 2.3 技能 12（`BloodSea.hpp` & `.cpp`）

```cpp
// BloodSea.hpp
#include "game/systems/skill/behaviors/generated/BloodSeaSpecState.gen.hpp"

namespace NoMoreDay::skills {

namespace BloodSeaNodes = BloodSeaNodesGen;
using BloodSeaCastSpec = BloodSeaCastSpecGen;

[[nodiscard]] inline BloodSeaCastSpec
ResolveBloodSeaCastSpec(const entt::registry &registry, const entt::entity owner) {
  return ResolveSpecState(registry, owner, kBloodSeaSkillId, kBloodSeaTableGen);
}

// 删除: BloodSeaPointBinding, BloodSeaFlagBinding, BloodSeaMechBinding
// 删除: kBloodSeaPointBindings, kBloodSeaFlagBindings, kBloodSeaMechBindings
// 删除: 原手写 struct BloodSeaCastSpec

} // namespace NoMoreDay::skills

// BloodSea.cpp::DoCast
void BloodSea::DoCast(entt::registry &registry, entt::entity owner, SkillExecution &exec) {
  const BloodSeaCastSpec spec = ResolveBloodSeaCastSpec(registry, owner);

  // 机制系数由消费点直接读取
  const float lowLifeThreshold = GetMech(kSkillId, 0u, "low_life_threshold", 0.35f);
  const float fieldDurationPerBloodthirst = GetMech(kSkillId, 0u, "field_duration_per_bloodthirst", 0.2f);
  // ... 其他机制浮点字段按需读取，替换原 spec.<field>
}
```

---

## 3. 原子任务拆分

### Phase 1: 数据 Descriptor 完善与生成头重新生成
- [ ] **T1.1** 修改 `assets/data/skill_specstate/skill_10.json`，在 `bindings` 中增加 1021（`poleStarOrbit`, flag）与 1022（`starfall`, flag）绑定。
- [ ] **T1.2** 运行 `python scripts/gen_skill_contracts.py --gen-specstate` 重新生成 `SevenStarSlashSpecState.gen.hpp`。
- [ ] **T1.3** 验证生成器三门禁通过：`--check --check-idempotency --check-determinism`。

### Phase 2: 技能 10 迁移与旧绑定退役
- [ ] **T2.1** 修改 `src/game/systems/skill/behaviors/SevenStarSlashShared.hpp`：
  - include 生成头 `SevenStarSlashSpecState.gen.hpp`；
  - 引入 `namespace SevenStarSlashNodes = SevenStarSlashNodesGen` 与 `using SevenStarSlashSpecState = SevenStarSlashSpecStateGen`；
  - 重写 `ResolveSpecState` 为调用 `skills::ResolveSpecState(..., kSevenStarSlashTableGen)` + `GetActiveTransmuterNode` 互斥保护；
  - 删除手写的绑定结构体与常量数组。
- [ ] **T2.2** 编译验证 `build.bat RelWithDebInfo`，确保 `SevenStarSlash.cpp` 编译通过且行为一致。
- [ ] **T2.3** 运行技能 10 功能测试：`NoMoreDayTests --test-case="*SevenStarSlash*"`。

### Phase 3: 技能 11 迁移与机制系数解耦
- [ ] **T3.1** 修改 `src/game/systems/skill/behaviors/HeavenlySwordDescent.hpp`：
  - include 生成头 `HeavenlySwordDescentSpecState.gen.hpp`；
  - 引入 `namespace HeavenlySwordNodes = HeavenlySwordDescentNodesGen` 与 `using HeavenlySwordCastSpec = HeavenlySwordCastSpecGen`；
  - 重写 `ResolveHeavenlySwordCastSpec` 为单行调用 `skills::ResolveSpecState(..., kHeavenlySwordDescentTableGen)`；
  - 删除手写的 `HeavenlySwordPointBinding`、`HeavenlySwordFlagBinding`、`HeavenlySwordMechBinding` 等旧结构。
- [ ] **T3.2** 修改 `src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp`：
  - 在 `DoCast` 入口将原 `spec.<mechCoeff>` 读取改为本地 `const float ... = GetMech(...)`。
- [ ] **T3.3** 编译验证并运行技能 11 功能测试：`NoMoreDayTests --test-case="*HeavenlySword*"`。

### Phase 4: 技能 12 迁移与机制系数解耦
- [ ] **T4.1** 修改 `src/game/systems/skill/behaviors/BloodSea.hpp`：
  - include 生成头 `BloodSeaSpecState.gen.hpp`；
  - 引入 `namespace BloodSeaNodes = BloodSeaNodesGen` 与 `using BloodSeaCastSpec = BloodSeaCastSpecGen`；
  - 重写 `ResolveBloodSeaCastSpec` 为单行调用 `skills::ResolveSpecState(..., kBloodSeaTableGen)`；
  - 删除手写的 `BloodSeaPointBinding`、`BloodSeaFlagBinding`、`BloodSeaMechBinding` 等旧结构。
- [ ] **T4.2** 修改 `src/game/systems/skill/behaviors/BloodSea.cpp`：
  - 在 `DoCast` 入口将原 `spec.<mechCoeff>` 读取改为本地 `const float ... = GetMech(...)`。
- [ ] **T4.3** 编译验证并运行技能 12 功能测试：`NoMoreDayTests --test-case="*BloodSea*"`。

### Phase 5: 测试体系升级与全量验收（关闭 L-2）
- [ ] **T5.1** 更新 `tests/unit/GeneratedSpecStateTests.cpp`：
  - 将技能 10~12 改为独立的 `CheckGeneratedMatchesExpected<State>` 期望表对拍，移除已删除手写表的引用。
- [ ] **T5.2** 更新 `tests/unit/SpecStateTableTests.cpp`：
  - 为技能 10~12 增加直接断言运行期 `ReadPoints`/`HasNode` 的测试，彻底关闭 L-2 遗留。
- [ ] **T5.3** 更新 `tests/integration/GameplaySystems.cpp`：
  - `behaviorFiles` 扩展为 12 个生成的 `.gen.hpp` 文件。
- [ ] **T5.4** 更新 `tests/functional/HeavenlySwordDescentNodes.cpp` 与 `tests/functional/BloodSeaNodes.cpp`：
  - 确认测试对改动后的 `spec` 纯 POD 结构完全绿，机制断言直接针对 `GetMech`。
- [ ] **T5.5** 运行全量测试套件并出具复审报告：
  - `build.bat RelWithDebInfo`（0 警告）
  - `ctest -C RelWithDebInfo -L ci`（100% 通过）
  - `ctest -C RelWithDebInfo -L skill`（100% 通过）
  - `ctest -C RelWithDebInfo -L integration`（100% 通过）
  - 生成器三门禁与数据检查通过。

---

## 4. 测试方法与命令清单

### 4.1 测试层级与范围

| 层级 | 目标 | 测试文件 |
|---|---|---|
| Unit | 模板表映射与对拍 | `tests/unit/SpecStateTableTests.cpp`、`tests/unit/GeneratedSpecStateTests.cpp`、`tests/unit/SpecStateMappingTests.cpp` |
| Functional | 技能 10~12 节点机制回归 | `tests/functional/SevenStarSlashNodes.cpp`、`HeavenlySwordDescentNodes.cpp`、`BloodSeaNodes.cpp` |
| Integration | 节点常量全集完整性 | `tests/integration/GameplaySystems.cpp` |
| Scripts | 数据与代码生成一致性 | `scripts/gen_skill_contracts.py`、`gen_skill_mechanics_schema.py`、`sync_skill_node_icon_ids.py` |

### 4.2 验证命令

```powershell
# 1. 重新生成并验证数据与生成物
python scripts/gen_skill_contracts.py --gen-specstate --check --check-idempotency --check-determinism
python scripts/gen_skill_mechanics_schema.py --check
python scripts/sync_skill_node_icon_ids.py --check

# 2. 编译
.\build.bat RelWithDebInfo

# 3. 定向功能测试
.\build\bin\RelWithDebInfo\NoMoreDayTests.exe --test-case="*SevenStarSlash*,*HeavenlySword*,*BloodSea*,*SpecState*"

# 4. CI 全量回归
ctest --test-dir build -C RelWithDebInfo -L ci
ctest --test-dir build -C RelWithDebInfo -L skill
ctest --test-dir build -C RelWithDebInfo -L integration
```

---

## 5. 退出标准与完成定义（DoD）

1. **D-A5 真正闭环**：全仓手写 `*PointBinding` / `*FlagBinding` 结构完全退役，`rg "struct (SevenStarSlash|HeavenlySword|BloodSea)PointBinding" src/` 为 0。
2. **D-A1 全仓统一**：全 12 技能的 `SpecState` 均为无机制浮点混杂的纯 POD 结构体。
3. **L-2 关闭**：技能 10~12 在 `SpecStateTableTests.cpp` 中具备直接验证运行期 `ReadPoints`/`HasNode` 的独立断言。
4. **测试 100% 绿**：全量单元与集成测试无回归，CI 套件全部通过。
5. **文档闭环**：产出 Track A-02 复审报告，结论为「提交」，并在 `docs/plans/2026-09-12-skill1-9-followup-backlog.md` 中将 A-02 / A-03 标记为已销项。
