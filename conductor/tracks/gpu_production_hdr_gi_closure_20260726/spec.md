# GPU Production HDR/GI Closure 规格说明书

> **Track ID**: `gpu_production_hdr_gi_closure_20260726`
> **类型**: P0 bugfix/integration
> **依赖**: `v5_jfa_distance_field_20260219`、`v5_radiance_cascades_gi_20260219`、`v5_validation_release_gate_20260219`
> **设计输入**: [GPU 渲染引擎架构审查](../../../docs/reviews/2026-07-26-gpu-rendering-engine-audit-review.md)
> **状态**: ✅ Complete

---

## 1. 问题与目标

`GameplayState::OnRender` 在 `BeginTextureMode(m_sceneRT)` 中调用 `RenderSystem::render`。当前 `RenderSystem` 对任意离屏 target 设置 `offscreenV3SafeMode`，跳过 Lighting、HeightShadow、GI、Fluid 与 PostProcess。临时避黑帧分支使实际 Gameplay 画面没有执行高级渲染生产链。

本 Track 恢复完整、可回退的 Gameplay 离屏 HDR/GI 路径，并修复遮挡缓存、SDF `-0.0` 和时域 history 的正确性缺口。它落实 SPH 的历史 NO-GO 决策，防止流体作为 Ultra 默认功能或生产 GI 写入者出现。

## 2. 范围

### 必须交付

- 建立离屏 target 到内部 HDR scene 的输入、输出、坐标和状态恢复合同，移除按离屏类型跳过 pass 的逻辑。
- 在离屏 Gameplay 路径按 feature/tier 执行 Shadow、LightCulling、Lighting、HeightShadow、OccluderExtract、JFA、RadianceCascades、GIComposite、Volumetric、PostProcess 与 Composite。
- 建立独立 GI 输入：`OccluderMask`、`SdfField`、`EmissiveLights`、`EmissiveMaterials`、`EmissiveVfx`、`EmissiveCombined` 和 `GiHistory`。
- 将 camera target/zoom、viewport/extent、render scale 和遮挡内容版本纳入 OccluderMask 失效键。
- 以严格负内部距离或独立 occupancy 消除 `-0.0`；GPU readback 必须验证符号和 ray-stop。
- 采用 2D camera 重投影、尺寸/zoom/版本/disocclusion 拒绝治理 GI history。
- 所有 shipped Tier 默认关闭 SPH，开发构建只能经显式 opt-in 运行探索代码，且不可写入生产 GI 资源。

### 非目标

- 不实现 RenderGraph 编译器、自动重排、资源 aliasing、DRS、曝光或 Vulkan。
- 不实现 JFA dirty-region 算法，不恢复 SPH GO 路径或优化 SPH。

## 3. 跨系统合同

### 离屏输入输出

`m_sceneRT` 是外部 composite target，`RenderSystem` 不取得其释放所有权。渲染器保存 framebuffer、viewport、scissor 后将已绘制 Level 内容复制到 `HdrSceneColor`，在 HDR 工作目标完成色彩重写，并将最终 LDR 输出 blit 回原 external target。关闭功能或初始化失败时，原始 scene 仍须回写，不能黑帧。

离屏与 backbuffer 路径只在 input/output target descriptor 上不同；相同 tier 和 flag 下 pass 可用性、资源尺寸、GI 失效和顺序一致。`EndTextureMode` 后的 HUD、菜单和 overlay 保持原生分辨率，不参与 HDR 后处理。

### GI 资源和顺序

| 资源 | 生产者 | 消费者 | 生命周期 |
| --- | --- | --- | --- |
| `OccluderMask` | static/dynamic extract compose | JFA、history rejection | 当前帧 + version |
| `SdfField` | SeedInit/JFA/Distance | Radiance、history rejection | 当前帧或合法缓存 |
| `EmissiveLights` | LightManager 投影 | emissive merge | 当前帧 |
| `EmissiveMaterials` | PBR emission | emissive merge | 当前帧 |
| `EmissiveVfx` | VFX snapshot/prepass | emissive merge | 当前帧 |
| `EmissiveCombined` | emissive merge | Radiance | 当前帧 |
| `GiHistory` | GIComposite | 下一帧 GIComposite | persistent ping-pong |

目标顺序是 `External scene seed -> shadow/light preparation -> Lighting/HeightShadow -> VFX emission snapshot -> OccluderExtract -> JFA -> emissive merge -> Radiance -> GIComposite -> visible VFX/UIWorld -> Volumetric/PostProcess -> Composite`。VFX 可见绘制可以在 GI 后，但 emission snapshot 必须在 Radiance 前冻结。Fluid 不是上述资源的生产者。

### SDF 和时域语义

`OccluderViewKey` 至少含 camera、zoom、target viewport、SDF extent、render scale 和 occluder content version。任一字段变化必须重建或有明确安全重投影；静态/动态实体签名不能单独决定缓存命中。

掩码内部像素必须写入严格小于零的 SDF，推荐 `-max(distance, insideEpsilon)`；若使用 occupancy，Radiance 必须优先读取它停止射线。不得依赖 IEEE `-0.0 < 0.0`。

history 的 previous UV 由当前/前一帧二维 camera transform 推导。UV 越界、extent/zoom 不同、occluder/emissive/light version 不同或前后 occupancy 不一致时，history weight 必须为零。

### 回退

- `render.gi.enabled=false` 清除 GI history 并完整回退到 V4/HDR 直接光照路径。
- capability/shader 初始化失败时记录 feature-disabled 原因并回退，禁止静默当作正常离屏路径。
- shipped Tier 一律 `fluidEnabled=false` 且粒子数为零；Release 忽略开发 opt-in。

## 4. 验收标准

- [ ] `BeginTextureMode(m_sceneRT)` 集成场景中，High/Ultra 已启用 pass 全部出现在 trace，指定 ROI 非黑。
- [ ] 离屏/backbuffer 在相同 scene/tier 的指定 ROI 一致，任何格式量化差异有明确说明。
- [ ] camera 平移、zoom、resize、动态遮挡和动态 emissive 不会复用过期 mask/SDF/history。
- [ ] GPU readback 证明内部 SDF 严格为负且内部样本使 Radiance 停止。
- [ ] history reproject、disocclusion、版本与全局 invalidate 用例通过。
- [ ] SPH 不在 Release/High/Ultra 自动启动，也不写生产 GI 资源。
- [ ] 构建、相关 CTest、Release performance 通过；实机证据由后续硬件 Gate 复查。

## 5. 风险

- framebuffer blit 的坐标原点、format 与 rlgl 状态恢复可能黑帧/翻转，必须以外部 target fixture 覆盖。
- 2D history 没有完整 depth reproject；版本和 occupancy 拒绝优先保证正确性，宁可降低平滑。
- 本 Track 仍使用手工 GL barrier；新增 compute 写后必须有显式、测试覆盖的同步，下一 Track 再编译化。
