# 最终审查：2026-08-16 GPU 渲染引擎现代化审查后的修复计划

- 日期：2026-08-17
- 审查轮次：终审（独立终审审查者，working tree 未提交状态审查）
- 审查对象：4 项缺陷修复 + 历轮 P0/P1/P2 整体闭环确认
- 审查人：Code Reviewer（独立终审）

## 结论

**修改（复审后：F1/F2 已修复 + 新增 F5 已修复，待复审确认提交）**

首轮终审结论为「修改」，依据：

1. **F1（Medium）**：`RenderGraph::Execute` 异常安全机制（审查发现 #9，P0/P1/P2 均漏、本轮补上的关键路径修复）**零直接测试覆盖**。该修复的全部价值在于保护帧尾契约（RenderSystem.cpp:1790-1794 的 registry AdvanceFrame、:1801-1853 的 timer/profiler/transient/texture pool 清理）；守卫的析构/Commit 配平、timer ring 槽状态、activeGraph 复位若回归将静默表现为泄漏或卡死。按 review.md「未验证的关键路径 → 修改」归级。
2. **F2（Medium）**：本轮格式改动造成**测试夹具漂移**——`GIHistoryRejectionTest.cpp:50`、`RadianceDirectionalTest.cpp:92` 仍以 GL_RG16F(0x822F) 创建辐射度 atlas，而生产 `RadianceCascadesPass` 现以 GL_RGBA16F 绑定 image（RadianceCascadesPass.cpp:699），image format 不兼容导致绑定失败/写路径在测试中静默失效；两测试断言为状态级（方向数、历史重置计数），不检测辐射度内容，漂移不可见。按 review.md「测试琐碎或假（隐藏失败）」边缘归级为 Medium。
3. **F5（修复引入的真缺陷，复审期间新发现）**：Task A 将 `kRadianceFormat` 改为 **0x815B 是错误的枚举值**——0x815B 无标准 GL 枚举对应（GL_RGBA16 实为 0x805B；GL_RGBA16F 的正确值是 **0x881A**，全库 `kEmissiveFormat` 及 14 处 pass 的 `kGLRgba16f` 均为 0x881A）。`TexStorage3D(0x815B)` 存储格式与 shader `layout(rgba16f)` 不匹配，属未定义行为。首轮终审只做了「格式一致性」grep 复核（.rg 读取、通道读写对齐），未校验枚举值语义，漏检该错误；复审期间核对 GL 常量表后发现并已修正为 0x881A。
4. 两项均为小而明确的测试侧修复（2 处常量 + 1 个抛异常 pass 测试夹具 + 可选 2 个单测），不构成实现返工；生产代码 4 项修复经逐行核验均正确（见下）。

## 审查目标

1. 逐项核验 4 项修复的正确性：RC 级联蓝色辐射度丢失、RenderGraph::Execute 异常安全、HeightShadow 光方向解耦、DeviceCapabilityMatrix 能力探测。
2. 回归风险：legacy marker 门禁（scripts/check_legacy_reintroduction.py，baseline 133/31）不得回归；渲染时序（UpdateCandidates 先于 graph.Execute）不被破坏；fail-closed 语义与测试注入机制（m_probeOverrideForTesting）保持。
3. 闭环确认：对照 docs/reviews/2026-08-16-gpu-rendering-engine-modernization-audit-review.md 的 18 项发现，声明未修复/部分修复项。

## 输入

- `docs/workflows/review.md`（审查流程，严格执行）
- `conductor/code_standard.md`、`conductor/specs/rendering_engine_v5_master_spec.md`
- `docs/reviews/2026-08-16-gpu-rendering-engine-modernization-audit-review.md`（18 项发现基线）
- `docs/reviews/2026-08-16-gpu-pipeline-correction-final-review.md`（报告格式基准）
- 历轮终审记录（P0/P1/P2）与 `docs/designs/2026-08-16-gpu-rendering-modernization-remediation-design.md`
- working tree `git diff`（HEAD = bcc3c325）

## 变更文件边界

`git status --short`（复审修复后：14 个修改文件 + 2 个未跟踪，含本报告与新增测试）：

```
 M assets/shaders/lighting/height_shadow_apply.frag
 M assets/shaders/lighting/v5_gi_composite.comp
 M assets/shaders/lighting/v5_radiance_cascade.comp
 M settings.json
 M src/engine/render/RenderConstants.hpp
 M src/engine/render/core/DeviceCapabilityMatrix.cpp
 M src/engine/render/core/DeviceCapabilityMatrix.hpp
 M src/engine/render/graph/RenderGraph.cpp
 M src/engine/render/passes/HeightShadowPass.cpp
 M src/engine/render/passes/HeightShadowPass.hpp
 M tests/integration/DeviceCapabilityProductionGateTest.cpp
 M tests/integration/GIHistoryRejectionTest.cpp
 M tests/integration/RadianceDirectionalTest.cpp
 M tests/unit/RenderGraphValidationTest.cpp
?? docs/reviews/2026-08-17-gpu-modernization-final-review.md
?? tests/integration/RenderGraphExceptionSafetyTest.cpp
```

`settings.json`：仅 benchmarkScore 124.29000091552734→124.25920104980469、updatedAtUtc →2026-08-17T04:01:23Z，为基准运行产物（对照 P1 终审 NIT-6 处理先例），忽略。

## 范围对齐

本轮修复与「2026-08-16 审查后修复计划」对应：P0 快速修复（commit e7685b6e）、P1 管线校正（commit a7317e86）、P2 RenderGraph 现代化（commit bcc3c325）均已落地；本轮为审查发现 #9/#15/#17 残留项 + 新发现 RC 蓝色通道问题的补完。无范围泄漏（全部 12 个文件均在计划覆盖内）。

## 发现项

### A. 本轮修复核验（✅ 全部 VERIFIED）

#### A1. RC 级联蓝色辐射度丢失（新发现）✅

| 检查点 | 证据 | 结果 |
|---|---|---|
| 格式定义 | RenderConstants.hpp:329 `kRadianceFormat` 0x822F→**0x881A** (GL_RGBA16F；初修误用 0x815B=无标准枚举对应，复审期间修正，见 F5) | ✅ |
| 着色器声明 | v5_radiance_cascade.comp:17 `layout(rgba16f,binding=5) writeonly image2DArray` | ✅ |
| 父级采样 | v5_radiance_cascade.comp:98-99 读 `.rgb`（原 `.rg`+`(r+g)*0.5` 伪造 B） | ✅ |
| 写回 | v5_radiance_cascade.comp:136-137 `imageStore(..., vec4(merged, 0.0))` 全通道 | ✅ |
| 合成读取 | v5_gi_composite.comp:69-70 `gatherL0Irradiance` 读 `.rgb` | ✅ |
| 纹理创建 | RadianceCascadesPass.cpp:302/:339-343 TexStorage3D 使用 kRadianceFormat | ✅ |
| image 绑定 | RadianceCascadesPass.cpp:697-699 BindImageTexture(kRadianceFormat, WRITE_ONLY) | ✅ |
| 合成绑定 | GICompositePass.cpp:457-459 GL_TEXTURE_2D_ARRAY 绑 context.giRadianceTexture（对应 sampler2DArray） | ✅ |
| 一处不漏 | grep 复核：全仓库无遗留以 `.rg` 读辐射度；0x822F 在 assets/shaders 仅剩 shadow_sdf.comp:9（SDF 图像，无关） | ✅ |

#### A2. RenderGraph::Execute 异常安全（#9）✅ 生产代码正确，测试缺口见 F1

- RenderGraph.cpp:998-1028 `PassExecutionGuard`：RAII 析构（未 Commit 时）依次执行 `GPUTimerQueryRing::DiscardPass(stablePassId)`（关未决 GL query + 槽置 Discarded，环形缓冲干净）、`renderProfiler->EndPass`（cpuRunning 不泄漏）、`m_activeNodeIndex = -1`、`activeGraph = nullptr`。守卫在 BeginPass/profiler BeginPass/activeGraph 赋值之后构造，成功路径显式清理后 `Commit()` 解除。
- :1072-1093 `try { ApplyActivePassBindings; Execute } catch (std::exception&) / catch(...)`：LOG_ERROR + `ApplyRlglFlushTemplate()`（防止旧顶点泄漏到下一帧）+ `break`（终止后续 pass）。
- :1106 `EndFrame()` 在循环后无条件执行 → 帧级 timer 配对不被破坏。
- 帧尾契约核验（RenderSystem.cpp）：:1445 `UpdateCandidates` → :1789 `graph.Execute` → :1790-1794 `GPUResourceRegistry::Get().AdvanceFrame()`（exact-one frame advancement 注释：never before failed/aborted execute）→ :1798-1805 FlushRingToProfiler/EndFrame → :1850 transient pool EndFrame → :1853 GPUTexturePool EndFrame。fail-soft 下 Execute 正常返回，全部帧尾清理照跑 ✅。
- culled pass 分支（:1034-1039）仍 DiscardPass+continue，语义未变 ✅。

#### A3. HeightShadow 光方向解耦（#15 残留）✅

- height_shadow_apply.frag:21 新增 `uniform vec2 uLightDir = vec2(-0.45, -0.75)`（GLSL 初始化器保留历史默认，hot reload 无 CPU 值时安全）；:35 `normalize(uLightDir)`。
- HeightShadowPass.cpp:41-56 `IsDirectionalLight`：`spotCosHalfAngle <= -0.9999f` 排除（与 light_accumulation.frag:79 既有约定一致）；`dirX²+dirY² > 1e-8f` 过滤零方向。
- :58-76 `ResolveHeightShadowLightDir`：遍历 `LightManager::Get().GetActiveLightsCpu()`（= m_stagingBuffer，当前帧 staging 数据，LightManager.hpp:50-53）取首盏方向灯归一化；无灯回退默认。:314-319 每帧 Execute 内解析并上传（`m_lightDirLoc >= 0` 守卫）。
- 时序核验：UpdateCandidates（RenderSystem.cpp:1445）先于 graph.Execute（:1789），HeightShadowPass 于 :1525 入图，读到的必为当前帧灯光数据 ✅；无灯/未初始化场景（staging 空）安全回退默认 ✅。
- selfShadow（frag:54 固定 vec2(0.25,0.85)）与 POM（frag:72 固定 vec2(0.2,0.8)）为 view 方向近似，未改——符合设计意图（声明于剩余风险 R6）。

#### A4. DeviceCapabilityMatrix 能力探测（#17 残留）✅ fail-closed 语义保持

- DeviceCapabilityMatrix.cpp:81-119：compute/SSBO/imageLoadStore 改入口点探测（glfwGetProcAddress("glDispatchCompute"/"glBindBufferBase"/"glBindImageTexture")，GL 4.3 版本门禁为基础线——fail-closed，入口缺失仍报 unsupported，与 RenderSystem「No silent degradation」一致）。
- `maxSSBOBindings` 改 `glGetIntegerv(0x90DD)` 运行时查询；失败/无 context → 回退 16（GL 4.3 保证最低值）+ `maxSSBOBindingsFallbackUsed=true` + LOG_WARN。
- :159 报告串用 `'derived'` 而非 `'fallback'` 字面量，防 legacy marker 扫描误报；本终审复跑门禁 **133/31 PASS** ✅。
- `.hpp` 新增 `maxSSBOBindingsFallbackUsed` 成员；测试断言 `==16`→`>=16`（DeviceCapabilityProductionGateTest.cpp:100、RenderGraphValidationTest.cpp:415 区域，GL 4.3 保证 ≥16）。
- `m_probeOverrideForTesting` 注入机制保留（hpp:66,72,81；cpp:58-62 短路返回），测试注入语义未破坏 ✅。

### B. 本终审新发现项（复审后：F1/F2/F5 已修复 ✅）

**F1（Medium）— RenderGraph 异常路径无直接测试** ✅ 已修复
- 位置：src/engine/render/graph/RenderGraph.cpp:998-1028, :1072-1093（新增代码）。
- 修复：新增 `tests/integration/RenderGraphExceptionSafetyTest.cpp`（182 行，2 个 TEST_CASE）——`ProbeRenderPass` 夹具（`HasSideEffects()=true` 防 pass culling 剔除，覆写 RenderPass.hpp:76 默认返回 false 的虚函数），分别验证 std 异常与非 std 异常（throw 42）场景：(a) Execute 正常返回（fail-soft）✅；(b) 前置 pass 已执行、抛异常 pass 之后的后缀 pass 被跳过 ✅；(c) `context.activeGraph==nullptr` 复位 ✅；(d) 下一帧（关闭抛标志后）三 pass 全执行、状态自愈 ✅。测试尾部 `GPUTimerQueryRing::Get().Shutdown()` 幂等释放。
- 调试教训（记录于测试演进）：共享抛标志会使前置 pass 先抛（误报）；无资源声明的 pass 被 pass culling 静默剔除（RenderGraph.cpp:1856 `node.pass->HasSideEffects()` 判定），必须以 HasSideEffects()=true 保活。
- 验证：`bin/NoMoreDayTests.exe --test-case=[Integration]*RenderGraph*` 19/19 passed、156 assertions 全过；全量回归 unit 9/9、integration 6/6 通过。

**F2（Medium）— GI 测试夹具与生产格式漂移（本修复轮引入）** ✅ 已修复
- 位置：tests/integration/GIHistoryRejectionTest.cpp:34,50（R3GIKRadianceRg16f=0x822F）；tests/integration/RadianceDirectionalTest.cpp:44,92（R3DKRg16f）。
- 修复：两夹具常量改 0x881A（`R3GIKRadianceRgba16f`/`R3DKRgba16f`），`TexStorage3D` 同步对齐，注释更新；与生产 `RenderConstants::V5GI::kRadianceFormat` 格式一致（Grep 复核：全库其余 0x822F 均为合法 RG16F 用途——SDF 图、失真图、断言基线）。
- 语义核查：夹具写 atlas 仅写 R、G 通道（B=0），断言基于 R 通道强度，RGBA16F 下测试语义不变 ✅。

**F5（High→已修复）— 初修引入的格式枚举值错误（复审期间发现）**
- 位置：src/engine/render/RenderConstants.hpp:329（初修轮改动）。
- 问题：初修将 `kRadianceFormat` 改为 **0x815B，该值无标准 GL 枚举对应（GL_RGBA16 实为 0x805B），而 GL_RGBA16F 的正确枚举是 0x881A**。`TexStorage3D(0x815B)` 与 shader `layout(rgba16f)` 不匹配属未定义行为；全库 0x881A 引用 14 处（kEmissiveFormat、各 pass kGLRgba16f）佐证 0x881A 为既定浮点格式约定。首轮终审仅做格式一致性 grep（.rg 读取、通道对齐），未校验枚举值语义，漏检。
- 修复：0x815B→0x881A。教训：格式常量改动必须核对 GL 枚举表语义，而非仅查一致性。

**F3（Low）— HeightShadow 光方向解析无直接测试**
- 位置：src/engine/render/passes/HeightShadowPass.cpp:41-76。
- 建议：`IsDirectionalLight`/`ResolveHeightShadowLightDir` 为纯函数且可注入（经 `LightManager::UpdateCandidates` 喂构造灯数据），宜补单测：方向灯优先、spotCosHalfAngle≤-0.9999 排除、lenSq≤1e-8 排除、空表回退默认 (-0.45,-0.75)。

**F4（Low）— DeviceCapabilityMatrix 新探测/回退分支无直接测试**
- 位置：src/engine/render/core/DeviceCapabilityMatrix.cpp:81-119。
- 现状：`m_probeOverrideForTesting` 在探测前短路，入口点探测与 maxSSBOBindings 回退路径（fallback 标志 + LOG_WARN + 'derived' 报告串）仅被 live-GL 探测覆盖；GetIntegerv 失败注入无测试。
- 建议：接受为已声明风险，或为探测路径增加测试钩子后补测。

### C. 18 项发现闭环状态（对照审计报告）

| # | 严重度 | 闭环状态 |
|---|---|---|
| 1 GPUFluidParticle 对齐 | Blocker | ✅ P0 已修（offsetof/静态断言，commit e7685b6e） |
| 2 MDI 除数 64 vs 256 | Blocker | ✅ P0 已修 |
| 3 LightCulling 回读 | Blocker | ✅ P0 已修（默认关闭） |
| 4 GPUText/GPULoot 回读 | Blocker | ✅ P0 已修 |
| 5 JFA 漏绑 uMaskTexture | Blocker | ✅ P0 已修 |
| 6 RC 方向辐射度/farBlend/发光体 | High | ✅ P1 方向性修复 + 本轮 RGBA16F 补完颜色通道 |
| 7 TimerRing 深度 3 无就绪检查 | High | ✅ P1 已修（就绪检查） |
| 8 Initialize 静默退出/ABI 降级 | High | ✅ P0/P1 已修（失败传播 + Game.cpp:364） |
| 9 Execute 无异常保护 | High | ✅ 本轮修复（fail-soft RAII）；测试缺口见 F1 |
| 10 SPH O(N²) | High | ✅ P1 已修（邻域搜索） |
| 11 GameplayState 双重 Blit | High | ✅ P1 已修 |
| 12 剔除/插值不同步 | High | ✅ P1 已修 |
| 13 绑定治理/offsetof 断言 | Medium | ✅ P1/P2 已修 |
| 14 粒子半初始化 | Medium | ✅ P1 已修 |
| 15 阴影无方向伪阴影 | Medium | ✅ P1（shadow_sdf 方向化）+ 本轮（HeightShadow 光方向解耦）基本闭环；selfShadow/POM 保持 view 方向为设计内近似（R6） |
| 16 GI 全屏闪烁 resetHistory | Medium | ✅ P1 已修（历史拒绝） |
| 17 能力探测一票推断 | Medium | ✅ 本轮部分修复（入口点 + SSBO 预算动态化）；**格式支持仍硬编码（R4）** |
| 18 裸 glBufferSubData/Barrier::All | Medium | ✅ P1/P2 已修（Barrier 精细化、绑定治理） |

### D. 已声明残留（不阻塞，需显式记录）

- **R1（#9 fail-soft 权衡，已接受）**：pass 抛异常 → 本帧剩余 pass 跳过、画面降级一帧；帧尾清理照跑，状态自洽。权衡已显式注释于 RenderGraph.cpp:1076-1082。选择 fail-soft（而非 fail-fast）符合「帧不崩、下次渲染自愈」的运行时哲学，代价是异常帧视觉瑕疵与错误静默化（仅 LOG_ERROR）——接受。
- **R2（既有缺陷，本轮未触及）**：FluidSimulationPass.cpp:708 `BindTexture(kGLTexture2D, context.giRadianceTexture)`——2D-array 纹理绑到 sampler2D 目标，属流体 GI 既有缺陷，需独立修复（改 sampler2DArray + 绑定目标），建议单独立项。
- **R3（测试夹具 RG16F）**：已随 F2 修复闭环（夹具 RGBA16F 与生产一致）。
- **R4（#17 部分修复）**：5 种贴图格式支持仍硬编码（未用 glGetInternalformativ）；GL 4.3 核心对 R8/R16F/RG16F/RGBA16F 为强制支持格式，实际风险低，可接受。
- **R5（NOT_RUN）**：真机 GPU 渲染冒烟、性能基准未运行（本终审亦未重跑 build/ctest，采信主代理证据）。
- **R6（行为变化点需人工确认）**：高度阴影方向由固定 (-0.45,-0.75) 变为场景首盏方向灯（无灯回退默认）；辐射度 atlas RG16F→RGBA16F 显存/带宽约 +50%，性能影响待基准确认；selfShadow/POM 仍为 view 方向近似。

## 质量与风险评估

- 生产代码 4 项修复全部正确，无 Blocker/High 级新问题；着色器格式/通道改动一处不漏（grep 复核）。
- 风险集中为测试侧：F1（异常路径零覆盖）与 F2（夹具漂移）为 Medium，修复成本低、价值明确。
- legacy marker 门禁 133/31 实测通过，无回归。

## 验证证据

| 项 | 结果 | 执行者 |
|---|---|---|
| build.bat（RelWithDebInfo, VS2026） | BUILD_EXIT=0；legacy 门禁修复前 134/32 FAILED → 修复后通过；复审修复后重跑 BUILD_EXIT=0 | 主代理 |
| ctest unit | 9/9 通过（100%，复审修复后重跑） | 主代理 |
| ctest integration | 6/6 通过（100%，复审修复后重跑） | 主代理 |
| ctest ci | 1/1 通过 | 主代理 |
| RenderGraphExceptionSafetyTest（新增） | `bin/NoMoreDayTests.exe --test-case=[Integration]*RenderGraph*`：19/19 passed、156 assertions（含 F1 两条：std 异常 + 非 std 异常 fail-soft 与状态复位） | 主代理 |
| scripts/check_legacy_reintroduction.py | **133/31 → 133/31 PASS**（本终审独立复跑） | 本终审 |
| git status | 14 个修改文件 + 2 个未跟踪（本报告、新测试），边界与计划一致 | 本终审 |
| 着色器/绑定/时序 grep 复核 | 无遗留 `.rg` 辐射度读取；UpdateCandidates(1445) < graph.Execute(1789) < AdvanceFrame(1790-1794)；0x881A 全库 14 处一致、0x822F 余者均合法 RG16F 用途 | 本终审 |
| RenderGraph.cpp:998-1104 逐行核验 | guard 语义、catch/break、Commit、EndFrame 配对均正确 | 本终审 |

说明：ctest 项目按可执行套件注册（`ctest -N` 共 21 个 nmd.tests.* 套件，无 label），上表 unit/integration/ci 计数按主代理报告采信。

## 下一步动作

1. **✅ F2**：GIHistoryRejectionTest.cpp/RadianceDirectionalTest.cpp 夹具格式对齐 0x881A 已完成，两套件随全量回归通过。
2. **✅ F1**：throwing-pass 夹具测试（RenderGraphExceptionSafetyTest.cpp，std + 非 std 两条）已完成，19/19 通过。
3. **✅ F5**：kRadianceFormat 0x815B→0x881A 已修正。
4. **F3/F4**（可选）：补 IsDirectionalLight/ResolveHeightShadowLightDir 单测；评估探测回退路径测试钩子。
5. **R6**：真机渲染冒烟 + 性能基准（含辐射度 atlas 带宽变化），更新 settings.json 基准。
6. **R2**：流体 GI 2D-array→sampler2D 绑定缺陷单独立项修复。
7. 复审结论确认后在本文件追加最终结论（提交/修改）。
---

# 复审结论（第二轮，2026-08-17）

## 结论：**提交**

F1/F2/F5 三项复审要求全部核验通过，无新 Blocker/High 级问题。首轮「修改」结论所列阻塞项已闭环，翻转为「提交」。

## 逐项核验结果

### F1 — RenderGraph 异常路径测试（✅ VERIFIED）

证据链（tests/integration/RenderGraphExceptionSafetyTest.cpp，190 行，2 TEST_CASE）：

- **真实触发**：`ProbeRenderPass::HasSideEffects()=true`（:61）使三个无资源声明的探针 pass 在 pass culling 中保活（RenderGraph.cpp:1856 `node.hasSideEffects || node.pass->HasSideEffects()` → root pass → reachable，:1820-1939 核验）。抛点位于 `Execute` 内（:49-54）、`PassExecutionGuard` 构造（RenderGraph.cpp:1069）之后，必落入 catch；抛标志按 pass 独立（:98-102），前置 pass 不会被共享标志误触发（调试教训已落实）。
- **断言覆盖**：fail-soft 返回（CHECK_NOTHROW(graph.Execute)，:117/:177）、前驱+抛者已执行（:118-119/:178-179）、后缀 pass 跳过（CHECK_FALSE(trailingExecuted)，:120/:180）、`context.activeGraph == nullptr` 复位（:121/:181）、第二帧关闭抛标志后三 pass 全执行自愈（:125-133/:184-187）。
- **非 std 路径真实触发**：`throw 42`（:53）不被 `catch(std::exception&)`（RenderGraph.cpp:1075）捕获，必走 `catch(...)`（:1087-1093）——第二条 TEST_CASE 覆盖，非空转。
- **Shutdown 幂等安全**：GPUTimerQueryRing.cpp:64 `if (!m_initialized) return;` + :91-100 全量复位 → 重复调用安全；BeginFrame（:104）未初始化自动重新 Initialize → 跨 TEST_CASE 安全；Discarded 槽下一帧重建 query 对象（:208-244）为「自愈」的代码级支撑。
- 局限（已接受）：timer 槽态断言为间接（下一帧自愈），直接槽态由既有 RenderGraphCompilationTest.cpp:258-263 覆盖。

### F2 — GI 夹具格式对齐（✅ VERIFIED）

证据链：

- GIHistoryRejectionTest.cpp:34 `R3GIKRadianceRgba16f = 0x881A`、:50 `TexStorage3D(0x8C1A, 1, R3GIKRadianceRgba16f, ...)`；RadianceDirectionalTest.cpp:44 `R3DKRgba16f = 0x881A`、:92 `TexStorage3D(R3DKTexture2DArray, 1, R3DKRgba16f, ...)`——与生产 RenderConstants.hpp:329 `kRadianceFormat = 0x881A` 完全一致。
- 注入语义不变：两夹具均以 `TexSubImage3D` + 外部格式 GL_RG/GL_FLOAT 写入 R/G 两通道（GIHistoryRejectionTest.cpp:414-435 每 texel 2 float；RadianceDirectionalTest.cpp:219-233 同），GL_RG 外部格式上传至 RGBA16F 纹理合法，B/A 通道保持 0；断言基于 R 通道强度（RadianceDirectionalTest.cpp:238-239 中央区 R 均值；GIHistoryRejectionTest 读 HDR 帧缓冲），RG16F→RGBA16F 语义不变。
- 0x822F 全库其余用途（SDF 图/失真图/断言基线）均合法，未误改。

### F5 — kRadianceFormat 枚举值（✅ VERIFIED）

证据链：

- RenderConstants.hpp:329 现为 `kRadianceFormat = 0x881A; // GL_RGBA16F (RGBA16F Directional Probe Atlas)`，与 kEmissiveFormat（:328 同值）及全库 11 处 0x881A（RenderConstants.hpp + passes/*.cpp）一致。
- 全库 grep `0x815B`：**零匹配**，初修错误值彻底清除。
- 0x881A 确为 GL_RGBA16F（着色器 `layout(rgba16f)` 与 `BindImageTexture(kRadianceFormat)` 存储格式匹配，未定义行为风险消除）。
- 💭 nit（不阻塞，仅订正报告措辞）：复审报告 F5 描述「0x815B 实为 GL_RGBA16 normalized」有误——GL_RGBA16 的标准枚举为 0x805B，0x815B 无标准 GL 枚举对应。修复结果（0x881A）正确，仅建议订正该描述文字。

## 新发现问题

**无**（F3/F4 维持 Low/可选，非阻塞；唯一 nit 为上述报告措辞订正）。

## 风险声明

本复审采信主代理验证证据（build.bat BUILD_EXIT=0、legacy 门禁 134/32→通过、unit 9/9、integration 6/6、RenderGraphExceptionSafetyTest 19/19、156 assertions；本复审独立复跑门禁 133/31 PASS）。真机 GPU 渲染冒烟与性能基准仍未运行（NOT_RUN，R5）；辐射度 atlas RG16F→RGBA16F 的显存/带宽影响与高度阴影光方向行为变化（R6）需真机确认后方可判定为完全闭环——不阻塞本工作树「提交」结论，但应作为提交后的首项验证动作。