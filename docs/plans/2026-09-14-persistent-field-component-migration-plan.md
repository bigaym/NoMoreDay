# 技能 11/12 持续场组件独立迁移实施计划

- 日期：2026-09-14
- 依据设计：`docs/designs/2026-09-14-persistent-field-component-migration-design.md`
- 依据标准：`docs/workflows/planning.md`
- 基线 commit：`0401c406`（Track A-02 交付物，包含全 12 技能 SpecState 生成化）
- 目标：将技能 11（天剑降临）与技能 12（血海）的重型专有场组件从底层通用头 `SkillDefs.hpp` 迁出，建立专用的持久场调度与专精解耦接口，阻断 UI 穿透依赖，达成架构物理层彻底收尾。

---

## 1. 实施思路与原理

1. **物理拆分与归位（Foundation 减肥）**：
   - 现状：`HeavenlySwordFieldComponent`（40+ 字段）与 `BloodSeaFieldComponent`（30+ 字段）位于 `src/game/foundation/components/SkillDefs.hpp`，违背了“基础头文件仅保留跨领域通用契约”的原则。
   - 方案：新建 `src/game/systems/skill/components/PersistentFieldComponents.hpp`，将两组件平移迁入，保留 `PersistentFieldHeader`（作为通用头）在 `SkillDefs.hpp`。通过 `static_assert` 继续强制 standard layout 与 trivially destructible。
2. **生命周期更新聚合（Orchestration Decoupling）**：
   - 现状：`SkillSystem.cpp:1413-1424` 显式写了两个 `registry.view` 循环分别调用 `HeavenlySwordDescent::UpdateField` 与 `BloodSea::UpdateField`。
   - 方案：引入 `src/game/systems/skill/PersistentFieldSystem.hpp/.cpp`，封装 `PersistentFieldSystem::Update(registry, grid, dt)`。调度层仅持有单行调用，消除 `SkillSystem.cpp` 对具体场组件的强依赖。
3. **专精切换服务解耦（Mastery Switch Decoupling）**：
   - 现状：`BladeMasteryService.cpp:48-95` 中使用显式模板特化 `DestroyOwnedFields<Component>` 销毁实体，并直接穿透读取 `field.original_formation_attack_interval` 等字段来还原攻速/引导频率。
   - 方案：
     - 利用实体上既有的 `PersistentFieldTag` 与 `SkillComponent`，实现通用 `DestroyOwnedPersistentFields(registry, owner, skill_id)`。
     - 将天剑特定的状态还原逻辑封装为 `skills::HeavenlySwordDescent::OnMasterySwitchCleanup(registry, owner)`，对 `BladeMasteryService` 隐藏内部字段。
4. **UI 与表现层只读门面（HUD Query DTO）**：
   - 现状：`GameUiSnapshotBuilder.cpp` 与 `PlayerHUD.cpp` 直接持有底层 40+ 字段组件的只读 view。
   - 方案：在行为模块中暴露轻量 DTO（`HeavenlyFieldHudSnapshot`、`BloodSeaHudSnapshot`）及其查询函数，表现层仅读 DTO，切断对底层结构的依赖。

---

## 2. 伪代码引导与接口骨架

### 2.1 独立组件头（`src/game/systems/skill/components/PersistentFieldComponents.hpp`）

```cpp
#pragma once
#include "game/foundation/components/SkillDefs.hpp"

namespace NoMoreDay {

// 技能 11: 天剑降临持久场组件 (纯 POD / Standard Layout)
struct HeavenlySwordFieldComponent {
  PersistentFieldHeader header = {
      .duration = 5.0f,
      .radius = 140.0f,
      .tick_interval = 0.5f,
  };
  float linked_cut_cooldown = 0.0f;
  // ... 完整平移原 40+ 字段保持逐字节一致
};
static_assert(std::is_standard_layout_v<HeavenlySwordFieldComponent>);
static_assert(std::is_trivially_destructible_v<HeavenlySwordFieldComponent>);

// 技能 12: 血海持久场组件 (纯 POD / Standard Layout)
struct BloodSeaFieldComponent {
  PersistentFieldHeader header = {
      .duration = 5.0f,
      .radius = 120.0f,
      .tick_interval = 0.25f,
  };
  float linked_pulse_cooldown = 0.0f;
  // ... 完整平移原 30+ 字段保持逐字节一致
};
static_assert(std::is_standard_layout_v<BloodSeaFieldComponent>);
static_assert(std::is_trivially_destructible_v<BloodSeaFieldComponent>);

} // namespace NoMoreDay
```

### 2.2 持续场生命周期系统（`PersistentFieldSystem.hpp/.cpp`）

```cpp
// PersistentFieldSystem.hpp
#pragma once
#include <entt/entity/fwd.hpp>

namespace NoMoreDay::systems {
class SpatialHashGrid;
}

namespace NoMoreDay::skills {

class PersistentFieldSystem {
public:
  static void Update(entt::registry &registry, systems::SpatialHashGrid &grid, float dt);
};

} // namespace NoMoreDay::skills

// PersistentFieldSystem.cpp
#include "game/systems/skill/PersistentFieldSystem.hpp"
#include "game/systems/skill/components/PersistentFieldComponents.hpp"
#include "game/systems/skill/behaviors/HeavenlySwordDescent.hpp"
#include "game/systems/skill/behaviors/BloodSea.hpp"
#include "game/foundation/components/Common.hpp"

namespace NoMoreDay::skills {

void PersistentFieldSystem::Update(entt::registry &registry, systems::SpatialHashGrid &grid, float dt) {
  auto heavenly_view = registry.view<HeavenlySwordFieldComponent, Position>();
  for (auto entity : heavenly_view) {
    auto &field = heavenly_view.get<HeavenlySwordFieldComponent>(entity);
    skills::HeavenlySwordDescent::UpdateField(registry, entity, field, dt, grid);
  }

  auto blood_view = registry.view<BloodSeaFieldComponent, Position>();
  for (auto entity : blood_view) {
    auto &field = blood_view.get<BloodSeaFieldComponent>(entity);
    skills::BloodSea::UpdateField(registry, entity, field, dt, grid);
  }
}

} // namespace NoMoreDay::skills
```

### 2.3 专精服务解耦接口（`PersistentFieldSystem` & `HeavenlySwordDescent.hpp`）

```cpp
// PersistentFieldSystem.hpp（通用销毁函数的唯一定义点；
// BladeMasteryService.cpp 与 HeavenlySwordDescent.cpp 均经此头文件调用）
namespace NoMoreDay::skills {
// skill_id == 0 沿用 DestroyOwnedAreaFields 的「全部销毁」哨兵约定。
// 技能 11/12 场实体同时挂 PersistentFieldTag + SkillComponent(skill_id=11/12)
// + AreaFieldComponent（同实体），销毁实体即连带清理 AreaField，与原
// DestroyOwnedAreaFields(11u/12u) 语义等价。
void DestroyOwnedPersistentFields(entt::registry &registry,
                                  const entt::entity owner,
                                  const uint32_t skill_id);
}

// HeavenlySwordDescent.hpp
namespace NoMoreDay::skills::HeavenlySwordDescent {
void OnMasterySwitchCleanup(entt::registry &registry, entt::entity owner);
}

// BladeMasteryService.cpp（CleanupSpecializedFields 重构后）
void CleanupSpecializedFields(registry, owner, selected_mastery) {
  if (selected_mastery != BladeMasteryId::DemonBlade) {
    DestroyOwnedPersistentFields(registry, owner, 12u); // 血海场 + 同实体 AreaField(12)
  }
  if (selected_mastery != BladeMasteryId::HeavenlySword) {
    skills::HeavenlySwordDescent::OnMasterySwitchCleanup(registry, owner);
  }
}

// HeavenlySwordDescent.cpp（OnMasterySwitchCleanup 内部，自
// BladeMasteryService.cpp:54-93 天剑分支平移，语义不变）
void OnMasterySwitchCleanup(registry, owner) {
  // 1. 读取场实体 original_formation_attack_interval / original_channel_tick_interval（>0 时）
  // 2. 还原 BladeFormationComponent::attack_interval
  // 3. 还原该玩家 skill_id==3 的 SpiritSwordAI::attack_interval
  // 4. 还原该玩家 skill_id==5 的 BeamChannelComponent::tick_interval
  //    并 tick_timer = min(tick_timer, tick_interval)（含 501 构筑覆盖限制注释平移）
  DestroyOwnedPersistentFields(registry, owner, 11u); // 天剑场 + 同实体 AreaField(11)
}
```

### 2.4 UI 轻量快照查询门面

```cpp
// 在 HeavenlySwordDescent.hpp / BloodSea.hpp
struct HeavenlyFieldHudSnapshot {
  float remaining_duration = 0.0f;
};
[[nodiscard]] HeavenlyFieldHudSnapshot QueryHeavenlyFieldHud(const entt::registry &registry, entt::entity player);

struct BloodSeaHudSnapshot {
  float remaining_duration = 0.0f;
  bool has_void_keystone = false;
  float miasma_duration_bonus = 0.0f; // 与组件字段同名，避免查询层二次映射
};
[[nodiscard]] BloodSeaHudSnapshot QueryBloodSeaHud(const entt::registry &registry, entt::entity player);
```

---

## 3. 原子任务拆分

- [x] **Task 1: 建立专用组件头并完成物理迁移**
  - [x] T1.1 新建 `src/game/systems/skill/components/PersistentFieldComponents.hpp`，移入 `HeavenlySwordFieldComponent` 与 `BloodSeaFieldComponent`。
  - [x] T1.2 从 `src/game/foundation/components/SkillDefs.hpp` 中移除两结构体，保留 `PersistentFieldHeader` 与 `PersistentFieldTag`。
  - [x] T1.3 更新 CMake 构建配置（`src/game/systems/skill/CMakeLists.txt`），确保新头文件纳入 target 依赖。
  - [x] T1.4 更新全部引用点的 include（以 `rg "HeavenlySwordFieldComponent|BloodSeaFieldComponent" src tests` 清单为准）：src 侧含 `HeavenlySwordDescent.hpp/.cpp`、`BloodSea.hpp/.cpp`、`GameplayRenderAdapter.cpp` 等；测试侧 9 个文件：`tests/unit/BladeMasteryTests.cpp`、`tests/tech/UITests.cpp`、`tests/integration/SkillSystemTests.cpp`、`tests/integration/SkillKeyNodeMatrixIntegrationTests.cpp`、`tests/unit/SkillBehaviorGuardTests.cpp`、`tests/functional/SkillBehaviors.cpp`、`tests/functional/BloodSeaNodes.cpp`、`tests/functional/HeavenlySwordDescentNodes.cpp`、`tests/functional/InfiniteBladesNodes.cpp`。

- [x] **Task 2: 实现持续场调度系统 `PersistentFieldSystem`**
  - [x] T2.1 新建 `src/game/systems/skill/PersistentFieldSystem.hpp` 与 `.cpp`，编排天剑与血海场更新逻辑，并在此头/源中声明定义通用 `DestroyOwnedPersistentFields(registry, owner, skill_id)`（被 BladeMasteryService 与 HeavenlySwordDescent 共用，不得放入任一调用方的匿名命名空间）。
  - [x] T2.2 在 `SkillSystem.cpp:1413-1424` 中，将两处硬编码循环替换为单行 `skills::PersistentFieldSystem::Update(registry, grid, dt)`。

- [x] **Task 3: 解耦 `BladeMasteryService` 专精清理逻辑**
  - [x] T3.1 在 `BladeMasteryService.cpp` 中改调 `DestroyOwnedPersistentFields`（血海分支 `12u`），移除模板特化 `DestroyOwnedFields<BloodSeaFieldComponent>` 与 `DestroyOwnedAreaFields(registry, owner, 12u)`（后者由同实体销毁语义等价覆盖）。
  - [x] T3.2 在 `HeavenlySwordDescent.hpp/.cpp` 中封装 `OnMasterySwitchCleanup`：自 `BladeMasteryService.cpp:54-93` 平移天剑分支全部状态还原（`BladeFormationComponent::attack_interval`、`skill_id==3` 的 `SpiritSwordAI::attack_interval`、`skill_id==5` 的 `BeamChannelComponent::tick_interval`/`tick_timer`）并调用 `DestroyOwnedPersistentFields(registry, owner, 11u)`。
  - [x] T3.3 在 `BladeMasteryService.cpp:48-95` 中改调新接口，彻底移除对天剑具体字段的读取。

- [x] **Task 4: 封装表现层只读查询门面（UI/HUD 解耦）**
  - [x] T4.1 在 `HeavenlySwordDescent.hpp/.cpp` 中实现 `QueryHeavenlyFieldHud`。
  - [x] T4.2 在 `BloodSea.hpp/.cpp` 中实现 `QueryBloodSeaHud`。
  - [x] T4.3 重构 `GameUiSnapshotBuilder.cpp:383-402` 与 `PlayerHUD.cpp:51-85`，改用新查询门面。
  - [x] T4.4 更新 `GameplayRenderAdapter.cpp` 包含路径，适配新组件头。

- [x] **Task 5: 测试验证与全量回归**
  - [x] T5.1 编译与无告警验证：`build.bat RelWithDebInfo`。
  - [x] T5.2 核心单测与功能测试回归：`ctest --test-dir build -C RelWithDebInfo -L "skill|ci|integration|unit"`。
  - [x] T5.3 架构边界审计：运行 `python scripts/check_module_boundaries.py`。

---

## 4. 测试方法与测试用例

1. **受影响测试套件**：
   - 单元测试：[`tests/unit/SkillBehaviorGuardTests.cpp`](file:///d:/PRJ/NoMoreDay/tests/unit/SkillBehaviorGuardTests.cpp)（天剑与血海节点守卫测试）
   - 功能测试：[`tests/functional/HeavenlySwordDescentNodes.cpp`](file:///d:/PRJ/NoMoreDay/tests/functional/HeavenlySwordDescentNodes.cpp)、[`tests/functional/BloodSeaNodes.cpp`](file:///d:/PRJ/NoMoreDay/tests/functional/BloodSeaNodes.cpp)
   - 集成测试：[`tests/integration/GameplaySystems.cpp`](file:///d:/PRJ/NoMoreDay/tests/integration/GameplaySystems.cpp)（全系统管线集成）、[`tests/unit/GameUiCommandHandlerTests.cpp`](file:///d:/PRJ/NoMoreDay/tests/unit/GameUiCommandHandlerTests.cpp)
2. **专项验证回归命令**：
   ```powershell
   # 1. 增量构建
   .\build.bat RelWithDebInfo
   # 2. 技能与持续场专项测试
   ctest --test-dir build -C RelWithDebInfo -R "HeavenlySword|BloodSea|SkillBehaviorGuard" --output-on-failure
   # 3. 全量 CI 标签回归
   ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure
   ```

---

## 5. 验证任务完成标准（DoD）

1. **头文件卫生（D-H1）**：`SkillDefs.hpp` 中 0 处 `HeavenlySwordFieldComponent` 与 `BloodSeaFieldComponent` 匹配（`rg` 结果为 0）。
2. **调度解耦（D-H2）**：`SkillSystem.cpp` 中 0 处直接视图迭代天剑场或血海场，全量由 `PersistentFieldSystem` 托管。
3. **专精解耦（D-H3）**：`BladeMasteryService.cpp` 中 0 处显式模板特化销毁场实体，0 处直接读取天剑场私有字段。
4. **表现层隔离（D-H4）**：`PlayerHUD.cpp` 与 `GameUiSnapshotBuilder.cpp` 零底层场组件依赖。
5. **构建与门禁（D-H5）**：`build.bat RelWithDebInfo` 0 错误、0 新增警告；全量 CI 标签测试 100% 通过。
