# GPU 渲染系统 实施进度追踪

> **对应设计文档**: [GPU_Rendering_System_2.md](../设计文档/特效和UI/GPU_Rendering_System_2.md) | [GPU_Rendering_System_3.md](../设计文档/特效和UI/GPU_Rendering_System_3.md)  
> **起始日期**: 2026-02-12  
> **最后更新**: 2026-02-19

---

## V2 总体进度

| 阶段 | 名称 | 状态 | 对应 Track | 关键产出 |
|---|---|---|---|---|
| **Phase 0** | 基础设施 (Foundation) | ✅ 已完成 | `rendering_foundation_migration_20260212` | RenderGraph、资源池、质量分级、RenderSystem 拆分 |        
| **Phase 1** | HDR + 后处理管线 | ✅ 已完成 | `hdr_postprocess_pipeline_20260212` | HDR SceneBuffer、Bloom、Tonemap、FXAA、Vignette、基准测试 |
| **Phase 2** | 动态光照系统 | ✅ 已完成 | `dynamic_lighting_system_20260212` | GPULight SSBO、LightManager、LightingPass、光源挂载 |
| **Phase 3** | 粒子与轨迹增强 | ✅ 已完成 | `particle_trail_enhancement_20260213` | 纹理粒子、GPU TrailRenderer |
| **Phase 4** | 材质与 VFX 序列器 | ✅ 已完成 | `material_vfx_sequencer_20260213` | 材质系统、VFXTimeline、Distortion Pass |
| **Phase 5** | 打磨与高级特性 | ✅ 已完成 | `polishing_advanced_features_20260213` | Color Grading、Volumetric Light、Profiler HUD、Shader Hot Reload |

---

## V3 总体进度

> **设计文档**: [GPU_Rendering_System_3.md](../设计文档/特效和UI/GPU_Rendering_System_3.md)

| Step | 名称 | 周期 | 状态 | 对应 Track | Tasks | 关键产出 |
|---|---|---|---|---|---:|---|
| **A** | V3 基线与契约 | 第1周 | ✅ 已完成 | `v3_baseline_contracts_20260216` | 20/20 | RenderConfig V3、ABI V3、Pass 顺序锁定、Binding 治理、Feature Flag |
| **B** | 阴影系统 | 第2-4周 | ✅ 已完成 | v3_shadow_pipeline_20260215 | 27/27 | SDF + Hybrid Shadow、ShadowPrepare/Build/Resolve、Atlas 分配器 |
| **C** | Clustered Lighting | 第3-5周 | ✅ 已完成 | `v3_clustered_lighting_20260215` | 25/25 | LightCullingPass (compute)、z-layer 映射、无回归门禁 |
| **D** | Material 2.0 | 第4-6周 | ✅ 已完成 | `v3_material_lighting_depth_20260215` | 30/30 | Schema v2、BRDF-lite、Texture2DArray、双缓冲热重载（D0.3 uplift 门禁迁移至 Step F） |
| **E** | VFX 联动 | 第6-8周 | ✅ 已完成 | `v3_vfx_lighting_integration_20260215` | 33/33 | Schema v3、3 类新事件、tierPolicy、预算估计器、12 模板 |
| **F** | 全链路验收 | 第8-10周 | ✅ 已完成 | `v3_validation_and_release_gate_20260215` | 37/37 | 4 层门禁、截图差异、压力测试、风险验证、回退演练（`F4.6` 临时豁免，`F6.2` 转 V4 前置依赖） |

**V3 总任务数**: 172（完成 172，剩余 0）

### 维护补充（2026-02-19）

- [x] 设置系统与渲染配置闭环补齐：`SettingsState` 图形页新增 V3 可调开关（V3 总开关 / Clustered Lighting / Normal Lighting / Specular Highlights），并通过 `QualityTierManager` 持久化到 `settings.json` 的 `render.v3`。
- [x] 修复 `GameSettings::Save` 覆盖写入导致的配置丢失风险：改为"读取现有 JSON 后合并更新基础设置字段"，避免覆盖 `render.v3` 与自动检测元数据。

---

## V2 归档验收记录

### Phase 5 验收 (polishing_advanced_features_20260213)

- [x] **A/B Color Grading**: LUT 资产就位，PostProcessPass 集成完成，支持 16/32 LUT。
- [x] **C Volumetric Light**: VolumetricLightPass 实现并集成至 Ultra 档 HDR 路径。
- [x] **D Profiler HUD**: RenderProfiler 核心实现，支持 CPU/GPU 采样与 HUD 实时绘制。
- [x] **E Shader Hot Reload**: 实现 0.5s 轮询监控，支持 12+ Shader 的热重载与安全回退。
- [x] **F 验证**: 
  - `NoMoreDayTests.exe` 170 个 Case 全部通过。
  - 性能摘要日志确认：ColorGrading/Volumetric 耗时均在预算范围内。

归档位置: `conductor/archive/polishing_advanced_features_20260213/`

### Phase 4 验收 (material_vfx_sequencer_20260213)

- [x] **A/B 材质底层**: `GPUMaterialData`、`MaterialManager`、SSBO 同步、预设注册 (A.1-B.3)
- [x] **C 材质管线**: JSON 资产解析、Schema 校验、热重载支持 (C.1-C.3)
- [x] **D Shader 集成**: `material_abi`、粒子材质采样、flags 编解码 (D.1-D.4)
- [x] **E/F/G VFX 序列器**: `VFXSequenceManager`、`VFXPlayerComponent`、`VFXSequencerSystem`、7 种事件执行器 (E.1-G.4)
- [x] **H Distortion Pass**: `DistortionPass`、环形扭曲 Shader、RenderGraph 集成 (H.1-H.4)
- [x] **I/K 基础与验证**: RenderConfig 扩展、全套单测 (`MaterialTest`/`VFXSequencerTest`/`DistortionTest`)、基准测试 (I.1-K.5)
- [x] **J 预制库**: 10 个 VFX 序列 JSON 资产已创建并集成至技能系统 (J.1-J.3)
- [x] **M 回归修复**: 修复 `GPUUtils` 状态丢失与 `GPUParticleSystem` 死锁问题 (BUG-20260213-001)，画质档位差异验证通过 (M.1-M.4)

归档位置: `conductor/archive/material_vfx_sequencer_20260213/`

### Phase 3 验收 (particle_trail_enhancement_20260213)

- [x] A/B/C/D/E/F 核心阶段全部完成。
- [x] 纹理粒子与 GPU Trail 运行证据（日志/单测）通过。
- [x] 验收文档已补全，Track 已关闭。

归档位置: `conductor/archive/particle_trail_enhancement_20260213/`

### V2 归档位置

- Phase 0: `conductor/archive/rendering_foundation_migration_20260212/`
- Phase 1: `conductor/archive/hdr_postprocess_pipeline_20260212/`
- Phase 2: `conductor/archive/dynamic_lighting_system_20260212/`

---

## 风险与状态（V2/V3）

| ID | 描述 | 状态 | 缓解措施 |
|---|---|---|---|
| R-001 | rlgl 状态与自定义后处理管线冲突 | 已解决 | Pass 边界强制 Flush + ScopedGLState |
| R-002 | 集显平台 HDR/FBO 性能不稳定 | 监控中 | 低档位回退路径 + Mip 等级可降阶 |
| R-003 | 最终视觉/稳定性结果依赖长时间实机运行 | 已验证 | 170+ 单测通过，10k 实体基准压测通过 |
| R-V3-001 | Shadow Atlas 溢出抖动 | 待验证 | 确定性淘汰 + 滞回策略 + 日志计数 |
| R-V3-002 | Cluster 溢出导致漏光 | 待验证 | 固定裁剪优先级 + 溢出统计 + 回归用例 |
| R-V3-003 | ABI 偏移错位 | 待验证 | 生成链路唯一化 + CI layout 快照 |
| R-V3-004 | Tier 降级抖动 | 待验证 | 降级冷却时间 + 恢复阈值滞回 |
| R-V3-005 | 热重载中断 | 待验证 | 双缓冲句柄 + 验证后替换 |

---

## 后续演进路线

> **V4 设计文档**: [GPU_Rendering_System_V4.md](../设计文档/特效和UI/GPU_Rendering_System_V4.md)  
> **V5 设计文档**: [GPU_Rendering_System_V5.md](../设计文档/特效和UI/GPU_Rendering_System_V5.md)  
> **V4 主控规格书**: [rendering_engine_v4_master_spec.md](./specs/rendering_engine_v4_master_spec.md)  
> **V5 主控规格书**: [rendering_engine_v5_master_spec.md](./specs/rendering_engine_v5_master_spec.md)

---

## 记忆对齐更新（2026-02-19）

- V4 依赖口径统一：`v4_pbr_material_pipeline_20260219` 依赖 `v4_gpu_text_rendering_20260219` 与 `v4_gpu_loot_rendering_20260219`，确保 V4-A 完成后进入统一 ABI V4 迁移窗口。
- V5 门禁口径统一：`v5_validation_release_gate_20260219` 的阻断依赖仅为 `v5_jfa_distance_field_20260219` 与 `v5_radiance_cascades_gi_20260219`；`v5_sph_fluid_exploration_20260219` 作为 GO/NO-GO 决策输入，不阻断核心发布。
- V5 验收指标量化：GI 拖影、叠加亮度比、极限场景回退性能、显存漂移、SDF 增量误差已转换为可测阈值（以 Track 9 spec/plan 为准）。
- V5 JFA 验收补充：half-res 上采样质量新增 RMS/P95 量化指标，替代“视觉可接受”类主观描述。

---

## V4 总体进度

> **设计文档**: [GPU_Rendering_System_V4.md](../设计文档/特效和UI/GPU_Rendering_System_V4.md)  
> **主控规格书**: [rendering_engine_v4_master_spec.md](./specs/rendering_engine_v4_master_spec.md)

| # | Phase | 名称 | 周期 | 状态 | 对应 Track | Tasks | 关键产出 |
|---|---|---|---|---|---|---:|---|
| 0 | Pre-flight | V3 Debt Closure | Week 0-1 | 📋 Pending | `v4_preflight_v3_closure_20260219` | 0/8 | V3 遗留闭环、风险确认、V4 绿灯 |
| 1 | V4-A | GPU Text (MSDF) | Week 1-3 | 📋 Pending | `v4_gpu_text_rendering_20260219` | 0/20 | MSDF Atlas、Compute 排版、MDI 绘制 |
| 2 | V4-A | GPU Loot Rendering | Week 1-3 | 📋 Pending | `v4_gpu_loot_rendering_20260219` | 0/18 | MDI 合批、FrustumCull、力导向避让 |
| 3 | V4-B | 2D PBR Material | Week 3-6 | 📋 Pending | `v4_pbr_material_pipeline_20260219` | 0/25 | GPUMaterialDataV3、BRDF-Lite、ABI V4 |
| 4 | V4-C | Advanced Lighting | Week 6-9 | 📋 Pending | `v4_advanced_lighting_20260219` | 0/28 | 4096 光源、HeightShadow、POM |
| 5 | Gate | Validation & Release | Week 9-11 | 📋 Pending | `v4_validation_release_gate_20260219` | 0/30 | 5 维度门禁、回退验证、发布判定 |

**V4 总任务数**: 129（完成 0，剩余 129）

---

## V5 总体进度

> **设计文档**: [GPU_Rendering_System_V5.md](../设计文档/特效和UI/GPU_Rendering_System_V5.md)  
> **主控规格书**: [rendering_engine_v5_master_spec.md](./specs/rendering_engine_v5_master_spec.md)

| # | Phase | 名称 | 周期 | 状态 | 对应 Track | Tasks | 关键产出 |
|---|---|---|---|---|---|---:|---|
| 6 | V5-A | JFA Distance Field | Week 0-3 | 📋 Pending | `v5_jfa_distance_field_20260219` | 0/22 | JFA 距离场、OccluderExtract、增量更新 |
| 7 | V5-A/B | Radiance Cascades GI | Week 3-8 | 📋 Pending | `v5_radiance_cascades_gi_20260219` | 0/35 | Emissive Buffer、6 级联 GI、时域稳定 |
| 8 | V5-B | SPH Fluid (⚠️探索) | Week 5-8 | 📋 Pending | `v5_sph_fluid_exploration_20260219` | 0/18 | SPH 核心、GI 交互、GO/NO-GO |
| 9 | Gate | Validation & Release | Week 8-10 | 📋 Pending | `v5_validation_release_gate_20260219` | 0/25 | 核心+可选门禁、架构评估 |

**V5 总任务数**: 100（完成 0，剩余 100）

---

## 里程碑时间线（V2→V5 全景）

```
V2 (已完成 · Phases 0-5)                    V3 (已完成 · Steps A-F · 172 tasks)
═══════════════════════                      ═══════════════════════════════════
Phase 0→1→2→3→4→5                            Step A → B/C → D → E → F → Bugfix Gate
                                                                              ↓
V4 Pre-flight → V4-A (Text+Loot) → V4-B (PBR) → V4-C (Light) → V4 Gate
                                                                        ↓
V5-A (JFA → Radiance Cascades) → V5-B (Full GI + SPH⚠️) → V5 Gate
                                                                ↓
                                                   渲染引擎成熟体完成
```

---

## V4/V5 风险追踪

| ID | 描述 | 概率 | 状态 | 缓解 | 监控 Track |
|---|---|:---:|---|---|---|
| V4-R01 | MSDF 中文字形超出单张图集 | 中 | 监控中 | 双图集 + LRU | Track 1 |
| V4-R02 | 力导向避让不收敛 | 低 | 监控中 | 阻尼衰减 + 3帧锁定 | Track 2 |
| V4-R03 | 2D PBR 法线过于统一 | 中 | 监控中 | Roughness bias + Fresnel 抑制 | Track 3 |
| V4-R04 | 4096 光源 Cluster 溢出 | 中 | 监控中 | 优先级裁剪 + 降级 | Track 4 |
| V4-R07 | ABI V4 迁移致 V3 回归 | 中 | 监控中 | V3→V4 兼容映射 | Track 5 |
| V5-R01 | JFA 精度不足致 GI 漏光 | 中 | 监控中 | JFA+1/+2 补偿 | Track 6 |
| V5-R03 | 帧预算不足（极限场景） | 高 | 监控中 | half-res + 帧间隔 + 关闭 | Track 7+9 |
| V5-R06 | SPH 粒子不稳定 | 中 | 监控中 | Leapfrog + CFL | Track 8 |
| V5-R07 | OGL 4.3 计算天花板 | 低 | 监控中 | 预研 Vulkan (V6) | Track 9 |
