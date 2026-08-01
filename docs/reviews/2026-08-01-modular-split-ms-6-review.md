# MS-6 GPU Entity Render Boundary Adapter — Review

Date: 2026-08-01

## 审查目标

验证 MS-6 实施包：GPU 实体渲染的 Game ECS/`SharedContext` 暴露替换为 Game adapter + 窄化 Engine DTO 契约，符合 `docs/plans/modular-split-exe-lib-dll-implementation-plan.md` MS-6 节与 MS-0 所有权台账。

## 结论

最终：`提交`

## 轮次

### Round One — 修改

**输入**：设计（受保护，排除）、MS-6 审计方案、实施包（`git status`/`git diff`）、`docs/reports/modular-split-exe-lib-dll/ms-6/evidence.md`。

**变更边界**：符合 MS-6 包范围（GPUEntitySync 移动、GPUEntitySystem 窄化、GPUEntityAdapter 新增、RenderSystem.cpp:714、Game.cpp/hpp、6 个测试文件、ledger、checker REQUIRED_P0_SOURCES）。受保护设计文档不在 diff 且未读取。

**发现**：

- **[HIGH]** `RenderSystem.cpp:714` 无条件解引用 `frame.context.renderContext`。HEAD 原 `GPUEntitySystem::Get().Render(frame.context, frame.camera)` 内部对 `renderContext==nullptr` 有兜底（走 RenderLegacy，且未 Init 时早退）。改动后 `frame.context.renderContext->GPU()...` 在 Render 内 fallback 生效前即解引用。既有集成测试 `GPUHardwareValidationGateTest`（GameplayRuntimeHarness 仅设 registry/settings/renderAlpha，renderContext 恒 nullptr）会命中空指针/UB；`-L unit` 不含集成测试故未发现。修复：加 null 守卫（参考 MDIRenderBenchmark.cpp 写法）。
- **[LOW]** `SetUpdatedStatsIndices` 移动赋值改拷贝赋值（非热路径，可接受）。
- **[LOW]** 三访问器偏差（`ShadowBuffer/VisualStatsBuffer/SetUpdatedStatsIndices`）——评估为可接受：`GPUData.hpp` 纯 DTO 零 Game 依赖、Engine 无 game include、无新反向边；`MAP_BOUNDARY` 字面量 5000.0f 等价 `Constants::World::MAP_BOUNDARY`（技术债，有注释）。

**已复核通过**：文件移动（hpp byte 一致、cpp 仅 line-1）、API 窄化、EntityRenderFrame 取值正确、Game.cpp 三调用点、ledger 精确删 20（实读 JSON 51 条）、checker 实跑 51/51、RG-3 资源生命周期与 HEAD 逐字一致、RenderGraph/passes/graph 构建/ResourceManager/GPUResourceRegistry/pch/CMake 零触碰、热路径零拷贝、MDI/RenderLegacy 行为保留、`frame.mdi==nullptr→Get()` fallback 保留。

### Round Two — 提交（修复复审）

**修复**：`RenderSystem.cpp:714` 加 `if (frame.context.renderContext != nullptr)` 守卫，包裹两处解引用；生产路径（Game.cpp:208/323 注入 renderContext）恒非空不受影响；null 路径跳过 GPU 实体渲染等价原 RenderLegacy 早退。diff 仅此一处。

**重点调查**：gate 测试"missing FixtureRenderDriver" NO_GO 非回归——4 个 TEST_CASE 均为 HEAD（S6 commit `0fdce87`）已提交内容；用例 4 是 S6 新增 "Missing driver fails closed NOT_RUN" 负例（`RunGate("TEST_REV_NODRIVER",...)` 无 driver），"FixtureRenderDriver is required" 唯一来源 `GPUHardwareValidationGate.cpp:616-623`，修正 agent 所见即该负例；用例 2 正常传 harness。4/4 通过。

**验证**：`check_module_boundaries.py` 51/51 PASS；`git diff --check` exit 0；修正 agent 实跑 gate 4/4 无崩溃、build 双标记、定向 2/2。

**已解决发现**：HIGH（null guard）已关闭。
**未解决发现**：无。
**接受的剩余风险**：三访问器为长期公开 API 技术债（后续批建议收敛 `Sync()` 契约）；`MAP_BOUNDARY` 字面量重复；`evidence.md` 已同步修正（Round One 后一行描述更新为带守卫版本）。

## 下一步

按用户授权分批次提交；提交后处理剩余 46 条 MS-6 边（RenderSystem 优先子批次），再 MS-7/8，最后 DOD-2 实机 gate。

---

## Batch 1 — 死 include 清理（363b196）

**审查结论**：`提交`。删除 RenderSystem.cpp 3 个死 include（DamagePopupManager/SkillSystem/PlayerHUD）；MonsterAffixSystem 保留（EnemyTag 传递源，待 Batch 2）。ledger 删 3 边 + 校正 8 边行号，51 -> 48。独立验证：checker 48/48、ModuleBoundaryCheckerTest 6/6、build 双标记（ms6-b1-build.log）、git grep 0 匹配、diff --check exit 0。无发现。

## Batch 2 — GameplayRenderAdapter（主体迁移）

**审查结论**：`提交`（只读 reviewer；主代理修复 2 项后验证）。

**发现**：
- **[M1]** ExecuteScenePass 绘制顺序改变（旧序 trail→sword→stash→sprite→GPU().Render→pixel→blood→molten→holo；新序 GPU().Render 首→adapter 全部）。代码注释声明有意（player 盖过敌人属修正方向）；需实机目视确认。**接受风险**（已记录于 evidence）。
- **[M2]** VFX pass 内 resist-overlay 移到 `GPUSkillEffectSystem::Render` 之前（旧序 Submit→Render→distortion→resist overlay），抗性描边环由 skill mesh 之上变之下，未注释。**接受风险**（已记录于 evidence）。
- **[M3]** null-hooks 早返回先于引擎原语 `GPU().Render()`（hooks null 但 renderContext 非空时 GPU 实体不渲染）。当前调用方全为 renderContext==nullptr 无实害。**已修复**：`GPU().Render()` 提前到 null 检查之前（RenderSystem.cpp:569-585，注释说明引擎原语独立于 hooks，null renderContext 守卫保留）。
- **[L1]** onFrameData 的 DTO flags 在 flag 计算前构造恒 false（adapter 当前不用，隐式脆弱点；Batch 3 前移）。
- **[L2]** reviewer 误判 `MonsterAffixSystem.hpp` 无引用——实际 `MoltenTrailTag` 定义于 `game/systems/combat/MonsterAffixSystem.hpp:37`（adapter.cpp:243 使用）。include 恢复有效。
- **[OBS]** adapter.hpp include `app/SharedContext.hpp`（Game→App 依赖，checker 不扫 src/game；SharedContext 归属待后续里程碑）。

**已复核通过**：变更边界 17 文件；hooks 零 game/app 依赖；SharedContext 前向声明无边；render() hooks 参数 null 安全；4 hook 迁移逐段对照（唯一差异 M1/M2）；buffer 填充契约（Engine 持静态 buffer、adapter 填 DTO 指针、Engine 绘）；7 game 文件引用 adapter::s_itemGrid/VisibleItemCache 一致（无 RenderSystem 残留）；ledger 精确删 17（48->31）；checker REQUIRED_P0_SOURCES 移除 RenderSystem.cpp 保留 RenderSystem.hpp；UITests BloodSea 锁定改 GameplayRenderAdapter.cpp 断言精确匹配；graph 构建仅 3 处 lambda 加 hooks 参数；RG-3 零触碰；adapter 生命周期（Game.hpp:60、Game.cpp:303-307 先于 RenderSystem::Initialize、cleanup 反向安全）。

**修复后验证**：checker 31/31（files 16）、build 双标记（ms6-b2-fix2-build.log）、定向 6/6 26 断言、ctest unit 单独重跑全过（全量并行偶发失败为既有 HeavenlySword hasFreeze flaky，二进制每次不同，非回归）、git grep game/ RenderSystem.cpp 0 命中、diff --check exit 0。

**接受的剩余风险**：M1/M2 叠层顺序未经实机目视验证；L1 flag 时序脆弱点；OBS SharedContext 归属（Game→App 依赖留待后续里程碑，checker 不扫 src/game）。

## 下一步（Batch 3）

render() 参数 DTO 化（删 RenderSystem.hpp App 边 1 条）+ graphContext.shared 替换评估（需 RG-3 授权）→ Batch 4（剩余 25 边 GPULoot 2/GPUParticle 1/GPUSkillEffect 1/lighting 6/passes 13/VFX 2）→ MS-7/8 → DOD-2。

---

## Batch 3 + Batch 3b —— render() DTO 化 + pass rewire

**审查结论**：`修改`（M1 必须修正 + L2/L3/I4 建议 + I5 信息），修复后复验通过，**最终 `提交`**。

**发现与处置**：
- **[M1]** evidence.md 只记录 Batch 3（30 条）且声称 "7 passes read shared->resources"、"Batch 4 deferred pass rewire"——与已落地的 Batch 3b 矛盾。**已修复**：追加 Batch 3b 章节（7 pass rewire、ledger 30→23、checker 移除 RenderSystem.hpp+4 pass、守卫语法修复与缩进统一、定向 13/13 178 断言）、改写 Deferred 段为剩余 18 条 Game 边。
- **[L2]** OccluderExtractPass.cpp:359 守卫从 5 子句削弱为 3 子句（丢 qualityManager null 检查）。**已修复**：恢复 `context.qualityManager == nullptr`（4 子句 registry/resources/qualityManager/camera）。
- **[L3]** RadianceCascadesPass.cpp:234 守卫删 `!context.hdrSceneBuffer.IsValid()`。**已修复**：恢复（4 子句 qualityManager/resources/camera/hdrSceneBuffer）。
- **[I4]** FluidSimulationPass.cpp:582-583 过时注释 "through SharedContext"。**已修复**：改 `graph::RenderContext::resources`。
- **[I5]** settings.json 被验证运行自动改写（benchmarkScore/updatedAtUtc）。用户指示（m0549）："settings 不用管，测试时不加载该配置"，故不还原、不纳入提交。

**已复核通过**：RenderFrameInput 4 字段纯指针零 app 依赖；render() DTO 签名与全部调用方（gate 实 6 处 L491/498/745/760/1025/1115 + GameplayState:985 + SingleGpuTimerOwnerRegressionTest）；graphContext.resources 赋值（RenderSystem.cpp:1585）；7 pass rewire（FluidSimulation/GIComposite 守卫已闭合，`git grep shared` 0 残留）；4 测试/基准文件等价替换；ledger 23 = 5 pch + 18 Game（App 边清零）；checker REQUIRED_P0_SOURCES 移除 RenderSystem.hpp + 4 pass；硬约束全零改动（RenderGraph 类/graph 构建结构/RG-3/ResourceManager/GPUResourceRegistry/pch/CMake/build.bat）；无新反向边；diff --check exit 0。

**修复后复验（主代理）**：checker 23/23（files 11）、ModuleBoundaryCheckerTest 6/6、完整构建双标记 0 error、定向 `*S1a*,*S1b*,*GPU ABI*,*RenderGraph V5*` 13/13 178 断言、ctest unit 失败均确认既有 HeavenlySword hasFreeze flaky、diff --check exit 0。

**提交**：`refactor(render): dto render input and rewire pass resources`（待提交）。

**剩余 18 条 MS-6 Game 边**：GPULoot 2 + GPUParticle 1 + GPUSkillEffect 1 + lighting 6（GlobalHeightField 4/LightManager 2）+ passes 6（OccluderExtract 2/ShadowBuild 2/RadianceCascades 1/HeightShadow 1）+ VFX 2，属 Batch 4。

---

## Batch 4-A（琐项：粒子常量下沉 + 高度场死 include）— `提交`

**审查目标**：核验 GPUParticleSystem 常量下沉 RenderConstants 与 GlobalHeightField 死 include 删除（ledger 23→21）。

**变更边界**：7 文件（ledger JSON、checker、GPUParticleSystem.cpp、RenderConstants.hpp、GlobalHeightField.cpp、Common.hpp、ModuleBoundaryCheckerTest.py）；settings.json 为用户指示既有残留（不 stage）；受保护设计文档排除。

**发现**：无 Blocker/High/Medium。唯一观察：GlobalHeightField ledger 行号 5/6/7→4/5/6 正确重编号（删一行 include 的自然位移），checker 21/21 证明一致。

**已复核通过**：4 常量值（10000/256/0.1f/0.016f）与 Common.hpp HEAD 逐字一致；命名空间 `NoMoreDay::RenderConstants::ParticleConfig`（RenderConstants.hpp:267-276 内嵌于 :5-6），引用点 :393/:568/:600-601 由两处 using（:392/:566）覆盖；`components::GPUParticle` 来自 GPUData.hpp 非 Common.hpp；全仓 grep 4 常量仅 GPUParticleSystem+RenderConstants；GlobalHeightField 删 AdvancedAffixComponents.hpp 无 Affix 使用残留；ledger 精确删 2 边；REQUIRED_P0_SOURCES 仅移除 GPUParticleSystem.cpp（GlobalHeightField.cpp 保留，仍 3 边）；ModuleBoundaryCheckerTest.py 仅换源 GPUParticleSystem→GPULootSystem（仍 P0 source）；越权零触碰（RG-3/pch/CMake/pass 逻辑/graph 结构）。

**独立重跑（审查员）**：checker 21/21 PASS（files 10 = MS-6 16 + pch 5）、git diff --check 干净、pytest ModuleBoundaryCheckerTest 6 passed 5 subtests。

**验证（主代理）**：完整构建双标记 0 error（ms6-b4a-build.log）、25 python tests OK。

**提交**：`refactor(render): sink particle constants and drop dead affix include`（待提交）。
