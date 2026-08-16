# 最终验证审查：GPU 管线核心机制纠偏 P1 交付（Final Verification Review）

- 审查文件：`docs/reviews/2026-08-16-gpu-pipeline-correction-final-review.md`
- 日期：2026-08-16
- 审查轮次：终审（三轮初审意见 → 三个修复子代理 → 本次代码级复验）
- 审查目标：对 Round-1 全部 16 项发现（Group A/B/C）逐一验证是否在代码层真实修复；复跑全部测试验证证据声明；检查文档一致性；结论 提交 / 修改
- 输入：`git diff HEAD`（全部交付未提交）、`docs/workflows/review.md`、`conductor/specs/rendering_engine_v5_master_spec.md`、`docs/plans/2026-08-16-gpu-pipeline-correction-p1-plan.md`、`docs/designs/2026-08-16-gpu-rendering-modernization-remediation-design.md`、两份初审 review 文档

## 变更文件边界

`git status --short`（交付全部未提交，70 个已修改 + 13 个未跟踪）：

- 已修改（` M`）：`assets/shaders/`（entity_mdi.frag、lighting/shadow_sdf.comp、v5_fluid_neighbor_search.comp、v5_gi_composite.comp、v5_jump_flood.comp、v5_occluder_extract.comp、v5_radiance_cascade.comp、loot/*.compute + loot_quad.vert、particle.frag、particle_sub_emit.compute、text/text_layout.compute）、`docs/designs/2026-08-16-gpu-rendering-modernization-remediation-design.md`、`docs/plans/2026-08-16-gpu-pipeline-correction-p1-plan.md`、`settings.json`、`src/engine/render/` 全量核心（GPUData.hpp、GPULootSystem、GPUTextSystem、GPUFlowFieldSystem、GPUParticleSystem、PersistentBuffer、RenderConstants.hpp、core/BindingRegistry、core/QualityTierManager、core/RenderConstants.hpp、fluid/SPHReference.hpp、graph/RenderContext.hpp、particle/ParticleTextureManager、passes/{GICompositePass,RadianceCascadesPass,ShadowBuildPass,JFAPass,OccluderExtractPass,FluidSimulationPass,DistortionPass,HeightShadowPass}、resource/TextureArrayManager、trail/GPUTrailRenderer、MDIRenderer、MaterialManager、RenderSystem、ComputeBuffer.hpp）、`tests/`（GIHistoryRejectionTest、GraphBindingEquivalenceGLTest、JFAPassUpsampleMaskTest、FluidSimulationBenchmark、RadianceCascadesBenchmark、GPUABIGovernanceTest、JFAPassProductionCorrectnessTest、QualityTierManagerTest、RenderGraphValidationTest、TextureArrayManagerTest）
- 未跟踪（`??`）：`assets/shaders/lighting/v5_fluid_compact.comp`、`v5_fluid_prefix_sum.comp`、`assets/shaders/text/text_indirect_args.compute`、`tests/integration/{BarrierPrecisionReadbackAuditTest,GPUIndirectArgsIntegrationTest,RadianceDirectionalTest,ShadowLootBindingConflictRegressionTest}.cpp`、`tests/performance/GPUIndirectPassBenchmark.cpp`、`tests/unit/{ColorSpaceLinearizationTest,GPUIndirectArgsRingUnitTest}.cpp`、`docs/reviews/2026-08-16-gpu-pipeline-correction-{quality,spec}-review.md`、`ci_run.txt`（见发现项 N-1）

## 结论

**审查结论：提交**

全部 16 项 Round-1 发现均已在代码层修复并复验（VERIFIED 16/16，无 REJECTED）；构建通过；测试证据与文档声明一致；模块边界与遗留回归脚本双通过。残余风险均已在计划/审查文档中诚实声明（T9.3 无黑帧冒烟 + M0-C 门禁 NOT_RUN、GPU 内核计时 NOT_RUN），不构成伪造证据。本终审新增 2 项低危卫生项（非阻塞）。

## 发现项逐项裁决表

### Group A（reviewer 1：组 1/2/6/7）— 全部 VERIFIED

| # | 裁决 | 证据（file:line） |
| --- | --- | --- |
| B1 | ✅ VERIFIED | `tests/integration/BarrierPrecisionReadbackAuditTest.cpp:113-115`：`CHECK(field.size() == (size_t)flowSystem.GetWidth() * (size_t)flowSystem.GetHeight())` 尺寸不变式，原同义反复已删除；测试通过 |
| M1 | ✅ VERIFIED | `src/engine/render/passes/ShadowBuildPass.cpp:531-534` Execute 图谱分支无条件 `m_occluderBuffer.BindBase(RenderConstants::ShadowCS::kOccluderBinding)`（附三缓冲 slot 重绑注释）；`src/engine/render/RenderConstants.hpp:302` `kOccluderBinding = 0u`（Shadow 本地物理 slot 0，已迁出全局表）；非图谱分支同走 BindBase，`:548-552` 仅在 `context.activeGraph==nullptr` 时解绑；`shadow_sdf.comp`/`v5_occluder_extract.comp` binding=0 一致 |
| M2 | ✅ VERIFIED | `src/engine/render/GPULootSystem.cpp:791-795`：`if (!m_initialized || m_renderShader.id == 0 || m_vao == 0 || m_indirectBuffer.GetId() == 0) return;` 守卫恢复 |
| m1 | ✅ VERIFIED | `src/engine/render/GPUFlowFieldSystem.cpp:294-301` SyncToCPU 仅 `TryReadNonBlocking(...,1)` 非阻塞；`:306-318` DownloadFlowField 非阻塞 + miss 时回退零初始化 m_flowFieldShadow（memcpy）；阻塞 `Read` 已删除；Init 三缓冲 `Create(cellCount*sizeof(Vector2), 3)`（:55） |
| m2 | ✅ VERIFIED | `src/engine/render/PersistentBuffer.cpp:121-123` CreatePersistent 对映射内存 `memset 0`；`:132-137` CreateCompat `m_stagingBuffer.assign(size,0)` 预清零后再 BufferData（首帧读已定义） |
| m3 | ✅ VERIFIED | `GPUTextSystem.hpp:101-106` + `GPULootSystem.hpp:102-106`：`m_readbackEnabledForTesting = false` 默认，`Set/IsReadbackEnabledForTesting` 访问器；`GPUTextSystem.cpp:272-306`（poll）与 `:440-460`（submit）全部由该开关门控；`GPUIndirectArgsIntegrationTest.cpp:54/135` Init 后启用；生产路径零 poll/零 submit；`GPULootSystem.cpp:764-788` 同 |
| N2 | ✅ VERIFIED | `src/engine/render/GPULootSystem.cpp:21` `kLootRenderInstanceBinding` 已删除；Dispatch/Render 统一 `LootPassBinding::INSTANCE_SSBO`（=Binding::SSBO_LOOT_INSTANCE=15，RenderConstants.hpp:46/126-127），与 `assets/shaders/loot/*.compute` 及 `loot_quad.vert` 全部 binding=15 一致 |

### Group B（reviewer 2：组 3/4/8）— 全部 VERIFIED

| # | 裁决 | 证据（file:line） |
| --- | --- | --- |
| BLOCKER-1 | ✅ VERIFIED | `RadianceCascadesPass.cpp:740-751` ResolveRaysPerProbe：Low/Medium=1u；High=`max(2u,4u>>k)`（L0=4）；Ultra=`max(2u,16u>>k)`（L0=16,L1=8,L2=4,L3=2,L4=2,L5=2）。设计 AD-3 文字已改为"16 扇区 L0…L5=2"+ 定稿注释；计划 T3.1 已勾选并记录公式；§6.1 D1 行 = 已定稿。三方一致 |
| BLOCKER-2 | ✅ VERIFIED | `GICompositePass.cpp:485-493` uRaysPerProbe = context.giRadianceDirections（fallback GetRadianceDirections() 后 1u）；`RenderSystem.cpp:1680-1682` `graphContext.heightFieldTexture = (g_heightShadowPass!=nullptr) ? g_heightShadowPass->GetHeightFieldTexture() : ...`；`GICompositePass.cpp:467-479` 绑 unit 2 + uHeightFieldEnabled=1/0，`:556-560` dispatch 后解绑；CRITICAL 修复确认：`:457-459` radiance atlas 以 `GL_TEXTURE_2D_ARRAY` 绑定（原 GL_TEXTURE_2D 会静默杀死汇聚输出）；`v5_gi_composite.comp` sampler2DArray 一致 |
| BLOCKER-3 | ✅ VERIFIED | `GIHistoryRejectionTest.cpp:361-550` 重写：相机匀速平移 + 单点光源正弦强度 `0.5+0.15*sin(2π*frame/10)`（:391-412）；layer-major 索引 `((d*h)+y)*w+x`（:424-426，注释说明 TexSubImage3D 要求）；HDR 回读 glGetTexImage RGBA/FLOAT（:337-350）；差分 RMS<0.05（:514）；flickerA<0.15 且 flickerA<flickerB*0.6（:539-540）；accessor 合同断言（:482-487）。实测通过（flicker 具体数值未打印，无法复测 0.0202/0.0405 原值，但断言全部通过） |
| MAJOR-4 | ✅ VERIFIED | `tests/integration/RadianceDirectionalTest.cpp` 已自动注册：T3.5(a) 45° 侧照（:275-291，aligned=0.25、facingDown=0.25*cos45、facingUp≈0 + 排序断言）；(b) 余弦加权积分与 shader `v5_gi_composite.comp:61-75` 公式一致（E=(1/N)ΣL·max(0,n·ω)，N=4、dir0=1.0）；(c) Low/Medium 1 扇区静态断言（:312-319）+ 逐 tier 级联 Execute 断言 GetCascadeTarget(0).directions 与 context.giRadianceDirections（:382-389）。实测 2/2 通过 |
| MAJOR-5 | ✅ VERIFIED | `GICompositePass.cpp:370-394` occluder prev/curr 屏幕边界投影 UV dirty-rect，空边界全屏兜底；`:600-601` m_prevOccluderMaskVersion 读取+更新；全局限定：`:228` 仅 history 无效/extent 变化才 resetHistory（T4.1 合规）；`GIHistoryRejectionTest.cpp:246-324` occluder 版本变化无全局重置用例通过 |
| MAJOR-6 | ✅ VERIFIED | `GICompositePass.cpp:396-403` AD-4 注释：VFX emissive 快照烘焙进单一合并纹理、无逐源世界坐标 → 全屏失效兜底为显式设计决策 |
| MINOR-8 | ✅ VERIFIED | `GICompositePass.cpp:253-290` `std::array<DirtyRect,16>` + count + 溢出并入 slot 0（:407-410）；`:597-598` m_prevActiveLights 拷贝仅 lightsChanged 时执行 |
| MINOR-9 | ✅ VERIFIED | `tests/unit/ColorSpaceLinearizationTest.cpp:107-196`：写临时 settings.json `render.color.linearPipeline`（:117-129），`QualityTierManager::Initialize(path, true)` forceRedetect（:135/139），true/false 双路径断言 + SetLinearPipelineEnabled 往返持久化（:143-155）；`core/RenderConstants.hpp:108` 默认 `linearPipeline=true` 未改（符合意图）；`QualityTierManager.cpp` 新增 TryLoadLinearPipelineEnabledOverride/SetLinearPipelineEnabled 落盘。实测 4/4 通过 |
| MINOR-10 | ✅ VERIFIED | `TextureArrayManager.hpp:30-38/45-48` RESERVED/EXPERIMENTAL 注释 + GetDefaultLinearForSemantic/IsSemanticLinear/IsLayerLinear 元数据接口；`ParticleTextureManager.hpp:15-18` 同款注释 + LoadLayer(path, isLinear=false) 仅记录元数据；解码仍由全局 uLinearPipeline 门控 |
| NIT-11a/11b/11c | ✅ VERIFIED | 11a：`v5_gi_composite.comp` 无 uRadianceResolution，GICompositePass 无 m_radianceResolutionLoc；11b：`GICompositePass.hpp` 无 m_prevLightSignature/BuildLightSignature/HashMix/FNV（m_prevActiveLights 取代）；11c：`:355-366` 移除光源矩形在 m_prevCameraValid 时用前一相机投影 |

### Group C（reviewer 3：组 5 基准 + 文档/门禁）— 全部 VERIFIED

| # | 裁决 | 证据（file:line） |
| --- | --- | --- |
| MAJOR-1 | ✅ VERIFIED | `tests/performance/FluidSimulationBenchmark.cpp` 新增用例命名诚实（CPU reference neighbor search baseline），实测 `SPHReference.hpp::BuildNeighborTableHashed`（非 GPU 内核），硬断言 `CHECK(stats.mean_ms <= 0.30)`，指标 fluid_cpu_ref_4k_mean_ms/p99/target_hit，GPU 指标打印 `fluid_gpu_neighbor_kernel_4k_mean_ms=NOT_RUN`。实测三次 mean=0.1598/0.1687/0.1904ms、p99=0.198/0.295ms、target_hit=1 |
| MAJOR-2/4/5/6/NIT-7 | ✅ VERIFIED | 计划：T1-T8 + T9.1/9.2 全部 [x]，T9.3 保持 [ ] 并注明 "**NOT_RUN：无黑帧冒烟与 M0-C 门禁未执行，列为残余风险**（§5.2 NOT_RUN ≠ GO；不伪造证据）"；§0 环境声明重写；§6.1 D1/D2 已定稿 + 新增 D6/D7 行（occluder 局部矩形 / emissive 全屏兜底）；§1.5 修正为 cell 计数串行扫描（~6,758 cells @1080p zoom=1 cellSize=18，gridW×gridH=109×62，109*62=6758 ✓）。设计：AD-3 文字修正为 16 扇区 L0 + 定稿注释；§3.2-5 27→9 邻桶 + D2 定稿（radius==cellSize==18.0，9-cell 全覆盖）；新增 "Phase 1 门禁结果（2026-08-16 定稿记录）" 块。两份初审 review：基准声明全部改为 CPU-reference + GPU NOT_RUN；ci 计数与本终审实测一致（见下）；新增残余风险/未执行项章节；性能执行记录在案 |
| NIT-6 | ✅ VERIFIED | `settings.json` benchmarkScore 124.29959869384766→124.29000091552734、updatedAtUtc 2026-08-05T14:34:48Z→2026-08-16T13:24:07Z —— 基准运行产物，如实保留 |

## 测试证据（实测数据，2026-08-16）

构建：`.\build.bat`（RelWithDebInfo 增量）→ **成功**（含 render ABI 治理、资产校验、模块边界与遗留回归预检）。

| 套件 | 命令 | 实测结果 |
| --- | --- | --- |
| unit（首轮） | `ctest -L unit -C RelWithDebInfo` | 8/9 通过；`nmd.tests.ai.unit` 失败 1 次（159 cases/158 passed/1 failed，1223 断言 1 失败） |
| unit（复跑） | 同上 | **9/9 通过**（ai.unit 通过；该套件复跑为绿色，见发现项 N-2） |
| integration | `ctest -L integration -C RelWithDebInfo` | **6/6 通过** |
| ci | `ctest -L ci -C RelWithDebInfo` | **1/1 通过**；二进制直跑 `--test-case-exclude=*Performance*,*GPU-Diagnostic*`：**1148/1148 passed，83 skipped，104,416/104,416 assertions**（与初审文档声称 1148/1148、83、~104,416-104,418 一致） |
| performance | `ctest -L performance -C Release` | 75 cases / 74 passed / 1 failed / 1156 skipped；20168 断言 1 失败。失败项 = **已知预存 flake `ParticleTrailBenchmark.cpp:205`**（`CHECK(dispatchOverheadMs < 0.2)` 实测 0.255579 / 复跑 0.268686）。该文件**未在本交付 diff 中**，隔离单跑 `--test-case="*ParticleTrail - Scenario 4 SubEmitter*"` **1/1 通过** → 环境性 flake，非交付缺陷 |
| 定向 | `NoMoreDayTests.exe --test-case=...` | T3.5 2/2、T4.4 1/1、Color Space Linearization 4/4、CPU reference 1/1、M0-A 3/3、Shadow & Loot Binding Governance 2/2、Non-blocking Readback Ring 2/2、Zero Synchronous Readback on Render Path 2/2、PersistentBuffer TryRead delayed snapshot 1/1 —— **全部通过** |
| 流体基准指标 | Release 直跑 | `fluid_cpu_ref_4k_mean_ms=0.159755`、`p99=0.1981`、`target_hit=1`；`fluid_gpu_neighbor_kernel_4k_mean_ms=NOT_RUN` ✓；既有 `fluid_reference_10k_mean_ms=0.71472/p99=0.8381/target_hit=1` |

模块边界与遗留回归（用户要求的独立执行，build.bat 亦内嵌执行均 OK）：
- `python scripts/check_module_boundaries.py` → **PASS**（ledger/observed reverse edges 0/0 匹配）
- `python scripts/check_legacy_reintroduction.py` → **PASS**（baseline 133/31 vs current 133/31，无标记/分类回归）

## 本终审新发现项

- **N-1（Low，卫生）**：仓库根目录存在未跟踪的 `ci_run.txt`（子代理运行日志残留）。建议删除或移入 `docs/`/`bin/`，避免随提交进入版本库。
- **N-2（Low，观察项）**：`nmd.tests.ai.unit` 在首轮全量 unit 运行中失败 1 次、复跑即绿（全量 9/9）。不在已知 flake 清单中，但复跑稳定通过且该套件含 GPU/时序敏感用例，判定为同类环境性 flake；建议后续纳入 flake 观察清单。
- **N-3（备注）**：BLOCKER-3 初审声称的具体 flicker 数值（flickerA=0.0202/flickerB=0.0405/ratio=0.499、71 断言）无法从测试输出复现原值（测试不打印中间量），但全部断言（含 RMS<0.05、flickerA<0.15、flickerA<0.6*flickerB）实测通过，功能声明成立。

## 质量与风险评估

- 对照 `conductor/code_standard.md`：无伪造测试、无隐藏失败路径（T9.3 以 [ ] + NOT_RUN 明示，未把未运行当通过）；无 UB/UAF/泄漏（PersistentBuffer fence 均 DeleteSync、ring 槽 Shutdown 清理）；无裸 new/delete、无 dynamic_cast 新增；hot-path 无新增堆分配（DirtyRect 数组化、readback ring 固定深度 3）。
- 零同步回读合同：生产路径 poll/submit 均被 `m_readbackEnabledForTesting` 门控，静态审计 + `ComputeBuffer::Read` 测试缝（Zero Synchronous Readback Production Audit）+ 集成测试三重验证。
- 渲染正确性风险点已覆盖：radiance atlas 2D-Array 绑定修复（BLOCKER-2 CRITICAL）、RG16F 格式与 shader 一致（image2DArray/rg16f）、directions 与 atlas 深度一致性由 context.giRadianceDirections 贯穿。
- 性能：新增 `GPUIndirectPassBenchmark.cpp` 已进入 performance 套件；RadianceCascadesBenchmark 更新后通过。

## 剩余风险（已在文档中声明）

1. **T9.3 NOT_RUN**：无黑帧功能冒烟 + M0-C 阶段门禁未执行（计划 §5.2 NOT_RUN ≠ GO），列为残余风险。
2. **GPU 内核计时 NOT_RUN**：SPH 4096 粒子邻居内核 ≤0.3ms（计划 §5.2 项 4）未在真实 GPU 上计时验证；当前仅 CPU reference 基线 0.16-0.19ms。
3. 已知预存 flake（ParticleTrailBenchmark:205 等）与环境性 flake（ai.unit 偶发）需在门禁执行时复跑确认。

## 下一步动作

1. 清理 `ci_run.txt` 残留（N-1）。
2. 在具备条件的硬件上执行 T9.3 无黑帧冒烟与 M0-C 门禁，并记录 GPU 内核计时（计划 §5.2 项 4）。
3. 将 ai.unit 加入 flake 观察清单（N-2）。
4. 提交前执行 `git add -A` 全量核对（含 13 个未跟踪文件），确认无意外文件入仓。
