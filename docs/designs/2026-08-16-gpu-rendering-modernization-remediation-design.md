# GPU 渲染引擎现代化修复与优化方案设计

> **日期**: 2026-08-16
> **类型**: 架构治理 / 性能 / 正确性
> **设计输入**: [GPU 渲染引擎架构与现代化对标审查报告](../reviews/2026-08-16-gpu-rendering-engine-modernization-audit-review.md)（结论 `修改`，18 项发现：5 Blocker / 7 High / 6 Medium）
> **基线文档**: [V5 主控技术规格书](../../conductor/specs/rendering_engine_v5_master_spec.md)、[渲染系统进度](../../conductor/rendering_system_progress.md)
> **产物目录约定**: 本方案及后续计划落在 `docs/designs/` 与 `docs/plans/`，不再新建 `conductor/tracks/` 目录；既有 conductor Track 仅作为历史基线引用。
> **修订 v1.1（2026-08-16）**: 依据技术评审修订——AD-7 纠正 fp16 位置精度错误结论（位置改回 float32）；AD-2 改为纯字段重排（消除类型双关）；AD-3 补辐照度汇聚规范；新增实例计数 Getter 时序合同与 `Initialize()` 失败清理；纳入 4 项审查遗漏（P0-8 网格排序原点、P0-9 GL 版本探测、Phase 1-7 JFA 空场景原子雪崩、Phase 1-8 色彩空间线性化）。

---

## 1. 问题陈述

2026-08-16 审查确认：引擎虽具备现代 GPU 渲染雏形（GPU-driven 剔除、MDI、持久映射三缓冲、ABI 契约、Clustered Forward+、JFA、RC GI、HDR 后处理），但本质仍是"带校验的线性 Pass 执行器 + 外部资源观察者"。核心缺陷分三层：

1. **正确性层（Blocker）**: `GPUFluidParticle` std430 ABI 错位、MDI 工作组尺寸 4 倍失配、JFA 升采样漏绑 mask、粒子系统半初始化、Timer Query 复用未检查就绪、`RenderSystem::Initialize()` 失败静默。
2. **性能层（Stall）**: LightCulling/GPUText/GPULoot 每帧同步回读（合计 1.5~3.5ms 硬阻塞）、SPH O(N²) 暴力邻居搜索、`Barrier::All` 滥用、双重 Blit、Resize 资源风暴。
3. **架构层（现代化差距）**: 无编译型资源 DAG / Pass Culling / Memory Aliasing、RC GI 丢失角度维度、RenderInstance 37.5% Padding、Gameplay 与 Engine 渲染流耦合。

对标 UE5 RDG / Frostbite FrameGraph / id Tech 7，需分三个阶段收敛。

## 2. 目标与非目标

### 目标

- G1: 消除全部 5 项 Blocker 与 6 项 Medium 中的状态治理缺陷，恢复数据正确性。
- G2: 消除每帧 CPU-GPU 硬同步回读，主渲染线程零 `glGetBufferSubData` / `glClientWaitSync` 阻塞点（滞留 1 帧的异步 ring 回读除外）。
- G3: 光影/GI 算法物理正确化：RC 恢复方向维度、GI 时域去噪方差裁切、SPH 邻居搜索复杂度回归 O(N)。
- G4: 建立编译型 RenderGraph（虚拟资源、Pass Culling、Memory Aliasing），削减 40%~60% 瞬态 VRAM 峰值。
- G5: 每阶段均有可测验收：GPUTimer P95、VRAM 峰值、回归用例与门禁证据。

### 非目标

- 不做 Vulkan 迁移（V6 范畴，需 M0-C 硬件证据支持后决策）。
- 不恢复 SPH 生产路径（维持历史 NO-GO；本方案仅修复其算法正确性与复杂度，运行仍限开发 opt-in）。
- 不新增视觉特性（特效、新 Pass）——本方案只做修复、纠偏与架构收敛。
- 不改动既有 `conductor/tracks/` 活跃 Track 的内容；重叠范围的推进关系见 §3.4。

## 3. 阶段划分

### 3.1 Phase 0 — 快速止血（1-2 天，9 项原子修复）

| # | 修复 | 位置 | 验证 |
|---|---|---|---|
| P0-1 | `m_readbackEnabledForTesting` 默认 `false`，仅测试 fixture 显式开启 | `src/engine/render/passes/LightCullingPass.hpp:67`、`LightCullingPass.cpp:202` | 帧耗时下降 ≥0.5ms；overflow 统计改走滞留 1 帧的延迟回读 ring |
| P0-2 | MDI 派发除数 64→256，与 `local_size_x=256` 一致 | `src/engine/render/MDIRenderer.cpp:244` | 单测断言 dispatch 组数 == `ceil(N/256)`；剔除结果不变 |
| P0-3 | `GPUFluidParticle` 字段重排为 std430 兼容布局（见 AD-2），同步 `gpu_abi.glslinc` 与 layout 快照 | `src/engine/render/GPUData.hpp:593`、`assets/shaders/generated/gpu_abi.glslinc:98` | ABI layout 快照测试更新；`offsetof` 静态断言防回归 |
| P0-4 | `JFAPass::RunUpsample` 绑定 `uMaskTexture` 并设置采样单元 | `src/engine/render/passes/JFAPass.cpp:543-600` | 半分辨率遮挡 ROI readback 符号为负的既有用例转绿 |
| P0-5 | 粒子系统半初始化：`LoadShaders()` 失败不置 `m_initialized`；`Update()` 加 `m_computeShader.id != 0` 守卫 | `src/engine/render/GPUParticleSystem.cpp:173,602,642` | 注入 shader 编译失败的单测 |
| P0-6 | `GPUTimerQueryRing` 复用 slot 前检查 `GL_QUERY_RESULT_AVAILABLE`，未就绪丢弃该样本 | `src/engine/render/debug/GPUTimerQueryRing.cpp:101-125` | 深度 3 + 5 帧延迟压测无 `GL_INVALID_OPERATION` |
| P0-7 | `RenderSystem::Initialize()` 返回 `bool`；**任一失败路径先调用幂等 `Shutdown()` 清理已分配的 Pass/FBO/缓冲再返回**，`Game.cpp` 阻断启动并给出明确日志/UI | `src/engine/render/RenderSystem.cpp:909-949`、`src/app/Game.cpp:364` | ABI 强制不匹配注入测试 + 失败后二次析构无句柄泄漏（GL 对象计数断言） |
| P0-8 | `grid_sort.compute` / `grid_count.compute` 补减 `gridOrigin`，消除非零原点下排序索引错乱与越界 | `assets/shaders/grid_sort.compute:29-30`、`assets/shaders/grid_count.compute:28-30` | 非零原点场景单测：cell 索引单调、无越界、实体归属正确 |
| P0-9 | `DeviceCapabilityMatrix` 版本判断 `== RL_OPENGL_43` 放宽为 `>=`，避免 GL 4.5/4.6 环境能力门禁误判 | `src/engine/render/core/DeviceCapabilityMatrix.cpp:46-59` | 契约断言：4.5/4.6 上下文不再落入降级分支 |

### 3.2 Phase 1 — 核心机制纠偏与流水线释放（1-2 周，8 项）

1. **纯 GPU 间接参数生成**（AD-1）：`GPUTextSystem` / `GPULootSystem` 增加单线程 `indirect_args.compute`，删除主线程 `counterBuffer.Read()`；`MDIRenderer` 的 DrawArraysInstancedIndirect / DrawElementsInstancedIndirect 直接消费 GPU 写入的 command buffer。上层计数 Getter（`GetActiveQuadCount()` 等）语义统一变更为滞留 1 帧的统计快照（§5）。
2. **Binding 治理与 ABI 断言加固**：`ShadowCS::kOccluderBinding` 迁出 Slot 15；`kGlobalSharedSSBOBindings` 补 TRAIL_HEADERS(10)/TRAIL_POINTS(11)/MATERIAL_DATA(12)/DISTORTION_DATA(13)；`GPUData.hpp` 全结构补 `offsetof` 静态断言。
3. **Radiance Cascades 物理纠偏**（AD-3）：方向性 Probe Atlas、RTE 级联合并、双 SDF 步进、GIComposite 辐照度汇聚。
4. **GI 时域去噪重构**（AD-4）：废除全局 `resetHistory`，改 3×3 邻域时域方差裁切 + 光源版本增量失效。
5. **SPH 邻居搜索算法化**（AD-5）：计数排序网格哈希桶，O(N) 构建 + 27 邻桶遍历。
6. **屏障精细化**：粒子系统 `Barrier::All` → `GL_SHADER_STORAGE_BARRIER_BIT` / `GL_COMMAND_BARRIER_BIT` 按消费者声明下发；`ShadowBuildPass.cpp:339`、`OccluderExtractPass.cpp:174` 裸 `glBufferSubData` 改走 `PersistentBuffer` 三缓冲写路径。
7. **JFA 空场景原子雪崩消除**：`v5_jump_flood.comp` 废除全屏像素对单一 `overflowCount` 的 `atomicAdd`；Host 端依据滞留 1 帧的遮挡体计数判定零遮挡时整帧跳过 JFA dispatch，非空场景改 workgroup 共享内存规约后每组单次原子写。
8. **色彩空间线性化收口**：场景纹理采样线性化（sRGB 内部格式或采样侧解码），光照累加全程 Linear；`postprocess_combined.frag` 仅在最终输出执行单一 ACES Filmic + sRGB 编码，消除 sRGB 直接参与光照累加与重复 Gamma。

### 3.3 Phase 2 — 架构级现代化（2-4 周，4 项）

1. **编译型 RenderGraph**（AD-6）：虚拟资源 Handle、声明式读写声明、反向可达性 Pass Culling、区间重叠 Memory Aliasing、图持久化与单次编译。
2. **Gameplay/Engine 渲染流解耦**：Tilemap、Entity、VFX、PostProcess 纳入 RenderGraph 编排，消除 `m_sceneRT ↔ s_hdrSceneBuffer` 双重 Blit（`GameplayState.cpp:984-1100`、`RenderSystem.cpp:1380-1397`）。
3. **紧凑 RenderInstance**（AD-7）：128B→32B，带宽降 75%。
4. **显存池与 Resize 防抖**（AD-8）：分级 `GPUTexturePool` + 200ms 防抖 + 池化复用。

### 3.4 与既有 conductor 整改 Track 的关系

| 既有 Track | 关系 |
|---|---|
| M0-A `gpu_production_hdr_gi_closure_20260726` | Phase 1-3（GI 纠偏）与其范围重叠，本方案采用其离屏 HDR/GI 合同（`External scene seed → … → Composite` 顺序、`OccluderViewKey` 失效键）作为约束输入，不再在该 Track 目录下追加任务 |
| M0-B `gpu_rendergraph_resource_foundation_20260726` | Phase 2-1 是其 compiled plan 目标的超集（新增 Pass Culling + Aliasing + 持久化），推进证据落 `docs/plans/` 新计划 |
| M0-C `gpu_hardware_validation_gate_20260726` | 每阶段完成后的门禁执行者，本方案产出物交其验证 |
| M1-D `gpu_jfa_incremental_update_20260816 审查项 P0-4` | Phase 0 修复其漏绑 mask 缺陷，增量 JFA 逻辑本身不动 |

## 4. 算法决策记录（设计期定型）

> 以下决策在设计阶段锁定，计划/实施阶段只做伪代码展开与验证，不再二次选型。

### AD-1 GPU 间接参数生成 — 单线程 Compute 直写 Command Buffer

- **选型**: 1×1×1 dispatch 的 `indirect_args.compute`，读实例计数 SSBO（atomic counter），写 `DrawArraysIndirectCommand` / `DrawElementsIndirectCommand`。
- **理由**: GL 4.3 下无 `NV_command_list` 等扩展依赖；单线程避免命令写入竞态；计数→命令的依赖由 `GL_COMMAND_BARRIER_BIT` 精确屏障表达。较 CPU 回读方案消除整条流水线排空；较 ARB_indirect_parameter 方案少一个扩展依赖且计数同时可被复用。
- **约束**: command buffer 归属各子系统持久分配；屏障仅命令位，不触发 `Barrier::All`。

### AD-2 GPUFluidParticle std430 兼容布局 — 纯字段重排（零类型双关）

- **选型**: 仅重排字段顺序，类型与名称保持不变：`Vector2 position`(offset 0) + `Vector2 velocity`(offset 8) → 合计 16B 天然对齐；`Vector4 color`(offset 16)；`float density`(32) + `float pressure`(36) + `float lifetime`(40) + `uint32_t flags`(44)。总计 48B，`alignas(16)`，无隐式 padding，C++ 与 GLSL std430 偏移完全一致。
- **理由**: std430 中 `vec4` 基准对齐 16B，`color` 紧随两个 `vec2` 之后恰好落在 offset 16；尾部四个 4B 标量自然填满至 48。相比"把 flags/density 等打包进 `vec4.w`"的方案——那需要对 `uint32_t` 位掩码做 `floatBitsToUint` 类型双关，存在 denormal/NaN 规范化破坏位模式的隐患——纯重排零转换开销、零语义歧义，GLSL 端字段名与类型原样保留。48B 总量与 ABI V5 声明一致，仅 layout 版本号递增。
- **同步**: `gpu_abi.glslinc` 重新生成、layout 快照测试更新、`offsetof` 断言补齐（0/8/16/32/36/40/44）。

### AD-3 Radiance Cascades 方向性恢复 — 固定角度扇区 Probe Atlas

- **选型**: 每级联每 probe 保留 **8 个方向扇区**（L0）至 **2 个扇区**（L5），扇区数随级联深度递减 `directions_k = max(2, 16 >> k)`；probe atlas 纹理布局 `(probeGridW × probeGridH × directions_k)` 打包进 RG16F 数组纹理，`vec2(dirX, dirY)` 编码单位方向。级联合并改为论文标准：子级 probe 的射线命中上一级时，对上一级做**空间双线性 + 角度最近扇区**插值后沿 RTE 累加 $L_{k}(x,\omega)=\int_{0}^{t} e^{-\sigma s} E(s) ds + e^{-\sigma t} L_{k+1}(x+t\omega,\omega)$；射线步进取 $\min(d_{occluder}, d_{emissive})$ 双 SDF。
- **理由**: 2D RC 的方向维度是各向异性间接光（方向性溢色、镜面感）的来源；扇区数随级联递减与射线张角守恒一致（级联越深单射线张角越大，方向分辨率需求越低），atlas 内存增长受控（估算 +18MB @6 级联 half-res）。`farBlend` 标量混合被 RTE 传输项替换，物理正确且消除发灰。
- **辐照度汇聚（Irradiance Gathering）**: `GICompositePass` 对 L0 每个 probe 的全部方向扇区执行 2D 漫反射积分：$E(x,n)=\frac{1}{N}\sum_{k} L_{0}(x,\omega_k)\,\max(0,\,n\cdot\omega_k)$（均匀环形扇区权重，N = L0 扇区数；表面法线 n 取 HeightField 梯度）。Composite 阶段禁止在此积分之外再做额外的角度平均或颜色混合。
- **回退**: 扇区数为 tier 配置项，Low/Medium 可退回 1 扇区（等价现状各向同性）。

### AD-4 GI 时域去噪 — 3×3 邻域方差裁切（Color AABB Clipping）

- **选型**: 保留 `GiHistory` ping-pong；每像素计算当前帧 3×3 邻域的 mean/σ，构造 `clipBox = mean ± 1.25σ`（γ=1.25，可调）；历史样本 reproject 后 clamp 到 clipBox 再以 `α=0.88~0.92` 混合。光源/emissive 版本变化不再触发全屏 `resetHistory`，改为**失效矩形联合**（受影响光源的屏幕 AABB 外扩 2×lightRadius）局部重置。
- **理由**: 方差裁切是 TAA/Denoiser 标准收敛手段，能同时抑制鬼影与闪烁；局部失效把"任意光变→全屏闪"降为局部 1-2 帧瞬态。备选的 variational clustering 实现复杂度不匹配 2D 场景收益。
- **验证**: 相机匀速平移 + 单光源强度正弦调制场景，历史帧差分 RMS 低于阈值且无可察觉闪烁。

### AD-5 SPH 邻居搜索 — 计数排序网格哈希（Counting Sort Grid Hash）

- **选型**: 复用 `v5_fluid_gridhash.comp` 的 cell 编码；新增 **O(N) 计数排序**两遍内核：pass A 统计每 cell 粒子数 → 前缀和（单组 scan，≤4K 粒子规模下串行 scan 于 1 个工作组内完成，无需多组树形归约）得到 `cellStart/cellEnd`；pass B 按 `cellStart[p] + atomicAdd(offset[cell],1)` 重排粒子索引到紧凑数组。邻居查询遍历自身 + 8 邻 cell（2D），每粒子平均候选 ≈ ρ·(3h)²，与粒子总数解耦。
- **理由**: 计数排序桶构建无比较、无二叉结构，GPU 友好；4K~10K 粒子量级前缀和单工作组串行 scan 足够（<0.01ms），避免 Blelloch 多 pass 扫描的实现复杂度。查询侧从 O(N²) 降为 O(N·k)，k≈30。
- **约束**: 保持 `v5_fluid_neighbor_search.comp` 输出合同（邻居列表格式）不变，Density/Force/Integrate 内核零改动。

### AD-6 编译型 RenderGraph — 声明式 DAG + 反向裁剪 + 区间首次适配别名

- **选型**:
  - **资源模型**: `RGTextureHandle` / `RGBufferHandle` 虚拟句柄 + 声明式 `Read(usage)` / `Write(usage)` / `RenderTarget(attachment)`；物理资源由编译器解析绑定。
  - **Pass Culling**: 从外部输出（backbuffer / 指定 export 资源）出发反向 BFS 可达性遍历，不可达 Pass 整体剔除；剔除率由调试 HUD 上报。
  - **Memory Aliasing**: 每个 transient 资源得到 `[firstUse, lastUse] pass 区间`；按区间重叠做**区间图着色**，同一颜色复用同一物理分配；分配器采用首次适配（first-fit）+ 256B 对齐，块内偏移打包。Vulkan 式 aliasing barrier（write-after-read 交接内存屏障）在 GL 4.3 下以 `glTextureBarrier`/`glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT)` 等价表达。
  - **持久化**: 图拓扑 hash + 声明 hash 缓存编译产物（pass 顺序、绑定表、屏障表、别名表），拓扑未变帧直接复用，仅在声明变化时重编译。
- **理由**: 反向可达性是死代码消除的标准算法，成本 O(V+E)；区间图着色是 interval graph 最优着色（贪心即可达最优），别名收益理论上限即区间重叠总面积；首次适配实现简单且碎片率在数百资源量级下足够。对比 buddy/ TLSF 分配器，first-fit 在块数 <1000 时性能与碎片均无劣势。
- **安全阀**: aliasing 首期仅作用于 `TransientResourcePool` 内的 transient 颜色/深度目标；persistent 资源（GiHistory、SDF 等）不参与；调试模式下可全局关闭别名以二分定位疑难 Bug。
- **度量**: 目标瞬态 VRAM 峰值 −40%（1080p Ultra，含 half-res GI 链的全部中间目标）。

### AD-7 紧凑 RenderInstance — 32B 布局（位置字段保留 float32）

- **选型**: `Vector2 position`(8B, float32) + `Vector2 prevPosition`(8B, float32) 保持全精度；剩余 16B 打包视觉属性：`int16 rotSin` / `int16 rotCos`（SNorm16 旋转分量）、`uint16 radiusHalf`、`uint16 textureId`、`uint16 materialId`、`uint8 glowIntensity`、`uint8 statusMask`、`uint16 flags`，合计 30B，`alignas(16)` 填充至 32B。最终字段集以 `entity_mdi.vert` / `cull.compute` 实际消费合同为准，在计划阶段完成映射（uv 偏移、anim/layer 等按需置换打包字段，位置/前帧位置不可降精度）。
- **理由**: **位置字段禁止 fp16**——IEEE 754 half 仅 10 位尾数，ULP 在值域 [1024,2048] 为 1.0、[2048,4096] 为 2.0 世界单位，中远距离实体会出现 1~2px 阶跃的量化抖动；此前"误差 <0.06px"的结论有误，予以纠正。float32 位置 + prevPosition 保插值物理平滑；旋转/半径/状态类属性对 SNorm16/UNorm16/UNorm8 量化不敏感。SoA 布局备选改动 MDI 顶点拉取合同面大，不采用。带宽仍从 128B 降至 32B（−75%）。
- **同步**: `entity_mdi.vert` / `cull.compute` 的 InstanceData 镜像更新；ABI 版本递增。

### AD-8 GPUTexturePool — 分级池化 + 防抖

- **选型**: 按 `(format, tier)` 分桶，桶内尺寸用**尺寸类别规整**（如 1080p 全屏类、half-res 类、atlas 类各持标准尺寸）而非精确尺寸池化；resize 事件进入 200ms 防抖窗口，窗口内沿用旧目标（内容拉伸 ≤1 帧可接受）或直接黑帧提示；窗口结束后按新尺寸从池中租借，旧目标延迟 3 帧归还（等待 in-flight 消费完成）。
- **理由**: 尺寸类别规整消除长尾碎片；延迟归还遵守 GL 同步语义（in-flight FBO 不可销毁）。对比每次精确匹配池，类别化的池命中率 >90% 且代码复杂度低一个量级。

### AD-9 视锥剔除时空同步 — 扫掠保守 AABB

- **选型**: culling compute 中以 `min(prev, cur)` / `max(prev, cur)` 构造扫掠框，外扩半径取 `r × (1 + |cos45°| + |sin45°|)` 即 `2.414r`（对旋转 45° 的外接对角），并以插值帧 `mix(prev, cur, α)` 的实际到达边界再外扩 `r`，合计保守半径 `3.5r`（现状 4.0r 偏大但安全，收敛到 3.5r 即可）。
- **理由**: 剔除必须对插值帧的连续可达域保守；旋转外接按对角而非轴对齐外包（现状 4.0r 是对的），修正点仅在"插值帧时空不同步"。3.5r 与 4.0r 的差额用于回收剔除收敛边界，行为差异可忽略，保留 4.0r 亦可接受——**实施时优先只修时空不同步，不改半径**。

## 5. 跨系统合同

| 合同 | 约束 |
|---|---|
| 间接命令所有权 | Text/Loot/MDI 的 indirect command buffer 由 GPU 独占写入；CPU 端仅初始化清零。任何 CPU 写入需走 persistent 三缓冲帧槽 |
| 计数回读 | 唯一合法回读路径为滞留 ≥1 帧的 readback ring（P0-1 LightCulling overflow 统计同此） |
| 实例计数 Getter | `GetActiveQuadCount()` / `GetLootCount()` 等 GPU 子系统计数 Getter 语义统一为"滞留 1 帧的 GPU 统计快照"：同帧内多次调用返回缓存值，当帧不发起同步回读；debug 构建经 readback ring 每帧刷新 |
| ABI 版本 | AD-2 / AD-7 各递增 layout 版本；`gpu_abi.glslinc` 生成链路唯一，CI layout 快照强校验 |
| RenderGraph 声明 | Phase 2 起 Pass 注册必须声明全部读写资源与 usage；未声明读取在 debug 构建下校验失败 |
| 帧稳定性 | 任意阶段完成帧不得出现黑帧；feature 失败走既有回退链（`render.gi.enabled=false` 等） |
| SPH 边界 | 所有修复仅限开发 opt-in 路径生效；shipped tier 维持 NO-GO 默认关闭 |

## 6. 验收标准

- [ ] Phase 0: 9 项各自单测/集成证据 + `build.bat` + `ctest -C RelWithDebInfo -L unit,integration,ci` 全绿；Valid GPU P95 帧耗时相对基线下降 ≥1.0ms（LightCulling+Text+Loot 回读消除的合计下限）；非零原点排序用例与 GL `>=` 版本探测契约断言通过；`Initialize()` 失败注入后无 GL 对象泄漏。
- [ ] Phase 1: 主渲染线程零每帧同步回读（GL debug 断言 + 代码审查双证）；RC 方向性用例（点光源 45° 侧照产生方向性溢色）与 Composite 余弦加权积分用例通过；SPH 4096 粒子邻居搜索内核耗时 ≤0.3ms（现状 O(N²) 基线对比）；GI 光源调制闪烁用例通过；空场景 JFA dispatch 为 0 且无全局原子加法（trace 佐证）；色彩空间全链路单一 Gamma 编码（golden image 差异审查）。
- [ ] Phase 2: 瞬态 VRAM 峰值 −40%（GPUTimerHUD/ glGetMemoryInfo 采样）；双重 Blit 消除（frame trace 无中间 LDR 拷贝）；Pass Culling 调试 HUD 上报剔除数；resize 防抖拖拽帧率波动 <10%。
- [ ] 全阶段: M0-C 硬件门禁 nightly 通过；无新增 ABI/Binding 冲突。

## 7. 风险与未决问题

| 风险 | 缓解 |
|---|---|
| AD-3 RC Atlas 显存 +18MB 且寄存器压力上升导致 Ultra 帧预算超标 | 扇区数 tier 化（High 4 扇区起步）；必要时 L0 降 8→6 |
| AD-6 aliasing 在 GL 驱动差异下的隐性 hazard | 首期仅 transient 颜色/深度；调试开关全局关闭；M0-C nightly 多驱动覆盖 |
| 间接参数化后 Text/Loot 计数不可见影响调试 | 滞留帧 readback ring 保留统计通道（仅 debug 构建） |
| Phase 2 与 M0-B conductor Track 的证据双轨 | 以 `docs/plans/` 新计划为准一轨推进，M0-B 仅保留门禁执行角色（§3.4） |
| 色彩空间线性化改变既有画面观感 | golden image 对照 + 分项灰度开关（`render.color.linearPipeline`），美术验收窗口后再默认启用 |

**未决（需用户决策）**:
1. Phase 1 与 Phase 2 是否允许并行开工（RC 纠偏与 RenderGraph 编译无直接依赖，可并行；但集成窗口建议串行以降低回归定位成本）。
2. AD-7 紧凑实例是否顺带迁移 GPUText/GPULoot 的实例布局（建议：否，仅 Entity MDI）。

## 8. 后续动作

1. 本设计评审通过后，按 `docs/workflows/planning.md` 产出 `docs/plans/2026-08-16-gpu-quickfix-p0-plan.md`（Phase 0 计划，先行）。
2. Phase 1 / Phase 2 计划在 Phase 0 验收后分别产出，算法均以本设计 §4 为唯一定型依据。
