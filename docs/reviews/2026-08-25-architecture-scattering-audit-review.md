# 架构分散性审计 Review（跨系统耦合与双实现漂移）

> **Track**: architecture_scattering_audit_20260825
> **Date**: 2026-08-25
> **Status**: 结论 `修改` — 审计性审查（非某次实施的验收），输出跨系统分散风险清单
> **审查范围**: src/ 全量架构审计（战斗双路径、状态/上帝类、分层、单例、数据散落、事件机制）
> **依据**: `AGENTS.md`、`conductor/code_standard.md`；证据来自 codebase-memory-mcp 图工具与源码 grep/read

---

## 结论

**结论：`修改`**。发现 1 项 Blocker（真实伤害路径在战斗内核切换失败时静默归零、无回退），1 项 High（GameplayState 耦合黑洞），其余 Medium/Low 若干。修复优先级见文末"下一步"。

## 审查文件边界

- `git status --short`：工作区干净（本审查为只读，未产生改动）。
- 审查对象为全量 `src/`，重点抽样：`src/game/systems/combat/DamagePipeline.cpp`、`src/game/application/states/GameplayState.cpp`、`src/game/application/ui/UIInventoryController.cpp`、`src/game/foundation/SharedContext.hpp`、`src/game/foundation/combat_v2/`、`src/game/systems/skill/`、`src/engine/render/` 巨文件集。

## 逐条发现

### Blocker-1 — 战斗内核切换失败静默归零，无回退（双实现漂移）

- `src/game/systems/combat/DamagePipeline.cpp:446-465`：非模拟路径（`!is_simulation`）且 base_pool 存在有效值时，**全量**走 `CombatV2::CombatV2RuntimeMode::CandidateOnly`；`runtimeResult.status != Ok` 时仅 `LOG_ERROR` 并 `return DamageResult{}` —— 伤害直接归零，玩家无感知，无旧路径回退。
- 双路径分歧：`is_simulation`（技能预览）仍走旧 DamagePipeline 内联逻辑（同文件 467 行起 interceptor 链：Invulnerable / Suppressor 距离减伤等）；模拟与真实对同一请求分走两套实现，任何单侧修改都会产生预览与实战结果漂移。
- 新内核成熟度存疑：`src/game/foundation/combat_v2/CombatV2RuntimeFacade.hpp` 的 `CombatV2RuntimeStatus` 默认值为 `NotImplemented`；`CombatV2RuntimeFacade.cpp:142` DualRunCompare 分支 `stageMask` 硬编码 `Multipliers | Final`，双跑校验尚未铺开到全阶段。
- 建议：CandidateOnly 失败时必须回退旧路径并记录 mismatch 报告；完成 DualRun 全阶段校验后再切换。

### High-1 — GameplayState 耦合黑洞（90+ include、单方法 453 行）

- `src/game/application/states/GameplayState.cpp`：include 90+ 头文件，横跨渲染、UI、战斗、物品、AI、VFX、世界系统。
- `GameplayState::OnUpdate` 423-876 行（453 行单方法）、`OnRender` 977-1173、`InitializeEntities` 194-380 —— 场景编排全部内联。
- 风险：任何系统的生命周期/初始化顺序变更都会波及该状态；状态难以拆分测试；新增玩法系统必须在此登记。

### High-2 — 分层双向穿透（UI↔Systems）

- UI 层直连系统层：`src/game/application/ui/UIInventoryController.cpp` include `game/systems/item/MaterialRegistry.hpp`、`RunewordSystem.hpp`。
- 系统层反向依赖 UI：`src/game/foundation/SharedContext.hpp` 以 `std::function` 回调补丁（`showMessageBox`、`openCraftingMergePanel`），注释自述 *"InventorySystem sits below the UI layer and must not include UI headers, so they route through this callback"* —— 分层被破坏后的逐例补救，每新增一个跨层需求就往 SharedContext 追加回调，属"回调堆栈"式蔓延。

### Medium-1 — 全局单例散落（7+ 处 `static X& Get()`）

- `DamagePopupManager.hpp:19`、`SharedStash.hpp:13`、`MaterialRegistry.hpp:32`、`PopupRenderer.hpp:56`、`GPUParticleSystem.hpp:20`、`GPUSkillEffectSystem.hpp:18`、`QualityTierManager`（`.Get` 20+ 调用者）；另有 `ModifierRuntimeRegistry::Get`（32 调用者）、`SkillRegistry::Get()`（被全部技能行为引用）。
- 无组合根注入；任意模块随手可取全局状态，测试隔离与生命周期管理困难。

### Medium-2 — 平衡数值/效果参数内联硬编码

- float 字面量密度（正则采样）：`UISkillSpecRenderer.cpp` 119、`GPUSkillEffectSystem.cpp` 81、`SkillSystem.cpp` 78、`SwordIntentVisualSystem.cpp` 45、`GameplayRenderAdapter.cpp` 41、`VisualFXSystem.cpp` 41、各技能 behavior 30-37。
- 唯一集中点 `CombatConstants.hpp`（33 个）。伤害系数、特效参数、UI 显示数字散落各处，调平衡需全局搜索且易漏改。

### Medium-3 — 事件机制仅存在于战斗域

- 全项目唯一事件总线 `src/game/contracts/impl/CombatEventDispatcher.hpp`（OnKill/OnCrit 等，被 MonsterAffixSystem、HazardSystem、FragmentDropSystem、FactionAggroSystem 监听）。
- 物品/技能/VFX 域之间全部直接方法调用；新增需求只能改调用链，无法横向扩展。

### Medium-4 — 巨型文件上帝类

- `RenderGraph.cpp` 110KB、`GPUHardwareValidationGate.cpp` 107KB、`SkillSystem.cpp` 105KB（2233 行，最大圈复杂度 73）、`RenderSystem.cpp` 88KB、`UIRenderer.cpp` 75KB、`QualityTierManager.cpp` 73KB、`SkillNodeAssetRegistry.hpp` 66KB（数据内联在 header）。
- `DamagePipeline.cpp` 65KB 内嵌 `FixedVector` 容器方法被同文件调用 138 次——单文件内强耦合、无边界。

### Low-1 — 技能行为注册依赖"强制链接"惯用法

- `src/game/systems/skill/behaviors/SkillBehaviorRegistry.cpp`：函数局部 static map + 外部注册函数强制链接；每个新 behavior 必须记得注册，否则**静默不生效**；技能数值仍散落在各 behavior .cpp。

## 剩余风险

1. Blocker-1 的修复需要确认 combat_v2 内核覆盖全部旧 interceptor（Invulnerable/Suppressor 等），否则回退与切换判断依据不足。
2. 本次审查基于静态证据，未运行游戏验证双路径行为；建议实机复现一次"新内核失败→伤害归零"场景留证。
3. DualRun 双跑校验目前仅 `Multipliers | Final` 两阶段，其余阶段（如 crit/减伤）无校验覆盖。

## 下一步

1. **Blocker-1（立即）**：CandidateOnly 失败回退旧路径；补 DualRun 覆盖全阶段；实机验证。
2. **短期**：将 UI 显示数值与效果参数收拢到数据表/注册表（优先 `UISkillSpecRenderer.cpp` 119 处）。
3. **中期**：GameplayState 按域拆分编排器；物品/技能域引入事件或显式依赖注入。
4. **长期**：统一系统注册模式（对齐 SkillBehaviorRegistry 实践）；单例收敛到组合根注入。
