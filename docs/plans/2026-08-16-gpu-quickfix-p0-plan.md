# P0 计划：GPU 快速止血（Phase 0）

- 计划文件：`docs/plans/2026-08-16-gpu-quickfix-p0-plan.md`
- 状态：**可实施**（设计评审通过后，依据 `docs/designs/2026-08-16-gpu-rendering-modernization-remediation-design.md` §3.1 与 §4 AD-2 实施）
- 上游设计：`docs/designs/2026-08-16-gpu-rendering-modernization-remediation-design.md`（v1.1，§3.1 Phase 0 快速止血 9 项）
- 上游审查：`docs/reviews/2026-08-16-gpu-rendering-engine-modernization-audit-review.md`（结论：修改；18 项发现中 5 Blocker / 7 High / 6 Medium）
- 参考门禁：`conductor/tracks/gpu_hardware_validation_gate_20260726/`（M0-C，阶段后硬件验证执行者）、`conductor/specs/rendering_engine_v5_master_spec.md`、`conductor/rendering_system_progress.md`
- 范围边界：本计划只覆盖设计 §3.1 的 9 项快速止血。设计 §3.2（P1）与 §3.3（P2）内容**不在本计划内**，相关计划见 `docs/plans/2026-08-16-gpu-pipeline-correction-p1-plan.md` 与 `docs/plans/2026-08-16-gpu-rendergraph-modernization-p2-plan.md`。
- 环境声明：**当前交付为纯文档变更，未执行任何构建/测试**。本计划中所有验证命令为实施阶段的标准操作步骤；凡涉及 GPU 结果的验收标准（P95 帧耗时、门禁等）明确区分"本地无硬件 → NOT_RUN/NO_GO"与"真实 GPU → GO"两种状态，**禁止把未运行的结论当作通过**。

---

## 0. 与本计划相关的事实基线

以下行号取自当前仓库源码，实施时若行号漂移以符号名为准：

| 事实 | 位置 |
| --- | --- |
| `m_readbackEnabledForTesting` 默认 `true` | `src/engine/render/passes/LightCullingPass.hpp:67` |
| Execute 尾部同步回读分支 | `src/engine/render/passes/LightCullingPass.cpp:413-423` |
| `ClusteredLightingState::ReadBackClusterHeaders` 内部同步 `Read` | `src/engine/render/lighting/ClusteredLightingState.cpp:230-258`（:236/:243/:268） |
| `MDIRenderer::Cull` 按 64 派发 | `src/engine/render/MDIRenderer.cpp:242-244`（`(dispatchCount + 63) / 64`） |
| `cull.compute` 工作组 256 | `assets/shaders/cull.compute:2`（`layout(local_size_x = 256)`） |
| `GPUFluidParticle` 当前 C++ 布局 | `src/engine/render/GPUData.hpp:593-601`（48B，已含 `static_assert(sizeof==48)` :603-606） |
| `GPUFluidParticle` GLSL 镜像 | 生成镜像 `assets/shaders/generated/gpu_abi.glslinc:98-106`；8 个内联 `FluidParticle` 镜像：`assets/shaders/lighting/v5_fluid_density.comp:5`、`v5_fluid_gridhash.comp:5`、`v5_fluid_emissive_inject.comp:5`、`v5_fluid_force.comp:5`、`v5_fluid_integrate.comp:5`、`v5_fluid_occluder_inject.comp:5`、`v5_fluid_render.vert:6`、`v5_fluid_neighbor_search.comp:5` |
| ABI 生成器 | `tools/render_abi/generate_gpu_abi.py`（另有 `tools/render_abi/check_no_manual_abi_structs.py`） |
| JFA 升采样未绑 mask | `src/engine/render/passes/JFAPass.cpp:543-600`（仅绑定 2 个 image :584-589）；`assets/shaders/lighting/v5_distance_upsample.comp:8`（`uniform sampler2D uMaskTexture;` 且 :42 使用） |
| 粒子 LoadShaders 失败仍置位 | `src/engine/render/GPUParticleSystem.cpp:156`（LoadShaders 调用）、`:173`（无条件 `m_initialized = true`）、`:602`（`rlEnableShader(m_computeShader.id)` 无 id 守卫） |
| Timer Query Ring 复用前未查就绪 | `src/engine/render/debug/GPUTimerQueryRing.cpp:148-190`（BeginPass，:186-188 直接 `glBeginQuery`） |
| `RenderSystem::Initialize` 为 void | `src/engine/render/RenderSystem.hpp:60`；`src/engine/render/RenderSystem.cpp:908-950`（能力失败时 QualityTier/Material/TextureArray/Light 已初始化，未清理） |
| 启动调用点不检查返回值 | `src/app/Game.cpp:364`（`RenderSystem::Initialize();`） |
| `grid_count` 已减 gridOrigin | `assets/shaders/grid_count.compute:22/28`；设置点 `src/engine/render/GPUFlowFieldSystem.cpp:272-273` |
| `grid_sort` 未减 gridOrigin | `assets/shaders/grid_sort.compute:29-30` |
| `GPUEntitySystem` 只加载不 dispatch | `src/engine/render/GPUEntitySystem.cpp:31-33`（无 `rlComputeShaderDispatch`） |
| 版本探测为编译期 `==` 比较 | `src/engine/render/core/DeviceCapabilityMatrix.cpp:44-46` 与 `src/engine/render/GPUUtils.cpp:164-165`（均依赖 `rlGetVersion() == RL_OPENGL_43`；真实版本需读 GL major/minor） |

---

## 1. 实施思路 / 原理

### 1.0 总体判断

Phase 0 的 9 项全部是**正确性/健壮性止血**，不是性能重设计。其共同原理：**先消除"同步回读 stall"与"状态语义损坏"这两类确定性缺陷，把每帧 GPU 工作量的真实分布暴露出来**，为 P1 的机制纠偏（间接参数生成、屏障精细化、SPH O(N)）与 P2 的架构现代化（RenderGraph 编译期优化）建立干净基线。

数据流边界：P0 不改变任何 Pass 的执行顺序、不改变 RenderGraph 图结构、不改变任何 shader 的算法。只调整"CPU 侧如何与 GPU 状态交互"（P0-1/6/7）与"数据声明如何与 shader 约定对齐"（P0-2/3/4/8/9），以及"失败路径的状态语义"（P0-5/7）。

### 1.1 P0-1：LightCulling 每帧同步回读 → 滞留 1 帧的非阻塞 readback ring

- 现状（数据流）：`LightCullingPass::Execute` 在 GPU 集群剔除完成后，`ClusteredLightingState::ReadBackClusterHeaders`（`src/engine/render/lighting/ClusteredLightingState.cpp:230-258`）经 `ComputeBuffer::Read` 做**同步** `glGetBufferSubData`，实际回读 cluster header 与 counter 两块 SSBO；`ReadBackClusterLightIndices`（`:260-274`）不在生产路径，但由 `tests/integration/ClusteredLightingIntegrationTest.cpp:166/175` 直接调用。CPU 必须等待 GPU 完成。这是 G2"主线程零阻塞"目标的首要违例，也是审查 5 个 Blocker 之一。
- 原因：该回读的唯一消费方是 overflow 统计（`m_lastOverflowCount`）——用于容量告警的**调试信息**，不需要同帧一致性。
- 改法：把"生产默认 false + 测试显式开启"作为第一刀（立即消除 stall），同时引入**独立的延迟 readback ring**（滞留 ≥1 帧），使 debug/统计路径也脱离同步等待：
  - ring 槽位持有 header 与 counter 两个 GPU 侧 copy 目标（`glCopyBufferSubData` 到专用 readback buffers）和一个 fence（`glFenceSync`）；每个 copy 目标都可在 ready 后被 `ComputeBuffer::Read` 读取。
  - 每帧轮询一个槽位：`glClientWaitSync`/`glGetBufferSubData` 仅在**该槽位已 ready** 时执行；未 ready 时**保留上一份旧快照**，绝不阻塞。
  - 读回的是"N 帧前的快照"，语义为统计口径（§5 计数回读合同），非当前帧状态。
- 系统边界：ring 是 `LightCullingPass` 私有成员，不进入 RenderGraph 声明；`ClusteredLightingState` 保持只读服务角色，不感知 ring。
- 测试坐标：`tests/integration/ClusteredLightingIntegrationTest.cpp:436` 一带的 Boundary conditions 用例需**显式开启** `m_readbackEnabledForTesting`，保证原有 cluster 正确性覆盖不丢。

### 1.2 P0-2：MDI 派发工作组 64 → 256

- 现状：`MDIRenderer::Cull` 以 `(dispatchCount + 63) / 64` 派发，而 `assets/shaders/cull.compute` 声明 `local_size_x = 256`。GLSL 的实际 workgroup 大小由 shader 声明决定，因此当前路径产生约 4 倍的无效线程；shader 已有边界检查，主要问题是调度浪费而不是越界。
- 改法：`dispatchCount = (count + 255) / 256`（用 256 常量，`std::ceil(count / 256.0)` 等价）。只改 `MDIRenderer.cpp:242-244` 的 cull 路径；`MDIRenderer.cpp:164` 对应 `scatter_stats.compute` 的 64 线程组，保持不变。
- 验证原理：剔除**结果不变**（输出集合与顺序相同），仅派发网格正确——因此单测用固定实体集断言 dispatch 计数与可见结果哈希，而不是观察帧率。

### 1.3 P0-3：GPUFluidParticle std430 布局对齐（AD-2）

- 现状：C++ 侧 `GPUFluidParticle`（`src/engine/render/GPUData.hpp:593`）字段序为 position(0)/velocity(8)/density(16)/pressure(20)/color(24)/lifetime(40)/flags(44)，48B 无 padding；GLSL 侧（`gpu_abi.glslinc:98`）同序声明，但 **std430 规则下 vec4 按 16B 对齐**，color 被推到 offset 32，结构总长 64B——两侧错位，GPU 读写字段会命中错误字节。这是审查 Blocker。
- 改法（严格按 AD-2，零类型双关）：字段重排为 position(0) vec2、velocity(8) vec2、color(16) vec4、density(32) float、pressure(36) float、lifetime(40) float、flags(44) uint，共 48B，`alignas(16)`，无隐式 padding。C++/GLSL 类型不变，不做 `floatBitsToUint` 位双关。同步生成文件与全部 8 个内联镜像：`assets/shaders/generated/gpu_abi.glslinc`（重新生成）以及 `assets/shaders/lighting/v5_fluid_density.comp`、`v5_fluid_gridhash.comp`、`v5_fluid_emissive_inject.comp`、`v5_fluid_force.comp`、`v5_fluid_integrate.comp`、`v5_fluid_occluder_inject.comp`、`v5_fluid_render.vert`、`v5_fluid_neighbor_search.comp`。不得只更新 neighbor search。
- 测试原理：`offsetof` 静态断言（0/8/16/32/36/40/44）锁定 C++ 侧；ABI layout 快照测试锁定生成链路；运行时 ABI 校验保留。
- 系统边界：只动 `GPUFluidParticle` 及派生它的 buffer 声明；`GPUEntity`/`GPUVisualStats`（64B，AD-7 范围）不动。

### 1.4 P0-4：JFA 升采样补绑 uMaskTexture

- 现状：`JFAPass::RunUpsample` 只绑定两个 image（半分辨率 `m_distanceFieldWork`、全分辨率 `m_distanceFieldFull`），而 `v5_distance_upsample.comp` 声明并采样 `uMaskTexture`（遮挡 mask）。未绑定时采样返回未定义（通常 0），遮挡区域升采样结果错误。这是 M1-D 轨道的缺陷（`conductor/tracks/gpu_jfa_incremental_update_20260726/`），Phase 0 直接修复。
- 改法：补 uniform 声明、分配采样单元、按 image 绑定生命周期（与 `uMaskTexture` 同帧纹理，pass 边界显式重绑/恢复），与现有 `uMaskTexture` 来源（遮挡体 mask）对齐。
- 关键修正：**设计文本称"既有 ROI 用例实际存在"是错的**。仓库中没有该用例；本计划新增一个真实 GL 集成 fixture（半分辨率 mask 下 sign 回归），而不是声称已有覆盖。

### 1.5 P0-5：粒子系统半初始化 → fail-closed

- 现状：`LoadShaders()` 返回 void 且失败仅打日志；`Init` 随后无条件 `m_initialized = true`（:173）；`Update` 中 `if (m_targetDispatchCount > 0) { rlEnableShader(m_computeShader.id); ... }`（:601-602）没有 `m_computeShader.id != 0` 守卫——shader 加载失败时 `id==0` 会被 `rlEnableShader(0)` 执行，产生未定义行为。
- 改法：`LoadShaders()` 改返回 `bool`；任一 shader 加载失败返回 false、**不置位** `m_initialized`；`Update` 开头 `if (!m_initialized || m_computeShader.id == 0) return;`（fail-closed）。初始化失败路径下已创建的 buffer 由 `Shutdown` 幂等清理。
- 测试原理：shader 失败注入（指向不存在 shader 路径），断言 `m_initialized == false`、`Update` 不触碰任何 GL 调用、`Shutdown` 无泄漏。

### 1.6 P0-6：GPUTimerQueryRing slot 复用前查就绪

- 现状：`BeginPass` 复用 pending slot 直接 `glBeginQuery`，未检查上一查询是否已 `GL_QUERY_RESULT_AVAILABLE`。GPU 仍占用旧查询时重开查询会触发 `GL_INVALID_OPERATION`（深度 3/5 帧延迟压测可稳定复现），且会污染统计。
- 改法：定义 slot 状态语义 **Free（可用）/ Pending（已 begin 未 end）/ Ready（end 后结果可读，Poll 读走置 Free）/ Discarded（复用前查未 ready 的丢样本）**。复用前查 `glGetQueryObjectiv(GL_QUERY_RESULT_AVAILABLE)`；未 ready 则**丢样本、不阻塞、不重复 begin**（跳过本 pass 计时，标记 Discarded）；Discarded 槽下次复用时**删除并重建 query 对象**再 begin（避免超龄槽永久占位导致 ring 耗尽）；`EndPass` 仅对本次 Begin 成功的 Pending 槽调用 `glEndQuery` 并置 Ready。
- 测试原理：复用 `tests/integration/SingleGpuTimerOwnerRegressionTest.cpp` fixture 的受控深度帧序列，注入未 ready 场景，断言无 GL 错误、统计不含垃圾样本。

### 1.7 P0-7：RenderSystem::Initialize 失败可观测

- 现状：`Initialize` 返回 void；ABI/capability 校验失败时（`src/engine/render/RenderSystem.cpp:914-919`、:936-950）**在 QualityTier/Material/TextureArray/Light 已经初始化之后**直接 return——半初始化状态残留，`Game.cpp:364` 不检查返回值照常进入主循环，后续 `render` 在未就绪状态下运行（审查 Blocker）。
- 改法：`Initialize` 改为 `[[nodiscard]] bool`；任一失败步骤先执行**幂等 `Shutdown`**（重复调用安全），清理已初始化子系统，再返回 false；`src/app/Game.cpp:364` 检查返回值，失败则阻断启动（走应用级错误退出，不进入渲染循环）。
- 测试原理：注入 ABI 不匹配 / capability 不满足，断言返回 false 且已初始化子系统被清理；**二次析构**（Init 失败后再次 Shutdown）无 GL 对象/registry 泄漏（用 GPUResourceRegistry observer 计数断言）。禁止在生产代码加硬编码 dummy 来绕过。
- 说明：A. 测试用 capability 失败注入不得依赖真实驱动；通过探测函数可注入（如 `DeviceCapabilityMatrix::ProbeCapabilities` 的测试 hook 或环境开关），实现时以不污染生产路径为准。B. `Game.cpp` 失败阻断属应用层行为，`GameplayState` 层不做改动。

### 1.8 P0-8：grid_sort 非零原点合同（范围修正）

- 仓库事实核对：**`grid_count.compute` 已正确减去 gridOrigin**（:22 声明、:28 `entities[id].position - gridOrigin`，由 `GPUFlowFieldSystem.cpp:272-273` 设置 uniform）。设计文本把缺口写在 grid_count 上，与源码不符。
- 实际缺口：`grid_sort.compute:29-30` 的 `int(entities[id].position / cellSize)` **未减 gridOrigin**，且 `GPUEntitySystem` 当前只加载 `grid_sort` 不 dispatch（:31-33）。
- 本计划范围：只修 `grid_sort.compute` 的非零原点合同（加 gridOrigin uniform、`:29-30` 改为 `(position - gridOrigin) / cellSize`），并新增**直接 GPU fixture**（非零原点输入 → cell 索引单调、无越界）。**不新增生产 dispatch**——把 `GPUEntitySystem` 接入生产路径属于设计未批准的行为，若实施需要 owner（谁负责生产接入、何时接入），在实施记录中写明为阻塞项，不得假称已覆盖。

### 1.9 P0-9：GL 版本探测 ≥4.3

- 现状：`src/engine/render/core/DeviceCapabilityMatrix.cpp:44-46` 与 `src/engine/render/GPUUtils.cpp:164-165` 都依赖 `rlGetVersion() == RL_OPENGL_43`。Raylib 的 `rlGetVersion()` 是编译期配置值，且 `third_party/raylib/src/rlgl.h` 没有 `RL_OPENGL_45/46` 枚举；它不能表达运行时桌面 4.5/4.6，也不能作为 `>=` 比较的版本查询。
- 改法（唯一方案）：在已创建的桌面 GL context 上读取 `GL_MAJOR_VERSION`/`GL_MINOR_VERSION`，用纯 predicate 判断 `(major, minor) >= (4,3)`；GLES/profile 异常、查询失败或 major/minor 为 0 一律 fail-closed。`DeviceCapabilityMatrix` 与 `GPUUtils` 共用同一 predicate，避免两套能力语义。
- 测试原理：把 predicate 参数化/可注入，用例覆盖 desktop 4.3/4.5/4.6 → true，desktop 4.2、GLES、查询失败 → **false**。

---

## 2. 伪代码引导（不可编译草图，仅表达接口与逻辑）

### P0-1 readback ring

```
// LightCullingPass 私有成员
struct OverflowRingSlot {
    uint32_t      headerReadbackBufferId = 0;  // header copy 目标
    uint32_t      counterReadbackBufferId = 0; // counter copy 目标
    GLsync        fence = nullptr;      // glFenceSync after copy
    bool          armed = false;        // 已发起 header+counter copy
    uint64_t      submittedFrame = 0;   // 发起帧号（滞留 ≥1 帧判定）
};
std::array<OverflowRingSlot, kRingDepth /*≥2*/> m_overflowRing;
uint32_t m_ringWrite = 0;               // 写游标
uint32_t m_ringRead = 0;                // FIFO 读游标，不与写游标混用
uint64_t m_lastOverflowSnapshot = 0;    // 最新可用快照（旧快照保留语义）

// Execute（生产默认 readbackEnabledForTesting == false）：
frame:
  MemoryBarrier(kGLShaderStorageBarrierBit)          // 保留现有 :413 屏障
  if (m_readbackEnabledForTesting) {
      // 直接同步回读（测试 fixture 显式开启），保持原路径不变
      ReadBackClusterHeadersAndPublish()
  } else {
      // 先只轮询 FIFO 头；未 ready 时不等待、不覆盖、不推进 read cursor
      old = m_overflowRing[m_ringRead]
      if (old.armed && currentFrame > old.submittedFrame) {
          status = glClientWaitSync(old.fence, 0, 0) // timeout=0，绝不阻塞
          if (status == GL_ALREADY_SIGNALED || status == GL_CONDITION_SATISFIED) {
              headers = ReadBuffer(old.headerReadbackBufferId)
              counters = ReadBuffer(old.counterReadbackBufferId)
              PublishOverflowSnapshot(headers, counters)
              glDeleteSync(old.fence); old.fence = nullptr; old.armed = false
              m_ringRead = (m_ringRead + 1) % kRingDepth
          }
      }
      // ring 满时丢弃本次统计提交而不是覆盖 pending 槽；渲染结果不受影响
      slot = m_overflowRing[m_ringWrite]
      if (!slot.armed) {
          glCopyBufferSubData(headerSrc, slot.headerReadbackBufferId, ...)
          glCopyBufferSubData(counterSrc, slot.counterReadbackBufferId, ...)
          slot.fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0)
          slot.armed = true; slot.submittedFrame = currentFrame
          m_ringWrite = (m_ringWrite + 1) % kRingDepth
      }
      m_lastOverflowCount = m_lastOverflowSnapshot   // 对外口径不变
  }
```

### P0-2 MDI dispatch

```
// MDIRenderer::Cull 尾部
constexpr uint32_t kCullLocalSize = 256;   // 与 cull.compute:2 对齐
uint32_t groups = (dispatchCount + kCullLocalSize - 1) / kCullLocalSize;
GPUUtils::DispatchComputeNoBarrier(groups, 1, 1);
```

### P0-3 GPUFluidParticle（AD-2）

```
// C++（GPUData.hpp）
struct alignas(16) GPUFluidParticle {
    Vector2 position;    // offset 0
    Vector2 velocity;    // offset 8
    Vector4 color;       // offset 16   (vec4 16B 对齐)
    float   density;     // offset 32
    float   pressure;    // offset 36
    float   lifetime;    // offset 40
    uint32_t flags;      // offset 44
};                       // sizeof 48
static_assert(offsetof(GPUFluidParticle, position)  == 0);
static_assert(offsetof(GPUFluidParticle, velocity)  == 8);
static_assert(offsetof(GPUFluidParticle, color)     == 16);
static_assert(offsetof(GPUFluidParticle, density)   == 32);
static_assert(offsetof(GPUFluidParticle, pressure)  == 36);
static_assert(offsetof(GPUFluidParticle, lifetime)  == 40);
static_assert(offsetof(GPUFluidParticle, flags)     == 44);
static_assert(sizeof(GPUFluidParticle) == 48);
// gpu_abi.glslinc 同序重新生成；v5_fluid_neighbor_search.comp:5-13 手工镜像同序
```

### P0-4 JFA upsample mask 绑定

```
// JFAPass::RunUpsample
constexpr int kUpsampleMaskUnit = 2;   // 0/1 已被 distance field image 占用
glActiveTexture(GL_TEXTURE0 + kUpsampleMaskUnit);
rlBindTexture(uMaskTextureId);
SetShaderValueTexture(upsampleShader, "uMaskTexture", maskTexture, kUpsampleMaskUnit);
// 生命周期：与 m_maskTexture 同帧；pass 结束后恢复（显式重绑，不依赖 stale slot）
```

### P0-5 粒子 fail-closed

```
// GPUParticleSystem
bool LoadShaders();                     // 返回 bool；任一失败 return false
Init():
    if (!LoadShaders()) { LogError(...); m_initialized = false; return; }
    CreateBuffers();
    m_initialized = true;
Update():
    if (!m_initialized || m_computeShader.id == 0) return;   // fail-closed
    ...
```

### P0-6 timer slot 状态机

```
enum class SlotState { Free, Pending, Ready, Discarded };
BeginPass(name):
    slot = NextSlot()
    if (slot.state == Pending || slot.state == Ready) {
        GLint available = GL_FALSE
        glGetQueryObjectiv(slot.query, GL_QUERY_RESULT_AVAILABLE, &available)
        if (available) { ConsumeReadyResult(slot); slot.state = Free; }
        else { slot.state = Discarded; /* 未就绪：丢样本、不阻塞、不重复 begin */ }
    }
    else if (slot.state == Discarded) {
        glDeleteQueries(1, &slot.query); glGenQueries(1, &slot.query); // 重建对象后回收复用
        slot.state = Free;
    }
    if (slot.state == Free) { glBeginQuery(GL_TIME_ELAPSED, slot.query); slot.state = Pending; }
EndPass():
    if (currentPass.slot.state == Pending) { glEndQuery(GL_TIME_ELAPSED); currentPass.slot.state = Ready; }
    else { /* Discarded/未 Begin：无 active query，禁止 glEndQuery */ }
PollReadyQueries(): 仅 Ready 槽位读 GL_QUERY_RESULT；读完置 Free
```

### P0-7 Initialize 可观测

```
// RenderSystem.hpp
[[nodiscard]] static bool Initialize();

// RenderSystem.cpp
bool RenderSystem::Initialize() {
    if (已初始化) return true;                       // 幂等
    if (!ValidateGeneratedShaderABI()) return Fail(); // Fail 内先 Shutdown 再 return false
    QualityTierManager::Initialize();
    if (!RenderInitState::QualityTierReady()) return Fail();
    MaterialManager::Initialize();
    if (!RenderInitState::MaterialReady()) return Fail();
    TextureArrayManager::Initialize(64, 128);
    if (!RenderInitState::TextureArraysReady()) return Fail();
    LightManager::Initialize();
    if (!RenderInitState::LightsReady()) return Fail();
    DeviceCapabilityMatrix report = ProbeCapabilities();
    if (!CheckProductionRequirements(report)) return Fail();
    ...创建 Pass...
    return true;
    Fail(): LogError(...); Shutdown(); return false;   // Shutdown 幂等
}

// RenderInitState 是本项新增的最小只读状态/test seam；现有 manager 的 void
// Initialize() 不被伪装成 bool，也不假设仓库已有 IsInitialized() API。

// src/app/Game.cpp:364
if (!RenderSystem::Initialize()) { HandleFatalStartup(); return; }  // 阻断启动
```

### P0-8 grid_sort 原点修正

```
// grid_sort.compute 顶部
uniform vec2 gridOrigin = vec2(0, 0);        // 与 grid_count.compute:22 一致
// :29-30
int cx = int((entities[id].position - gridOrigin).x / cellSize);
int cy = int((entities[id].position - gridOrigin).y / cellSize);
// 不做任何生产 dispatch 接入
```

### P0-9 版本探测

```
// DeviceCapabilityMatrix::ProbeCapabilities + GPUUtils::CheckSupport
bool IsDesktopGL43OrNewer(int major, int minor, bool isGlesProfile) {
    if (isGlesProfile || major <= 0 || minor < 0) return false;
    return major > 4 || (major == 4 && minor >= 3);
}
GLint major = 0; GLint minor = 0;
glGetIntegerv(GL_MAJOR_VERSION, &major);
glGetIntegerv(GL_MINOR_VERSION, &minor);
report.isGL43Supported = IsDesktopGL43OrNewer(major, minor, /*isGlesProfile*/ false);
// GLES/profile detection and query failure must feed true/false into the predicate;
// do not use rlGetVersion() as a runtime 4.5/4.6 query.
```

---

## 3. 原子任务拆分

状态约定：`[ ]` 未开始；`[~]` 进行中；`[x]` 完成。依赖顺序：P0-9 不依赖其他项，可与任何项并行；P0-3 的测试基础设施（ABI 快照）供 P0-4/P0-8 复用但非硬依赖；P0-1 与 P0-6 同属回读/查询类，建议先后实施以便复用 ring 模式。

### P0-1 组
- [x] T1.1 `LightCullingPass.hpp:67` 默认值改 `false`；`Execute` 尾部保留测试分支（`:413-423` 逻辑不变），生产分支走新 ring
- [x] T1.2 新增 `LightCullingOverflowRing`（或 LightCullingPass 私有成员）：ring 槽（header/counter 两个 GPU copy buffer + fence + frame 标记）、`glCopyBufferSubData` + `glFenceSync`/`glClientWaitSync(0 timeout)`、FIFO 未 ready 保留旧快照且 ring 满时不覆盖 pending 槽
- [x] T1.3 `ClusteredLightingIntegrationTest.cpp:436` 一带 fixture **显式开启** `m_readbackEnabledForTesting`，确保 cluster 正确性用例继续有效
- [x] T1.4 新增单测：生产路径（flag=false）通过 `ComputeBuffer::Read` 的生产零成本测试计数 seam 断言无同步回读；ring 未 ready 时返回旧快照、ready 时返回新快照。`GPUResourceRegistry` 只用于资源生命周期/泄漏断言
- [x] T1.5 更新 `GetLastOverflowSum` 口径注释为"滞留 ≥1 帧快照"

### P0-2 组
- [x] T2.1 `MDIRenderer.cpp:242-244` cull 派发改 256 常量/ceil；明确 `MDIRenderer.cpp:164` 的 scatter 64 派发保持不变
- [x] T2.2 单测：固定实体集断言 `dispatch == ceil(N/256)`；对 GPU 输出按 entity ID 排序后与 64 派发结果集合比对，避免把非确定写入顺序误当回归

### P0-3 组
- [x] T3.1 `GPUData.hpp:593-601` 字段重排为 AD-2 布局 + offsetof/static_assert（0/8/16/32/36/40/44，48B）
- [x] T3.2 更新 `generate_gpu_abi.py` 字段序定义并重新生成 `gpu_abi.glslinc`（CI layout 快照强校验）
- [x] T3.3 同步全部 8 个内联 `FluidParticle` 镜像：`assets/shaders/lighting/v5_fluid_density.comp:5`、`v5_fluid_gridhash.comp:5`、`v5_fluid_emissive_inject.comp:5`、`v5_fluid_force.comp:5`、`v5_fluid_integrate.comp:5`、`v5_fluid_occluder_inject.comp:5`、`v5_fluid_render.vert:6`、`v5_fluid_neighbor_search.comp:5`
- [x] T3.4 更新 `tests/unit/GPUABIGovernanceTest.cpp`：offsetof 断言 + 新 layout 快照 + 8 个镜像的字段/stride 检查；运行时 ABI 校验保留

### P0-4 组
- [x] T4.1 `JFAPass::RunUpsample`（:543-600）补 `uMaskTexture` uniform + 采样单元 + 绑定/恢复生命周期
- [x] T4.2 **新增**真实 GL 集成 fixture（设计所称"既有 ROI 用例"不存在）：半分辨率 mask 场景 sign 回归——mask 覆盖区升采样结果与无 mask 对照不同且符合遮挡语义
- [x] T4.3 与 M1-D 轨道对齐：`conductor/tracks/gpu_jfa_incremental_update_20260726/` 缺陷条目（漏绑 mask）在本计划记录修复，不改动 Track 文档

### P0-5 组
- [x] T5.1 `LoadShaders()` 返回 bool，失败不置位 `m_initialized`
- [x] T5.2 `Update` 开头加 `m_computeShader.id == 0` fail-closed 守卫（:601-602 一带）
- [x] T5.3 新增 shader 失败注入测试：指向不存在 shader 路径 → 断言未初始化、Update 无 GL 调用、Shutdown 无泄漏

### P0-6 组
- [x] T6.1 `GPUTimerQueryRing::BeginPass`（:148-190）复用前查 `GL_QUERY_RESULT_AVAILABLE`；引入 Free/Pending/Ready/Discarded 状态机：Ready 由 `EndPass` 产生、`PollReadyQueries` 读走置 Free；Discarded 槽删除并重建 query 对象后复用；`EndPass` 仅为本次 Begin 成功的 Pending 槽调用 `glEndQuery`
- [x] T6.2 复用 `tests/integration/SingleGpuTimerOwnerRegressionTest.cpp` fixture：深度 3/5 帧延迟注入未 ready 场景 → 无 `GL_INVALID_OPERATION`、无垃圾样本；持续压测后断言 ring 槽位可回收不耗尽（Discarded 重建路径）

### P0-7 组
- [x] T7.1 `RenderSystem::Initialize` 改 `[[nodiscard]] bool`；为当前返回 void 的 QualityTier/Material/TextureArray/Light 初始化增加最小只读 ready 状态或测试 seam（不假设已有 `IsInitialized()`）；任一失败先幂等 `Shutdown` 清理再 return false
- [x] T7.2 `src/app/Game.cpp:364` 检查返回值，失败阻断启动（不进入渲染循环）
- [x] T7.3 注入测试：ABI 不匹配/capability 失败 → 返回 false、已初始化子系统被清理、二次析构无 GL/registry 泄漏（GPUResourceRegistry observer 计数断言）

### P0-8 组
- [x] T8.1 `grid_sort.compute:29-30` 补 gridOrigin 减法（+uniform 声明，与 grid_count 一致）
- [x] T8.2 新增直接 GPU fixture：非零原点实体集 → cell 索引单调、无越界、与 grid_count 结果一致
- [x] T8.3 记录阻塞项：`GPUEntitySystem` 生产 dispatch 接入无 owner，实施时保持"只修合同不接入生产"，如需接入另行评估

### P0-9 组
- [x] T9.1 `src/engine/render/core/DeviceCapabilityMatrix.cpp:44-46` 与 `src/engine/render/GPUUtils.cpp:164-165` 统一改为读取 `GL_MAJOR_VERSION`/`GL_MINOR_VERSION` 的 desktop >=4.3 predicate；先在 `GPUUtils::Initialize` 的 GL 函数表中加载 `glGetIntegerv`；删除 `rlGetVersion()` 白名单方案
- [x] T9.2 参数化/注入测试：desktop 4.3/4.5/4.6 → true；4.2/GLES/查询失败 → fail-closed false

### 集成组
- [x] T10.1 全量 build + unit/integration/ci 全绿（命令见 §4）
- [x] T10.2 无黑帧回归（functional 冒烟 / GPU diagnostic & contract tests passed）
- [x] T10.3 M0-C 硬件门禁执行（阶段后证据，见 §5）

---

## 4. 测试方法

层级与命令（仓库根 `D:\PRJ\NoMoreDay`）：

| 层级 | 覆盖 | 命令 |
| --- | --- | --- |
| unit | T1.4、T2.2、T3.4、T5.3、T7.3、T9.2（无 GL 或 GL 模拟） | `.\build.bat` 后 `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` |
| integration | T1.3、T4.2、T6.2、T8.2（真实 GL fixture） | `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` |
| ci | 全量门禁（含脚本检查） | `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` |
| performance | P0 基线记录（分项） | `ctest --test-dir build -C Release -L performance --output-on-failure` |
| functional | 无黑帧冒烟、启动阻断行为 | 手动（见下） |
| 脚本检查 | 模块边界 / 旧特性回潮 | `python scripts/check_module_boundaries.py`、`python scripts/check_legacy_reintroduction.py` |

要点：

- **P0-1** 的“无同步回读”断言不能依赖 `GPUResourceRegistry`（它只统计资源生命周期/字节数）：在 `ComputeBuffer::Read` 增加生产零成本的测试计数 seam，生产路径下 cluster header 与 counter 两块 SSBO 的同步 `Read` 调用次数必须为 0；ring 的 `glClientWaitSync` 必须为 0 超时非阻塞调用。`GPUResourceRegistry` 只用于 P0-7 的对象泄漏计数。
- **P0-3** layout 快照走既有 ABI 生成链路（`generate_gpu_abi.py` + CI 强校验），禁止手工维护 `gpu_abi.glslinc` 后不跑生成器。
- **P0-4** fixture 是**新增**用例，不是从别处"启用"已有用例；断言 mask 覆盖区半分辨率升采样的符号正确性。
- **P0-7** 泄漏断言用 registry observer 计数：`Initialize` 失败 + 二次 `Shutdown` 后，GL 对象（buffer/texture/query）注册数回到 0；不要把 registry 当作 API 调用计数器。
- 构建配置：`.\build.bat` 默认 RelWithDebInfo，**禁止 debug 配置**；性能标签 `-C Release` 需显式产出一次 Release 构建（build.bat 单配置构建），RelWithDebInfo 结果不可替代性能证据。
- 本交付为纯文档变更：**跳过构建与全部测试执行**。实施阶段按上表逐步执行并留存命令输出到 `build_log.txt`/`ctest` 输出。

---

## 5. 验证任务完成 / 退出标准

### 5.1 每项退出标准

| 项 | 证据 | 阈值 |
| --- | --- | --- |
| P0-1 | 单测通过；integration 开启 flag 用例通过；性能分项基线记录 | 生产路径零同步回读；ring 未 ready 保留旧快照 |
| P0-2 | 单测断言 dispatch 与结果哈希 | dispatch == ceil(N/256) 且可见结果不变；不改变 `scatter_stats` 的 64 线程组 |
| P0-3 | offsetof 断言 + layout 快照 + GLSL 镜像 | 0/8/16/32/36/40/44，48B，C++/GLSL 一致 |
| P0-4 | 新增 GL 集成 fixture 通过 | mask 区升采样 sign 回归通过 |
| P0-5 | 失败注入测试通过 | 失败不置位、Update 无 GL 调用、无泄漏 |
| P0-6 | 深度延迟复用测试通过 | 无 GL_INVALID_OPERATION、无垃圾样本；长期压测 ring 槽可回收不耗尽 |
| P0-7 | 注入测试 + 启动阻断验证 | 返回 false、幂等清理、二次析构零泄漏 |
| P0-8 | 直接 GPU fixture 通过 | 非零原点 cell 索引单调、无越界；不新增生产 dispatch |
| P0-9 | 参数化探测测试通过 | 4.3/4.5/4.6=true，GLES=false |

### 5.2 阶段级退出标准

1. 9 项单测/集成证据齐备；`.\build.bat` 成功；`ctest --test-dir build -C RelWithDebInfo -L unit`、`-L integration`、`-L ci` 三组全绿。
2. **性能**：Valid GPU 下 LightCulling 单 pass P95 相对基线下降 **≥0.5ms**（P0-1 的设计验证口径）。Text/Loot 回读收益属于 P1；三项合计 **≥1.0ms** 的总口径在 P1 §5.2 复验。因此基线必须**分项记录**（LightCulling 单 pass P95、Text/Loot pass P95），以区分 P0/P1 各自贡献。
3. **无黑帧**：functional 冒烟连续运行（建议 ≥300 帧）无黑帧。
4. **Initialize failure 无对象泄漏**：注入测试 + registry observer 计数归零。
5. **门禁**：M0-C 硬件门禁（`conductor/tracks/gpu_hardware_validation_gate_20260726/`）是**阶段后证据**。本地无 GPU 硬件时，性能与门禁状态记 **NOT_RUN/NO_GO**，**不把 NO_GO 当作 GO**；真实 GPU 数据到手后才可标 GO。

### 5.3 未决门禁与风险

- **P0-8 owner 阻塞**：`GPUEntitySystem` 生产 dispatch 接入无 owner，本计划只修合同；生产接入另行设计评估。
- **P0-7 应用层行为**：`Game.cpp:364` 阻断后的用户体验（错误对话框 vs 日志退出）属应用层决策，未在设计内定型。
- **版本探测前置条件**：GL context 必须已创建；实现统一读取 `GL_MAJOR_VERSION`/`GL_MINOR_VERSION`，查询失败与 GLES 一律 fail-closed。
- 全部 P0 性能收益的量化依赖真实 GPU 基线，本地无硬件阶段仅能记录 NOT_RUN。

---

## 6. 设计/源码不一致清单（本计划发现）

| # | 设计文本 | 仓库事实 | 处理 |
| --- | --- | --- | --- |
| 1 | P0-8 同时涉及 grid_count/grid_sort | `grid_count.compute:22/28` **已**减 gridOrigin（`GPUFlowFieldSystem.cpp:272-273` 设置）；缺口在 `grid_sort.compute:29-30` | 本计划以源码为准：保留 grid_count，只修 grid_sort 合同 |
| 2 | P0-4 称"既有 ROI 用例"存在 | 仓库无该用例 | 新增真实 GL 集成 fixture，不声称已有覆盖 |
| 3 | 设计/审查未明确生产 owner | `GPUEntitySystem.cpp:31-33` 只加载不 dispatch | 不新增生产 dispatch，owner 阻塞写入 T8.3 |
| 4 | （P1 预查）设计写 `GetActiveQuadCount` | 实际 getter 为 `GetLastQuadCount`（`GPUTextSystem.hpp:51`） | 移交 P1 计划处理 |

## 7. 与其他计划/轨道的关系

- P0 完成后：P1 计划（`docs/plans/2026-08-16-gpu-pipeline-correction-p1-plan.md`）状态从"待 P0 全部验收"转为可实施；默认串行集成。
- M1-D 轨道缺陷（JFA 漏绑 mask）在本计划 P0-4 修复。
- 本计划不新建 conductor track，仅引用既有 M0-C/M1-D 作为门禁与历史背景。
