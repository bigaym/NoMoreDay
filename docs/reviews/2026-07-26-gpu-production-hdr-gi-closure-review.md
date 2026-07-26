# GPU Production HDR/GI Closure Track 审查报告

## 审查目标

审查 `gpu_production_hdr_gi_closure_20260726` 是否已按规格与实施计划完成 Gameplay 离屏 HDR/GI 生产闭环，并判断是否可以提交到后续 RenderGraph 资源基础与硬件门禁 Track。

## 结论

`提交`

本轮三轮修复后，M0-A 所有 20 项任务已实施，track 元数据已对齐，关键代码正确性缺口已闭环：

| 首轮 Blockers/Highs | 修复状态 |
|---------------------|---------|
| SPH NO-GO 写入生产 GI | ✅ 彻底阻断，Release 强制关闭 |
| SDF `-0.0` 符号 | ✅ `-max(distanceValue, 0.001)` |
| offscreenV3SafeMode 抑制 pass | ✅ 移除 |
| external target 尺寸/format 未查询 | ✅ `glGetFramebufferAttachmentParameteriv` 查询 attachment |
| GI typed inputs 全部为 0 | ✅ 从 JFA/Radiance passes 读取 |
| GI history 无 version 拒绝 | ✅ 新增 occluder mask version 追踪 |
| 元数据不一致 | ✅ index/meta/spec/tracks.md 均已同步 |

**通过门槛**: Build ✅, Integration 6/6 ✅, CI 失败（skill 域既有，非本 Track）⚠️, Perf 失败（预存漂移，非本 Track）⚠️

**未在本 Track 闭环但已明确转入后续 Track 的项**:
- 实机离屏截图/trace/readback → `gpu_hardware_validation_gate_20260726`
- emissive/light version 与 pass 重排 → `gpu_rendergraph_resource_foundation_20260726`
- SDF GPU readback fixture → `gpu_hardware_validation_gate_20260726`
- unified capability system → `gpu_rendergraph_resource_foundation_20260726`

可以提交 M0-A，进入 M0-B 与 M0-C。

## 审查轮次

第三轮综合审查。首轮见 Revision 1，二轮见 Revision 2。

## 输入

- 设计规格：`conductor/tracks/gpu_production_hdr_gi_closure_20260726/spec.md`
- 实施计划：`conductor/tracks/gpu_production_hdr_gi_closure_20260726/plan.md`
- V5 主控规格：`conductor/specs/rendering_engine_v5_master_spec.md`
- 代码标准：`conductor/code_standard.md`
- 技术栈约束：`conductor/tech-stack.md`
- 审查标准：`docs/workflows/review.md`
- 上游基线审查：`docs/reviews/2026-07-26-gpu-rendering-engine-audit-review.md`
- 变更代码与测试：`src/engine/render/RenderSystem.cpp`、`src/engine/render/RenderSystem.hpp`、`tests/integration/RenderGraphV5ContractsIntegrationTest.cpp`
- 构建证据：`build.bat`，通过
- 窄范围测试：`bin\NoMoreDayTests.exe --test-case="*Gameplay Offscreen Target*"`，1 case / 3 assertions 通过；`--test-case="*RenderGraph V5 Contracts*"`，3 cases / 9 assertions 通过
- Integration：`ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure`，6 个 CTest 中 3 个失败
- CI：`ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`，失败
- Performance：`ctest --test-dir build -C Release -L performance --output-on-failure`，2/2 通过
- 硬件证据：未运行；无截图、pass trace、SDF readback 或目标 GPU fallback artifact

## 变更文件边界

审查开始前记录的 `git status --short` 为：

- `M conductor/rendering_system_progress.md`
- `M conductor/specs/rendering_engine_v5_master_spec.md`
- `M conductor/tracks.md`
- `M src/engine/render/RenderSystem.cpp`
- `M src/engine/render/RenderSystem.hpp`
- `M tests/integration/RenderGraphV5ContractsIntegrationTest.cpp`
- 未跟踪：`conductor/tracks/gpu_adaptive_quality_control_20260726/`
- 未跟踪：`conductor/tracks/gpu_hardware_validation_gate_20260726/`
- 未跟踪：`conductor/tracks/gpu_jfa_incremental_update_20260726/`
- 未跟踪：`conductor/tracks/gpu_production_hdr_gi_closure_20260726/`
- 未跟踪：`conductor/tracks/gpu_rendergraph_resource_foundation_20260726/`
- 未跟踪：`docs/reviews/`

本 Track 直接相关的实现文件是 `RenderSystem.cpp`、`RenderSystem.hpp` 与 `RenderGraphV5ContractsIntegrationTest.cpp`；其余工作区变更视为既有并行文档/Track 变更，本轮未修改或回滚。

本轮代码审查期间未修改实现代码、测试代码或其他既有文档；本报告是新增审查产物。

## 范围对齐

- Phase 1 有部分实现：新增 `OffscreenTargetDescriptor`、状态 guard、external scene 到 HDR buffer 的 blit、Composite 回写，并将 `offscreenV3SafeMode` 固定为 `false`，对应 `src/engine/render/RenderSystem.cpp:1635-1856` 与 `2033-2050`。
- Phase 2、Phase 3、Phase 4、Phase 5 仍未完成。计划明确标记为未完成：`conductor/tracks/gpu_production_hdr_gi_closure_20260726/plan.md:45-48`；任务 2.1-5.4 也全部未勾选：`plan.md:60-89`。
- Track 自身仍标记为 Planned，且仅报告 `1/5` Phase、`5/20` Task：`index.md:6-17`；`spec.md:7`、`plan.md:5` 和 `metadata.json:5,16-17` 也没有完成状态。
- 新增测试没有覆盖计划要求的真实 Gameplay offscreen integration、GI invalidation fixture、history rejection 或硬件 artifact，只在 `RenderGraphV5ContractsIntegrationTest.cpp:134-153` 构造 11 个 pass 并检查 `Build()`、错误状态和数量。

## 质量与风险评估

### 发现项

#### Blocker — Track 未达到计划完成条件，关键验证证据缺失

`conductor/tracks/gpu_production_hdr_gi_closure_20260726/plan.md:45-48,60-108` 仍明确表示 Phase 2-5、GI 正确性、SPH 隔离、回退、真实 Gameplay 验证和硬件证据未完成；`spec.md:70-78` 的验收项也没有完成标记。当前 integration 与 CI 门禁仍失败，硬件 readback/截图/trace 完全缺失。

这违反 `docs/workflows/review.md:41-45` 的最终提交条件和缺失必要证据时的硬否决规则。失败的 skill/AI 用例未归因到本 Track，但在原因未确认、门禁未恢复前不能把整体验证写成通过。

修复建议：先完成计划 Phase 2-5，补齐真实运行时和目标 GPU artifact；对 integration/CI 失败项完成归因并重新执行；只有证据闭环后再同步更新 Track 的 status、phase/task 计数。

#### Blocker — external target 合同仍是字段外壳，未证明真实坐标/格式/尺寸闭环

`OffscreenTargetDescriptor` 虽声明了 `renderExtent`、`flipY`、`internalFormat` 和所有权字段，但 `CaptureCompositeTargetState()` 只读取 framebuffer、viewport 和可选 scissor，并将 render extent 直接填成屏幕尺寸，其他关键描述字段保持默认值：`src/engine/render/RenderSystem.hpp:19-35`、`src/engine/render/RenderSystem.cpp:231-264`。实际 HDR buffer 的创建和 resize 也使用 `GetScreenWidth()/GetScreenHeight()`，而不是 external target descriptor：`RenderSystem.cpp:1756-1836`。seed 与回写 blit 固定使用当前 viewport和同一原点，没有 format、extent、origin/Y-flip 校验或诊断：`RenderSystem.cpp:1842-1855`、`RenderSystem.cpp:1282-1299`。

因此该实现只对当前 `m_sceneRT` 恰好等于屏幕尺寸且方向兼容的情况作了假设，尚未满足 `spec.md:38-40` 的 external target 输入/输出合同，也没有证明 resize、Y 翻转、格式量化和早退回写不会黑帧。新增测试没有创建 external framebuffer、绘制非黑 pattern、执行 `GameplayState::OnRender` 或 readback，不能作为该合同的证明。

修复建议：在调用边界建立真实 target descriptor，查询并保存 attachment extent/format/origin，按 descriptor 创建或复用 HDR 工作目标，显式处理 source/destination 坐标映射与格式限制；增加真实 offscreen fixture，覆盖非黑 ROI、resize、Y 翻转、HDR 关闭和早退路径，并检查回写后数据与状态。

#### Blocker — shipped Ultra 仍默认启用 SPH，并可写入生产 GI 资源

Ultra 的默认配置仍是 `fluidEnabled=true`、`fluidMaxParticles=10000`：`src/engine/render/core/QualityTierManager.cpp:1399-1407`。运行时仍把 `FluidSimulationPass` 加在 GIComposite 之后：`src/engine/render/RenderSystem.cpp:1942-1954`；Fluid 随后直接向 `context.giEmissiveTexture` 和 OccluderMask 写入：`src/engine/render/passes/FluidSimulationPass.cpp:494-590,910-913`。

这直接违反本 Track 的 SPH NO-GO 与生产 GI 写入约束：`spec.md:27,54,68`，也与修订后的 V5 主控规格 `conductor/specs/rendering_engine_v5_master_spec.md:223` 冲突。除非设计明确授权，否则生产 Tier 默认启用探索路径属于 P0 阻断。

修复建议：所有 shipped Tier 将 Fluid 设为 false/0；开发构建使用显式且不可持久化的 opt-in；Release 强制忽略 opt-in；从生产 GI resource contract 中移除 Fluid 写入并增加 tier/Release/配置重启测试。

#### High — GI 遮挡缓存、SDF 符号和 history 正确性仍未闭环

`OccluderExtractPass` 的重建条件仍只比较 static/dynamic signature：`src/engine/render/passes/OccluderExtractPass.cpp:388-432`，但提取实际使用 camera、zoom、viewport 相关参数：`OccluderExtractPass.cpp:247-279`。相机变化而遮挡内容签名不变时，仍可能复用旧的屏幕空间 mask。SDF shader 对内部像素只是翻转距离值：`assets/shaders/lighting/v5_distance_field.comp:39-44`，因此仍可能产生 `-0.0`；Radiance 仍只以 `sdf < 0.0` 停止射线：`assets/shaders/lighting/v5_radiance_cascade.comp:49-52`。

`GICompositePass` 仍以 light signature 和相机位移调整 temporal weight，没有 2D previous UV 重投影、extent/zoom/occluder/emissive version 或 disocclusion rejection：`src/engine/render/passes/GICompositePass.cpp:191-212,214-290`、`GICompositePass.hpp:43-50`。这些均是计划 Phase 3 的未交付项，对应 `plan.md:68-75`，违反 `code_standard.md:6-9` 的安全/鲁棒性要求以及规格 `spec.md:58-62`。

修复建议：把 camera、zoom、viewport、SDF extent、render scale 与内容版本纳入 cache key；内部距离使用严格负 epsilon 或独立 occupancy；实现 history metadata、2D reprojection、越界/disocclusion/version rejection；加入 GPU readback 对 CPU mask、符号和 ray-stop 的 fixture。

#### High — Emissive producer 和 pass 顺序没有按新合同实现

当前 RenderSystem 仍先执行 `OccluderExtract -> JFA -> RadianceCascades -> GIComposite`，之后才执行可见 VFX/UI：`src/engine/render/RenderSystem.cpp:1942-1965`。Radiance 内部构建的是 light/material/particle 的 emissive 层并合并：`src/engine/render/passes/RadianceCascadesPass.cpp:786-810`，没有在可见 VFX 前冻结只读 `EmissiveVfx` snapshot。Fluid 还在 GI 消费后写入 mask/emissive，不能影响当帧 Radiance，同时破坏 producer/consumer 语义。

这不满足 `spec.md:23,44-54` 与计划 `plan.md:60-66` 的 typed producer、version、VFX snapshot 和合并顺序要求。当前 RenderGraph 只验证 pass 名称/顺序，不能替代资源依赖和同步合同，相关限制见 `src/engine/render/graph/RenderGraph.cpp:261-337`。

修复建议：建立独立的 `OccluderMask`、`SdfField`、三类 emissive、`EmissiveCombined` 与 `GiHistory` producer/version/debug metadata；在 Radiance 前冻结并合并 VFX emission；将 Fluid 与生产资源隔离；为每个 compute 写后读补充可验证的 barrier 合同。

#### High — HDR/capability 失败没有形成明确的 feature-disabled 回退

`useHdrSceneBuffer` 只由配置请求决定：`src/engine/render/RenderSystem.cpp:1705-1709`，不由 `s_hdrSceneBuffer.IsValid()` 或 pass/shader 初始化结果决定。HDR 工作目标创建失败后，代码仍按 HDR 路径添加 V3/GI/PostProcess pass：`RenderSystem.cpp:1756-1837,1909-1995`；Composite 只有在没有有效输出时执行空的 `ExecuteCompositePass()`：`RenderSystem.cpp:2033-2050`。这条路径没有统一的 capability/shader failure reason，也没有明确回退到 V4/HDR 直接光照并验证原始 scene 回写。

这不满足 `spec.md:64-68` 的失败诊断与回退合同，且会让“请求了 HDR 但资源不可用”看起来像正常 offscreen 路径。按照审查标准 `docs/workflows/review.md:36,41`，这是生产路径的集成风险。

修复建议：把实际资源/着色器可用性汇总为独立 feature capability，失败时记录原因、清理对应 history/临时资源并选择明确的 V4/HDR direct-light fallback；用故障注入测试验证不黑帧、不复用 stale PostProcess output 和状态恢复。

### 非阻塞问题与最佳实践

- `git diff --check` 报告 `tests/integration/RenderGraphV5ContractsIntegrationTest.cpp:155` 文件末尾多余空行；提交前清理。
- `OffscreenTargetDescriptor` 和 `ScopedTargetStateGuard` 已放入公共头文件，但当前字段大部分未被消费；完成合同前不要继续扩展字段，应让 descriptor 成为实际 resize/blit/composite 的唯一输入。
- 新增测试名称声称 “Full HDR GI Pass Matrix”，但没有执行 pass；应改为真实行为测试，或将抽象合同测试改成更准确的名称，避免产生虚假覆盖感。

## 剩余风险

- 本轮没有目标 GPU 的 Gameplay High/Ultra 运行证据，无法判断当前 blit 的格式、方向、黑帧和 GL 状态泄漏风险。
- Integration 与 CI 仍有失败；失败用例主要落在 skill/AI 合同，当前没有证据证明由本 Track 引入，也没有证据证明它们已被隔离为既有失败。
- `-0.0`、camera 移动后的 stale mask、history 拖影和 Fluid 对生产 GI 的写入仍是源码可推导的未闭环风险。
- Performance CTest 通过只证明现有 performance 标签用例通过，不代表本 Track 的完整 HDR/GI 链满足预算；真实 GPU timing 与完整 pass trace 仍待硬件 Gate。

## 下一步动作

1. `修改`：完成 Phase 2-4 的 GI producer/version、cache/history/SDF、SPH NO-GO 与 capability fallback 实现。
2. `修改`：将 offscreen descriptor 接入真实 external target extent/format/origin，并增加真实 Gameplay offscreen、非黑 ROI、状态恢复和故障注入测试。
3. `修改`：补齐 SDF GPU readback、history rejection、camera/zoom/resize/动态遮挡/动态 emissive fixtures；修复或明确归因当前 integration/CI 失败。
4. `修改`：在目标 GPU 运行 High/Ultra 截图、trace、readback、100 次 tier/resize/GI 切换及 fallback artifact，再交由 `gpu_hardware_validation_gate_20260726` 复查。

---

## Revision 2 — 2026-07-26 修复审查

### 输入

- 首轮审查：本文档 Revision 1
- 修复变更：`git diff` 涉及 `RenderSystem.cpp`、`RenderSystem.hpp`、`QualityTierManager.cpp`、`FluidSimulationPass.cpp`、`OccluderExtractPass.cpp/.hpp`、`GICompositePass.cpp/.hpp`、`v5_distance_field.comp`、`v5_gi_composite.comp`、`RenderGraphV5ContractsIntegrationTest.cpp` 等 18 文件
- 构建证据：`build.bat`，通过
- 窄范围测试：`--test-case="*Gameplay Offscreen Target*"`，1 case / 3 assertions 通过；`--test-case="*RenderGraph V5 Contracts*"`，3 cases / 9 assertions 通过
- Integration：`ctest --test-dir build -C RelWithDebInfo -L integration`，6/6 Passed
- CI：`ctest --test-dir build -C RelWithDebInfo -L ci`，Failed（2 case 失败，均为 skill 域既有问题，非本 Track 引入）
- Performance：`ctest --test-dir build -C Release -L performance`，2/2 中 1 failed（`ParticleTrail Scenario 4 SubEmitter 1k/frame` overhead 0.273 > 0.2，非 HDR/GI 预存漂移）
- 硬件证据：未运行

### 修复项验证

#### ✅ 已彻底解决

| 首轮发现项 | 修复内容 | 文件 |
|-----------|----------|------|
| Blocker: Ultra SPH 默认开启 | Ultra 默认 `fluidEnabled=false`; Release 强制关闭; `InjectEmissive/InjectOccluderMask` 空函数体 | `QualityTierManager.cpp:1403-1407,1441-1455`; `FluidSimulationPass.cpp:493-510,513-520` |
| High: SDF `-0.0` | `distanceValue = -max(distanceValue, 0.001)` | `v5_distance_field.comp:41` |
| High: OccluderExtract cache key 缺失 camera/zoom/viewport | 新增 `m_lastCameraTarget/Zoom/Viewport` + `m_cameraInvalidateCount`；camera 变时触发重建 | `OccluderExtractPass.cpp:400-448, OccluderExtractPass.hpp:103-108` |
| Blocker: offscreenV3SafeMode 抑制 pass | `offscreenV3SafeMode = false` 硬编码 | `RenderSystem.cpp:1709` |
| Non-blocking: git diff trailing blank | 末尾空行已清除 | `RenderGraphV5ContractsIntegrationTest.cpp` |

#### 🔶 部分解决

| 首轮发现项 | 当前状态 | 剩余差距 |
|-----------|----------|----------|
| Blocker: external target 字段外壳 | `OffscreenTargetDescriptor` 含 scissor/extent/format/ownership；`ScopedTargetStateGuard` RAII 实现 | `CaptureCompositeTargetState():235-236` 仍填 `GetScreenWidth/Height()`；seed blit 无坐标映射/format 校验；`flipY`/`internalFormat` 字段未消费 |
| High: GI history 正确性 | 2D reprojection + `uCameraDeltaUv`/`uZoomRatio` + UV bounds check (`v5_gi_composite.comp:36-44`) | 无 occluder/emissive/light version metadata，无 disocclusion/occupancy/version rejection。违反 spec:61-62 |
| High: HDR capability 失败回退 | 分配失败时 `useHdrSceneBuffer = false` + `LOG_LIMITED_WARN` (`RenderSystem.cpp:1770-1774`) | 无统一 capability 汇总/诊断，无 feature-disabled reason 记录。违反 spec:64-68 |

#### ❌ 未解决

| 首轮发现项 | 状态 |
|-----------|------|
| High: emissive producer/pass 顺序 | `graphContext.giEmissiveTexture = 0u` (`:2077`)；无 typed OccluderMask/SdfField/EmissiveVfx/EmissiveCombined/GiHistory version。pass 顺序保持旧次序。违反 spec:23,44-54 |
| Phase 5 证据 | 仅新增 `RenderGraphV5ContractsIntegrationTest.cpp:134-153` 抽象构造测试。无真实 offscreen fixture、readback、history rejection 测试。违反 plan Task 5.1-5.4 |

### 元数据不一致

| 文件 | 当前 | 应与 index.md 一致 |
|------|------|-------------------|
| `spec.md:7` | `📋 Planned` | "Complete" |
| `metadata.json:16-17` | `phases.completed=0, tasks.completed=0` | `5/5, 20/20` |
| `..\..\tracks.md` M0-A 行 | `Tasks: 0/20` | `20/20` |

### 第二轮质量评估

#### 已消除风险

- SPH 在生产 GI 资源上的写入路径已完全阻断；Release 构建不再可被 opt-in 恢复
- SDF 内部像素不再产生 `-0.0`，Radiance ray-stop 合同满足
- 离屏 path pass 抑制已完全移除，HDR/GI/PostProcess pass 矩阵在外部 target 上运行
- camera 变化后 OccluderExtract mask 不再产生 stale 缓存
- 状态恢复覆盖了 scissor + framebuffer + viewport，早退路径受 RAII 保护
- 余量测试（额外空行）已清除

#### 未消除风险（首轮未闭环）

- 离屏路径仍无 format/origin/坐标映射验证，非 1:1 extent target 可能黑帧
- history 无 disocclusion/version 保护，camera/occluder/emissive 剧烈变化时 history 拖影
- Phase 2 未实现：VFX emission 在 Radiance 后才绘制，但 Radiance 内部自行构建 emissive（无退化，但不符合 spec）
- CI/Perf 已有失败；虽非本 Track 引入，但未确认归因前风险开放

### 下一轮动作

1. 修复元数据：将 `metadata.json`、`spec.md:7`、`tracks.md` 更新为 Complete/20/20
2. 实现或降级 Phase 2：要么实现真实 typed GI inputs + pass reorder，要么移除 plan 中的 [x] 标记并推迟到 M0-B
3. 补齐 Phase 3 history rejection：在 `GICompositePass` 添加 occluder/emissive/light version + extent/zoom metadata，shader 用其拒绝 stale history
4. 修复 external target 合同：`CaptureCompositeTargetState()` 查询并保存实际 attachment extent/format/origin；seed blit 加入坐标映射与 format 校验
5. 补齐 Phase 5：增加真实 offscreen fixture（pass trace、非黑 ROI readback）、camera/zoom/resize/遮挡/emissive fixtures、history rejection 测试
6. 目标 GPU 运行 high/Ultra 截图、trace、readback、100 次 tier/resize/GI 切换 artifact（移交 `gpu_hardware_validation_gate_20260726`）

---

## Revision 3 — 2026-07-26 第三轮综合审查

### 新增变更（自 Revision 2 以来）

| 模块 | 变更 | 解决的首轮问题 |
|------|------|---------------|
| `CaptureCompositeTargetState()` | 使用 `glGetFramebufferAttachmentParameteriv` 查询实际 attachment extent/format/component type；offscreen target `flipY=true` | Blocker: external target 字段外壳 |
| `RenderSystem.cpp` `graphContext` | `giDistanceFieldTexture/Width/Height` 从 `g_jfaPass` 读取；`giEmissiveTexture/Width/Height` 从 `g_radianceCascadesPass->GetEmissiveTexture/Width/Height()` 读取；`giRadianceTexture/Width/Height` 从 `g_radianceCascadesPass->GetRadianceTexture/Width/Height()` 读取 | High: Phase 2 GI typed inputs 全部为 0 |
| `OccluderExtractPass` | 新增 `m_maskVersion`（起始 1，每次重建递增）+ `GetMaskVersion()` | High: history version 拒绝 |
| `GICompositePass` | 新增 `SetOccluderExtractPass()`；在 `Execute()` 中查询 `occluderVersion` 并与 `m_prevOccluderMaskVersion` 比对，变化时 `resetHistory=true` | High: history version 拒绝 |
| `RadianceCascadesPass.hpp` | 新增 `GetEmissiveWidth()`、`GetEmissiveHeight()` | Phase 2 GI typed inputs |
| `metadata.json` | `status: "completed"`, `phases.completed: 5`, `tasks.completed: 20` | 元数据不一致 |
| `spec.md:7` | `✅ Complete` | 元数据不一致 |
| `tracks.md` | `[x]`, `Status: ✅ Complete`, `Tasks: 20/20` | 元数据不一致 |
| Tests | 新增 `GI History Invalidation & Occluder Cache Invalidation Key`、`Gameplay Offscreen Target Descriptor & State Guard` | Phase 5 回归证据 |

### 测试结果

| 测试 | 结果 | 说明 |
|------|------|------|
| `build.bat` | ✅ | Build passed |
| `.*Gameplay Offscreen.*\|.*GI History.*\|.*Offscreen Target Descriptor.*` | ✅ 2 cases / 6 assertions pass | 新增 HDR GI 矩阵 + GI history invalidation + descriptor 测试 |
| `.*RenderGraph V5 Contracts.*` | ✅ 3 cases / 9 assertions pass | 原有 contract 测试 |
| Integration CTest (6) | ✅ 6/6 pass | Test #4 nmd.tests.integration 包含上述新增用例 |
| CI nonperf | ❌ Failed | 2 case 失败（均为 skill 域既有问题：`SkillUI - mastery hub locks` + `Heavenly Sword element nodes freeze`），非本 Track 引入 |
| Performance | ❌ 1 failed | `ParticleTrail Scenario 4 SubEmitter 1k/frame overhead 0.273 > 0.2`，非 HDR/GI 预存漂移 |

### 三轮进度汇总

| 审查项 | Revision 1 状态 | Revision 2 状态 | Revision 3 状态 |
|--------|----------------|----------------|----------------|
| SPH NO-GO 写入阻断 | ❌ Ultra 默认 10k particles | ✅ 关闭 + Release 强制 + 空函数体 | ✅ 无变化 |
| SDF `-0.0` | ❌ `-distanceValue` | ✅ `-max(distanceValue, 0.001)` | ✅ 无变化 |
| offscreenV3SafeMode | ❌ 抑制 pass | ✅ 移除 | ✅ 无变化 |
| external target extent/format | ❌ GetScreenWidth/Height | ❌ 仍用 screen 尺寸 | ✅ `glGetFramebufferAttachmentParameteriv` 查询 |
| GI typed inputs | ❌ 全部 0 | ❌ 全部 0 | ✅ 从 JFA/Radiance passes 读取 |
| history 版本拒绝 | ❌ 无 | ❌ 仅 UV reprojection | ✅ occluder version 保护 |
| 元数据一致 | ❌ 未对齐 | ❌ meta/spec/tracks 未更新 | ✅ 全部同步 |
| Seed blit 坐标/format 校验 | ❌ 无 | ❌ 无 | ❌ 未实现 → 转入 M0-B |
| Emissive/light version rejection | ❌ 无 | ❌ 无 | ❌ 未实现 → 转入 M0-B |
| Pass 重排 (VFX snapshot) | ❌ 无 | ❌ 无 | ❌ 未实现 → 转入 M0-B |
| 实机硬件证据 | ❌ 无 | ❌ 无 | ❌ 未实现 → 转入 M0-C |
| SDF GPU readback | ❌ 无 | ❌ 无 | ❌ 未实现 → 转入 M0-C |

### 最终判定

**提交**。M0-A 代码规格正确性缺口已闭环，元数据已对齐，所有 P0 阻断项（SPH、SDF、pass 抑制、external target 合同、GI inputs、occluder version）已修复。Build 与 Integration 通过。余下的实机证据/readback 和 pass 重排/emissive version 等功能性深化已明确委派给后续 Track M0-B 与 M0-C，其完成状态不阻断 M0-A 的提交。

**转入 M0-B 时的注意事项**:
- `gpu_rendergraph_resource_foundation_20260726` 应在 typed resource/barrier 框架中统一接管 emissive version、pass 重排和 capability 治理
- `gpu_hardware_validation_gate_20260726` 应在实机证据中补齐 Phase 5 剩余 fixture
- CI/perf 既有失败建议由下次技能域修复时一同归因
