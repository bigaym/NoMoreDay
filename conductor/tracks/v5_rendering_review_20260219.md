# V5 GPU Rendering 全面回顾与重规划（2026-02-19）

> 范围：`设计文档/特效和UI/GPU_Rendering_System_V5.md` + `conductor/tracks/v5_*` + 当前渲染代码基线  
> 目标：校验设计一致性、工程可行性、关键算法优化方向，并给出可执行重规划

---

## 1. 结论摘要

1. **设计目标与 Track 拆分总体一致**：V5 被拆分为 `JFA -> Radiance Cascades -> SPH(探索) -> Gate`，方向正确。  
2. **当前工程基线已完成 V5-0 脚手架，可启动 Track 6/7**：原阻断项（合同、配置、ABI、格式能力）已在代码基线上收敛。  
3. **关键算法需做策略收敛**：  
   - JFA 是近似算法，应预设误差控制与可回退路径（JFA+2 或精确 EDT 分支）。  
   - Radiance Cascades 已从“全级固定 4 rays”调整为“角分辨率随级联增长”的 profile 策略，后续重点转为预算内参数整定。  
   - Holographic RC 当前应保持“研究分支”，不进入 V5 核心门禁路径。  
4. **可行性结论**：  
   - **JFA + 标准 RC（half-res/4-cascade 起步）可行**。  
   - **6-cascade full-res 需依赖严格降级和预算腾挪，不宜在 Phase A 绑定硬门禁**。  
   - **SPH 维持探索定位合理**。

---

## 2. 一致性审计（设计 vs Track vs 代码）

### 2.1 对齐项（通过）

1. V5 Track 依赖链与主控规格书一致：`6 -> 7 -> 9`，`8` 为探索输入不阻断核心发布。  
2. 设计文档中的核心门禁（JFA 精度、GI 可辨性、性能/稳定性/回退）在 Track 9 已有量化条目。  
3. 渲染架构仍保持 ECS + Render 子系统分离，满足既有架构约束。

### 2.2 已修正项（2026-02-19）

1. `conductor/tracks/v5_sph_fluid_exploration_20260219/index.md` 与 `.../spec.md` 已对齐到设计文档 `§5`。  
2. `conductor/archive/v5_validation_release_gate_20260219/index.md` 与 `.../spec.md` 已对齐到设计文档 `§12`。  
3. SPH `GPUFluidConfig` 已与设计文档附录一致（`vec2 gravity` + `surfaceTension` + `maxParticles`）。

---

## 3. 工程可行性审计（代码基线）

## 3.1 前置阻断项收敛情况（2026-02-19）

1. **RenderGraph V5 合同扩展**：已完成。  
   - 已纳入 `OccluderExtract/JFA/RadianceCascades/GIComposite` 资源与 owner 约束，`RENDERGRAPH_CONTRACT_VERSION=3`。  
2. **RenderProfiler V5 Pass 预算位**：已完成。  
   - 已增加 4 个 V5 pass id 与预算槽位。  
3. **RenderConfig/QualityTier 配置链路**：已完成。  
   - 已纳入 `render.gi.*` 与 `render.fluid.*` 字段、tier 默认与 settings 持久化读取。  
4. **ABI V5 升级脚手架**：已完成。  
   - `GPU_ABI_VERSION` 升级到 5，新增 `RadianceCascadeConfig/GPUFluidParticle/GPUFluidConfig`，manifest 与单测已对齐。  
5. **Framebuffer 纹理格式能力**：已完成。  
   - 已补齐 `R8/R16F/RG16UI` 路径，覆盖上传格式与显存统计。

## 3.2 预算风险（高）

1. V4 现有预算总和已紧张，V5 新增 pass 需要严格依赖“联合降级 + half-res + 帧间隔更新”。  
2. 因此建议将“4-cascade half-res 达标”设为核心里程碑，再推进 6-cascade full-res。

---

## 4. 关键算法联网对比与策略

## 4.1 JFA / EDT

1. **JFA 是高效近似**：原始工作强调其并行效率与实用性。  
2. **JFA+1 可显著减误差但不保证全局精确**。  
3. **PBA（Parallel Banding）为精确 EDT 路线**，是可选兜底/对照实现。

**规划建议**  
1. Track 6 以 JFA 为主线。  
2. 增加“参考精确 EDT 对照工具链”（可离线/测试路径），用于误差门禁。  
3. 将 `JFA+2` 和“精确 EDT 回退分支”作为异常场景兜底，不进入默认实时路径。

## 4.2 Radiance Cascades / Holographic RC

1. RC 论文仓库说明其为当前主线，并强调对泄漏/角落偏暗等问题的改进经验。  
2. 论文实现思路倾向于“随级联提升角分辨率”，而非全级固定 4 rays。  
3. Holographic RC 当前仍是前沿预印本与研究实现，工程成熟度低于标准 RC。

**规划建议**  
1. Track 7 采用“标准 RC 主路径 + HRC 研究分支”双轨。  
2. 将 rays-per-probe 改为可配置增长策略（例如按级联分档），并通过性能门禁裁剪。  
3. 将 HRC 的 GO/NO-GO 固定在 V5-B 后半阶段，不阻断发布门禁。

## 4.3 OpenGL 同步与内存可见性

1. Compute -> 后续采样/读取之间必须显式 `glMemoryBarrier`。  
2. 同一 dispatch 内若存在依赖，需使用 shader 内 `memoryBarrier()/barrier()` 模式。

**规划建议**  
1. 将 barrier 点纳入每个新 pass 的 AC。  
2. 在 Track 9 增加“barrier 审计清单”并配合 RenderDoc 检查。

---

## 5. 重规划（执行顺序）

## 5.1 V5-0（新增前置阶段，1~2 周）

目标：解除工程阻断，避免 Track 6/7 中途返工。

1. RenderGraph V5 合同扩展（新 pass stage/resource/owner + 测试更新）。  
2. RenderProfiler 增加 V5 pass id 与预算槽位。  
3. RenderConfig + QualityTier + settings.json 扩展 `render.gi.*`/`render.fluid.*`。  
4. FramebufferManager 增加 V5 所需格式与 resize 路径测试。  
5. ABI V5 脚手架（先结构声明与 manifest，锁定升级点）。

## 5.2 Track 6（JFA）

1. 先做全屏 full-recompute 正确性闭环。  
2. 再加 half-res + interval update。  
3. 最后做增量更新（允许保留 feature flag 作为实验态）。

## 5.3 Track 7（RC）

1. 先 4-cascade half-res + temporal，形成可验收主路径。  
2. 再扩 6-cascade full-res（Ultra only）。  
3. HRC 仅做独立评估，不并入核心 AC。

## 5.4 Track 8（SPH）

1. 维持探索，不绑定核心发布时间。  
2. GO 条件必须同时满足：稳定性 + 预算 + 资源释放。

## 5.5 Track 9（Gate）

1. 核心门禁仅锁定 JFA+RC 主路径。  
2. SPH 只要求决策闭环（GO 或 NO-GO 均可通过 gate）。

---

## 6. 风险更新

1. 新增风险：**V5-C01 RenderGraph 合同不先扩展导致实施阻断**（高）。  
2. 新增风险：**V5-C02 纹理格式/带宽超预算导致 6-cascade 不可达**（高）。  
3. 新增风险：**V5-C03 RC 角分辨率增长策略若裁剪过度，仍可能在远场产生方向欠采样**（中）。

---

## 7. 决策建议（Recommended）

1. **立即执行 V5-0 前置阶段**，再启动 Track 6。  
2. **将 4-cascade half-res 作为 V5 核心交付基线**。  
3. **HRC 与 SPH 都保持“可失败且不阻断发布”的研究路径**。  

---

## 8. 联网检索证据（关键来源）

1. Jump Flooding 原始论文（I3D 2006，近似并行算法）  
   - <https://doi.org/10.1145/1111411.1111431>
2. JFA 变体（含 JFA+1/JFA+2 误差改进讨论）  
   - <https://www.comp.nus.edu.sg/~tants/jfa-variants.html>
3. 精确 EDT GPU 路线（PBA, I3D 2010）  
   - <https://doi.org/10.1145/1730804.1730818>
4. Radiance Cascades 作者资料（级联角分辨率增长思路）  
   - <https://jason.today/rc>  
   - <https://radiance-cascades.com/>
5. Holographic Radiance Cascades（arXiv 2025）  
   - <https://arxiv.org/abs/2505.02041>
6. OpenGL 内存可见性 / barrier 规范来源  
   - <https://wikis.khronos.org/opengl/GlMemoryBarrier>  
   - <https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.60.pdf>
