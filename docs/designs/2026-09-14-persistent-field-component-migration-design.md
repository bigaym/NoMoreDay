# 技能 11/12 持续场组件独立迁移与生命周期解耦设计规范

- 日期：2026-09-14
- 状态：草案（方案设计阶段）
- 依据标准：`docs/workflows/design.md`
- 前置依赖：
  - `docs/designs/2026-09-13-skill-abstraction-and-b2-cleanup-design.md`（§8 持久场交付原型化原则）
  - `docs/reviews/2026-09-14-skill-abstraction-a02-review.md`（R5 明确组件迁移另立计划）
  - `docs/plans/2026-09-12-skill1-9-followup-backlog.md`（A-02 条目之组件迁移拆离项）
- 涉及范围：
  - `HeavenlySwordFieldComponent`（技能 11 天剑降临持续场）
  - `BloodSeaFieldComponent`（技能 12 血海持续场）
  - `SkillDefs.hpp`（基础组件头文件解耦）
  - `PersistentFieldComponents.hpp`（新建专用组件头，`src/game/systems/skill/components/`）
  - `PersistentFieldSystem.hpp/.cpp`（新建统一调度与通用清理）
  - `SkillSystem.cpp`（持续场生命周期调度）
  - `BladeMasteryService.cpp`（专精切换清理与攻速/引导频率还原）
  - `HeavenlySwordDescent.hpp/.cpp`、`BloodSea.hpp/.cpp`（行为头文件：`OnMasterySwitchCleanup`、DTO 查询门面、include 更新）
  - `GameUiSnapshotBuilder.cpp` / `PlayerHUD.cpp` / `GameplayRenderAdapter.cpp`（UI与渲染查询）
  - 引用两组件的测试文件 include 更新（共 9 个：`BladeMasteryTests.cpp`、`UITests.cpp`、`SkillSystemTests.cpp`、`SkillKeyNodeMatrixIntegrationTests.cpp`、`SkillBehaviorGuardTests.cpp`、`SkillBehaviors.cpp`、`BloodSeaNodes.cpp`、`HeavenlySwordDescentNodes.cpp`、`InfiniteBladesNodes.cpp`）

---

## 1. 背景与目标（Why & What）

### 1.1 现状与痛点

在 NoMoreDay 技能系统中，技能 11（天剑降临）和技能 12（血海）作为高级专精技能，各自生成大范围的持续地表领域（Persistent Field）。在先前的架构演进中，我们通过 `PersistentFieldHeader` 统一了脉冲、时长与归属的核心元数据，但仍遗留了显著的物理与逻辑耦合：

1. **头文件物理层污染（Monolithic Header Bloat）**：
   - `HeavenlySwordFieldComponent`（40+ 字段）与 `BloodSeaFieldComponent`（30+ 字段）作为专属于具体技能的重型状态组件，仍直接定义在 `src/game/foundation/components/SkillDefs.hpp`（约 :1306-1384）。
   - 这导致底层几乎所有包含 `SkillDefs.hpp` 的系统（AI、UI、Combat、Render 等）都在承受非必要的编译依赖和头文件污染。
2. **生命周期更新硬编码（Hardcoded Update Dispatch）**：
   - 在 `SkillSystem.cpp:1413-1424` 中，调度层显式遍历 `registry.view<HeavenlySwordFieldComponent, Position>()` 与 `registry.view<BloodSeaFieldComponent, Position>()`，逐个调用 `skills::HeavenlySwordDescent::UpdateField` 与 `skills::BloodSea::UpdateField`。
   - 调度层对具体技能的私有场组件产生直接类型依赖，违背了基于原型与标签的松耦合调度原则。
3. **专精服务强侵入式清理（BladeMasteryService Invasive Cleanup）**：
   - 在 `BladeMasteryService.cpp:48-95` 中，切换专精时通过显式模板参数 `DestroyOwnedFields<HeavenlySwordFieldComponent>` 与 `DestroyOwnedFields<BloodSeaFieldComponent>` 销毁实体。
   - 此外，为了还原天剑场被动改变的剑阵攻击间隔（`formation_attack_interval`）与引导频率（`channel_tick_interval`），`BladeMasteryService` 直接穿透读取 `field.original_formation_attack_interval` 等私有字段，逻辑散落在专精服务中。
4. **UI 与渲染层穿透依赖（UI/HUD Penetrating Query）**：
   - `GameUiSnapshotBuilder.cpp:383-402` 与 `PlayerHUD.cpp:51-85` 直接获取 `HeavenlySwordFieldComponent` 与 `BloodSeaFieldComponent` 的只读 view，读取剩余时长及特殊标志（如 `has_void_keystone`、`miasma_duration_bonus`），形成了表现层对技能专有状态结构的直接依赖。

### 1.2 核心目标

1. **物理拆分与归位**：
   - 将 `HeavenlySwordFieldComponent` 与 `BloodSeaFieldComponent` 迁出 `SkillDefs.hpp`，移入专用组件头文件（如 `src/game/systems/skill/components/PersistentFieldComponents.hpp`），保持 `SkillDefs.hpp` 仅包含跨领域通用的轻量契约（`PersistentFieldHeader`、`PersistentFieldTag`、`AreaFieldComponent`）。
2. **统一生命周期调度**：
   - 抽象持续场生命周期统一分派逻辑，消除 `SkillSystem.cpp` 对天剑场和血海场的硬编码内联循环。
3. **专精切换服务解耦**：
   - 基于已有的 `PersistentFieldTag` + `SkillComponent` 实现通用的 `DestroyOwnedPersistentFields(registry, owner, skill_id)` 清理逻辑，消除具体组件类型依赖。
   - 将天剑场的攻速/引导频率还原逻辑封装为技能模块内的成员服务 `skills::HeavenlySwordDescent::OnMasterySwitchCleanup(registry, owner)`，对外部隐藏具体字段细节。
4. **UI/HUD 查询接口封装**：
   - 在行为层或查询门面中提供轻量只读 DTO（`HeavenlyFieldHudSnapshot`、`BloodSeaHudSnapshot`，详见 §2.4），UI 仅通过只读函数查询，不再依赖 40+ 字段的底层组件定义。

### 1.3 明确非目标（Non-Goals）

1. **不修改任何战斗数值与 GDD 机制**：持续场的伤害计算、抗性削减、脉冲间隔、层数递增等机制逻辑完全保持原样。
2. **不合并为单一同构组件**：天剑场拥有 40+ 个独特的雷冰火天剑共鸣与剑痕字段，血海场拥有独特的鲜血与虚空脉冲字段，二者字段差异巨大，强行 `std::variant` 或 `union` 会引入内存膨胀或运行时开销。维持两者的独立结构，统一其生命周期与外部交互。
3. **不触碰 SpecState 架构**：Track A-02 已闭环的 `*SpecStateGen` 及 `SpecStateTable` 体系保持不变。

---

## 2. 架构设计与契约收敛

### 2.1 组件分层与物理布局

```text
【重构前】
SkillDefs.hpp (foundation/components/)
  ├── AreaFieldComponent (通用轻量)
  ├── PersistentFieldHeader (通用头)
  ├── PersistentFieldTag (通用标记)
  ├── HeavenlySwordFieldComponent (技能 11 专有，40+ 字段) ──▶ 被全仓引用
  └── BloodSeaFieldComponent (技能 12 专有，30+ 字段)       ──▶ 被全仓引用

【重构后】
SkillDefs.hpp (foundation/components/)
  ├── AreaFieldComponent
  ├── PersistentFieldHeader
  └── PersistentFieldTag

src/game/systems/skill/components/PersistentFieldComponents.hpp
  ├── #include "game/foundation/components/SkillDefs.hpp"
  ├── struct HeavenlySwordFieldComponent { PersistentFieldHeader header; ... };
  └── struct BloodSeaFieldComponent { PersistentFieldHeader header; ... };
```

- **类型约束**：
  - 迁移后的两个组件依然必须严格满足：
    ```cpp
    static_assert(std::is_standard_layout_v<HeavenlySwordFieldComponent>);
    static_assert(std::is_trivially_destructible_v<HeavenlySwordFieldComponent>);
    static_assert(std::is_standard_layout_v<BloodSeaFieldComponent>);
    static_assert(std::is_trivially_destructible_v<BloodSeaFieldComponent>);
    ```
  - 消除动态内存分配，确保 ECS 连续内存紧凑性。

### 2.2 持续场调度统一化

在 `src/game/systems/skill/` 下规划统一更新入口或在 `PersistentFieldSystem` 中进行聚合：

```cpp
namespace NoMoreDay::skills {

// 持续场统一更新管线
class PersistentFieldSystem {
public:
    static void Update(entt::registry &registry, systems::SpatialHashGrid &grid, float dt);
};

} // namespace NoMoreDay::skills
```

- **实现策略**：
  `PersistentFieldSystem::Update` 集中管理所有带有 `PersistentFieldTag` 的实体更新。
  内部统一调用：
  ```cpp
  void PersistentFieldSystem::Update(entt::registry &registry, systems::SpatialHashGrid &grid, float dt) {
      // 1. 天剑场更新
      auto heavenly_view = registry.view<HeavenlySwordFieldComponent, Position>();
      for (auto entity : heavenly_view) {
          skills::HeavenlySwordDescent::UpdateField(registry, entity, heavenly_view.get<HeavenlySwordFieldComponent>(entity), dt, grid);
      }
      // 2. 血海场更新
      auto blood_view = registry.view<BloodSeaFieldComponent, Position>();
      for (auto entity : blood_view) {
          skills::BloodSea::UpdateField(registry, entity, blood_view.get<BloodSeaFieldComponent>(entity), dt, grid);
      }
  }
  ```
- **收益**：`SkillSystem.cpp` 仅需保留单行 `PersistentFieldSystem::Update(registry, grid, dt);`，后续新增第 13、14 个专精持续场时，无需修改 `SkillSystem.cpp` 核心流。

### 2.3 专精清理与恢复解耦

在 `BladeMasteryService.cpp` 中：

1. **通用持久场清理函数**：
    - 该函数被 `BladeMasteryService.cpp` 与 `HeavenlySwordDescent.cpp`（经 `OnMasterySwitchCleanup`）共同调用，因此**不得**定义在 `BladeMasteryService.cpp` 的匿名命名空间内；应声明于 `PersistentFieldSystem.hpp`、定义于 `PersistentFieldSystem.cpp`，两处调用方各自 include 该头文件。
    - 语义说明：技能 11/12 的场实体同时挂载 `PersistentFieldTag`、`SkillComponent`（`skill_id` 为 11/12、`owner` 为施法者）与 `AreaFieldComponent`（见 `HeavenlySwordDescent.cpp:653-656`、`BloodSea.cpp:426-429` 与 :767/:599 的同实体 emplace），销毁实体即连带清理其上的 `AreaFieldComponent`，与现行 `DestroyOwnedAreaFields(registry, owner, 11u/12u)` 语义等价。
    ```cpp
    void DestroyOwnedPersistentFields(entt::registry &registry, const entt::entity owner, const uint32_t skill_id) {
        std::vector<entt::entity> to_destroy;
        const auto view = registry.view<PersistentFieldTag, SkillComponent>();
        for (const entt::entity entity : view) {
            const auto &skill = view.get<SkillComponent>(entity);
            if (skill.owner == owner && (skill_id == 0 || skill.skill_id == skill_id)) {
                to_destroy.push_back(entity);
            }
        }
        for (const entt::entity entity : to_destroy) {
            if (registry.valid(entity)) {
                registry.destroy(entity);
            }
        }
    }
    ```
    - 注：`SkillComponent` 仅有 `skill_id` / `owner` / `active_nodes` 三个字段（`SkillDefs.hpp:737-742`），按 `skill_id` 过滤；`skill_id == 0` 沿用现行 `DestroyOwnedAreaFields` 的「全部销毁」哨兵约定。
2. **天剑特定状态恢复委托**：
    - 转移至 `HeavenlySwordDescent.cpp`：
      ```cpp
      namespace NoMoreDay::skills::HeavenlySwordDescent {
      void OnMasterySwitchCleanup(entt::registry &registry, entt::entity owner);
      }
      ```
    - 内部闭环执行以下状态还原（自 `BladeMasteryService.cpp:54-93` 天剑分支平移，语义不变），再调用 `DestroyOwnedPersistentFields(registry, owner, 11u)`：
      1. 读取场实体上的 `original_formation_attack_interval` / `original_channel_tick_interval`（>0 时）作为还原基准；
      2. 还原 `BladeFormationComponent::attack_interval`；
      3. 还原该玩家 `skill_id == 3` 的 `SpiritSwordAI::attack_interval`（剑灵继承剑阵攻速，遗漏将导致切换专精后剑灵攻速漂移）；
      4. 还原该玩家 `skill_id == 5` 的 `BeamChannelComponent::tick_interval` 并同步 `tick_timer = min(tick_timer, tick_interval)`（含 501 构筑覆盖限制注释一并平移）。
    - `BladeMasteryService` 仅调用接口，不再直接接触 `field.original_formation_attack_interval`。

### 2.4 UI 与渲染层只读接口设计

在 `HeavenlySwordDescent.hpp` 与 `BloodSea.hpp` 中暴露紧凑只读 DTO：

```cpp
struct HeavenlyFieldHudSnapshot {
    float remaining_duration = 0.0f;
};

struct BloodSeaHudSnapshot {
    float remaining_duration = 0.0f;
    bool has_void_keystone = false;
    float miasma_duration_bonus = 0.0f;  // 与组件字段同名，避免查询层二次映射
};

[[nodiscard]] HeavenlyFieldHudSnapshot QueryHeavenlyFieldHud(const entt::registry &registry, entt::entity player);
[[nodiscard]] BloodSeaHudSnapshot QueryBloodSeaHud(const entt::registry &registry, entt::entity player);
```

- `GameUiSnapshotBuilder.cpp` 与 `PlayerHUD.cpp` 仅包含轻量 DTO 头，不再包含底层组件定义，切断依赖传递。
- 渲染层 `GameplayRenderAdapter.cpp` 包含 `PersistentFieldComponents.hpp`，继续以高性能直接读取渲染所需数据（如 `torrent_form`、由 `header.tick_timer/tick_interval` 派生的脉冲进度等）。

---

## 3. 验收标准与测试计划

1. **编译与依赖隔离断言**：
   - `SkillDefs.hpp` 中不出现 `HeavenlySwordFieldComponent` 与 `BloodSeaFieldComponent`。
   - `SkillSystem.cpp` 中不直接 include `PersistentFieldComponents.hpp`，不显式遍历 `HeavenlySwordFieldComponent` 或 `BloodSeaFieldComponent`。
   - `BladeMasteryService.cpp` 中不包含 `PersistentFieldComponents.hpp`，无 `field.original_formation_attack_interval` 直接访问。
   - 全部引用点（`rg "HeavenlySwordFieldComponent|BloodSeaFieldComponent" src tests` 列出的文件）include 更新完毕，全仓编译通过。
2. **行为等价性与功能回归**：
   - 单元测试：`tests/unit/SkillBehaviorGuardTests.cpp` 全绿。
   - 功能测试：`tests/functional/HeavenlySwordDescentNodes.cpp` 与 `BloodSeaNodes.cpp` 全绿。
   - 集成测试：`tests/integration/GameplaySystems.cpp` 与专精切换用例全绿。
   - 构建质量：`build.bat RelWithDebInfo` 0 警告通过。
