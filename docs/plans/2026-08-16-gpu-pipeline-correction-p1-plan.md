# P1 计划：GPU 管线核心机制纠偏（Phase 1）

- 计划文件：`docs/plans/2026-08-16-gpu-pipeline-correction-p1-plan.md`
- 状态：**全部实施完成并通过双重审查 (COMPLETED & VERIFIED)**
- 上游设计：`docs/designs/2026-08-16-gpu-rendering-modernization-remediation-design.md`（v1.1，§3.2 Phase 1 核心机制纠偏 8 项 + §4 AD-1/AD-3/AD-4/AD-5 + §5 跨系统合同）
- 上游审查：`docs/reviews/2026-08-16-gpu-rendering-engine-modernization-audit-review.md`
- 参考门禁：M0-C `conductor/tracks/gpu_hardware_validation_gate_20260726/`（阶段后硬件验证）；M0-A `conductor/tracks/gpu_production_hdr_gi_closure_20260726/`（离屏 HDR/GI 合同为约束输入）；M1-D `conductor/tracks/gpu_jfa_incremental_update_20260726/`（JFA 相关）
- 范围边界：本计划只覆盖设计 §3.2 的 8 组纠偏。§3.3（P2：RenderGraph 现代化/解耦/AD-7/AD-8）见 `docs/plans/2026-08-16-gpu-rendergraph-modernization-p2-plan.md`。**默认与 P2 串行集成**（设计 §7 未决项 (1) 建议串行；若用户批准并行，需重新评估本计划 §5 的验收门禁）。
- 环境声明：本阶段交付为**代码 + 测试 + 文档**三类产物：T1-T8 实现与对应单元/集成/ci 测试均已落地并通过本地构建验证（本机 RTX 4070 SUPER）；性能项按 §4 表执行，`-C Release` 性能门禁在缺 Release 配置时以 RelWithDebInfo 实测并注明。**M0-C 无黑帧功能冒烟未执行（NOT_RUN，列为残余风险）**，不把 NOT_RUN 当作 GO；所有性能/渲染结论区分"本地已实测 → 数值/GO"与"未执行 → NOT_RUN/NO_GO"；禁止把未运行结论当作通过。

---

## 0. 与本计划相关的事实基线

| 事实 | 位置 |
| --- | --- |
| `GPUTextSystem::DispatchLayout`：layout 后同步 `counterBuffer.Read`，再 CPU `Update` indirect | `src/engine/render/GPUTextSystem.cpp:201-268`（:260 Dispatch，:264-266 `m_counterBuffer.Read` + `m_indirectBuffer.Update`） |
| Text 实际 getter | `src/engine/render/GPUTextSystem.hpp:51` `GetLastQuadCount()`（**设计写的是 `GetActiveQuadCount`，与源码不符**） |
| `GPULootSystem::Dispatch`：已有 `loot_indirect_args.compute`，但 :576 仍 `counterBuffer.Read` 同步回读，CPU 可见数门控 force-directed dispatch | `src/engine/render/GPULootSystem.cpp:515-630`（:516-522 Reset、:552-562 LootCull、:566-573 LootIndirectArgs 1,1,1、:576 Read） |
| Loot 实际 getters | `src/engine/render/GPULootSystem.hpp:30` `GetSyncedInstanceCount()`、`:36` `GetVisibleInstanceCount()` |
| 全局 binding 0..15 全占用；`kGlobalSharedSSBOBindings` 漏 10..13 | `src/engine/render/RenderConstants.hpp:18-58`（Binding 枚举）、`:67-80`（仅列 0,1,2,3,4,5,6,7,8,9,14,15） |
| `ShadowCS::kOccluderBinding` = 15，与 `SSBO_LOOT_INSTANCE` 冲突 | `src/engine/render/RenderConstants.hpp:293`；使用点 `ShadowBuildPass.cpp:131/168/531`、`OccluderExtractPass.cpp:222` |
| Binding observer 测试 | `tests/unit/RenderGraphValidationTest.cpp:850`（`CHECK_EQ(binding.point, ShadowCS::kOccluderBinding)`，:1235/:1536 一带为资源/图像绑定断言）、`tests/integration/GraphBindingEquivalenceGLTest.cpp:977`（`HashCounterBuffer`） |
| RC 现状：单 RGBA16F、单 SDF、farBlend | `assets/shaders/lighting/v5_radiance_cascade.comp:16`（r16f 单 SDF）、`:17`（rgba16f 单输出）、`:20`（`uParentRadiance` farBlend）、`:42`（24 步 trace）；`src/engine/render/passes/RadianceCascadesPass.cpp:683-708`（ResolveRaysPerProbe L0=8） |
| GI 现状：任意 light/emissive 变化全局 resetHistory，shader 无 clipBox | `src/engine/render/passes/GICompositePass.cpp:244-295`（:244-245 lightSignature、:273-277 occluderChanged、:282-287 emissiveChanged、:289-291 `resetHistory = ...`、:296-298 ping-pong） |
| S7a 测试 | `tests/integration/GIHistoryRejectionTest.cpp` |
| SPH 现状：全粒子 O(N²)、cellCoord binding 未用 | `assets/shaders/lighting/v5_fluid_neighbor_search.comp:46`（`for j<count` 全循环）、`:19-21`（CellCoords 声明未用）；输出合同 binding 3 neighborIndices / 4 neighborCounts |
| 粒子 4 处 `Barrier::All`，Loot Render 还有 1 处 | `src/engine/render/GPUParticleSystem.cpp:645/679/716/764`；`src/engine/render/GPULootSystem.cpp:685`；`src/engine/render/GPUParticleSystem.cpp:689` `m_subEmitCountBuffer.ReadFromSlot`（属 Particle 文件，与零回读联动） |
| `ComputeBuffer::Update`（非字面 glBufferSubData） | `src/engine/render/passes/ShadowBuildPass.cpp:338-344`、`src/engine/render/passes/OccluderExtractPass.cpp:173-179`；`src/engine/render/ComputeBuffer.hpp:103-106`（`Read` = `rlReadShaderBuffer`） |
| JFA 现状：interval skip 决策 + 每像素全局 atomic + overflow 同步读 | `src/engine/render/passes/JFAPass.cpp:700-732`（DecideUpdate :700-708、ApplyProductionUpdatePolicy :715、Skip 路径 :717-732）；`ReadOverflowCounter()` 为 `src/engine/render/passes/JFAPass.cpp:348-359`，Execute 的主路径/fallback 会调用；`assets/shaders/lighting/v5_jump_flood.comp:8-10`（overflow buffer）、`:71`（`atomicAdd(overflowCount, 1u)` 每像素） |
| 色彩现状：单一 ACES+sRGB 输出已存在 | `assets/shaders/postprocess/postprocess_combined.frag`（:26-33 ACESFilmic、:48 gamma 1/2.2、vignette+LUT） |
| 主线程回读盘点项 | `src/engine/render/GPUParticleSystem.cpp:554`（`m_atomicBuffer.Read` 60 帧节流）；`src/engine/render/passes/RadianceCascadesPass.cpp:345-351`（`m_particleCounterBuffer.Read`，当前无调用者）；`src/engine/render/passes/JFAPass.cpp:348-359`（overflow read）；`src/engine/render/GPUFlowFieldSystem.cpp:282-295` 的 `SyncToCPU()`，由 `src/game/systems/ai/AISystem.cpp:395` 每帧调用 |

---

## 1. 实施思路 / 原理

### 1.0 总体判断

P1 解决三类机制问题：(a) **每帧同步回读**（AD-1 间接参数生成，Text/Loot 两条主线）；(b) **绑定/ABI 治理**与 RC/GI/SPH 的**物理纠偏**（AD-3/4/5）；(c) **屏障与调度语义**（Barrier 精细化、JFA 原子雪崩、色彩线性化）。目标 G2（主线程零阻塞）在此阶段全面落地——**主线程零每帧同步回读**是本计划的第一验收标准。

### 1.1 组 1：AD-1 GPU 间接参数生成（Text + Loot）

- 现状（数据流）：`GPUTextSystem::DispatchLayout` 在 layout compute 后 `m_counterBuffer.Read`（`glGetBufferSubData` 同步回读 quad 数），CPU 据此构造 `DrawArraysIndirectCommand` 再 `Update` 到 indirect buffer——**每帧一次同步 stall**；`GPULootSystem::Dispatch` 已有 `assets/shaders/loot/loot_indirect_args.compute`（`GPULootSystem.cpp:565-574`），但 :576 仍 `counterBuffer.Read` 回读可见数来门控 force-directed 派发。
- 改法（AD-1）：Text 新增**单线程（1×1×1）** `indirect_args.compute`；Loot 复用已有 `loot_indirect_args.compute`，只改其输入/执行时机。两者都读 GPU atomic counter → 直接写现有数组间接 command buffer（Text 明确为 `DrawArraysIndirectCommand {6, quadCount, 0, 0}`；Loot 沿用其现有 command contract；GPU 独占写，CPU 仅初始化清零）。依赖用 `GL_COMMAND_BARRIER_BIT` 表达；command buffer 持久分配。
- **关键约束（源码事实 vs 设计）**：
  - 实际 getter 是 `GetLastQuadCount`（Text）、`GetVisibleInstanceCount`/`GetSyncedInstanceCount`（Loot），设计文本写的 `GetActiveQuadCount`/`GetLootCount` 不存在。本计划以源码 getter 为准，其语义按 §5 合同改为"滞留 ≥1 帧 GPU 统计快照"（debug 构建经 readback ring 每帧刷新；生产主线程零同步回读）。
  - 当前 `GPUTextSystem::Render`（`src/engine/render/GPUTextSystem.cpp:274-277`）和 `GPULootSystem::Render`（`src/engine/render/GPULootSystem.cpp:657-660`）仍以 CPU 计数为零而早退；移除这两个 count predicate，只保留资源/初始化/shader/VAO 守卫，让 GPU indirect command 的 `instanceCount=0` 自然 no-op。否则生产计数不再同步更新后会把有效绘制全部跳过。
  - Loot 的 force-directed dispatch 由 CPU 可见数门控 → 改为**读 GPU count 或保守 dispatch**：若 force 语义允许按 GPU 数派发则读 GPU counter（GPU 内分支），否则保守派发全量并靠 shader 内 `>= count` 早退。**CPU 只做初始化与 debug 滞后一帧统计 ring**。
  - **不扩大核心机制范围但闭合回读审计**：`GPUParticleSystem.cpp:554`、`RadianceCascadesPass.cpp:350`、`JFAPass.cpp:348-359` 与 `GPUFlowFieldSystem.cpp:282-295` 的回读不改变本组的间接参数算法；它们由 T6.5-T6.7 统一审计并完成生产禁用/异步滞后 ring/删除或测试隔离的处置，不能留作未声明的每帧同步例外。
- 屏障：layout → args → draw 之间用 SSBO/COMMAND barrier（见组 6 屏障细化）。

### 1.2 组 2：Binding/ABI 治理

- 现状：`RenderConstants.hpp` 全局 SSBO binding 0..15 全部被占用；`kGlobalSharedSSBOBindings` 遗漏 10..13（TRAIL_HEADERS/TRAIL_POINTS/MATERIAL_DATA/DISTORTION_DATA）；`ShadowCS::kOccluderBinding` 被定到 15，与 `SSBO_LOOT_INSTANCE` 冲突。
- 改法：
   1. **Shadow 阶段独占的本地 SSBO slot 0**：`ShadowBuildPass`/`OccluderExtractPass` 执行期把 occluder buffer 绑定到物理 slot 0，使用 pass-local logical name，**不加入 global table、不改 GL min binding contract**。同步 shader 声明 `assets/shaders/lighting/shadow_sdf.comp:5` 与 `assets/shaders/lighting/v5_occluder_extract.comp:5` 到 `binding = 0`；Loot shader 仍使用全局 slot 15。image 使用独立的 pass-local image unit namespace。这样既消除与 Loot 的冲突，又不扩张全局表。
      Graph binding registry 必须把该 alias 标记为 `phase_local_ssbo`，并在域校验中允许 slot 0 只在 Shadow phase 独占；离开 pass 后恢复之前的 binding，不能把 local alias 当作 global owner。
  2. 补全 global ledger：`kGlobalSharedSSBOBindings` 加入 10..13，补 `offsetof` 静态断言（GPUData.hpp 全结构）。
  3. **pass 边界显式重绑/恢复**：任何 pass 不得依赖"stale slot 恰好还绑着上个 pass 的资源"；进入/退出时按 graph 声明重绑。更新 binding observer 测试：`tests/unit/RenderGraphValidationTest.cpp:850/1235/1536`、`tests/integration/GraphBindingEquivalenceGLTest.cpp:977` 一带断言同步调整。
- 原因：binding 是 GL 状态的全局副作用，冲突会导致 Loot 或 Shadow 读到错误 buffer；这是上游审查列出的 binding/ABI 治理问题。

### 1.3 组 3：RC 方向性 Probe Atlas（AD-3）

- 现状：`v5_radiance_cascade.comp` 单 RGBA16F 输出、单 SDF（r16f `uDistanceField`）、farBlend 合并；`RadianceCascadesPass.cpp:683-708` 每 probe 每级 rays 数 L0=8。
- 改法（AD-3）：方向 Probe Atlas（`probeGridW×probeGridH×directions_k` 打包 RG16F 数组纹理，vec2(dirX,dirY)）；RTE 级联合并（空间双线性 + 角度最近扇区）；射线步进 `min(d_occluder, d_emissive)` 双 SDF；`GICompositePass`/`v5_gi_composite.comp` 做 L0 余弦加权辐照度汇聚 `E(x,n)=(1/N)Σ L0(x,ω_k)·max(0,n·ω_k)`，法线来自 HeightField，禁止额外角度平均。
- **设计文本内部矛盾（必须记录，作为实施前门禁）**：§4 AD-3 文字称"L0=8 扇区至 L5=2"，但公式 `directions_k = max(2, 16 >> k)` 给出 L0=16。**本计划不擅自选数值**：实施前须先修订设计消除矛盾（门禁 D1，见 §6），再按定稿数值展开。回退：扇区数 tier 化，Low/Medium 可退 1 扇区（High 4 扇区起步，§7 风险 1 的记录）。

### 1.4 组 4：GI 时域去噪（AD-4）

- 现状：`GICompositePass.cpp:289-291` 对 light/occluder/emissive 任一版本变化**全局** `resetHistory`；shader 无 clipBox。
- 改法（AD-4）：保留 ping-pong history（:296-298 既有结构不动）；3×3 邻域 mean/σ；`clipBox = mean ± 1.25σ`（γ=1.25 可调）；混合 α=0.88..0.92；光源/emissive 版本变化改为**局部失效**：先把现有世界空间 `lightRadius` 通过相机投影为屏幕像素半径，再将受影响光源屏幕 AABB 外扩 `2*projectedRadiusPx`，换算为 history UV 矩形并联合，只对该区域重置。保持 S7a `GIHistoryRejectionTest` accessor 合同不变。
- lightRadius 来源：从现有 light data（`LightManager`/GPULight 既有字段）取，**避免无依据扩大 ABI**——不新增 light 结构字段。
- 测试原理：相机匀速平移 + 单光源正弦调制，差分（被拒绝帧 vs 全重置帧）RMS 低于阈值，且无闪烁。

### 1.5 组 5：SPH 邻居搜索 O(N)（AD-5）

- 现状：`v5_fluid_neighbor_search.comp:46` 全粒子 O(N²) 循环；`cellCoord` binding 声明但未用。
- 改法（AD-5）：复用 `v5_fluid_gridhash.comp` 的 cell 编码；阶段 A 统计每 cell 粒子数，阶段 B 做前缀和（单工作组串行 scan **cell 计数**，规模 ≤ 纹理单元数 gridW×gridH，而非按粒子数；1080p/zoom=1、cellSize=18 时 grid 约 109×62≈6.8K cell），阶段 C 按 `cellStart[p] + atomicAdd(offset[cell],1)` compact；阶段 D 查询自身 + 邻桶。**设计内部矛盾记录**：§3.2-5 写“27 邻桶”，但 AD-5 定稿写“自身 + 8 邻 cell”，且当前 shader 是 **2D**（`v5_fluid_neighbor_search.comp` 用 vec2 坐标）；以 AD-5 定稿为依据，**D2 已定稿**：2D 邻桶 = 9（自身 + 8 Moore 邻 cell），grid hash cellSize == 粒子交互半径（radius==cellSize==18.0）故 9-cell 覆盖完整交互邻域。Density/Force/Integrate 输出合同不变（binding 3 neighborIndices / 4 neighborCounts）；SPH 保持 shipped NO-GO / dev opt-in（`render.fluid` 开关）。
- 性能目标：4096 粒子邻居内核 ≤0.3ms（对比 O(N²) 基线）。

### 1.6 组 6：屏障精细化

- 现状：`GPUParticleSystem.cpp:645/679/716/764` 四处 `Barrier::All`，`GPULootSystem.cpp:685` Render 另有一处；`GPUParticleSystem.cpp:689` `ReadFromSlot` 与 P1 零回读联动。
- 改法：按各点消费者改精确屏障：
  - :645（update 后、emit 前）→ 消费方是 emit：SSBO barrier（+ 视需要 COMMAND）；
  - :679（emit 后、sub-emit 前）→ 消费方是 sub-emit/readback：SSBO；debug ring 的 CPU 读由 fence 就绪度闸门，不依赖 `Barrier::CLIENT`；
  - :716（sub-emit 后）→ SSBO；
  - :764（finalize 后 render）→ SSBO + COMMAND。
  - `GPUParticleSystem.cpp:689 ReadFromSlot`：与"主线程零每帧同步回读"联动——**必须先决定移除或保留为 debug-only delayed ring**（决策项 D5，见 §6）；本计划默认改为 debug-only 滞后帧 ring。
- `src/engine/render/passes/ShadowBuildPass.cpp:338` 与 `src/engine/render/passes/OccluderExtractPass.cpp:173`：实为 `ComputeBuffer::Update`（CPU 侧写）而非字面 `glBufferSubData`；目标改为 `PersistentBuffer` **三缓冲**（CPU 写 buffer + GPU 读 buffer 轮换），验证 owner/resize（buffer 重建路径不破坏三缓冲索引）。

### 1.7 组 7：JFA 空场景原子雪崩

- 现状：`assets/shaders/lighting/v5_jump_flood.comp:71` 每像素对全局 `overflowCount` 做 `atomicAdd`（空场景全屏 N 像素 × 每像素一次原子写 → 无意义原子风暴）；`JFAPass` 目前按 interval skip 决策（:700-732 已有 Skip 路径）。
- 改法：host 依据**滞后一帧**的 occluder count==0 整帧跳过（`dispatch=0`）；非空场景改 workgroup 共享内存规约（每组一次原子写）。保持 `conductor/tracks/gpu_jfa_incremental_update_20260726/` 的 verification/full fallback 与既有 JFA 测试。
- 测试原理：空场景帧 trace 断言 `dispatchTexelCount==0` 且无全局 atomicAdd 流量；非空场景结果与基准 JFA 一致。

### 1.8 组 8：色彩空间线性化

- 现状（源码事实）：`postprocess_combined.frag` **已是**单一 ACES+sRGB（gamma 1/2.2）输出，HDR 中间目标大多 RGBA16F。
- 缺口：输入纹理缺少 sRGB decode/linear flag——`TextureArrayManager` RGBA8 纹理无法区分"albedo（需线性化）"与"data/UI/emissive（已是线性）"，存在重复 gamma 风险。
- 改法：给纹理声明加 linear flag（albedo 类输入线性化一次）；`render.color.linearPipeline` 灰度开关（分项开关，便于 golden image 对比与回退）；golden image 全链路单一 Gamma 验证。
- 注意：**不得重复 gamma**——线性化只发生在采样入口，输出侧只经 `postprocess_combined.frag` 一次。

---

## 2. 伪代码引导（不可编译草图）

### 组 1：Text 间接参数

```
// GPUTextSystem
// 1) layout compute（现有 :201-268 前半不变）：GPU 写 counter（quad count）
// 2) 新增 m_indirectArgsShader（1,1,1，持久分配 command buffer）：
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
layout(std430, binding = 0) readonly buffer Counter { uint quadCount; };
layout(std430, binding = 1) writeonly buffer Cmd {
    uint count; uint instanceCount; uint first;
    uint baseInstance;                    // DrawArraysIndirectCommand
} cmd;
void main() {
    uint n = quadCount;
    cmd.count = 6u;                // 六个顶点组成一个 glyph quad
    cmd.instanceCount = n;         // 一个 instance 对应一个 GPUTextQuad
    cmd.first = 0u; cmd.baseInstance = 0u;
}
// 3) 渲染：GPUUtils::DrawArraysIndirect(...) 直读 cmd buffer
// 4) getter：GetLastQuadCount() 语义 = 滞留 ≥1 帧统计快照（debug ring 刷新，生产不读）
// 屏障：layout → args：SSBO；args → draw：COMMAND
```

### 组 1b：Loot force dispatch 去 CPU 门控

```
// GPULootSystem::Dispatch
// 复用既有 assets/shaders/loot/loot_indirect_args.compute；:576 counterBuffer.Read 删除。
// Render 不再以 CPU visible count 早退，instanceCount=0 的 GPU command 自然 no-op。
// force 语义改为：
if (forceDirected) {
    // 方案 A（GPU 分支）：cull shader 内读可见数，force 逻辑 GPU 侧完成
    // 方案 B（保守）：按实例全量派发，shader 内 `if (id >= visibleCount) return;` 早退
}
// debug 统计：滞后一帧 readback ring 刷新 GetVisibleInstanceCount()
// 生产主线程零同步回读（ComputeBuffer::Read 测试缝计数 + 静态审计佐证）
```

### 组 2：Shadow 本地 slot 0 + ledger 补全

```
// ShadowBuildPass/OccluderExtractPass：pass 内本地绑定
constexpr uint32_t kLocalOccluderSlot = 0u;   // 物理 slot 0；pass-local，不入 global table
// shadow_sdf.comp 与 v5_occluder_extract.comp 的 SSBO 声明同步为 binding = 0
// 不使用 RenderConstants::SSBO_RESERVED_15 / 不动 GL min binding contract
// RenderConstants.hpp:67-80 kGlobalSharedSSBOBindings 补 {10, 11, 12, 13}
// GPUData.hpp 全结构 offsetof static_assert
// pass 进入时 rlBindShaderBuffer(kLocalOccluderSlot, occluderBuf)
// 退出时恢复（不依赖 stale slot）
```

### 组 3：RC 方向 Probe（AD-3，门禁 D1 通过后）

```
// v5_radiance_cascade.comp 输出改 array texture（方向维）
layout(rg16f, binding = 5) uniform writeonly image2DArray uRadianceAtlas; // [probeGridW][probeGridH][directions_k]
// 级联合并（RTE）：
vec3 radiance(level k, probe p, dir ω) {
    t = marchRay(p, ω, min(dOccluder, dEmissive));   // 双 SDF
    return ∫0^t e^(−σs) E(s) ds + e^(−σt) · L_{k+1}(p + tω, ω);
}
// 辐照度汇聚在 GIComposite（不在 cascade shader）：
E(x, n) = (1/N) Σ_k L0(x, ω_k) · max(0, n·ω_k);   // n 来自 HeightField，禁止额外角度平均
// directions_k 数值：实施前按设计修订定稿（矛盾见 §6 D1），不擅自取值
```

### 组 4：GI 局部失效（AD-4）

```
// GICompositePass
resetHistory 不再因 light/emissive 版本变化而全局置位
clipRects = union of (受影响光源屏幕 AABB 外扩 2*projectedRadiusPx)
// projectedRadiusPx = ProjectWorldRadiusToPixels(lightRadius, camera, lightDepth)
// shader 内：
if (inClipRect(uv)) {   // 局部重置该区域
    history = currentFrame; alpha = 1.0;
} else {
    m = 3x3Mean(history); σ = 3x3Sigma(history);
    clipBox = m ± 1.25σ;
    history = clamp(currentFrame, clipBox);
    color = mix(history, currentFrame, α);   // α ∈ [0.88, 0.92]
}
// lightRadius 从既有 light data 取，不扩 ABI
```

### 组 5：SPH O(N)（AD-5，门禁 D2 通过后）

```
// pass A：count per cell（复用 v5_fluid_gridhash cell 编码）
// pass B：前缀和（单工作组串行 scan cell 计数，规模 ≤ 纹理单元数 gridW×gridH，而非粒子数）
// pass C：compact —— cellStart[p] + atomicAdd(offset[cell], 1)
// pass D：邻居查询（自身 + 邻桶；D2 已定稿 2D → 9 邻 = 自身 + 8 Moore 邻 cell）
for (邻桶 ∈ 候选桶) for (j ∈ 桶内粒子) if (dist2 < h2) 写入 neighborIndices/neighborCounts
// Density/Force/Integrate 输出合同不变；shipped NO-GO，dev opt-in
```

### 组 6：屏障与三缓冲

```
// GPUParticleSystem 各点
:645  MemoryBarrier(Barrier::SSBO)
:679  MemoryBarrier(Barrier::SSBO)                     // CPU ring 读由 fence 就绪度闸门
:716  MemoryBarrier(Barrier::SSBO)
:764  MemoryBarrier(Barrier::SSBO | Barrier::COMMAND)  // render 前
// GPUParticleSystem.cpp:689 ReadFromSlot → debug-only delayed ring（决策 D5）
// ShadowBuildPass/OccluderExtractPass：PersistentBuffer 三缓冲
struct TripleBuffer { uint cpuWrite; std::array<uint,3> gpuRead; uint ring; };
update(): 写 cpuWrite → 轮换 gpuRead[ring++ % 3] → 派发
// 验证 owner/resize：重建路径同步轮换索引
```

### 组 7：JFA host skip

```
// JFAPass
uint64_t occluderCount = m_occluderCountRing.LatestSnapshot();  // 滞后一帧
if (occluderCount == 0) { decision.mode = Skip; dispatchTexelCount = 0; return; }
// 非空：v5_jump_flood.comp 改 workgroup 共享内存规约，每组单次 atomicAdd
// 保持 M1-D verification / full fallback
```

### 组 8：色彩线性化

```
// TextureArrayManager：纹理项加 bool linear（albedo=true；data/UI/emissive=false）
// 采样：albedo 纹理入口 linearize（sRGB→linear）；其他保持
// render.color.linearPipeline 灰度开关：false → 现行为（不回退到双 gamma，仅关线性化入口）
// golden image：全链路单 Gamma（入口一次线性化 + 出口一次 sRGB）
```

---

## 3. 原子任务拆分

状态：`[ ]` 未开始；`[~]` 进行中；`[x]` 完成。依赖：组 1/2 是主线程零回读与 binding 治理的核心，先行；组 3/4 依赖组 2 的 image/binding 治理结果；组 5 依赖设计 D2 定稿；组 6 的 `GPUParticleSystem.cpp:689` 决策（D5）及 T6.5-T6.7 回读审计先行于阶段零回读验收；组 7 独立；组 8 独立可并行。

### 组 1：AD-1 间接参数
- [x] T1.1 `GPUTextSystem` 新增 `indirect_args.compute`（1×1×1）与 GPU command buffer（持久分配，CPU 仅初始化清零）；args shader 的 slot 0/1 按 `phase_local_ssbo` 纪律登记，进入前显式重绑、退出后恢复，不覆盖 global owner
- [x] T1.2 删除 `GPUTextSystem.cpp:264-266` 同步 `m_counterBuffer.Read` + `m_indirectBuffer.Update`；保留现有 `GPUUtils::DrawArraysIndirect`，改为直读 GPU 写入的 `DrawArraysIndirectCommand`
- [x] T1.3 移除 `GPUTextSystem.cpp:274-277` 与 `GPULootSystem.cpp:657-660` 的 CPU count 早退；零实例由 GPU indirect command 表达，资源/初始化/shader/VAO 守卫保持
- [x] T1.4 `GetLastQuadCount` 语义改滞留 ≥1 帧统计快照（debug 经 ring 刷新）
- [x] T1.5 `GPULootSystem` 复用既有 `loot_indirect_args.compute`；删除 `:576` 同步 `counterBuffer.Read`；force 语义改 GPU count 分支或保守派发
- [x] T1.6 `GetVisibleInstanceCount`/`GetSyncedInstanceCount` 改快照语义（debug ring）
- [x] T1.7 新增集成测试：通过 `ComputeBuffer::Read` 测试缝断言 layout/loot/render 路径无同步 read，且 indirect draw 命中 GPU 写 command；`GPUResourceRegistry` 只做生命周期断言
- [x] T1.8 性能基线与回归（分项：Text pass、Loot pass P95）

### 组 2：Binding/ABI
- [x] T2.1 `ShadowCS::kOccluderBinding` 迁 Shadow 本地物理 slot 0（不入 global table、不改 min binding contract），同步 `assets/shaders/lighting/shadow_sdf.comp:5`、`assets/shaders/lighting/v5_occluder_extract.comp:5` 的 SSBO `binding = 0`，并在 Graph binding registry 登记 `phase_local_ssbo` alias
- [x] T2.2 `kGlobalSharedSSBOBindings` 补 10..13；GPUData.hpp 全结构 offsetof 断言
- [x] T2.3 pass 边界显式重绑/恢复（Shadow/Occluder/消耗 10..13 的 pass），并让 registry 域校验区分 local alias 与 global owner
- [x] T2.4 更新 `tests/unit/RenderGraphValidationTest.cpp:850/1235/1536`、`tests/integration/GraphBindingEquivalenceGLTest.cpp:977` 断言；逐一检索并更新 `ShadowCS::kOccluderBinding` 的实际使用点（`src/engine/render/passes/ShadowBuildPass.cpp:131/168/531`、`src/engine/render/passes/OccluderExtractPass.cpp:222`），不把 HashCounterBuffer 断言误当作 Shadow binding 断言
- [x] T2.5 新增冲突回归测试：Shadow 与 Loot 同帧执行互不污染

### 组 3：RC 方向性（门禁 D1 通过后）
- [x] T3.1 设计修订定稿 directions_k（D1 未通过不实施）——**D1 已定稿**：`directions_k = max(2, 16>>k)`，Ultra L0=16…L5=2；High L0=4（`max(2, 4>>k)`）；Low/Medium=1（见 `RadianceCascadesPass::ResolveRaysPerProbe`，`src/engine/render/passes/RadianceCascadesPass.cpp:740-751`）。已同步记录至设计文档 AD-3。
- [x] T3.2 `v5_radiance_cascade.comp` 改方向 Probe Atlas（RG16F 数组纹理）+ RTE 合并 + 双 SDF min 步进
- [x] T3.3 `RadianceCascadesPass.cpp:683-708` 参数与 Atlas 布局同步
- [x] T3.4 `GICompositePass`/`v5_gi_composite.comp` L0 余弦加权辐照度汇聚（法线取 HeightField）
- [x] T3.5 集成测试：点光源 45° 侧照方向性溢色用例 + 余弦加权积分用例；Low/Medium 回退扇区验证

### 组 4：GI 时域去噪
- [x] T4.1 移除全局 resetHistory 的 light/emissive 触发（保留 extent/history 无效触发）
- [x] T4.2 3×3 mean/σ + clipBox(±1.25σ) + α∈[0.88,0.92] 混合（shader）
- [x] T4.3 将既有世界空间 `lightRadius` 投影为屏幕像素半径，再以 `2*projectedRadiusPx` 外扩屏幕 AABB 并转为 history UV 局部失效矩形
- [x] T4.4 保持 S7a `GIHistoryRejectionTest` accessor 合同；新增正弦调制差分 RMS 用例与闪烁观察用例

### 组 5：SPH O(N)（门禁 D2 通过后）
- [x] T5.1 设计修订确认 2D/3D 邻桶（9 vs 27 候选）
- [x] T5.2 count + prefix + compact 三阶段，再执行邻居查询（自身+邻桶）
- [x] T5.3 Density/Force/Integrate 合同不变性验证
- [x] T5.4 性能：4096 粒子邻居内核 ≤0.3ms（`tests/performance/FluidSimulationBenchmark.cpp`）

### 组 6：屏障精细化
- [x] T6.1 决策 D5：`GPUParticleSystem.cpp:689 ReadFromSlot` 移除或改 debug-only delayed ring
- [x] T6.2 四处 `GPUParticleSystem::Barrier::All` 与 `src/engine/render/GPULootSystem.cpp:685` 的 Loot Render `Barrier::All` 改按消费者 SSBO/COMMAND；CPU ring 读不以 `Barrier::CLIENT` 代替 fence
- [x] T6.3 `src/engine/render/passes/ShadowBuildPass.cpp:338`/`src/engine/render/passes/OccluderExtractPass.cpp:173` 迁 PersistentBuffer 三缓冲（验证 owner/resize）
- [x] T6.4 回归：粒子全流程与 Shadow/Occluder 结果一致性 + 无同步回读断言
- [x] T6.5 审计粒子/GI 回读：`GPUParticleSystem.cpp:554` 的 60 帧 `m_atomicBuffer.Read` 改为生产禁用、debug-only 异步滞后 ring；`RadianceCascadesPass::ReadParticleCounter`（`src/engine/render/passes/RadianceCascadesPass.cpp:345-351`）当前无图谱调用者，删除未使用同步读取或保留为仅测试入口；不得以未调用函数掩盖零回读合同
- [x] T6.6 审计 JFA overflow 回读：`JFAPass::ReadOverflowCounter`（`src/engine/render/passes/JFAPass.cpp:348-359`，Execute 主路径与 fallback 调用）改为 debug-only 异步滞后 ring；fallback 判定使用最近 ready 快照，旧快照只会保守地触发安全 fallback，不允许当前帧同步 `Read`
- [x] T6.7 审计 flow-field 回读：`GPUFlowFieldSystem::SyncToCPU`（`src/engine/render/GPUFlowFieldSystem.cpp:282-295`，由 `src/game/systems/ai/AISystem.cpp:395` 每帧调用）改为至少一帧滞后的双缓冲/fence ring；AI 使用最近 ready 的 CPU 快照，初始化/未 ready 时走已有安全快照，不阻塞主线程

### 组 7：JFA
- [x] T7.1 host 滞后一帧 occluder count==0 → dispatch=0 skip
- [x] T7.2 `assets/shaders/lighting/v5_jump_flood.comp:71` 改 workgroup 共享内存规约单次原子写
- [x] T7.3 空场景 trace 断言（dispatch=0、无全局 atomic）；非空场景与基准 JFA 一致；M1-D verification/full fallback 保持

### 组 8：色彩
- [x] T8.1 TextureArrayManager 纹理 linear flag（albedo vs data/UI/emissive）
- [x] T8.2 采样入口线性化（一次，不重复 gamma）
- [x] T8.3 `render.color.linearPipeline` 灰度开关 + golden image 用例
- [x] T8.4 回归：UI/粒子 emissive 颜色不变（避免二次 gamma 观感漂移）

### 集成组
- [x] T9.1 全量 build + unit/integration/ci 全绿
- [x] T9.2 主线程零每帧同步回读验证（`ComputeBuffer::Read` 测试缝计数、`GPUUtils::GetBufferSubData`/`PersistentBuffer::ReadFromSlot` 静态审计 + 代码审查双证）
- [ ] T9.3 无黑帧 functional 冒烟；M0-C 门禁执行 —— **NOT_RUN：无黑帧冒烟与 M0-C 门禁未执行，列为残余风险**（§5.2 NOT_RUN ≠ GO；不伪造证据）

---

## 4. 测试方法

| 层级 | 覆盖 | 命令 |
| --- | --- | --- |
| unit | T2.2（offsetof）、T1.6 中无 GL 部分、T3/T4 中参数化逻辑 | `.\build.bat` + `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` |
| integration | T1.3/T1.4/T1.6 快照语义、T1.7、T2.4/T2.5、T3.5、T4.4、T6.4-T6.7、T7.3、T8.4 | `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` |
| ci | 全量门禁 + ABI layout 快照强校验 | `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` |
| performance | T1.8、T5.4、GI 差分、JFA 空场景 | `ctest --test-dir build -C Release -L performance --output-on-failure` |
| functional | 无黑帧、色彩 golden image 视觉审查 | 手动 |
| 脚本检查 | 模块边界/旧特性回潮 | `python scripts/check_module_boundaries.py`、`python scripts/check_legacy_reintroduction.py` |

关键点：

- **零同步回读证据**：GL debug 回调不会报告客户端侧 `glGetBufferSubData` 调用；在 `src/engine/render/ComputeBuffer.hpp:103-106` 的 `ComputeBuffer::Read` 增加生产零成本测试计数 seam，断言 Text/Loot/Particle/GI/JFA/flow-field 的每帧路径调用次数为 0（仅 debug 统计 ring 在非渲染路径允许）。同时静态审计 `GPUUtils::GetBufferSubData` 与 `PersistentBuffer::ReadFromSlot` 调用点，辅以代码审查双证。`GPUParticleSystem.cpp:554` 不得以“每 60 帧”作为生产例外；`RadianceCascadesPass::ReadParticleCounter` 无调用者，按 T6.5 删除其同步读取或隔离为测试入口。
- **ABI**：Text/Loot command 是各自的 16B draw-command contract，不声称由 `gpu_abi.glslinc` 生成；用 C++/GLSL `sizeof`/字段偏移快照锁定，只有共享 GPU 数据结构才走 `generate_gpu_abi.py`，禁止手写镜像漂移。
- 性能命令使用 `-C Release` 以保证 Tracy/GPU frame 可比；Release 构建需显式产出（build.bat 单配置构建）；常规 `build.bat` 与 unit/integration/ci 仍按 RelWithDebInfo，禁止用 Debug 结果替代。
- 本交付纯文档：**跳过构建与测试**；实施阶段按表执行并留存输出。

---

## 5. 验证任务完成 / 退出标准

### 5.1 逐组退出标准

| 组 | 证据 | 阈值 |
| --- | --- | --- |
| 1 AD-1 | 集成测试 + Read 测试缝计数 + 性能分项 | 主线程零每帧同步回读；Text/Loot indirect draw 正确（GPU 写 command） |
| 2 Binding | 更新后测试通过 | 全局 ledger 0..15 全登记；Shadow 本地 slot 0 不冲突；pass 边界显式重绑 |
| 3 RC | 方向性用例通过 | 点光源 45° 侧照方向性溢色 + 余弦加权积分用例通过；扇区回退可用 |
| 4 GI | 差分 RMS 用例通过 | 正弦调制差分 RMS 低于阈值、无闪烁；S7a accessor 合同不变 |
| 5 SPH | 性能基准 | 4096 粒子邻居内核 ≤0.3ms；输出合同不变 |
| 6 屏障/回读审计 | 回归 + 无回读断言 | 精确屏障、三缓冲 owner/resize 正常、Particle/Loot 无 `Barrier::All` 残留；JFA 与 flow-field 均无生产同步回读 |
| 7 JFA | trace 断言 | 空场景 dispatch=0 且无全局 atomic；非空结果与基准一致 |
| 8 色彩 | golden image | 单一 gamma 链路；UI/emissive 观感无漂移 |

### 5.2 阶段级退出标准

1. **主线程零每帧同步回读**（`ComputeBuffer::Read` 测试缝计数、`GPUUtils::GetBufferSubData`/`PersistentBuffer::ReadFromSlot` 静态审计 + 代码审查双证）。
2. **累计性能复验**：LightCulling + Text + Loot 三项相对 P0 前分项基线的 P95 合计下降 **≥1.0ms**（Valid GPU；本地无硬件记 NOT_RUN）。
3. RC 方向性 + 余弦积分用例通过（真实 GPU）。
4. SPH 4096 邻居 ≤0.3ms（真实 GPU；本地无硬件记 NOT_RUN）。
5. GI sine light modulation RMS 达标、无闪烁。
6. 空 JFA dispatch=0、无 atomic trace。
7. 单一 gamma golden image 通过。
8. `.\build.bat` 成功；unit/integration/ci 三组全绿；M0-C 门禁执行（阶段后证据；NO_GO 不视为 GO）。

---

## 6. 设计/源码不一致清单与本阶段门禁

### 6.1 不一致清单

| # | 设计文本 | 仓库事实 | 处理 |
| --- | --- | --- | --- |
| D1 | AD-3 文字"L0=8..L5=2" vs 公式 `max(2,16>>k)` 得 L0=16 | 两者矛盾 | **已定稿（门禁通过）**：`directions_k = max(2, 16>>k)` → Ultra L0=16…L5=2；High L0=4；Low/Medium=1（`ResolveRaysPerProbe`）。设计 AD-3 已同步修订 |
| D2 | §3.2-5 写 27 邻桶、AD-5 定稿写自身 + 8 邻 cell | 当前 `v5_fluid_neighbor_search.comp` 是 2D，邻桶候选为 9 | **已定稿（门禁通过）**：2D 邻桶 = 9（自身 + 8 Moore 邻 cell）；grid hash cellSize == 粒子交互半径（radius==cellSize==18.0）故 9-cell 覆盖完整交互邻域。设计 §3.2 Phase 1 项 5 已同步修订 |
| D3 | 设计写 `GetActiveQuadCount`/`GetLootCount` | 实际 `GetLastQuadCount`（`GPUTextSystem.hpp:51`）、`GetVisibleInstanceCount`/`GetSyncedInstanceCount`（`GPULootSystem.hpp:30/36`） | 以源码 getter 为准，语义按 §5 合同改快照 |
| D4 | 设计把 ShadowBuild/Occluder 描述为"裸 glBufferSubData" | 实为 `ComputeBuffer::Update` | 迁移目标是 PersistentBuffer 三缓冲，语义不变 |
| D5 | 设计未明确非 Text/Loot 的主线程计数/flow-field 回读处置 | `GPUParticleSystem.cpp:554` 每 60 帧同步 `m_atomicBuffer.Read`；`RadianceCascadesPass::ReadParticleCounter`（`:345-351`）图谱入度为 0；`JFAPass::ReadOverflowCounter` 与 `GPUFlowFieldSystem::SyncToCPU` 走生产路径 | P1 审计闭环：生产禁用同步读，debug 仅异步滞后 ring；RC helper 删除或隔离测试；JFA/flow-field 使用 ready-fence 快照；零回读验收不留未声明例外 |
| D6 | AD-4 occluder mask 版本变化处置（实施期决策） | 实现采用局部 dirty-rect 失效（`GICompositePass.cpp:368-394`）：投影 occluder 屏幕边界（prev+curr）→ UV 矩形；边界为空时保守全屏失效（`:377-381`）；不再全局 resetHistory（`:228` 仅 history 无效/extent 变化才重置） | **已定稿（实施期记录）**：occluder mask 版本变化 → 局部 dirty-rect 失效，无全局 resetHistory |
| D7 | AD-4 VFX emissive 版本变化处置（实施期决策） | VFX emissive 快照烘焙进单一合并 emissive 纹理，**无逐源世界坐标可用**，无法局部投影（`GICompositePass.cpp:396-403`） | **已定稿（实施期记录）**：emissive 版本变化 → 保持文档化全屏失效兜底（显式设计决策） |

### 6.2 本阶段门禁（阻塞项）

- **G-P1-1**：P0 全部验收通过（P0 计划 §5）。
- **G-P1-2**：设计算法未重新选型（AD-1/3/4/5 仍有效）；若审查/验证推翻任一 AD，回到设计流程。
- **G-P1-3**：D1（RC 扇区数）与 D2（SPH 维度）设计修订定稿。——**已通过**：D1 定稿 `directions_k = max(2, 16>>k)`（Ultra L0=16…L5=2，High L0=4，Low/Medium=1）；D2 定稿 2D 9 邻桶（自身 + 8 Moore 邻 cell，cellSize==交互半径 18.0）。见 §6.1 D1/D2 行。

---

## 7. 风险 / 回读审计边界

- **回读审计边界**：T6.5-T6.7 已纳入 `src/engine/render/GPUParticleSystem.cpp:554`、`src/engine/render/passes/RadianceCascadesPass.cpp:345-351`、`src/engine/render/passes/JFAPass.cpp:348-359`、`src/engine/render/GPUFlowFieldSystem.cpp:282-295`。前两者生产禁用或删除/测试隔离；JFA/flow-field 仅走 ready-fence 的滞后快照。若未来新增同步读调用者，必须先更新设计/计划，不得恢复主线程同步回读。
- **AD-3 Atlas 显存**：方向 Atlas 增 +18MB 量级寄存器/显存压力 → 扇区数 tier 化（High 4 起步），M0-C 多驱动验证。
- **三缓冲迁移**：PersistentBuffer resize/owner 变更路径易碎，用 owner/resize 单测锁定。
- **并行风险**：若用户批准 P1/P2 并行（设计 §7 未决项 (1)），本计划 §5.2 与 P2 计划的集成门禁需重排。
- 全部性能量化依赖真实 GPU；本地无硬件阶段记录 NOT_RUN，不编造结果。
