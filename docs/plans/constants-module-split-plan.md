# 常量下沉到所属编译模块 整改方案

**Plan for:** 把 `src/game/components/Common.hpp` 中按模块归属的常量下沉到对应编译模块，借机整理全项目常量管理
**Workflow:** planning (`docs/workflows/planning.md`)
**Build gate:** `build.bat`（RelWithDebInfo，唯一构建路径）+ `ctest -L ci` + `build.bat check`

## 1. 现状分析

### 1.1 Common.hpp 的三段式构成（共 794 行）

| 区段 | 行号 | 内容 | 性质 |
|---|---|---|---|
| `NoMoreDay::Constants` | 11–498 | 14 个顶级子命名空间，约 230 个常量 | **本次整改对象** |
| 生物群系枚举 + 位掩码辅助 | 500–587 | `BiomeStyle` / `BiomeFeature` / `BiomeID` + `ToBiomeFeatureMask` 等 | 类型，本次一并下沉到 `data/BiomeTypes.hpp`（见 §3.6） |
| ECS 组件 | 589–794 | `Position` / `Velocity` / `HealthComponent` 等 | 全模块共用，保留在 Common.hpp |

### 1.2 编译开销成因

`src/game/pch.hpp:70` 包含 `Common.hpp`，且**每个** game 子目标都 `target_precompile_headers(... PRIVATE src/game/pch.hpp)`（`systems/ai/CMakeLists.txt`、`systems/world/CMakeLists.txt` 等已确认）。因此：

- 每个 game TU 都会**强制解析全部 ~230 个常量**（含 `Combat`、`AI`、`Astrolabe` 等与自身无关的块）；
- 修改任意一个常量都会改变 Common.hpp → **使共享 PCH 失效 → 全 game 模块重编**。

### 1.3 常量归属与消费者清单（grep 证据）

| 常量命名空间（Common.hpp 行号） | 归属模块 | 模块外消费者 |
|---|---|---|
| `World` (+`Map`/`Fog`) 14–42 | world | `scene/SceneManager.cpp:195,204,287`、`states/GameplayState.cpp`、`render/HeightFieldAdapter.cpp:62-64`、`render/GPUEntityAdapter.hpp:90`、`render/GameplayRenderAdapter.cpp:210`、`systems/physics/PhysicsSystem.cpp:221-222`(仅 MAP_BOUNDARY)、`systems/render/AirWallRenderer.cpp:88-90`、`systems/skill/ProjectileSystem.hpp:36-38`(GRID_COLS/ROWS/CELL_SIZE) |
| `Generator::Cave` 45–62 | world | 无（MapSystem + MosaicMapGenerator 同属 world） |
| `AI`(+Support/Assassin/Tank/Patrol/Chase) 65–127 | ai | `Enemy` 依赖 `AI::DORMANCY_THRESHOLD`（见 §3.2） |
| `Items`(等级缩放) 131–139 | item | 无（ItemFactory 属 item；测试 ItemLevelScalingTest） |
| `Combat`(+Elite/Pipeline/Conversion/System/Attribute/Scaling/Cap) 141–309 | combat | `components/Stats.hpp`(默认成员值)、`components/Combat.hpp:27`、`systems/stats/AttributePipeline.cpp`、`utils/MonsterScaling.cpp`、`states/GameplayState.cpp:261,315,762`、`systems/ui/UICharacter.cpp:432` |
| `Physics` 312–326 | physics | `systems/world/TilemapCollisionSystem.cpp:93`、`states/GameplayState.cpp:776,930` |
| `Enemy` 329–364 | world | `systems/ui/UIMinimap.cpp:270`、`components/EnemyComponent.hpp:135`、`states/GameplayState.cpp` |
| `Item`(拾取/药水) 367–379 | item | 无（InventorySystem 属 item） |
| `Astrolabe` 381–424 | data | `systems/ui/AstrolabeRenderer.cpp`、`systems/ui/UIAstrolabe.cpp`（AstrolabeRegistry / TalentLayoutService 在 data） |
| `StashConfig` 426–449 | item | 无（StashSystem / SharedStash 属 item） |
| `Movement` 452–459 | combat | 无（MovementStanceSystem 已归属 systems/combat，唯一消费者） |
| `Skill`(+BladeWard) 462–482 | skill | 无（ProjectileSystem 属 skill，唯一消费者） |
| `Render` 485–491 | engine/render | `app/Game.cpp:301,318` |
| `Visuals`(COLOR_BLADE_ASCENDANT) 493–497 | vfx | `systems/skill/SkillSystem.cpp:601`、`systems/skill/behaviors/FlowingThrust.cpp:451` |

**要点：**
- `Combat`、`World`、`Enemy`、`Astrolabe` 是**跨模块**常量，下沉后由外部 TU 显式 include；
- `Movement`、`Skill`、`Item`、`StashConfig`、`Generator::Cave` 是**模块内独占**常量，下沉收益最大（彻底移出 PCH 解析面）；
- `Combat` 被 `Stats.hpp`/`Combat.hpp`（PCH 常驻组件头）用作默认成员值，是唯一需要"拆引用"的块（见 §3.3）。

### 1.4 发现的既有重复常量（顺带整改）

| 重复项 | 位置 | 说明 |
|---|---|---|
| `Constants::Render::MAX_PARTICLES_DEFAULT=100000`、`MAX_SKILL_EFFECTS=10000` | Common.hpp 485–491 | 与 `engine/render/RenderConstants.hpp` 中 `RenderConstants::GPU::MAX_PARTICLES=200000`、`MAX_SKILL_EFFECTS=1024` **数值不一致且均未查实谁权威**。app 引导只用 Common.hpp 版本。需归一到 engine 并核对数值 |
| `Constants::Visuals::COLOR_BLADE_ASCENDANT={195,248,245,255}` | Common.hpp 495 | 与 `engine/render/GPUData.hpp` `NoMoreDay::components::Colors::BLADE_CYAN={195,248,245,255}` **完全同值**。3 处调用点可改指 Colors::BLADE_CYAN |

## 2. 整改目标

1. **编译优化**：把按模块归属的常量移出 Common.hpp，使各 game TU 只解析自己用到的常量块；改某模块常量不再触发共享 PCH 失效（不再全量重编）。
2. **归属清晰**：常量头文件与所属编译模块同目录（对齐 M4 按目录拆分的子目标结构）。
3. **去重**：消除 §1.4 的两处重复常量。

## 3. 实施思路与关键决策

### 3.1 目标头文件布局（命名空间保持不变，仅位置迁移）

消费者代码中的 `using namespace NoMoreDay::Constants::AI;` 等写法**全部保留**，只改 include，零 API 改动：

```text
src/game/systems/world/WorldConstants.hpp        // Constants::World(+Map/Fog)
src/game/systems/world/MapGeneratorConstants.hpp // Constants::Generator::Cave
src/game/systems/world/EnemyConstants.hpp        // Constants::Enemy
src/game/systems/ai/AIConstants.hpp              // Constants::AI(+Support/Assassin/Tank/Patrol/Chase)
src/game/systems/combat/CombatConstants.hpp      // Constants::Combat(+Elite/Pipeline/.../Cap)
src/game/systems/combat/MovementConstants.hpp    // Constants::Movement
src/game/systems/physics/PhysicsConstants.hpp    // Constants::Physics
src/game/systems/item/ItemConstants.hpp          // Constants::Item + Constants::Items
src/game/systems/item/StashConfig.hpp            // Constants::StashConfig（含 UNLOCK_COSTS 数组 + getUnlockCost）
src/game/systems/skill/ProjectileConstants.hpp   // Constants::Skill(+BladeWard)
src/game/data/AstrolabeConstants.hpp             // Constants::Astrolabe
src/game/data/BiomeTypes.hpp                     // BiomeStyle/BiomeFeature/BiomeID + 掩码辅助（§3.6）
engine/render/RenderConstants.hpp                // 并入 Constants::Render（§1.4 去重，见 §3.4）
```

**命名规避**：不新建 `SkillConstants.hpp` —— `components/SkillDefs.hpp:22` 已有 `NoMoreDay::SkillConstants` 命名空间，避免同名头/命名空间混淆，改用 `ProjectileConstants.hpp`（该块内容全部是投射物参数）。

**CMake 影响**：新头文件均为纯 header（constexpr / inline constexpr / inline），无新 `.cpp`，无需改任何 `target_link_libraries`；include 目录本就是根相对 `${CMAKE_SOURCE_DIR}/src`，无新增层间依赖，`check_module_boundaries.py` 规则不受影响。

### 3.2 跨模块依赖（头文件内 include）

- `EnemyConstants.hpp` 中 `AWAKEN_DISTANCE_MAX/MIN` 依赖 `AI::DORMANCY_THRESHOLD`（Common.hpp:354-357）→ `EnemyConstants.hpp` 需 `#include "game/systems/ai/AIConstants.hpp"`。
- `CombatConstants.hpp` 内部 `Combat::Cap::DODGE/BLOCK` 依赖 `Combat::Scaling::DODGE_MAX_CHANCE/BLOCK_MAX_CHANCE`（同文件，天然满足）。

### 3.3 Stats.hpp / Combat.hpp 的拆引用（唯一行为相关改动点）

`components/Stats.hpp`（PCH 常驻）用 `using namespace NoMoreDay::Constants::Combat;` 取 `DEFAULT_MAX_HEALTH/MANA/MOVE_SPEED/CRIT_CHANCE/CRIT_DAMAGE/ATTACK_SPEED` 作字段默认值；`components/Combat.hpp:27` 用 `Combat::System::DEFAULT_ATTACK_COOLDOWN`。若让这两个 PCH 头 include `CombatConstants.hpp`，则 Combat 块仍被 PCH 强解析，`CombatConstants.hpp` 变更仍失效 PCH，**白拆**。

**方案**：把这两个头引用的 7 个常量改为**字面量默认值 + 注释指向常量名**（这些值本就是"测试用默认值"，注释已注明），从而：
- Stats.hpp / Combat.hpp 不再依赖 Combat 常量头 → Combat 块彻底离开 PCH 解析面；
- 共享 PCH 不再因 Combat 数值调整而失效。

> 例外确认项：若产品希望 `CombatStats` 默认值与 `CombatConstants` 保持单点同步，则改为 `Stats.hpp` 显式 include `CombatConstants.hpp` 并接受 Combat 块留在 PCH 中 —— 需用户裁决（见 §7 Q1）。

### 3.4 Constants::Render 归一与去重（GPU 相关常量全貌）

**GPU 常量分三层，仅一层需要动：**

| 层 | 位置 | 状态 |
|---|---|---|
| `NoMoreDay::Constants::GPU`、`Constants::Lighting` | `engine/render/GPUData.hpp`（TEXTURE_LAYER_SIZE、MAX_TEXTURE_LAYERS、MAX_FORCE_FIELDS、MAX_TRAILS、STATUS_*、FIRE_* 等） | **已正确归位**，不在 Common.hpp，无需改动 |
| `NoMoreDay::RenderConstants::GPU`（MAX_ENTITIES/MAX_PARTICLES/MAX_SKILL_EFFECTS/MAX_POPUPS/MAX_GLYPHS） | `engine/render/RenderConstants.hpp` | **已归位**，但数值待核实（见下） |
| `Constants::Render`（MAX_PARTICLES_DEFAULT=100000、MAX_SKILL_EFFECTS=10000） | Common.hpp 485–491 | **本次整改对象**（T13） |

**查实结果（grep 全 src）：**
- 运行时真实容量由 `app/Game.cpp:301,318` 用 game 侧常量传入：`GPUParticleSystem::Init(100000)`、`GPUSkillEffectSystem::Init(rm, 10000)`；且 `GPUParticleSystem::Init` 默认参数就是 `100000`（GPUParticleSystem.hpp:23）。
- engine 侧 `RenderConstants::GPU` 的 `MAX_ENTITIES=200000`、`MAX_PARTICLES=200000`、`MAX_SKILL_EFFECTS=1024` 在 src 中**零使用点**（grep 仅命中定义行，且 `MAX_GLYPHS` 才是真正被引用者——RenderSystem.cpp:971；`MAX_POPUPS` 另有 PopupRenderer 自己的 `static constexpr`）。
- 结论：**真实权威值 = game 侧当前值（粒子 100000 / 技能特效 10000）**，engine 侧 200000/1024 是死常量。

**T13 动作修正：** 把 Common.hpp 的 `Constants::Render` 块并入 `RenderConstants.hpp`（作为权威值 100000/10000），app/Game.cpp 改引用；同时**删除或校正 engine 侧未使用的 `MAX_ENTITIES`/`MAX_PARTICLES`/`MAX_SKILL_EFFECTS`**（`MAX_GLYPHS`/`MAX_POPUPS` 保留），`GPUData.hpp:19 MAX_TEXTURE_LAYERS` 也需核实是否被 shader/其他机制引用后再定去留。

### 3.5 Visuals 去重

`Constants::Visuals::COLOR_BLADE_ASCENDANT` 与 `components::Colors::BLADE_CYAN` 同值。3 个调用点（SwordIntentVisualSystem.cpp:68、SkillSystem.cpp:601、FlowingThrust.cpp:451）改指 `NoMoreDay::components::Colors::BLADE_CYAN`，删除 `Constants::Visuals` 块，不新建 vfx 常量头。

### 3.6 生物群系枚举下沉 + PCH 瘦身（用户确认）

**用户裁决：** 让 PCH 仅包含真正的、通用的、不常改动的头文件。生物群系枚举**本次一并下沉**到 `data/BiomeTypes.hpp`。

- `BiomeStyle/BiomeFeature/BiomeID` + `ToBiomeFeatureMask/AddBiomeFeature/RemoveBiomeFeature/HasBiomeFeature`（Common.hpp:500-587）→ `game/data/BiomeTypes.hpp`，命名空间 `NoMoreDay` 保持不变。
- 消费者显式补 include `game/data/BiomeTypes.hpp`：`components/WorldState.hpp`（json 序列化 + `biome` 字段）、`components/MapComponent.hpp`、`data/BiomeRegistry.{hpp,cpp}`、`data/MosaicData.hpp`、`data/ResonanceCalculator.cpp`、`scene/SceneManager.{hpp,cpp}`、`states/GameplayState.cpp`、`states/MosaicEditorState.cpp`、`states/MainMenuState.cpp`、`render/GameplayRenderAdapter.cpp`、`render/GPUEntityAdapter.hpp`、`game/WorldState` 相关序列化点。
- `WorldState.hpp` 的 `to_json/from_json(BiomeID)` 序列化函数随枚举迁移到 `BiomeTypes.hpp`（或在原处保留并 include 新头，取后者改动最小）。
- **PCH 瘦身核查（T16）**：Common.hpp 移除常量+枚举后仅剩纯组件（Position/Velocity 等，通用且稳定）→ 保留 PCH。其余 PCH 常驻项按"通用 + 不常改"原则复核：`TagRegistry.hpp`（生成文件）、`Stats.hpp`/`Combat.hpp`/`SkillDefs.hpp`、`engine/resource/EquipmentAssetRegistry.hpp`/`RuneAssetRegistry.hpp`。`SkillDefs.hpp` 内含 `NoMoreDay::SkillConstants`（也是常改动常量块，同属"项目常量管理"范畴）→ 列入 Phase 3 单独评估是否下沉。本次不拆 Stats/Combat/SkillDefs 组件内容（非本任务常量范围）。

## 4. 原子任务拆分

### Phase 1：常量下沉（按依赖顺序）

- [ ] T1 新建 `systems/world/WorldConstants.hpp`，从 Common.hpp 迁移 `World`(+`Map`/`Fog`) 块；给 `SceneManager.cpp`、`GameplayState.cpp`、`HeightFieldAdapter.cpp`、`GPUEntityAdapter.hpp`、`GameplayRenderAdapter.cpp`、`PhysicsSystem.cpp`、`AirWallRenderer.cpp`、`ProjectileSystem.hpp`、`FogOfWarSystem.cpp`、`TilemapCollisionSystem.cpp`、`EnemySpawnSystem.cpp`、`MapSystem.cpp`、`LevelManager.cpp` 补 include；删 Common.hpp 块。
- [ ] T2 新建 `systems/world/MapGeneratorConstants.hpp`，迁移 `Generator::Cave`；`MapSystem.cpp`、`MosaicMapGenerator.cpp` 补 include；删 Common.hpp 块。
- [ ] T3 新建 `systems/ai/AIConstants.hpp`，迁移 `AI` 全部子块；`EnemyAIBehaviors.cpp`、`AISystem.cpp` 补 include；删 Common.hpp 块。
- [ ] T4 新建 `systems/combat/MovementConstants.hpp`，迁移 `Movement`；`MovementStanceSystem.cpp` 补 include；删 Common.hpp 块。
- [ ] T5 新建 `systems/skill/ProjectileConstants.hpp`，迁移 `Skill`(+`BladeWard`)；`ProjectileSystem.cpp` 补 include；删 Common.hpp 块。
- [ ] T6 新建 `systems/item/ItemConstants.hpp`，迁移 `Item` 与 `Items`；`InventorySystem.cpp`、`ItemFactory.cpp` 补 include；删 Common.hpp 两块。
- [ ] T7 新建 `systems/item/StashConfig.hpp`，迁移 `StashConfig`；`StashSystem.cpp`、`SharedStash.{hpp,cpp}` 补 include；删 Common.hpp 块。
- [ ] T8 新建 `systems/physics/PhysicsConstants.hpp`，迁移 `Physics`；`PhysicsSystem.cpp`、`TilemapCollisionSystem.cpp`、`GameplayState.cpp` 补 include；删 Common.hpp 块。
- [ ] T9 新建 `systems/world/EnemyConstants.hpp`，迁移 `Enemy`（**内部 include AIConstants.hpp**，见 §3.2）；`EnemySpawnSystem.cpp`、`UIMinimap.cpp`、`EnemyComponent.hpp`、`GameplayState.cpp` 补 include；删 Common.hpp 块。
- [ ] T10 新建 `data/AstrolabeConstants.hpp`，迁移 `Astrolabe`；`AstrolabeRegistry.cpp`、`TalentLayoutService.cpp`、`AstrolabeRenderer.cpp`、`UIAstrolabe.cpp` 补 include；删 Common.hpp 块。
- [ ] T11 新建 `systems/combat/CombatConstants.hpp`，迁移 `Combat` 全部子块；同时按 §3.3 拆掉 `Stats.hpp` / `Combat.hpp` 对 Combat 常量的引用；给 `CombatSystem.cpp`、`DamagePipeline.cpp`、`DamageMitigationService.cpp`、`EliteModifierSystem.cpp`、`AttributePipeline.cpp`、`MonsterScaling.cpp`、`UICharacter.cpp`、`GameplayState.cpp`、`CombatFormula.hpp` 补 include；删 Common.hpp 块。
- [ ] T12 新建 `data/BiomeTypes.hpp`，下沉生物群系枚举（`BiomeStyle/BiomeFeature/BiomeID` + 掩码辅助，Common.hpp:500-587）；给 §3.6 列出的消费者补 include；`WorldState.hpp` 的 BiomeID json 序列化随迁或留驻；删 Common.hpp 枚举块。

### Phase 2：去重与收尾

- [ ] T13 查实 §1.4 渲染预算权威数值（已核实：真实运行时容量=game 侧 100000/10000，engine 侧 RenderConstants::GPU 的 MAX_ENTITIES/MAX_PARTICLES/MAX_SKILL_EFFECTS 为死常量）→ `Constants::Render` 并入 `engine/render/RenderConstants.hpp`（权威值 100000/10000），`app/Game.cpp:301,318` 改引用，删 Common.hpp 块；清理 engine 侧死常量（MAX_GLYPHS/MAX_POPUPS 保留），核实 `GPUData.hpp:19 MAX_TEXTURE_LAYERS` 去留。
- [ ] T14 3 处 `COLOR_BLADE_ASCENDANT` 调用点改指 `Colors::BLADE_CYAN`，删 `Constants::Visuals` 块。
- [ ] T15 `build.bat` 全量构建通过；`ctest -L ci` 通过；`build.bat check` 通过；`rg "Constants::|BiomeStyle|BiomeFeature|BiomeID" src/game/components/Common.hpp` 确认常量/枚举归零。
- [ ] T16 **PCH 瘦身核查**：确认 Common.hpp（纯组件）保留 PCH；按 §3.6 原则复核其余 PCH 常驻项；`SkillDefs.hpp::SkillConstants` 是否下沉给出评估结论并记录到 memory（可排入 Phase 3）。

## 5. 测试方法

| 级别 | 命令 | 目的 |
|---|---|---|
| 构建 | `build.bat`（RelWithDebInfo，j=7） | 每任务后编译门禁 |
| CI | `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` | 行为回归 |
| 模块门禁 | `ctest ... -L combat -L skill -L item -L world -L ai` | 领域回归 |
| 预检 | `build.bat check` | JSON/模块边界/ABI 治理（确认零新增跨层 include） |
| 静态 | grep 确认 Common.hpp 常量归零、消费者 include 齐全 | 任务完成证明 |
| 编译收益 | 记录改动前后"改单个常量触发重编译的 TU 数/时长" | 验证优化目标 |

## 6. 验证任务完成（退出标准）

1. Common.hpp 中 `NoMoreDay::Constants` 块与生物群系枚举全部移除（`rg "Constants::|BiomeStyle|BiomeFeature|BiomeID" Common.hpp` 无命中；`Render`/`Visuals` 已去重）。
2. 每个新常量头只被其消费者 include，无新增 PCH 常驻路径（`Stats.hpp`/`Combat.hpp` 不再依赖 Combat 常量头；生物群系枚举已移到 `data/BiomeTypes.hpp`）。
3. `build.bat` 干净；`ctest -L ci` 全绿；`build.bat check` 绿；伤害数值断言不变。
4. 数值行为零变化：常量值原样迁移，仅 §3.4 的渲染预算与 §3.3 的默认值拆引用需在 T13 明确核对。
5. PCH 仅保留通用+不常改头文件（T16 核查结论记录到 memory）。

## 7. 已确认决策（用户裁决）

- **Q1（§3.3）**：`Stats.hpp`/`Combat.hpp` 对 Combat 常量的耦合 → **字面量默认值+注释拆引用**，Combat 块彻底移出 PCH。
- **Q2（§1.4 / T13）**：渲染预算权威值查实后**直接归并**，无需再确认。
- **Q3（§3.6）**：生物群系枚举**本次一并下沉**到 `data/BiomeTypes.hpp`；PCH 瘦身为本次目标，仅保留真正通用+不常改的头文件。
