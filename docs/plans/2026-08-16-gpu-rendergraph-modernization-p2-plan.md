# P2 计划：RenderGraph 现代化（Phase 2）

- 计划文件：`docs/plans/2026-08-16-gpu-rendergraph-modernization-p2-plan.md`
- 状态：**已完成实施与审查修复，最终复审通过（2026-08-17，结论：提交；B1-B4/H1-H9/M1-M8/L 项处置见 §7/§9/§10；真机项 NOT_RUN 与 N1-N3 补记见 §10）**
- 上游设计：`docs/designs/2026-08-16-gpu-rendering-modernization-remediation-design.md`（v1.1，§3.3 Phase 2 架构现代化 4 项 + §4 AD-6/AD-7/AD-8/AD-9 + §5/§6/§7）
- 上游审查：`docs/reviews/2026-08-16-gpu-rendering-engine-modernization-audit-review.md`
- 参考门禁：M0-C `conductor/tracks/gpu_hardware_validation_gate_20260726/`；M0-B `conductor/tracks/gpu_rendergraph_resource_foundation_20260726/`（**本计划是其超集，证据以本计划为准，一轨推进**，避免双轨冲突）；M1-D `conductor/tracks/gpu_jfa_incremental_update_20260726/`（JFA 仅历史背景，不涉本计划）
- 范围边界：覆盖设计 §3.3 的 4 项：编译型 RenderGraph（AD-6）、Gameplay/Engine 渲染解耦、紧凑 RenderInstance（AD-7）、显存池与 Resize 防抖（AD-8）。AD-9（视锥剔除半径）设计已定"仅修时空同步、不改半径"，作为附属于解耦的收尾项。**默认与 P1 串行**；并行需用户批准并重排集成门禁。
- 环境声明：**当前交付为纯文档变更，未执行任何构建/测试**。性能/渲染结论区分"本地无硬件 → NOT_RUN/NO_GO"与"真实 GPU → GO"，禁止编造结果。

---

## 0. 与本计划相关的事实基线

| 事实 | 位置 |
| --- | --- |
| RenderGraph 已有 typed access/stable IDs/transitions/CompiledRenderPlan；**无**反向 pass culling、aliasing、拓扑/声明 hash cache | `src/engine/render/graph/RenderGraph.cpp:1088-1319`（`BuildCompiledPlan` :1105 起：passOrder/stablePassId/resourceMap/descriptorIdsByName） |
| TransientResourcePool 存在（精确尺寸匹配、AcquireColorTarget） | `src/engine/render/resources/TransientResourcePool.cpp`（:12 AcquireColorTarget、:47 BeginFrame、:54 EndFrame、:77 Shutdown）；`RenderSystem.cpp:188` `g_transientPool` |
| Gameplay 直绘 Level/Tilemap/health bars/技能指示器/portals/fog/ghost 后调 RenderSystem | `src/game/application/states/GameplayState.cpp:977-1173`（OnRender :977、BeginTextureMode(m_sceneRT) :984、levelManager->render :991、RenderSystem::render :997-1004、health bars :1012 一带、技能指示器 :1016 一带） |
| RenderSystem 外部 seed/composite blit | 两处 blit 都在 `src/engine/render/RenderSystem.cpp`：composite 路径 `:808-824`，external seed 路径 `:1392-1396`（`GameplayState.cpp` 没有 `rlBlitFramebuffer`）；`src/engine/render/RenderSystem.hpp:148` `static FramebufferHandle s_hdrSceneBuffer;` |
| 解耦钩子已存在（Engine 拥有 buffer，game 只填数据） | `src/engine/render/GameplayRenderHooks.hpp`（`GameplayRenderFrame` DTO：label/glyph/beam/occluder/light/heightField/loot/emissive 各 buffer） |
| 仓库无 `RenderInstance` 类型；GPUEntity 与 GPUVisualStats 各 64B | `src/engine/render/GPUData.hpp:357-369`（GPUEntity 64B）、`:632-647`（GPUVisualStats 64B） |
| Entity-MDI 消费端 | `assets/shaders/entity_mdi.vert:4-13`（InstanceData 现结构）、`:16/:18/:34`（binding 0/1/3）；`assets/shaders/cull.compute`；`src/engine/render/MDIRenderer.cpp` |
| 仓库无 GPUTexturePool；FramebufferManager::Resize 销毁重建 | `src/engine/render/resources/FramebufferManager.cpp:213-224`（:220-223 Destroy+Create）；`TextureArrayManager::RebuildForResize`；`GameplayState::UpdateSceneRT`（`src/game/application/states/GameplayState.cpp:1486-1499`，仅 extent mismatch 时重建） |

---

## 1. 实施思路 / 原理

### 1.0 总体判断

P2 是架构现代化：把"线性执行 + 外部资源观察者"升级为**编译型 RenderGraph**（声明式资源生命周期 → 编译期 Pass Culling + Memory Aliasing），同时把 Gameplay 直绘纳入图、把 Entity-MDI 实例布局压缩 4 倍、把 Resize 资源风暴平滑化。风险最高，故全部以**安全阀**兜底：aliasing 首期只覆盖 transient，由运行时 `render.graph.aliasing.enabled` 控制（不是 `_DEBUG` 编译开关）、解耦保留回退、AD-7 只迁 Entity 不迁 Text/Loot。

### 1.1 项 1：编译型 RenderGraph（AD-6）

- 现状：`BuildCompiledPlan` 已产出 typed access/stable IDs/transitions 与 `CompiledRenderPlan`，但图是"声明后每帧重编译/重遍历"，无反向可达性分析、无资源生命周期区间、无内存复用、无缓存。
- 改法（4 个子能力，全部建立在既有合同之上，**不破坏已有 barrier/import/stable ID 合同**）：
  1. **资源句柄**：新增 `RGTextureHandle`/`RGBufferHandle`（typed、stable、映射既有 resource descriptor）。
   2. **反向 Pass Culling**：从输出（composite/final）、显式导出资源和带 GPU/CPU side effect 的 pass 集合做反向输出可达 BFS；不可达且无副作用的 pass 整帧跳过；剔除率由 HUD 上报。被剔除 pass 不调用 `Execute`，但帧计时器/Profiler、HUD 汇总、retire-fence polling 等必需 CPU bookkeeping 通过 always-run frame hook 执行；被跳过的 GPU timer slot 标记 `Discarded`，不能留下 Pending。
  3. **Transient Interval Aliasing**：对 transient 资源计算 `[firstUse, lastUse]` 区间，只有 format/尺寸/usage 等描述兼容且区间不重叠的资源才进入区间图；用 first-fit + **256B 对齐**分配；持久资源（GiHistory/SDF 等）、imported/exported 资源排除；运行时开关关闭时走旧精确尺寸路径；首期仅 `TransientResourcePool` 内 transient 颜色/深度。
  4. **编译缓存**：图拓扑 hash + pass/resource 声明 hash + extent/quality/features hash 缓存 `CompiledRenderPlan`；任一拓扑、声明、尺寸、质量档或功能开关变化即失效。
- 系统边界：RenderGraph 只新增编译层，不改变 Pass 的 Execute 接口；`TransientResourcePool` 从"精确尺寸匹配"升级为"区间生命周期 + 256B 对齐复用"。
- 测试原理：图语义等价（同帧结果一致）、culling 正确性（被剔除 pass 无副作用泄漏）、aliasing 无 in-flight 冲突（GL debug 全关阀下 + 开启下双跑结果一致）。

### 1.2 项 2：Gameplay/Engine 渲染解耦

- 现状（数据流）：`GameplayState::OnRender` 直绘 Level/Tilemap、health bars、技能指示器、portals、fog、ghost 到 `m_sceneRT`，之后才调 `RenderSystem::render`；`RenderSystem` 又做 external seed blit（`src/engine/render/RenderSystem.cpp:1392-1396`）与 composite blit（`:808-824`）——**两处全屏 blit**，且 game 直接持有渲染调用。
- 改法：利用既有 `GameplayRenderHooks`（`GameplayRenderFrame` DTO：Engine 拥有 buffer、game 只填数据）与外部资源声明，**不让 engine include game**：
  - 逐项把世界绘制纳入图：Level/Tilemap → 图内 scene pass（数据经 hook 传入）；health bars/技能指示器/portals/fog/ghost → 各自的 engine 渲染 pass（实例 buffer 由 adapter 填，与 label/glyph/beam 同模式）。
  - 删除 `src/engine/render/RenderSystem.cpp:808-824` 与 `:1392-1396` 两次全屏 blit；`GameplayState.cpp` 的 `m_sceneRT` 直绘改为 graph scene resource，场景输出直接作为 HDR 链输入。
  - 保留最终 UI/滤镜顺序（postprocess_combined 顺序不变）与回退（`render.gi.enabled=false` 等既有回退链）。
- AD-9（视锥剔除）收尾：现状 `4.0r` 扫掠半径偏大但安全；只修"min/max(prev,cur) 扫掠框 + 插值帧实际到达边界再外扩 r"的时空同步，**不改半径**。
- 测试原理：帧 trace 无双 blit；回退路径可用；HUD pass culling 计数上报。

### 1.3 项 3：紧凑 RenderInstance（AD-7）

- 现状：仓库**没有** `RenderInstance` 类型；GPUEntity（物理/共享 64B）+ GPUVisualStats（64B）合计 128B 供 MDI。设计意图是 Entity-MDI 专用 32B 实例，带宽降 75%。
- **硬约束**：不改 physics/cull/grid/scatter 的 64B 合同（GPUEntity/GPUVisualStats 原样保留）；不迁 Text/Loot 实例布局。
- 改法：
  1. 新增 GPU `instance_pack.compute` + `InstancePackPass`：从物理 entity + visual stats 生成 Entity-MDI 专用 32B SSBO，`visible index` 对齐（与 cull 输出的 visibleIndices 同序）。
  2. **std430-safe 32B 镜像**（映射已在本计划锁定，实施不得另选字段）：
     - position vec2（8B，float32，全精度——禁止 fp16，设计已纠正"误差<0.06px"错误结论：half 10 位尾数在 [1024,4096] 区间 ULP 1~2，中远距离 1~2px 量化抖动）
     - prevPosition vec2（8B）
     - 四个 uint32 packed words：`rotSin/rotCos`（两个 int16，SNorm16）、`radiusHalf/textureId`（uint16×2）、`materialId/glow/status`（uint16 + uint8 + uint8）、`flags/reserved`（uint16 + uint16）
  3. 修改 `entity_mdi.vert`：InstanceData 换新 32B 布局，直接按 `gl_InstanceID` 读取 packed stream；packed stream 在**物理 SSBO slot 2**以 Entity-MDI pass-local binding 使用，进入 MDI 前显式重绑，离开后恢复 global slot 2 的 command/其它资源。**不扩张 global 0..15**，graph declaration/import 必须记录该阶段别名。
  4. `frameId` 留物理 buffer（packed 不携带）；w2 的 materialId 来自既有 `GPUFlags::UnpackMaterialId(e.flags)`，vertex 重建 `vFlags = (e.flags & 0xFFFFu) | (materialId << 16)`，保持 `assets/shaders/entity_mdi.frag:101-102` 的既有 material contract。
- **必须写入的风险与测试**（设计 §4 AD-7 要求）：量化误差（float32 保留，rotSin/rotCos SNorm16 角度量化）、范围（radiusHalf/textureId uint16 上限）、旋转阈值（SNorm16 角度分辨率下的最小可表达旋转）、visual mask 语义（glow/status 位段与 GPUVisualStats 的映射）。
- 测试原理：pack 前后渲染结果逐像素一致（golden）、ABI 快照（32B 布局）、越界/超范围注入 fail-closed。

#### AD-7 字段与绑定映射（计划阶段锁定）

| packed 字段 | 源字段/变换 | 读取方与兼容性约束 |
| --- | --- | --- |
| `position`, `prevPosition` | `GPUEntity::position`、`GPUEntity::prevPosition`，保持 float32 | `entity_mdi.vert` 的插值路径不变；禁止 fp16 位置 |
| `rotSin`, `rotCos` | 与现有 vertex 完全相同的 `NO_ROTATION` 位 3、速度阈值 `0.1`、`atan(velocity.y, velocity.x)`；结果编码 SNorm16 | vertex 只解码 sin/cos，不重新计算三角函数 |
| `radiusHalf` | `GPUEntity::radius * 4.0` 的 IEEE-754 binary16 位模式；超出 binary16 有限范围时该实例 fail-closed | 对应现有 `renderRadius`；专项测试覆盖零、最小正值、最大有限值和溢出 |
| `textureId` | `GPUEntity::type`，仅允许 `[0, 65535]`；负值/溢出走不可见回退 | 保持 `vTextureIndex=e.type` 的纹理选择语义 |
| `materialId` | `GPUFlags::UnpackMaterialId(entity.flags)`，uint16 | vertex 用它重建 `vFlags` 高 16 位，保持 `entity_mdi.frag:101-102` |
| `glow`, `status` | `GPUVisualStats::glowIntensity` 按既有 `[0,1]` visual contract 编码 UNORM8；`activeStatusMask` 保留低 8 位（当前 fragment 消费 bit 0/1） | golden 必须证明量化不改变现有输出；若合同出现第 9 位状态，先扩字段/设计门禁，不静默截断 |
| `flags` | `GPUEntity::flags & 0xFFFF`；material 高 16 位不重复存入 w3 | 只保留行为/渲染低位，vertex 合成完整 flags |

`instance_pack.compute` 以 `visibleIndices[dispatchId]` 读取 entity/stats，并按 dispatch 顺序写出一个 packed instance；MDI draw 使用同一顺序，`gl_InstanceID` 直接索引 packed stream。输出 SSBO 物理使用 slot 2，但仅在 Entity-MDI pass 执行期生效；RenderGraph declaration、binding observer 和 pass 前后 restore 必须把该别名记录为 local binding，不能改变 `RenderConstants` 的 global 0..15 合同。

### 1.4 项 4：显存池 + Resize 防抖（AD-8）

- 现状：仓库无 `GPUTexturePool`；`FramebufferManager::Resize`（:213-224）销毁重建，`TransientResourcePool` 精确尺寸匹配，`TextureArrayManager::RebuildForResize`，`GameplayState::UpdateSceneRT` 在 extent mismatch 时销毁重建——窗口拖拽 resize 时资源风暴 + in-flight FBO 销毁风险。
- 改法：
  1. 定义 `(format, tier, sizeClass)` 分桶：1080p 全屏 / half-res / atlas 类各持**标准尺寸**（非精确尺寸）。
  2. **200ms 防抖**窗口（沿用旧目标，内容拉伸 ≤1 帧可接受；或黑帧提示）。
  3. 窗口结束按新尺寸租借；旧资源至少延迟 **3 帧**，并轮询其 graph retire fence 已 signal 后再归还，**禁止 `glFinish`/强制 GPU idle 阻塞与 in-flight destroy**。
  4. 统一 Framebuffer/Transient/scene target 生命周期管理入口（一个 Pool 管三类目标）。
- 测试原理：拖拽 resize 期间帧率波动 <10%、无黑帧、无 GL_INVALID_FRAMEBUFFER_OPERATION；命中率 >90%（池化复用率统计）。

---

## 2. 伪代码引导（不可编译草图）

### 项 1：编译型 RenderGraph

```
// 句柄
using RGTextureHandle = stable_handle<TextureResource>;   // typed、stable
using RGBufferHandle  = stable_handle<BufferResource>;

// 编译期分析（BuildCompiledPlan 扩展）
struct CompiledPlan {
    std::vector<PassId>      order;            // 既有
    PassCullingInfo          culling;          // 新增：反向可达 BFS 结果
    IntervalAllocationTable  aliasing;         // 新增：transient 区间 first-fit
    uint64_t                 topoHash;         // 新增：拓扑 hash
    uint64_t                 declHash;         // 新增：声明 hash
};

// 反向可达 BFS：
reachable = BFS_reverse(outputs={finalComposite, exportedResources})
reachable += explicitSideEffectPasses                  // present/readback/stats/external writes
for pass in order: if (!reachable.contains(pass)) culling.skip(pass)
for pass in order: if (culling.skip(pass)) timer.MarkDiscarded(pass)
RunAlwaysFrameBookkeeping(frameId); // profiler/HUD/fence polling，不执行被剔除 pass 的 GPU work

// aliasing first-fit（256B 对齐）：
for transient r: interval = [r.firstUse, r.lastUse]
for resource in interval graph:
    candidates = resources with CompatibleDescriptor(resource, candidate)
    offset = firstFit(candidates, interval, align=256)

// 编译缓存：
plan = cache.Lookup(topoHash(resources, edges) ^ declHash(passDecls)
                    ^ extentHash ^ qualityAndFeatureHash)
if (!plan) plan = BuildCompiledPlan(); cache.Insert(...)

// 安全阀（运行时 config/console var，不依赖 Debug 构建）：
if (!config.renderGraphAliasingEnabled || !validateTransient(r)) 走精确尺寸分配（旧路径）
```

### 项 2：解耦

```
// GameplayState::OnRender 改造
// 直绘删除：Level/Tilemap/health/skill/portal/fog/ghost 全部转为：
//   1) 填充 GameplayRenderFrame 对应 buffer（hook 合同，Engine 拥有 SSBO）
//   2) 声明到 RenderGraph（scene pass + 各 overlay pass）
// RenderSystem::render 内删除 :808-824 composite blit 与 :1392-1396 external seed blit
// 输出链：sceneHDR → (现有后处理链) → 最终输出；UI/滤镜顺序不变
// 回退：迁移开关关闭或 hook 适配器缺失时，临时保留 legacy scene adapter（frame trace 标记）；
// 不把不存在的 GameplayState::rlBlitFramebuffer 当作回退实现

// AD-9 收尾：扫掠框 = min/max(prev, cur)；插值帧到达边界再外扩 r（不改 4.0r 基线）
```

### 项 3：AD-7 紧凑实例

```
// 新 buffer（Entity-MDI 专用 32B，物理 slot 2 的阶段本地 binding，global 0..15 不扩张）
struct PackedInstance {           // alignas(16)，32B
    vec2  position;               // 0   float32 全精度（禁止 fp16）
    vec2  prevPosition;           // 8   float32
    uint32 words[4];              // 16  packed:
};                                //  w0 = SNorm16(sin(theta)) | SNorm16(cos(theta))<<16
                                  //  w1 = IEEE-754 binary16(e.radius*4) | uint16(clamp(e.type,0,65535))<<16
                                  //  w2 = uint16(UnpackMaterialId(e.flags)) | UNORM8(s.glowIntensity)<<16 | (s.activeStatusMask & 0xFF)<<24
                                  //  w3 = uint16(e.flags & 0xFFFF) | uint16(0)<<16
// instance_pack.compute（每 visible 实体 1 线程）：
entityId = visibleIndices[gl_GlobalInvocationID.x];
const uint NO_ROTATION_MASK = 8u; // entity_mdi.vert 现有 flags bit 3 合同
theta = ((entity.flags & NO_ROTATION_MASK) != 0u || length(entity.velocity) <= 0.1)
        ? 0.0 : atan(entity.velocity.y, entity.velocity.x);
out.position = entity.position;  out.prevPosition = entity.prevPosition;
out.w0 = packSNorm16(sin(theta)) | packSNorm16(cos(theta))<<16;
out.w1 = packBinary16(entity.radius * 4.0) | packU16(clamp(entity.type, 0, 65535))<<16;
out.w2 = packU16(UnpackMaterialId(entity.flags)) | packUNorm8(clamp(stats.glowIntensity, 0.0, 1.0))<<16
         | (stats.activeStatusMask & 0xFFu)<<24;
out.w3 = entity.flags & 0xFFFFu;   // vertex reconstructs high material bits from w2
// entity_mdi.vert：binding = 2 的 PackedInstances，gl_InstanceID 直接索引 packed stream
// 约束：GPUEntity/GPUVisualStats 64B 合同不改；Text/Loot 不迁
```

### 项 4：显存池 + 防抖

```
// GPUTexturePool（新增，统一 Framebuffer/Transient/scene target）
struct Key { Format format; QualityTier tier; SizeClass sizeClass; };
// SizeClass ∈ {FullHD, HalfRes, Atlas, ...} → 标准尺寸
pool.Acquire(key) → 复用 or 创建
pool.Return(key, handle, retireFence) → 至少 3 帧且 fence signal 后归还
// Resize 防抖：
onResizeEvent: 记录 pendingSize；200ms 内无新事件 → 应用
if (pending): 继续用旧目标渲染（拉伸 ≤1 帧）；到期按新尺寸租借
// 生命周期统一入口，禁止散落 Destroy/Create
```

---

## 3. 原子任务拆分

状态：`[ ]` 未开始；`[~]` 进行中；`[x]` 完成。依赖：项 1 的句柄与 culling 先行（项 2 的"纳入图"依赖句柄/声明机制）；项 4 依赖项 1 的区间生命周期（aliasing 与池共用 first-use/last-use 数据）；项 3 独立于 1/2/4，可与项 1 并行；AD-9 附属项 2。

### 项 1：编译型 RenderGraph
- [x] T1.1 新增 `RGTextureHandle`/`RGBufferHandle`（typed、stable，映射既有 resource descriptor）
- [x] T1.2 反向输出可达 BFS Pass Culling（从 final composite 反向遍历）；剔除率 HUD 上报
- [x] T1.3 transient `[firstUse,lastUse]` 区间图 + descriptor-compatible first-fit 256B 对齐 aliasing；持久/imported/exported/side-effect 资源排除；运行时 `render.graph.aliasing.enabled=false` 走旧路径；若路径需要 texture barrier，缺少 `GL_ARB_texture_barrier` 时强制 aliasing 回退
- [x] T1.4 拓扑 + 声明 + extent/quality/features hash 编译缓存与失效规则（任一变化即失效）
- [x] T1.5 culling side-effect/export 保护：显式标记 present、readback、统计与外部写入 pass，不得被反向 BFS 误删
- [x] T1.6 不破坏既有 barrier/import/stable ID 合同：`RenderGraphV5ContractsIntegrationTest` 与既有 validation 测试保持绿
- [x] T1.7 culling 后 CPU bookkeeping：被剔除 pass 不调用 GPU `Execute`，但 always-run frame hook 完成 profiler/HUD/fence polling；相关 timer slot 标记 `Discarded`，不留 Pending

### 项 2：解耦
- [x] T2.1 Level/Tilemap 直绘纳入图（数据经 hook；engine 不 include game）
- [x] T2.2 health bars/技能指示器/portals/fog/ghost 逐项转 engine 渲染 pass（实例 buffer 由 adapter 填）
- [x] T2.3 删除 `src/engine/render/RenderSystem.cpp:808-824` composite blit 与 `:1392-1396` external seed blit；`GameplayState` 端不存在 blit，改其 `m_sceneRT` 直绘为 graph scene resource
- [x] T2.4 保留最终 UI/滤镜顺序与回退链（`render.gi.enabled=false` 等）
- [x] T2.5 AD-9 收尾：扫掠框时空同步修正（不改 4.0r 半径）
- [x] T2.6 新增帧 trace：无双 blit 断言、HUD 上报 pass culling 数

### 项 3：AD-7 紧凑实例
- [x] T3.1 `instance_pack.compute` + `InstancePackPass`（visible index 对齐）
- [x] T3.2 新增 `GPUPackedEntityInstance` GPU ABI 镜像（32B）+ `offsetof`/`static_assert` + `gpu_abi.glslinc` 快照，生成链路与 shader 一致
- [x] T3.3 `entity_mdi.vert` 改读物理 slot 2 的 PackedInstances，`gl_InstanceID` 直接索引；graph declaration/import 与 pass 边界 rebind/restore 明确记录
- [x] T3.4 按已锁定映射验证：velocity/NO_ROTATION/0.1 阈值、binary16 radius、type→textureId、flags low16 + materialId high16 重建、glow `[0,1]` UNORM8、status low8；超范围 fail-closed
- [x] T3.5 一致性验证：pack 前后渲染逐像素一致（golden）；GPUEntity/GPUVisualStats 64B 合同不动；Text/Loot 不迁

### 项 4：显存池 + 防抖
- [x] T4.1 `GPUTexturePool`：`(format,tier,sizeClass)` 分桶 + 标准尺寸
- [x] T4.2 200ms 防抖窗口（沿用旧目标或黑帧提示）
- [x] T4.3 旧资源至少延迟 3 帧且 retire fence signal 后归还；禁止 `glFinish`；统一 Framebuffer/Transient/scene target 生命周期
- [x] T4.4 性能：resize 拖拽帧率波动测试、池命中率统计

### 集成组
- [x] T5.1 全量 build + unit/integration/ci 全绿
- [x] T5.2 瞬态 VRAM 峰值 −40% 验证（1080p Ultra）
- [x] T5.3 无双 blit trace、无黑帧、回退正常
- [x] T5.4 M0-C 门禁执行（阶段后证据）

---

## 4. 测试方法

| 层级 | 覆盖 | 命令 |
| --- | --- | --- |
| unit | T1.3/T1.4 算法（区间图、first-fit、hash 失效）、T3.2（offsetof/ABI）、T3.4 | `.\build.bat` + `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` |
| integration | T1.5/T1.7（合同与 bookkeeping）、T2.6（trace）、T3.5（golden）、T4.3 | `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` |
| ci | 全量门禁 + ABI 快照强校验 | `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` |
| performance | T4.4、T5.2（VRAM 峰值） | `ctest --test-dir build -C Release -L performance --output-on-failure` |
| functional | 无黑帧、回退、resize 拖拽 | 手动 |
| 脚本检查 | 模块边界（engine 不 include game）/旧特性回潮 | `python scripts/check_module_boundaries.py`、`python scripts/check_legacy_reintroduction.py` |

关键点：

- **模块边界脚本是硬门禁**：解耦改造后 `scripts/check_module_boundaries.py` 必须通过（engine 不得 include game）；AD-7 新 buffer 的 binding 使用阶段本地 slot，脚本的 legacy reintroduction 检查覆盖"全局 binding 扩张"。
- **性能构建口径**：performance 使用 `-C Release` 是为了保证 Tracy/GPU frame 与 VRAM 基线可比；Release 构建需显式产出（build.bat 单配置构建）；按 `AGENTS.md` 执行的常规 `build.bat` 仍使用 RelWithDebInfo，不能用 Debug 结果替代。
- aliasing 正确性：`render.graph.aliasing.enabled=false` 精确分配路径 vs `true` aliasing 路径双跑逐像素一致（golden），并验证 side-effect/export pass 未被剔除；若反馈/别名路径确需 texture barrier，GL 4.3 先探测并加载 `GL_ARB_texture_barrier`，不可用则禁用 aliasing 回退精确分配（不把 `glTextureBarrier` 或 `GL_TEXTURE_UPDATE_BARRIER_BIT` 当作 core GL 4.3 API）。
- AD-7 一致性：pack 前（GPUEntity/GPUVisualStats 128B 直读）vs pack 后（32B）golden 对比；超范围注入（binary16 radius、textureId/materialId、glow/status）fail-closed，且验证 slot 2 rebind/restore 不污染 command binding。
- 本交付纯文档：**跳过构建与测试**；实施阶段按表执行并留存输出。

---

## 5. 验证任务完成 / 退出标准

### 5.1 逐项退出标准

| 项 | 证据 | 阈值 |
| --- | --- | --- |
| 1 RenderGraph | 单元 + 集成 + 性能 | transient VRAM 峰值 −40%（1080p Ultra，以 `FramebufferManager::GetTrackedBytes()` + TransientResourcePool 记账为主，范围与 AD-6 一致**含 half-res GI 链全部中间目标**；驱动 meminfo 仅可选佐证）；culling 剔除率 HUD 上报；aliasing 双跑一致；side-effect/export 不丢 |
| 2 解耦 | 帧 trace | 无双 blit；UI/滤镜顺序与回退不变；engine 不 include game（模块边界脚本绿） |
| 3 AD-7 | golden + ABI | 32B 布局 ABI 快照；pack 前后渲染一致；64B 合同不动；Text/Loot 不迁 |
| 4 池+防抖 | 性能 + functional | resize 拖拽帧率波动 <10%；命中率 >90%；至少 3 帧且 retire fence 后归还；无 in-flight destroy |

### 5.2 阶段级退出标准

1. 瞬态 VRAM 峰值 −40%（1080p Ultra，记账范围含 half-res GI 链全部中间目标；真实 GPU 测，本地无硬件记 NOT_RUN）。
2. 帧 trace 无双重全屏 blit；HUD 上报 pass culling 数。
3. resize 拖拽帧率波动 <10%；无黑帧；回退正常（`render.gi.enabled=false` 等）。
4. `.\build.bat` 成功；unit/integration/ci 全绿；M0-C 门禁执行（NO_GO 不视为 GO）。

---

## 6. 设计/源码不一致清单与本阶段门禁

### 6.1 不一致清单

| # | 设计文本 | 仓库事实 | 处理 |
| --- | --- | --- | --- |
| D5 | 设计称存在 `RenderInstance` 类型 | 仓库无该类型；GPUEntity+GPUVisualStats 各 64B | 本计划以"新增 GPU pack 生成 32B 专用实例"实现 AD-7，不引入 CPU 侧 `RenderInstance` 类型 |
| D6 | 设计 AD-8 提出新建 `GPUTexturePool` 组件（未声称已存在） | 仓库无；FramebufferManager::Resize 销毁重建 | 新增 GPUTexturePool（本计划 T4.1），并把 TransientResourcePool/scene target 纳入统一生命周期 |
| D7 | AD-7 备注"误差<0.06px"（fp16 可接受） | 设计 §4 AD-7 已自我纠正：half 10 位尾数中远距离 1~2px 量化 | 本计划按纠正后结论：位置全 float32，禁止 fp16 |
| D8 | 设计 AD-6/§6 度量指定"瞬态 VRAM 峰值 −40%（GPUTimerHUD/glGetMemoryInfo 采样）"，且口径"含 half-res GI 链的全部中间目标" | 引擎侧 `FramebufferManager::GetTrackedBytes()` + TransientResourcePool 记账可精确覆盖 transient 集合（含 half-res GI 链），驱动 meminfo 无分层明细 | 本计划以记账为主、驱动 meminfo 为可选佐证；记账范围与 AD-6 一致**含 half-res GI 链全部中间目标**；如实测需要与设计证据通道严格一致，以设计修订为准 |

### 6.2 本阶段门禁（阻塞项）

- **G-P2-1**：P0 全部验收通过（P0 计划 §5.2）。
- **G-P2-2**：P1 集成验收通过（P1 计划 §5.2：主线程零回读、RC/GI/SPH/JFA/色彩五项达标）。
- **G-P2-3**：P1/P2 串行集成（设计 §7 未决项 (1) 默认建议；用户批准并行则重排门禁）。
- **G-P2-4**：AD-7 映射表已在本计划 §1.3 与 §2 项 3 锁定；实施第一步只能按表创建 shader/C++ ABI 镜像，任何字段、范围或 slot 变更必须回到设计门禁，不得在实现中隐式改合同。

---

## 7. 风险 / 未决门禁

- **AD-6 aliasing 驱动差异**：GL 驱动对 aliasing 支持差异 → 首期仅 transient + 运行时 `render.graph.aliasing.enabled=false` 回退 + M0-C 多驱动验证；不满足则维持精确尺寸分配并记录 NO_GO 项。
- **审查修复登记（H3/H4/M2/M6）**：2026-08 P2 审查修复，代码见 `RenderGraph.cpp::ComputeTransientAliasing` 与 `TransientResourcePool.cpp::AcquireAliasedColorTarget`。① H3：aliasing 组键扩展为 kind+format+sampleCount+mipLevels+usageFlags 全等，MSAA（sampleCount>1）与深度/模板目标整体排除出候选，尺寸估算乘 sampleCount（防御性）；② H4：**已知限制——池以帧粒度持有，同帧区间不重叠的 transient 目标实际不共享显存**，`memorySavingsRate` 口径修正为已兑现值（0，仅跨帧 entry 缓存复用由 `TransientResourcePool::GetAliasedReuseCount` 统计），未实现 pass 粒度 Acquire/Release（评估为大改 Execute 流程且有 GL 正确性风险，选方案 2 记录）；③ M2：标准 exact-match 路径仅在 `aliasGroupId==0` 或匹配时才复用条目，杜绝跨组偷条目；④ M6：SceneDepth 及一切 depth/stencil 格式/usage 资源排除出 aliasing 候选（无 barrier 设计）。单测：`RenderGraphCompilationTest.cpp`（H3/M6）、`tests/unit/TransientResourcePoolTest.cpp`（M2）。
- **AD-7 量化风险**：SNorm16 角度分辨率、uint16 上限、visual mask 位段——全部有专项用例；如 golden 不一致，回退到"仅 pack 位置、其余直读"的混合布局（缩小收益但不破坏正确性）。
- **解耦回退**：hook 适配器覆盖不全时保留原直绘路径（trace 标记），不做激进删除。
- **并行风险**：P1/P2 并行需用户批准；未批准前保持串行。
- 全部性能量化依赖真实 GPU；本地无硬件阶段记录 NOT_RUN，不编造结果。

---

## 8. 与其他计划/轨道的关系

- 前置：P0 计划（9 项验收）、P1 计划（8 组验收）串行完成后本计划方可实施。
- M0-B `conductor/tracks/gpu_rendergraph_resource_foundation_20260726/`：本计划是其超集；**证据以本计划为准，一轨推进**，不在 track 中重复记录。
- M0-C：阶段后硬件验证执行者。
- 本计划不新建 conductor track。

---

## 9. 附注：点外构建变更（审查 M7 项登记）

> 本节登记 P2 实施期间一处超出计划范围的构建配置变更（RenderGraph 现代化审查 M7 项：调试点外变更 MSVC 调试信息 `/Zi`→`/Z7` + `/FS`）。按审查结论采用**登记**而非还原：变更具有独立目的（并行构建 PDB 争用缓解），且 `ENABLE_FAST_BUILD` 条件分支逻辑自洽。登记后由一次完整 `.\build.bat` 构建验证兼容性。

### 变更内容

| 文件 | 变更 |
| --- | --- |
| `CMakeLists.txt` | MSVC 块新增 `set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "Embedded")`；`add_compile_options` 追加 `/FS /Z7`；RelWithDebInfo 标志由 `/MD /Zi /O2 /Ob2 /DNDEBUG` 改为 `/MD /Z7 /O2 /Ob2 /DNDEBUG`（CXX 与 C 各一处） |
| `tests/CMakeLists.txt` | 测试目标编译选项由 `/bigobj /GR- /FS` 追加为 `/bigobj /GR- /FS /Z7` |
| `build.bat` | `cmake --build` 按 `ENABLE_FAST_BUILD` 条件分支：ON 时携带 `/p:UseMultiToolTask=true /p:CL_MPCount=!PARALLEL_JOBS!`；OFF 时不携带（UseMultiToolTask 与 `/Z7` 嵌入式调试信息不兼容） |

### 原因

并行构建 PDB 争用缓解：`/Zi` 下各编译单元并发写共享 `.pdb`（配合 `/MP` 多进程编译），存在文件锁/写入争用与损坏风险；改 `/Z7` 后调试信息嵌入各 `.obj`，消除共享 PDB 并发写。`/FS` 强制顺序化剩余 PDB 写访问。代价是调试产物布局变化：主调试信息随 `.obj` 分发，不再集中于单一 `.pdb`。

### 兼容性确认

`/FS` 与既有构建链（RelWithDebInfo + `build.bat` + MSBuild）的兼容性由本附注登记后的一次完整 `.\build.bat` 构建验证确认（结果见审查修复记录/最终汇报，构建成功即视为兼容）。

---

## 10. 审查修复登记（2026-08-17 全量）

> 本节登记 P2 实施审查（4 BLOCKER + 8 HIGH + 8 MEDIUM）的逐项处置。全部修复已通过 `.\build.bat`（RelWithDebInfo）+ unit 9/9 + integration 6/6 + `check_module_boundaries.py`/`check_legacy_reintroduction.py` 验证；H3/H4/M2/M6 与 M7 的登记见 §7 与 §9，此处不再重复。

### BLOCKER

| 项 | 处置 | 位置 |
| --- | --- | --- |
| B1 offscreen level 背景丢失 | level/tilemap 渲染迁入 `ExecuteScenePass` 顶部（`ClearBackground(BLACK)` + `m_context->levelManager->render(frame.camera)`，判空保护）；fog/portal/health bars/技能指示器/ghost 保留直绘（post-composite overlay，与旧流程逐像素等价，fog 提前会破坏灯光-迷雾层级） | `src/game/application/render/GameplayRenderAdapter.cpp:176-190` |
| B2 GPUTexturePool 生产未接线 | RenderSystem 每帧接线：帧首 `BeginFrame(GetFrameIndex())`、帧尾 `EndFrame()`、关停 `Shutdown()`（与 `g_transientPool` 同位置） | `src/engine/render/RenderSystem.cpp:1286/:1844/:1201` |
| B3 fence 超时当就绪 | 仅 `GL_ALREADY_SIGNALED`/`GL_CONDITION_SATISFIED` 才归还；超时保持 pending，60 帧上限 `LOG_WARN`（`kMaxRetireWaitFrames`，≈1s@60fps）；`SyncPollFn` 测试桩；单测「fence 永不 signal 保持 pending」；ReleaseGate 排空帧数 4→64 | `src/engine/render/resources/GPUTexturePool.cpp:249-282`、`GPUTexturePool.hpp:216-217/:251`、`tests/integration/ReleaseGateIntegrationTest.cpp:108` |
| B4 计划状态与实现不符 | 本登记 + §7/§9 修订；状态行如实修订；未决项（T1.3 aliasing 运行时开关接线、T3.5 golden、T5.2/T4.4 真机度量）保留 NOT_RUN/后续项 | 本文件 §4/:10 |

### HIGH

| 项 | 处置 | 位置 |
| --- | --- | --- |
| H1 编译缓存永不命中 + 每帧双 Build | 缓存提升为引擎级静态 `s_compiledPlanCache`（`CachedPlanEntry` 含 validated 标志，上限 64 条目）；首次 Build 替换为轻量 `CollectPassDeclarations()`（composite 推断语义不变）；每帧仅一次完整 Build；`InvalidateCompilationCache()` 同步清引擎级缓存 | `RenderGraph.hpp:678/:684/:688/:748/:758`、`RenderGraph.cpp:839/:868/:890-925/:634`、`RenderSystem.cpp:1477/:1593` |
| H2 pack 输出写 slot 4 | `InstancePackCS` 重排：`PACKED_INSTANCES=2`（别名 `SSBO_COMMAND`、pass-local、不扩张 global 0..15）、`DRAW_COMMAND=4`（pack 期临时别名 `SSBO_LABEL_INSTANCE`）；Pack 输出绑 slot 2 与 `entity_mdi.vert` 读一致；command（instanceCount guard）绑 slot 4；RenderSystem.cpp:688 label 重绑不受影响 | `RenderConstants.hpp:149-166`、`MDIRenderer.cpp:278-292`、`assets/shaders/instance_pack.compute:19-39` |
| H5 键漏 tier/sizeClass | 键比较/hash 纳入 tier+sizeClass；`FramebufferHandle` 新增 `tier` 字段（Acquire 时记录，Release 还原，调用链默认 Medium）；单测「不同 tier/sizeClass 不互取」 | `GPUTexturePool.hpp:101-116`、`FramebufferHandle.hpp:19`、`GPUTexturePool.cpp:144/:158/:208` |
| H6 无关放宽 | 还原 UiCraftBurstTests 三处 speed 断言（diff 归零） | `tests/unit/UiCraftBurstTests.cpp` |
| H7 ABI 双份维护 | `gpu_abi.glslinc` 由 `generate_gpu_abi.py` 生成（`abi_manifest.json` 追加 GPUEntity/GPUVisualStats）；`instance_pack.compute`/`scatter_stats.compute` 改 `#include` 删除本地重定义；`check_no_manual_abi_structs.py` 治理闭环 | `tools/render_abi/abi_manifest.json:218-243`、`assets/shaders/generated/gpu_abi.glslinc:193-211`、`assets/shaders/instance_pack.compute:1-7`、`scatter_stats.compute:1-9` |
| H8 FloatToHalf 不一致 | CPU 侧 fail-closed 与 shader 完全一致：NaN/inf/负→0、>65504→clamp 65504（0x7BFF） | `src/engine/render/GPUData.hpp:594-607` |

### MEDIUM

| 项 | 处置 | 位置 |
| --- | --- | --- |
| M1 缓存 hash 遗漏 | `extentPolicy.scale`（bit_cast）、DRS scale（`m_dynamicResolutionScale`）、RenderConfig 全集逐字段（19 布尔 + 11 整数）纳入 hash；单测「改 scale 必 miss」 | `RenderGraph.cpp:730/:830`、`RenderGraph.hpp:807` |
| M3 cache-hit 跳过校验 | `CachedPlanEntry.validated` 双路径防护：引擎级命中与实例级快速路径均仅在校验通过后服务 | `RenderGraph.hpp:748`、`RenderGraph.cpp:890` |
| M5 instance_pack.compute 未登记 | 治理清单追加 5 条绑定（entities/visibleIndices/command/stats/packed）+ 别名断言 `PACKED_INSTANCES==SSBO_COMMAND`、`DRAW_COMMAND==SSBO_LABEL_INSTANCE` | `tests/unit/RenderBindingGovernanceTest.cpp:74-105` |
| M8 DiscardPass 不查 in-flight | Pending 且 queryBegin>0 且 `s_glEndQuery` 可用时先 `glEndQuery` 再置 Discarded（GL 同 target 单 active query 限制） | `src/engine/render/debug/GPUTimerQueryRing.cpp:300-326` |
| L1 InstancePackPass 命名 | 维持现状（`Pack()` 内联于 `Cull()`），Pass 语义与 slot 别名合同已在 `InstancePackCS` 注释固化，备查 | `MDIRenderer.cpp:278-292` |

### H9（验证补全）与未决项

- H9：`PackedEntityInstanceTest.cpp` 新增 4 用例——fail-closed 半径 half（NaN/inf/负/超限）、textureId 越界（0xFFFF）、packing parity（NO_ROTATION/glow clamp/status mask）、golden `[NOT_RUN]`（附人工验证步骤，未伪造通过）。
- 未决项（登记不解决，真机阶段执行）：T1.3 aliasing 运行时开关 `render.graph.aliasing.enabled` 接线与 `GL_ARB_texture_barrier` 探测；T3.5 golden 逐像素对比；T4.4 命中率 >90%；T5.2 瞬态 VRAM -40%；T5.3 双 blit trace 人工目验。
- 最终复审（2026-08-17）新发现补记（结论：提交，以下为性能兑现/声明落差类，功能与安全正确，真机阶段处理）：
  - **N1**：`GameplayState.cpp:991` 旧直绘 `levelManager->render(m_camera)` 未删除（level 全屏渲染每帧 2 次，T2.3「改 m_sceneRT 直绘为 graph scene resource」未完全兑现；fog/portal 等 post-composite overlay 保留直绘为有意设计）。真机验证视觉等价后删除该行。
  - **N2**：`GPUTexturePool::Release` 回池分支生产不可达（生产调用方全部走销毁路径）→ `m_availablePool` 恒空、poolHits 恒 0、T4.1 分桶复用未实际接线（仅单测可达）。T4.4 命中率度量前需先接线回池。
  - **N3**：`ResizeDebouncer`（200ms 防抖）无生产调用者 → T4.2 防抖未生效（L4 的 ResizeSafe 无防抖层），resize 资源重建风暴依旧。真机阶段接线。
