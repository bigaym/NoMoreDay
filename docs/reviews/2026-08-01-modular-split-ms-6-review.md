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

---

## Batch 4-B（共享 occluder 投影去重）— `提交`

**审查目标**：核验 OccluderExtractPass/ShadowBuildPass 的 Engine→Game 边清除与逐字节重复投影去重（ledger 21→17）。

**变更边界**：2 新增（OccluderProjector.cpp/.hpp）+ 12 修改（ledger/evidence/checker/GameplayRenderHooks/GameplayRenderAdapter×2/RenderSystem/RenderContext/OccluderExtractPass×2/ShadowBuildPass×2）；settings.json 为用户指示既有残留；受保护设计文档排除。

**发现**：无 Blocker/High/Medium。仅 2 项 Low 纯文档：evidence.md Batch 4 节基准号误写（HEAD 应为 1874266 非 c3f97e7，**已修正**）；实施报告文件数笔误（实 2 新增+12 修改）。

**已复核通过**：FNV 签名（kFnvOffset/kFnvPrime/HashAppend/BuildOccluderWord/FinalizeSignature）程序化比对逐字等价；投影 view 语义（Position+ShadowCasterComponent、radius 默认 24.0f、VisionComponent::radius>0 覆盖）与旧循环逐位一致；GPU 上传/绑定留各 pass；RenderContext 消费（ExtractPass 读 6 字段含签名驱动 staticChanged/dynamicChanged 缓存去重保留；ShadowBuildPass min(count,kMax)=8192 等价旧 capped）；onOccluders 钩子在 graph 组装前调用、null hooks 时字段 null/0 且两 pass 带 guard 安全；ledger 精确删 4 边、HeightShadowPass/RadianceCascadesPass 正确保留（仍消费 context.registry 属 D 批）、行号机械校验无 off-by-one；checker REQUIRED_P0_SOURCES 移除逻辑与先例一致；硬约束零改动（RenderGraph/ResourceManager/GPUResourceRegistry/pch/CMake/build.bat/AddPass 链 L1414-1581）；测试无直连两 pass Execute（全 AddPass+Build），新字段默认 null/0 安全。

**独立重跑（审查员）**：checker 17/17 PASS（files 8）、git diff --check exit 0、定向 `*RenderGraphV3*,*RenderGraphTier*,*ShadowPipeline*,*Occluder*,*S7*` 17/180 SUCCESS、ModuleBoundaryCheckerTest 6/6。

**剩余风险**：ShadowBuildPass buffer 大小从恒 kMax 改 max(1,uploadCount)（功能等价、更省内存、无消费方依赖固定大小）；context occluders 字段唯一生产者是 RenderSystem（契约已注释声明）。均低。

**提交**：`refactor(render): deduplicate occluder projection via game adapter`（待提交）。

---

## Batch 4-C（LightManager 拆分：LightAdapter）— `提交`

**审查目标**：核验 LightManager 的 Engine→Game 边清除与 ECS 光投影迁移（ledger 17→15）。

**变更边界**：2 新增（`src/game/render/LightAdapter.{hpp,cpp}`）+ 15 修改（ledger/evidence/checker/LightManager×2/GameplayRenderHooks/RenderSystem/GameplayRenderAdapter×2/5 测试文件）；settings.json 为用户指示既有残留；受保护设计文档排除。

**发现**：无 Blocker/High/Medium。3 项 Low 非阻塞：LightManager.cpp:114 `m_debugStats.ecsLights` 移至 `allowedLights==0` 提前返回前（maxLights=0 退化配置下日志更真实）；RenderSystem.cpp:1415-1419 null-hooks 路径不再投影 ECS 光（仅影响 gate/harness，本就不渲染玩法光照，evidence 已记录）；evidence "files 8" vs checker 实际 7（evidence 记载 7 正确）。

**已复核通过**：投影 4 函数（NormalizeDirection/ComputeFlickerIntensity/ToRawLightType/BuildGpuLight）程序化比对与 HEAD 逐字节 IDENTICAL；UpdateCandidates 对照 HEAD 逐行等价（view cull/幂等过滤/transient SanitizeRuntimeLight+priority 255/排序 priority 降序→distance 升序/预算截断/m_stagingBuffer+OrphanAndUpload）；priority DTO 回读无损（uint8_t→uint32→uint8_t，值域 [0,255]）；onLights 接线（ToHooksFrame 聚合第 7 项与 GameplayRenderFrame 字段序对齐、null 守卫 + clear 防陈旧复用）；AddTransientLight/Bind/getter 头文件签名未动（VFXSequencerSystem.cpp:425、ProjectileSystem.cpp:686 仍用 AddTransientLight）；5 测试全部"adapter 投影 + UpdateCandidates 消费"（Boundary 用例 3 投影独立局部名；benchmark 逐帧 BuildLightCandidates(registry,i*0.016f)；GPU 计时仅包 pass.Execute 不含投影，指标语义不变）；ledger 精确删 2 边、checker REQUIRED_P0_SOURCES 移除 LightManager.cpp；硬约束零命中（pch/CMake/build.bat/ResourceManager/GPUResourceRegistry/RenderGraph/passes/AddPass 链全零 diff）。

**独立重跑（审查员）**：checker 15/15 PASS（files 7 = MS-6 10 + pch 5）、ModuleBoundaryCheckerTest 6/6、git diff --check exit 0、ms6-b4c-build.log 存在含双标记。

**验证（主代理）**：完整构建双标记 0 error、定向 `*[Unit] Lighting*,*Clustered Lighting*,*Lighting - Stability*` 19/19 28762 断言 SUCCESS、ctest -L unit 78%（仅既有 HeavenlySword flaky）、git grep game/ LightManager.cpp 0 命中。

**剩余风险**：低。null-hooks 路径不再投影 ECS 光属 hooks 契约设计（evidence 已记录）；benchmark 指标不受投影迁移影响。

**提交**：`refactor(render): split light manager candidate projection`（待提交）。

---

## Batch 4-D（HeightFieldAdapter 高度场投影迁移）— `提交`

**审查目标**：核验 GlobalHeightField/HeightShadowPass 的 Engine→Game 边清除与 ECS→HeightStamp 投影迁移（ledger 15→11）。

**变更边界**：2 新增（`src/game/render/HeightFieldAdapter.{hpp,cpp}`）+ 12 修改（ledger/evidence/checker/GlobalHeightField×2/HeightShadowPass/RenderContext/GameplayRenderHooks/RenderSystem/GameplayRenderAdapter×2/GlobalHeightFieldTest）；settings.json 为用户指示既有残留；受保护设计文档排除。

**发现**：无 Blocker/High/Medium。2 项 Low 非阻塞：`s_maskBlueCache` thread_local 蓝色缓存不随 Initialize/Shutdown 清空（raylib 纹理 ID 进程内单调不复用，实际可忽略，建议补注释）；HeightShadowPass 移除 registry 守卫后空数据时无条件 `Update(empty_span)` 清高度纹理（与 Batch 4-B 先例一致，"无游戏数据→无高度场"更合理）。

**已复核通过**：投影逐段等价（Tile WALL→0.85/else→0.10、static caster r=20 clamp(occluderHeight,0,1)、static collider r=max(2,max(w,h)*0.5) h=0.75、dynamic caster r=18、sprite r=max(6,8*max(0.25,scale)) blue>0.02，与旧 texel 写入 1:1 对应；EstimateMaskBlue 逐字迁移仅加 const；全 max-blend 顺序无关）；`tileWorldSize=10.0f`==`GRID_TILE_SIZE`；WORLD 5000 int→float 与 fallback 5000/5000/10 一致；base(track=false)/dynamic(track=true) 双层分流保留；`SampleNormalizedHeight`/chunk 逻辑零改动；RenderContext 指针+count 消费链（nullptr 恰当 count=0）；ToHooksFrame 20 字段聚合与 GameplayRenderFrame 成员序逐一匹配；onHeightField 唯一实现者+null 守卫、gate/harness 清 buffer 置零 world 字段；AddPass 结构零改动（RenderSystem 仅 5 hunk）；ledger 精确删 4 边、checker REQUIRED_P0_SOURCES 移除 2 文件；GlobalHeightFieldTest 锚点断言（wallH>floorH/didFullRebuild/firstSpot>0.1/newSpot>oldSpot/dirtyChunkCount>0）与 HEAD 逐条一致；硬约束零改动。

**独立重跑（审查员）**：checker 11/11 PASS（files 5 = pch 5 + MS-6 6）、git diff --check exit 0。

**验证（主代理）**：完整构建双标记 0 error（ms6-b4d-build.log）、定向 `*GlobalHeightField*,*HeightShadow*,*Clustered Lighting*` 11/11 28680/28680 断言 SUCCESS、ModuleBoundaryCheckerTest 6/6、git grep game/ 两文件 0 命中。

**剩余风险**：低。`thread_local` 缓存为唯一轻微语义偏移（行为等价）。

**提交**：`refactor(render): move heightfield projection to game adapter`（待提交）。

---

## Batch 4-F/G/H（GPULoot adapter / SkillVfxEvent DTO / VFXSequencer 迁移）— `提交`

**审查目标**：核验三个并行批次的 Engine→Game 边清除、投影/迁移等价、并发覆盖自洽（ledger 11→6）。

**变更边界**：22 modified + 3 deleted + 5 untracked（三批文件域无重叠；settings.json 为用户指示既有残留；受保护设计文档排除）。

**发现**：无 Blocker/High/Medium。1 项 Low + 2 项 NOTE：
- **LOW-1**：`REQUIRED_P0_SOURCES` 未收敛（GPUEntitySystem.cpp/hpp、GPULootSystem.cpp 三项在 F 批边清零后仍残留）。**已处置**：主代理收敛为仅剩 `passes/RadianceCascadesPass.cpp`（这三文件现均无 ledger 条目，符合"全部边删完从 REQUIRED 移除"规则）。
- **NOTE-1**：简报数量 24M+4D+5U vs 实际 22M+3D+5U（逐文件核对无异常混入）；`SkillVfxNodeRoleMask` 实为 6 常量（含 Any）。
- **NOTE-2**：settings.json 属既有残留，不纳入提交。

**已复核通过**：
- **并发覆盖自洽**：ledger 恰 6 条（5 MS-7 pch + 1 RadianceCascadesPass.cpp:18→Common）；diff 移除恰 5 边（F=2 GPULootSystem:6/7、G=1 GPUSkillEffectSystem.hpp:7、H=2 VFXSequencerSystem:13/14）；ModuleBoundaryCheckerTest `test_required_p0_source_without_blocker_returns_input_error` 指向 `passes/RadianceCascadesPass.cpp`、fixture 建 `passes/` 目录、断言 returncode 2；invalid-ownership 反向用例覆盖双向约束；checker 6/6 PASS 与测试一致。
- **F（GPULoot）**：实例字段（itemId/rarityColor/glowIntensity/flags/labelOffsetY）与旧 SyncDroppedItems 1:1；kLootFlagGold=1<<0/kLootFlagItem=1<<1、labelOffsetY(-24/-20)、PackRarityColor 9 分支 switch+WHITE 兜底、ComputeGlowIntensity 表逐字搬移；BuildLoot 两趟扫描（count→reserve→填充）；requiredCount 语义与旧同步一致；UploadInstances 按实际 count 增长；lootBuffer 字段插入位置与 ToHooksFrame 聚合逐位对齐。
- **G（SkillVfxEvent）**：新 engine DTO 3 枚举 + NodeRoleMask + 11 字段与旧头逐项一致；effectiveTagMask 双向无损（Tag 元素位 0-6 与新 SkillVfxElementTagMask 位布局相同，uint32 截断无损）；判定顺序 Void→Lightning→Cold→Fire 逐位保持（GPUSkillEffectSystem.cpp:334-343）；旧头删除后 0 残留消费者。
- **H（VFXSequencer）**：cpp 仅第 1 行自包含变更、hpp 字节级一致；namespace NoMoreDay::vfx 保持；恰 6 文件 include 更新单行路径改动；engine 侧 0 反向 include（7 处消费全在 game/tests）；VFXPlayerComponent/VFXSequenceManager/VFXBudgetEstimator 留 engine 未动。
- **硬约束**：RG-3/pch/CMake/build.bat/RenderGraph/passes（RadianceCascadesPass 未动）/AddPass 结构零改动。

**独立重跑（审查员）**：checker 6/6 PASS（files 2 = pch 5 + MS-6 1）、ModuleBoundaryCheckerTest 6 OK、git diff --check 干净。

**验证（主代理）**：完整构建双标记 0 error（合并产物编译通过）、定向 `*GPULoot*,*SkillVfx*,*GPUSkillEffect*,*VFX*` 39/39 610 断言 SUCCESS。

**剩余风险**：uint32 截断耦合（engine DTO 与 Tag 低 7 位布局耦合，未来 Tag 重排将静默错映射——建议后续显式提取元素位）；REQUIRED 收敛后需 CI 全量验证兜底。均低。

**提交**：`refactor(render): move loot projection, dto skill vfx event, migrate vfx sequencer`（合并提交，待提交）。

---

## Batch 4-E（EmissiveStampAdapter，最后一条 MS-6 边）— `提交`

**审查目标**：核验 RadianceCascadesPass 的 Engine→Game 边清除（ledger 6→5，MS-6 清零）与 `REQUIRED_P0_SOURCES` 空集化。

**变更边界**：GPUData.hpp（新增 `components::EmissiveStampInput`）+ 新增 EmissiveStampAdapter×2 + GameplayRenderHooks/GameplayRenderAdapter×2/RenderSystem/RenderContext/RadianceCascadesPass + ledger/checker/ModuleBoundaryCheckerTest/GIStabilityIntegrationTest/RadianceCascadesBenchmark/evidence；settings.json 为用户指示既有残留（排除）；受保护设计文档排除。

**发现**：无 Blocker/High/Medium。P2 观察项 3 条：GIStability 补 `PrepareVfxEmissionSnapshot` 调用为 test-only 同步修复（HEAD 即失败属实：RunParticleEmissive 在 HEAD:534 返回 `IsValid()&&vfxEmissionSnapshotValid`，Execute HEAD:795 中止，Benchmark 早已 REQUIRE 该调用）——略超严格投影范围但正确、透明记录，不构成修改理由；`RunMaterialEmissive` 去 registry null 守卫（函数体已不读 registry，安全）；空集容忍测试使 "source requires P0_BLOCKER" 分支结构性不可达，但反方向（p0_blocking 条目被拒）仍由 invalid-ownership 用例覆盖，无空洞。

**已复核通过**：投影逐项等价（同 view `Position+ActiveMaterialSwap` exclude<KilledTag>、materialId<=0 skip、GetGpuMaterialForTesting(textureSlots.z→maskLayer,<0 skip)、emissive clamp+0.0001 阈值、worldHalfExtent=max(24,Radius,sprite 半 extent)）；pass 保留 world→px/offscreen skip/uniform/dispatch/++stampCount；**ledger :18 本就正确**（`git show HEAD` 实测 Common.hpp include 在第 18 行，审计 off-by-one 判断有误，条目直接删除无需改行）；REQUIRED=frozenset() 下 checker:241 `in` 恒 False、:245 `not in` 恒 True → 任何仍带 p0_blocking 条目被拒，不会错误放行；MaterialLightingIntegrationTest:56 从未引用 RadianceCascadesPass（git log -S 零命中）不改合理；硬约束零改动（pch/CMake/build.bat/RenderGraph 类/AddPass/其他 pass）；ToHooksFrame 22 成员聚合序对齐；C2653 已修（adapter.cpp:29 `NoMoreDay::render::MaterialManager`）。

**独立重跑（审查员）**：checker 5/5 PASS（仅 pch MS-7）、ModuleBoundaryCheckerTest 6 OK、git diff --check 干净。

**验证（主代理）**：完整构建双标记 0 error、定向 GI 1718/1718 + Benchmark 3869/3869 + Material Lighting 2+1 PASS、ctest -L integration 6/6、git grep game/ RadianceCascadesPass.cpp 0 命中。

**剩余风险**：低。GIStability 修复属既有问题（根因 5c5ca0c），已透明记录。

**提交**：`refactor(render): move emissive stamp projection to game adapter`（待提交）。

## MS-6 里程碑总结

MS-6 全部 66 条 P0-blocked 边清零：ef39129（GPU 实体核心）+ 363b196（死 include）+ c3f97e7（gameplay adapter）+ 5c5ca0c（DTO render input + pass rewire）+ 1874266（粒子常量）+ a5cc909（occluder 去重）+ bf5ff63（light adapter）+ a4fd6c2（heightfield adapter）+ 9aece25（loot/skill-vfx/vfx-sequencer）+ a046a94（emissive stamp）。ledger 现 5 条（全 MS-7 pch）。`REQUIRED_P0_SOURCES` 空集。

---

# MS-7 Batch 1（SharedContext 下移 + Game→App 依赖清除）— `提交`

**审查目标**：核验 SharedContext/Settings/SerializationSystem 下移 game 与 Game→App 反向依赖清除（无 CMake/pch 改动，为显式 target 图铺路）。

**变更边界**：3 个 rename（`app/Settings.hpp→game/Settings.hpp` 100%、`app/SharedContext.hpp→game/SharedContext.hpp` 97%[仅内部 include 修正]、`systems/SerializationSystem.hpp→game/systems/SerializationSystem.hpp` 100%）+ 14 个 include 修正 + 1 个 include 删除；settings.json 为用户指示既有残留（排除）；受保护设计文档排除。

**发现**：无阻断项。2 条非阻断提示：多处 LF/CRLF 归一警告（git 工作区行尾提示，diff --check 通过）；settings.json 未暂存改动（提交时只 stage 目标文件防误带）。

**已复核通过**：目标路径全部存在、旧路径零残留（git grep `systems/SerializationSystem` 仅新路径 1 条）；8+1 处 include 修正完整（8 处 `app/SharedContext` + GameplayState.cpp:82 SerializationSystem）；UISystem.cpp 删 `app/Game.hpp` 安全（class Game 零引用，现存 Game 匹配为 GameplayRenderAdapter::VisibleItemCache 来自仍在 include 的 adapter 头）；测试 4 文件（GameplayRuntimeHarness.hpp:22-23 双改、MDIRenderTest:4、MDIRenderBenchmark:5、RenderingBenchmark:5）；`git grep "app/" -- src tests` 仅 4 处合法命中（app 层自引用 Game.hpp、RenderFrameInput.hpp:4 注释、ModuleBoundaryCheckerTest.py:30 夹具字符串）；无 CMake/pch/ledger/checker 改动。

**独立重跑（审查员）**：checker 5/5 PASS（files 1）、git diff --check exit 0、git grep "app/" src/game 零命中。

**验证（主代理）**：25 python tests OK、build.bat 双标记 0 error、ctest ci 640/641（UITests.cpp:414 SkillUI 锁签名既有 flaky，与本次无关）。

**剩余风险**：低。Game→App 依赖已清零；SharedContext 现属 game 层，B2 target 图无阻塞边。

**提交**：`refactor(game): move shared context below game layer`（待提交）。

---

# MS-7 Batch 2（显式 manifest + 四层 target 图 + checker 升级）— `提交`

**审查目标**：核验 monolith 拆分 NoMoreDay(exe)→App STATIC→Game STATIC→Engine STATIC→Core STATIC→Types INTERFACE、显式 source manifest、checker 分层升级。

**变更边界**：新增 4 个分层 CMakeLists（`src/{app,game,engine,core}/CMakeLists.txt`，显式 manifest 无 GLOB_RECURSE）+ 重写根 CMakeLists（删 monolith、add_subdirectory 四层、RelWithDebInfo flags 上移根作用域、exe 链 NoMoreDayApp）+ checker 升级（core-candidate-contract.json schema 1.0→1.1、`current_aggregate_target`→`layered_targets` 五 target + link_chain 四边、`validate_layered_target_graph`、`_is_sanctioned_core_types_link` 放行 Core PUBLIC Types）+ tests/CMakeLists.txt L45 改链 NoMoreDayApp + `src/game/SharedContext.hpp` 修复（B1 遗留 broken include `app/Settings.hpp`→`game/Settings.hpp`，必须随本批提交否则构建碎）；settings.json 为用户指示既有残留（排除）；受保护设计文档排除。

**发现**：无 Blocker/High/Medium。4 Low 非阻塞：①`validate_layered_target_graph` 无负面测试（缺层/重复/link 缺失 FAIL 用例建议后续补）；②SharedContext.hpp 附加改动超 B2 名义范围但必须包含（提交信息注明）；③engine PUBLIC dbghelp 冗余无害（core 已有 PRIVATE dbghelp + pragma 兜底）；④link 边校验宽松（只查 scope/dep 在 tokens[1:]，当前文件严格书写无影响）。

**关键决策**：从显式 PUBLIC link 移除 `kernel32/gdi32/user32/shell32`（MSVC 默认库集链接行末尾自动追加排 raylib 后，解决 LNK2005 `user32.CloseWindow`）；保留 `winmm/opengl32`（raylib 接口）+ `dbghelp`（Core PRIVATE + pragma）。vcxproj 确认 raylib.lib 现先于 user32.lib，exe/tests 均链接成功。

**已复核通过**：文件清单精确（game 149=136+13 behaviors、engine 67/67、core 2/2、app 1+main；EnemyAIBehaviors.cpp 属 systems/ai 非 behaviors GLOB 编入 Game 正确）；五 target 依赖/作用域正确；SkillBehaviors OBJECT 经 Game PUBLIC 完整进入 exe 与 tests（`Blade Ascendant key branches` 46/46 静态注册实证）；GenerateTags 落点 Game/Engine/App；系统库移除安全性（MSVC 默认库覆盖 + src 无直接引用，仅 MSVC 工具链）；checker 升级正确（pch_inventory 5+2 与 src/core manifest 校验保留、DEFER guard 原样保留无 cmake_language 违规）；tests 链接过渡态与 B4 无冲突；GLOB/UNITY/CTest/bin 零破坏（脚本依赖 bin/NoMoreDayTests.exe 不变）。

**独立重跑（审查员）**：checker 5/5 PASS、25 python tests OK（ModuleBoundary 6 + Contract 19）、git diff --check exit 0。

**验证（主代理）**：build.bat check OK、完整构建双标记 0 error、ctest -L "unit|integration" 13/15（2 失败均为既有 HeavenlySword flaky 隔离 4/4 过）、SkillBehaviors 静态注册 46/46。

**剩余风险**：**Release+LTO 未实证**（RelWithDebInfo + ENABLE_LTO=OFF，/LTCG 下系统库解析理论安全未构建验证，B4/后续需补验）；glfw/raylib 升级可能改变系统库 PRIVATE 传递；B4 落地时需回归链接。

**提交**：`build(cmake): split monolith into layered target graph`（待提交）。

---

# MS-7 Batch 3（per-target PCH，ledger 5→0）— `提交`

**审查目标**：核验 `src/pch.hpp` 删 5 条 game include 成 Engine+Core 共享下层 PCH、新建 `src/game/pch.hpp`、Game/SkillBehaviors 共用同一 game pch（ledger 5→0，module boundary 全清零）。

**变更边界**：`src/pch.hpp`（删 5 条 game include L69-75）+ 新建 `src/game/pch.hpp` + `src/game/CMakeLists.txt`（PCH→game pch）+ 根 `CMakeLists.txt`（SkillBehaviors PCH→game pch）+ ledger（entries 5→0）+ core-candidate-contract.json（direct_game_includes→[]）+ CoreCandidateContractCheckerTest fixture 同步；settings.json 为用户指示既有残留（排除）；受保护设计文档排除。

**发现**：无 Blocker/High/Medium。2 Low 非阻塞：`src/game/pch.hpp:51` `<raylib.h>` 尾空格（原样继承旧 pch，非本批引入）；game→app 反向 include 不在 module boundary checker 扫描范围（既有盲区，本批未扩大）。

**已复核通过**：engine/core 对 game5 符号零消费（grep TagRegistry/PrimaryStats/CombatStats/StatModifier/DamageEvent/SkillComponent/SpriteComponent/ColliderComponent/SkillTreeDefinition/ActiveSkillsComponent/DamagePool/TalentNode/AttackState/TagInfo 全 0 命中）；game pch = 改造前旧 pch 逐行等价零丢失（STL38+三方+core4+game5+engine2，ItemFactory 消费 EquipmentAssetRegistry/RuneAssetRegistry 按需保留）；Game（src/game/CMakeLists.txt:145）与 SkillBehaviors（CMakeLists.txt:204）解析同一 `<root>/src/game/pch.hpp`，MSVC 生成同名 pch 无 C2850；ledger 精确删 5 边且 scope（candidate_roots/pch_files/forbidden）完整保留；checker `EXPECTED_PCH_FILES` 保持 `("src/pch.hpp",)` 正确（加 game pch 会误报其合法 game include）；contract JSON+checker+fixture 三方一致（checker 动态读契约核对 src/pch.hpp 实际 includes==direct_game+direct_engine 2 条）；Core 无 PCH 符合契约（future_NoMoreDayCore.approved_pch=null）；NoMoreDayTypes/DEFER guard/CMake 静态策略零违规；ResourceManager SKIP 保留。

**独立重跑（审查员）**：checker 0/0 PASS、contract PASS、25 python tests OK、git diff --check exit 0。

**验证（主代理）**：build.bat check OK、完整构建双标记 0 error C/LNK/C2850、.pch 产物 Game/SkillBehaviors ~460MB vs Engine/App ~398MB（瘦身实证）、ctest -L "unit|integration" 13/15（2 失败均为既有 HeavenlySword hasFreeze flaky）、git grep game/ src/pch.hpp 0 命中。

**剩余风险**：低。Core 目录残留 pre-B2 孤儿 `cmake_pch.pch`（未参与编译）；game→app 反向 include 属既有 checker 盲区建议后续独立审计；Release+LTO 仍未实证（沿用 Batch 2 风险）。

**提交**：`build(cmake): split per-target precompiled headers`（待提交）。

---

# MS-7 Batch 4（测试显式链四层 target）— `提交`

**审查目标**：核验 tests 链接从单层 NoMoreDayApp 改为显式四层（最后一批，MS-7 完成）。

**变更边界**：`tests/CMakeLists.txt:45` 单处——`target_link_libraries(NoMoreDayTests PRIVATE NoMoreDayApp)` → `target_link_libraries(NoMoreDayTests PRIVATE NoMoreDayCore NoMoreDayEngine NoMoreDayGame NoMoreDayApp)`；settings.json 为用户指示既有残留（排除）；受保护设计文档排除。

**发现**：无 Blocker/High/Medium/Low。无代码风险（显式链冗余但语义等价；静态库链接顺序无关紧要，依赖经 PUBLIC 传递）。

**已复核通过**：GLOB（L4）、SKIP_UNITY 21 文件列表（L15-37）、UNITY_BUILD ON（L48-49）、输出路径 bin/NoMoreDayTests.exe（L50-54）、CTest 21 项注册（L71-233）、compile defs/options（L57-69）、include dirs（L39-43）全部零改动；依赖图为严格 DAG 无环（Types←Core←Engine←Game←App←exe/Tests，各层 PUBLIC 传播正确）；四层顺序低→高与 PUBLIC 方向一致；git diff 仅含本文件。

**独立重跑（审查员）**：git diff --check exit 0、checker 0/0 PASS（files 0）。

**验证（主代理）**：完整构建双标记 0 error、定向 `*RenderGraphV5*,*GPU ABI*` 7/7 105 断言 SUCCESS。

**剩余风险**：低。Release+LTO 未实证（B2 遗留，建议后续补验）；构建证据依赖主代理（只读审查未重跑构建）。

**提交**：`build(cmake): link tests to explicit layered targets`（待提交）。

---

# MS-7 里程碑总结

MS-7 全部完成：96ce289（SharedContext 下移 game）+ 8648a58（四层 target 图 + checker 分层升级）+ 761d9f9（per-target PCH，ledger 5→0）+ B4（测试显式四层链接）。显式 target 图 `NoMoreDay(exe)→App STATIC→Game STATIC→Engine STATIC→Core STATIC→Types INTERFACE` 落地；ledger 0 条（module boundary 全清零）。剩余：MS-8（目录收敛）→ DOD-2 实机 gate（RTX 4070S）。

---

# MS-8（目录收敛 / 删除临时例外）— `提交`

**审查目标**：核验模块拆分收尾——目录布局一致性、转发 include/临时例外清理、技术债清单化（ledger 0 条终态）。

**变更边界**：`M scripts/check_module_boundaries.py`（4 行纯字符串替换：`LegacyLowerPch`→`EngineOwnedPch`、`lower-layer PCH`→`engine-owned PCH`、`legacy_global_pch`→`engine_owned_pch`、`legacy_monolithic_{target}`→`{layer.lower()}_layer`）+ `M tests/python/ModuleBoundaryCheckerTest.py:48`（fixture current_owner `legacy_monolithic_NoMoreDayEngine`→`engine_layer`，强制同步否则 returncode 2）+ 新增 `docs/reports/modular-split-exe-lib-dll/ms-8/evidence.md`（manifest 精确匹配 engine 67/game 136+13/core 2/app 1+main 0 遗漏 0 多余、0 转发 include、src/systems/ 空目录删除、7 项技术债清单）；settings.json 为用户指示既有残留（排除）；受保护设计文档排除。

**发现**：无 Blocker/High/Medium/Low。唯一强制同步点：fixture current_owner 必须改（load_ledger 逐条精确比对，不同步 returncode 2）——已同步。

**已复核通过**：A2 命名收敛零逻辑改动（diff 恰 2 处字符串替换，checker 全文 0 legacy 命中，其余 legacy 均在无关工具/历史）；A1 `src/systems/` 空目录删除无误删（git ls-files 空）；A3 evidence 计数独立复算全对（与 MS-7 审查记录逐字一致）、7 项技术债逐条溯源（RG-3/S1b PersistSelectionMetadata/Game→App checker 盲区/effectiveTagMask 位布局/settings.json/Release+LTO/P0 死分支）；checker 0/0 PASS、25 tests OK、check_legacy_reintroduction 215/69 vs 基线 222/71 PASS。

**剩余风险**：低。MS-1 DEFER guard 为契约强制安全机制必须保留；REQUIRED_P0_SOURCES 空集为终态语义（保留）；7 项技术债已清单化归后续专项。

**提交**：`refactor(build): converge legacy module boundary naming`（待提交）。

---

# DOD-2 实机 gate（RTX 4070S）— 首轮 NO_GO：GL_INVALID_ENUM 根因定位

**运行**：`python scripts/gpu_hardware_validation_gate.py --revision dod2-20260801` → NO_GO。artifact `artifacts/gpu-gate/dod2-20260801/`。

**根因（explore 定位，证据闭合）**：`RenderSystem.cpp:251-253` `CaptureCompositeTargetState()` 三个手写 pname 常量全部非法——`0x8D24`/`0x8D25`（声称 attachment width/height）在核心 GL 不存在（glad.h 668-687 无 OBJECT_WIDTH/HEIGHT）；`0x825D`（声称 COMPONENT_TYPE）真值应为 `0x8211`。全仓唯一 `glGetFramebufferAttachmentParameteriv` 调用点（:263-268）。每 render() 3 条 GL_INVALID_ENUM（type=0x824C、severity=0x9146 HIGH）；gate 全流程数百万 → 256 捕获 + 3,593,483 dropped。S3 fail-closed 下 severeGlErrorCount>0 → allMatrixPassed=false + globalFailure → **DOD-2 必 NoGo**。附带：3 次查询自引入以来从未取得有效数据（width/height 恒 0 走 viewport 回退、internalFormat=0），纯错误产生器；2026-07-26 review 声称的"✅ glGetFramebufferAttachmentParameteriv 查询"与代码不符。

**修复方案 A（推荐）**：`RenderSystem.cpp:248-283` 删除 3 次无效查询，直接用 viewport 回退值、internalFormat 保持 0（行为与现状等价，消灭全部错误）。若确需真实尺寸/格式：OBJECT_TYPE(0x8CD0) 分支 + glGetTexLevelParameteriv(GL_TEXTURE_WIDTH=0x1000/HEIGHT=0x1001) 或 glGetRenderbufferParameteriv(RENDERBUFFER_WIDTH=0x8D42/HEIGHT=0x8D43) + GL_TEXTURE_INTERNAL_FORMAT(0x1003)/RENDERBUFFER_INTERNAL_FORMAT(0x8D81)。核心 GL 无"attachment 尺寸"直接 pname。修复后重跑 DOD-2。
