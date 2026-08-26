# NoMoreDay 子系统分散性与可维护性风险深度审查报告

> **审查目标**: 全量代码库子系统逻辑分散性、跨模块耦合度、数据生命周期一致性与长期可维护性风险审计
> **结论**: `修改`
> **审查轮次**: 首次审查（全量专项架构审计）
> **输入**: 
> - 审查标准: [`docs/workflows/review.md`](file:///d:/PRJ/NoMoreDay/docs/workflows/review.md)
> - 架构与性能规范: [`conductor/code_standard.md`](file:///d:/PRJ/NoMoreDay/conductor/code_standard.md) (V2.1)
> - 技术栈基线: [`conductor/tech-stack.md`](file:///d:/PRJ/NoMoreDay/conductor/tech-stack.md)
> - 检索证据源: `codebase-memory-mcp` 依赖图谱与源码 AST / Grep 分析

---

## 结论摘要

**结论：`修改`**。
全量代码审查表明，随着系统功能的持续迭代，当前项目在**技能特殊逻辑**、**状态效果 (Buff/Ailment)**、**物品与持久化数据生命周期**、**怪物机制切分**等核心领域出现了显著的**逻辑分散 (Logic Scattering)** 与**模式异化 (Paradigm Divergence)** 现象。多处违反了 DOD POD 组件规范、热路径无字符串规范以及单一事实来源 (Single Source of Truth) 原则，存在隐蔽的状态覆盖、生命周期悬挂和级联重构风险。

---

## 变更文件边界

- `git status --short`：干净（本轮为架构审计，不直接修改业务源码）。
- 审查覆盖范围：`src/core/`, `src/engine/`, `src/game/`, `src/app/`。重点审计：
  - `src/game/systems/skill/`
  - `src/game/systems/combat/`
  - `src/game/systems/item/`
  - `src/game/systems/world/`
  - `src/game/application/ui/`
  - `src/game/application/render/`
  - `src/game/foundation/components/`

---

## 质量与风险评估

依据 [`conductor/code_standard.md`](file:///d:/PRJ/NoMoreDay/conductor/code_standard.md) 的关键条款对照：
1. **§2.1 堆分配与热路径控制**：发现 `BuffEffect`、`ItemComponent` 等高频实体组件包含动态 `std::string` 与 `std::vector`；高频战斗伤害结算依赖全局 `std::function` 包装。
2. **§5.3 EnTT 安全与组件生命周期**：发现 `SharedStash` 持有 `entt::entity` 句柄，跨场景依赖 JSON 序列化中转避免实体 ID 失效；`EffectSystem` 存在跨实体遍历局部状态覆盖 `PlayerStats` 的逻辑缺陷。
3. **§7.2 字符串无关核心逻辑**：发现技能行为中大量使用手写字符串字面量创建与索引 Buff。
4. **§1.1 单一职责与单向依赖**：发现技能专属分支逻辑散落在 8 个不同系统，渲染适配层直接穿透游戏玩法状态。

---

## 详细发现项 (Findings)

### Blocker-1 — 技能魔法 ID 与专属逻辑多处散落硬编码（违背数据驱动与开闭原则）

* **引用位置**：
  * [`src/game/contracts/impl/StatsSystem.cpp:256, 395`](file:///d:/PRJ/NoMoreDay/src/game/contracts/impl/StatsSystem.cpp#L256) (`skill_id == 2`, `source_skill_id == 9`)
  * [`src/game/systems/combat/DamagePipeline.cpp:548, 967`](file:///d:/PRJ/NoMoreDay/src/game/systems/combat/DamagePipeline.cpp#L967) (`skill_id == 1`, `source_skill_id == 9`)
  * [`src/game/systems/skill/ProjectileSystem.cpp:652-685`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/ProjectileSystem.cpp#L652-L685) (`switch (skill_id) { case 1: ... case 2: ... case 7: ... case 8: case 9: ... }`, `if (skill_id == 2)`)
  * [`src/game/systems/combat/VisualFXSystem.cpp:69-72`](file:///d:/PRJ/NoMoreDay/src/game/systems/combat/VisualFXSystem.cpp#L69-L72) (`if (evt.skill_id == 2) ... else if (evt.skill_id == 7)`)
  * [`src/game/application/ui/PlayerHUD.cpp:32`](file:///d:/PRJ/NoMoreDay/src/game/application/ui/PlayerHUD.cpp#L32) (`if (summon.skill_id == 3)`)
  * [`src/game/application/states/GameplayState.cpp:1073`](file:///d:/PRJ/NoMoreDay/src/game/application/states/GameplayState.cpp#L1073) (`if (chan.skill_id == 7)`)
  * [`src/game/systems/skill/SkillSystem.cpp:1096, 1175, 1213`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSystem.cpp#L1096) (`chan.skill_id == 5`, `chan.skill_id == 7`)

* **问题陈述**：
  技能机制（专属光照参数、特定投射物裂变、属性特殊缩放、引导状态渲染）没有收拢在技能行为定义（`SkillBehavior`）或配置注册表中，而是以 `skill_id` 魔法数字的形式穿透到了属性计算、伤害管线、投射物移动、特效广播、HUD 和关卡状态机中。
* **风险评估**：
  新增或重构技能时必须跨 8 个系统同步排查修改，极易发生“改一漏十”，导致属性不同步、特效缺失、伤害结算漏算等回归缺陷。

---

### Blocker-2 — Buff/异常状态机制碎片化与非 POD 组件内存开销（DOD 违规与状态覆盖）

* **引用位置**：
  * [`src/game/foundation/components/Buff.hpp:55-86`](file:///d:/PRJ/NoMoreDay/src/game/foundation/components/Buff.hpp#L55-L86) (`struct BuffEffect` 包含 3 个 `std::string` 和 `std::vector<StatModifier>`)
  * [`src/game/systems/skill/behaviors/BladeBoomerang.cpp:157, 254`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/BladeBoomerang.cpp#L157)
  * [`src/game/systems/skill/behaviors/FlowingThrust.cpp:347, 416`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/FlowingThrust.cpp#L347)
  * [`src/game/systems/combat/EffectSystem.cpp:75-93`](file:///d:/PRJ/NoMoreDay/src/game/systems/combat/EffectSystem.cpp#L75-L93)

* **问题陈述**：
  1. **手写字符串与随意构造**：各个技能行为类直接就地 `get_or_emplace<ActiveEffectsComponent>`，手写字面量字符串 ID（如 `"flowing_thrust_swift"`）并构造临时的 `vector<StatModifier>`，缺乏统一的效果模板与工厂。
  2. **违反 DOD 规范**：`BuffEffect` 不是 POD 结构，高频挂载/刷新导致大量堆分配。
  3. **状态覆盖 Bug**：`EffectSystem.cpp` 遍历 `buffView` 时，将最后一个实体的 `playerRooted` 局部变量单向覆盖到所有 `PlayerStats` 上，在多玩家/召唤物/分身场景下存在逻辑覆盖漏洞。
  4. **多头推进**：`StatsSystem::UpdateBuffs`、`EffectSystem::Update`、`AilmentEngine::Tick` 分别推进不同类型的 Buff 与 DoT，规则分散。

---

### High-1 — 物品实体与持久化数据生命周期错位（SharedStash 跨场景 JSON 序列化中转）

* **引用位置**：
  * [`src/game/foundation/components/ItemComponent.hpp:152-206`](file:///d:/PRJ/NoMoreDay/src/game/foundation/components/ItemComponent.hpp#L152-L206) (30+ 字段上帝组件)
  * [`src/game/systems/item/SharedStash.hpp:11-52`](file:///d:/PRJ/NoMoreDay/src/game/systems/item/SharedStash.hpp#L11-L52)
  * [`src/game/systems/item/SharedStash.cpp:101-130`](file:///d:/PRJ/NoMoreDay/src/game/systems/item/SharedStash.cpp#L101-L130)

* **问题陈述**：
  `SharedStash` 作为全局单例，持有了当前关卡 `entt::entity` 句柄。当关卡切换清空 Registry 时，为了防止句柄失效，必须在 `suspend()` 中全量将仓库物品深拷贝转成 JSON 存入 `m_suspendedData`，在关卡加载完成后通过 `resume()` 重新解析 JSON 并调用 `ItemFactory::restoreItem` 重新建实体。
* **风险评估**：
  ECS Entity（世界交互实体）与 Data Model（数据/存档模型）概念混淆。频繁经过 JSON 进行内存中转不仅产生巨大的序列化开销，一旦生命周期调用时序异常（如异常退出、未完成恢复中断），将导致**全仓库物品数据损坏或丢失**。

---

### High-2 — 伤害管线全局 `std::function` 钩子解耦（热路径性能与单向依赖破坏）

* **引用位置**：
  * [`src/game/contracts/DamageResolutionHooks.hpp:17-48`](file:///d:/PRJ/NoMoreDay/src/game/contracts/DamageResolutionHooks.hpp#L17-L48)
  * [`src/game/systems/combat/DamagePipeline.cpp:1124`](file:///d:/PRJ/NoMoreDay/src/game/systems/combat/DamagePipeline.cpp#L1124)
  * [`src/game/systems/combat/CombatSystem.cpp:500`](file:///d:/PRJ/NoMoreDay/src/game/systems/combat/CombatSystem.cpp#L500)

* **问题陈述**：
  为了打破 `combat` 与 `skill` 的 CMake 循环依赖，使用了全局静态 `std::function`（`DamageResolutionHooks::execute`）。高频的伤害结算同时存在 `DamagePipeline::Execute`、`ResolveDamage` 间接调用、以及底层 `CombatSystem::ApplyDamage` 直调等 3 种入口。
* **风险评估**：
  每帧数千次调用的战斗热路径存在虚函数/函数指针跳转开销；底层直调 `ApplyDamage` 会旁路伤害减免、格挡、反伤与监控日志，造成机制不一致。

---

### High-3 — 怪物机制切分过度与 1347 行巨型内联头文件（Monster Subsystems Fragmentation）

* **引用位置**：
  * [`src/game/systems/combat/MonsterAffixSystem.hpp:1-1347`](file:///d:/PRJ/NoMoreDay/src/game/systems/combat/MonsterAffixSystem.hpp) (全代码内联在头文件)
  * [`src/game/systems/combat/EliteModifierSystem.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/combat/EliteModifierSystem.cpp)
  * [`src/game/systems/ai/AISystem.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/ai/AISystem.cpp)
  * [`src/game/systems/nemesis/NemesisGenerator.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/nemesis/NemesisGenerator.cpp)
  * [`src/game/systems/combat/BossFrameworkSystem.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/combat/BossFrameworkSystem.cpp)

* **问题陈述**：
  1. `MonsterAffixSystem.hpp` 超过 1300 行全部实现在 `.hpp` 中，包含大量复杂的伤害、粒子、物理判定代码，导致引入该头文件的编译单元极慢。
  2. 怪物的完整行为被割裂到 7 个子系统，各系统各自向 `CombatEventDispatcher` 注册回调，通过硬编码 priority（如 `-10`, `50`）调节顺序，隐性依赖严重。

---

### Medium-1 — 世界与关卡子系统所有权模式不一（Paradigm Inconsistency）

* **引用位置**：
  * [`src/game/systems/world/LevelManager.cpp:55-57`](file:///d:/PRJ/NoMoreDay/src/game/systems/world/LevelManager.cpp#L55-L57) (`std::unique_ptr<MapSystem>`, `EnemySpawnSystem`, `FogOfWarSystem`)
  * [`src/game/contracts/impl/StatsSystem.hpp:10`](file:///d:/PRJ/NoMoreDay/src/game/contracts/impl/StatsSystem.hpp#L10) (纯静态无状态系统)
  * [`src/game/systems/item/SharedStash.hpp:13`](file:///d:/PRJ/NoMoreDay/src/game/systems/item/SharedStash.hpp#L13) (全局单例)

* **问题陈述**：
  系统中并存“静态 ECS 系统”、“全局单例”与“LevelManager 堆持有系统”三套生命周期范式，关卡重置与热重载时各组件与系统销毁顺序不透明。

---

### Medium-2 — UI “上帝快照”与全能命令执行器（UI Snapshot & Command Hub Bloat）

* **引用位置**：
  * [`src/game/application/ui/GameUiSnapshotBuilder.cpp:1-981`](file:///d:/PRJ/NoMoreDay/src/game/application/ui/GameUiSnapshotBuilder.cpp) (981 行，引入几乎全域头文件)
  * [`src/game/application/ui/GameUiCommandHandler.cpp:1-1550`](file:///d:/PRJ/NoMoreDay/src/game/application/ui/GameUiCommandHandler.cpp) (1550 行，单文件包含全部 UI 交互业务)

* **问题陈述**：
  每帧全量扫描 ECS Registry 构建巨大的扁平 UI 快照；所有 UI 用户操作（物品、加点、技能、星盘、打造）全部分支集中在单一 1550 行的巨型文件中，缺乏按领域划分的命令分发器。

---

### Medium-3 — GPU 渲染适配层反向穿透游戏玩法逻辑（Render Adapter Gameplay Leakage）

* **引用位置**：
  * [`src/game/application/render/GPUEntitySync.cpp:72-95`](file:///d:/PRJ/NoMoreDay/src/game/application/render/GPUEntitySync.cpp#L72-L95)

* **问题陈述**：
  `GPUEntitySync` 渲染适配层直接判断 `PlayerTag`、`EnemyTag`、`ItemComponent`、`AIComponent` 并硬编码特定玩法渲染规则（如“怪物不旋转”、“物品不画 Sprite”），而非读取通用的 `RenderProxy` 组件。

---

## 最佳实践与重构建议 (Recommendations)

### 阶段一：P0 核心战斗与状态机止血治理

1. **技能机制全面数据化与行为收拢**：
   * 将 `ProjectileSystem`、`DamagePipeline`、`StatsSystem` 中的 `skill_id` 特判全面剔除；
   * 投射物光照参数、命中派生、裂变数量等下沉至 `SkillRegistry` 的 JSON 配置或由 `SkillBehaviorBase` 的回调方法提供。
2. **状态效果引擎统一与 POD 化**：
   * 将 `BuffEffect` 重构为固定大小 POD 结构，消除 `std::string` 和嵌套 `vector`；
   * 建立统一的 `StatusEffectSystem`，统一接管通用属性 Buff、异常状态 (Ailment)、控制效果 (Root/Silence) 与 DoT 计算。

### 阶段二：P1 领域模型与数据生命周期正规化

1. **物品数据模型与 ECS 实体脱耦**：
   * 明确分离 `ItemData`（纯数据，用于背包、仓库、存档）与 `ItemEntity`（仅用于场景掉落物与可视化）；
   * `SharedStash` 直接管理 `ItemData` 数组，彻底废除关卡切换时的 JSON 临时序列化。
2. **拆分 `MonsterAffixSystem.hpp`**：
   * 将实现移至 `.cpp` 编译单元；将怪物的 AI 决策、词缀触发与伤害输出整理为规范的行为流水线。
3. **确定单向模块依赖**：
   * 规范 `contracts` -> `foundation` -> `systems` 的单向分层，消除全局静态函数指针解耦带来的隐藏开销。

### 阶段三：P2 架构分治与 UI/渲染解耦

1. **UI 命令处理器领域分治**：
   * 将 `GameUiCommandHandler` 拆分为 `InventoryCommandHandler`、`SkillCommandHandler`、`CharacterCommandHandler` 等子处理器；
   * Snapshot 改为增量或按需构建。
2. **渲染代理组件化**：
   * 引入 `RenderProxyComponent`（承载旋转约束、材质 ID、图层掩码），渲染层只读 Proxy，彻底移除对游戏业务 Tag 的穿透判断。

---

## 剩余风险

1. 技能 `skill_id` 硬编码分支较多，重构为配置驱动时需逐一核对已有技能的数值和特效表现，避免手感差异。
2. `SharedStash` 改造涉及存档格式与旧数据迁移，必须保留向下兼容的反序列化升级路径。

---

## 下一步动作

1. [ ] 创建 Track：`skill_data_driven_decoupling`，优先清理 `ProjectileSystem.cpp` 和 `DamagePipeline.cpp` 中的 `skill_id` 魔法分支。
2. [ ] 创建 Track：`buff_ailment_engine_unification`，重构 `BuffEffect` 为 POD 结构并统一状态机推进。
3. [ ] 创建 Track：`item_model_ecs_separation`，实现 `ItemData` 纯数据化与 `SharedStash` 安全生命周期。
4. [ ] 将 `MonsterAffixSystem.hpp` 实现代码分离下沉至 `MonsterAffixSystem.cpp`。
