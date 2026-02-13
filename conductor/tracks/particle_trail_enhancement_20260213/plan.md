# 粒子与轨迹增强 实施计划 (V1.0)

> **Track ID**: `particle_trail_enhancement_20260213`
> **依赖 Spec**: `spec.md` (V1.0)
> **预计工时**: 5~7 天
> **前置依赖**: Phase 0/1/2 已完成

---

## 阶段总览

| 阶段 | 名称 | 核心产出 | 预估工时 | 状态 |
|------|------|----------|----------|------|
| **Phase A** | 数据层 & ABI | GPUParticle 结构改造、GPUTrailPoint 定义、RenderConfig 扩展 | 3h | ✅ |
| **Phase B** | 纹理粒子系统 | ParticleTextureManager、着色器改造、纹理加载 | 6h | 🚧 |
| **Phase C** | GPU 轨迹渲染器 | GPUTrailRenderer、trail shaders、VFXPass 集成 | 8h | 🚧 |
| **Phase D** | 力场系统 | ForceFieldManager、compute shader 力场采样 | 4h | ⏳ |
| **Phase E** | 子发射器 | particle_sub_emit.compute、死亡检测逻辑 | 5h | ⏳ |
| **Phase F** | 集成 & 测试 | VFXPass 总集成、基准测试、回归验证 | 4h | ⏳ |

**关键路径**: Phase A → Phase B → Phase F (纹理粒子是最高优先级)
**可并行**: Phase C 与 Phase B 可并行；Phase D 与 Phase C 可并行

```
Phase A ──→ Phase B ──┬──→ Phase F
     │               │
     └──→ Phase C ──→┤
           │          │
           └→ Phase D ┘
                │
           Phase E ──→┘
```

---

## Phase A: 数据层 & ABI (3h)

### Task A.1: GPUParticle 结构改造 (1.5h)
- [x] **修改** `src/engine/render/GPUData.hpp`
  - 将 `float padding[3]` 替换为纹理扩展字段（见 spec §2.1）
  - 保持 `static_assert(sizeof(GPUParticle) == 64)`
  - 更新 `GPUParticle()` 默认构造：所有新字段初始化为兼容默认值
- [x] **新增** `GPUTrailPoint` 和 `GPUTrailHeader` 结构体
- [x] **新增** `GPUForceField` 结构体和 `ForceFieldType` 枚举
- [x] **新增** `Constants::GPU` 中的 `MAX_FORCE_FIELDS`, `MAX_TRAILS`, `MAX_TRAIL_POINTS_PER_TRAIL`
- [x] **验证**: 编译通过，所有现有 `static_assert` 无报错

**交付物**: GPUData.hpp 更新，所有结构体 ABI 守卫通过

### Task A.2: RenderConfig & QualityTier 扩展 (1h)
- [x] **修改** `src/engine/render/core/RenderConstants.hpp`
  - RenderConfig 新增 Phase 3 字段（见 spec §2.5）
- [x] **修改** `src/engine/render/core/QualityTierManager.cpp`
  - `UpdateConfigForTier()` 中按 Tier 填充新字段（见 spec §2.6）
- [x] **验证**: `QualityTierManager::GetConfig()` 返回正确的 Tier 值

**交付物**: RenderConfig 扩展完成，4 档配置可查询

### Task A.3: RenderConstants Binding 扩展 (0.5h)
- [x] **修改** `src/engine/render/RenderConstants.hpp`
  - `ParticleCS` 命名空间新增 `FORCE_FIELDS = 4`, `SUB_EMISSION = 5`
  - 新增 `TrailBinding` 命名空间（binding 10, 11）
  - `Binding` 枚举中 `SSBO_RESERVED_10` → `SSBO_TRAIL_HEADERS`，`SSBO_RESERVED_11` → `SSBO_TRAIL_POINTS`
- [x] **验证**: 编译通过，无命名冲突

**交付物**: Binding 分配文档化并编译通过

---

## Phase B: 纹理粒子系统 (6h)

> **前置依赖**: Phase A 完成

### Task B.1: ParticleTextureManager 实现 (2h)
- [x] **新建** `src/engine/render/particle/ParticleTextureManager.hpp`
- [x] **新建** `src/engine/render/particle/ParticleTextureManager.cpp`
  - `Init()`: 创建 Texture2DArray (`glTexImage3D`, RGBA8, 128x128, maxLayers 层)
  - `LoadLayer()`: 加载单张图片到指定层 (`glTexSubImage3D`)
  - `Bind(unit)` / `Unbind(unit)`: 绑定到纹理单元
- [x] **新增** `assets/shaders/textures/particles/` 目录
  - 放置至少 3 张测试纹理 (fire_01.png, smoke_01.png, spark_01.png)
- [ ] **验证**: 加载纹理后绑定到 `TextureUnit::TEX_PARTICLE_ATLAS` 不报 GL 错误

**交付物**: ParticleTextureManager 类实现，测试纹理资产就位

### Task B.2: 粒子着色器改造 — Vertex (1.5h)
- [x] **修改** `assets/shaders/particle.vert`
  - Particle 结构体更新为 Phase 3 版本（新增 texInfo, animData, subEmitParam）
  - 新增 `out flat int vTextureIndex`
  - 新增 `out vec2 vAtlasUV` （序列帧 UV）
  - 新增 `out flat uint vBlendMode`
  - 实现序列帧 UV 计算逻辑（见 spec §4.1）
- [x] **验证**: 着色器编译通过；`textureIndex = -1` 时无行为变化

**交付物**: particle.vert Phase 3 版本

### Task B.3: 粒子着色器改造 — Fragment (1h)
- [x] **修改** `assets/shaders/particle.frag`
  - 新增 `uniform sampler2DArray particleAtlas`
  - 新增纹理粒子分支：`vTextureIndex >= 0` 时采样 Texture2DArray
  - SDF 分支完全保留（`else` 块）
  - 支持 Alpha / Additive 混合模式
- [x] **验证**: 着色器编译通过；SDF 粒子视觉无变化

**交付物**: particle.frag Phase 3 版本

### Task B.4: Compute 着色器 ABI 同步 (0.5h)
- [x] **修改** `assets/shaders/particle.compute`
  - Particle 结构体更新为 Phase 3 版本
  - 新字段在物理模拟中透传（不参与计算，保持 ABI 一致即可）
- [x] **修改** `assets/shaders/particle_emit.compute`
  - 同步 Particle 结构体
- [x] **验证**: Compute shader 编译通过，粒子模拟无回归

**交付物**: 所有粒子着色器 ABI 统一

### Task B.5: GPUParticleSystem 集成 (1h)
- [x] **修改** `src/engine/render/GPUParticleSystem.cpp`
  - `Init()`: 初始化 `ParticleTextureManager`
  - `Render()`: 
    - 检查 `RenderConfig::particleTexturesEnabled`
    - 如果启用，绑定 `ParticleTextureManager` 到 `TextureUnit::TEX_PARTICLE_ATLAS`
    - 设置 `particleAtlas` uniform location
  - `Shutdown()`: 清理 `ParticleTextureManager`
- [ ] **验证**: 发射 `textureIndex >= 0` 的粒子可看到纹理；`textureIndex = -1` 保持 SDF

**交付物**: 纹理粒子端到端可用

---

## Phase C: GPU 轨迹渲染器 (8h)

> **前置依赖**: Phase A 完成
> **可并行**: 与 Phase B 并行

### Task C.1: GPUTrailRenderer 核心框架 (3h)
- [x] **新建** `src/engine/render/trail/GPUTrailRenderer.hpp`
- [x] **新建** `src/engine/render/trail/GPUTrailRenderer.cpp`
  - `Init()`: 创建 Header SSBO (32 × MAX_TRAILS) 和 Points SSBO (32 × MAX_TRAILS × MAX_POINTS)
  - `AllocateTrail()`: 从未使用槽分配，返回 trailId
  - `FreeTrail()`: 标记槽为非活跃
  - `AppendPoint()`: CPU 侧环形写入控制点数组
  - `Update(dt)`: 衰减所有控制点生命值，移除过期点
  - `ClearAll()`: 释放所有槽
- [x] **验证**: 分配/释放测试通过，无越界

**交付物**: GPUTrailRenderer 骨架实现

### Task C.2: 轨迹 Shader 编写 (2h)
- [x] **新建** `assets/shaders/trail/trail.vert`
  - 从 TrailHeader 读取轨迹配置
  - 从 TrailPoints 读取控制点
  - 环形索引解析，法线展宽生成三角带顶点
  - 宽度/颜色插值
  - 传递 UV 和颜色到 Fragment
- [x] **新建** `assets/shaders/trail/trail.frag`
  - 边缘柔化 (`smoothstep`)
  - 尾部淡出
  - Alpha 阈值丢弃
- [x] **验证**: 着色器编译通过

**交付物**: trail.vert + trail.frag

### Task C.3: GPUTrailRenderer::Render() 实现 (2h)
- [x] **实现** `Render(camera)`:
  - 同步 CPU 数据到 GPU SSBO (`m_headerSSBO.Update()`, `m_pointsSSBO.Update()`)
  - 绑定 Header 和 Points SSBO 到 `TrailBinding::HEADERS/POINTS`
  - 遍历活跃轨迹，设置 `trailIndex` uniform，DrawArrays(TRIANGLE_STRIP)
  - 构建 MVP 矩阵（复用 `GPUParticleSystem::BuildMVP` 逻辑）
- [x] **实现** Alpha Blend 模式切换（Additive 轨迹 vs Alpha 轨迹）
- [ ] **验证**: 单条轨迹正确渲染，宽度渐变、颜色渐变、尾部淡出

**交付物**: 轨迹渲染端到端可见

### Task C.4: TrailSystem GPU 路径切换 (1h)
- [x] **修改** `src/game/components/vfx/MotionTrailComponent.hpp`
  - 新增 `int gpuTrailId = -1` 字段（GPU Trail 槽位 ID）
  - 新增 `bool useGPUTrail = false` 字段
- [x] **修改** `src/game/systems/vfx/TrailSystem.cpp`
  - 当 `RenderConfig::trailEnabled && trail.useGPUTrail` 时：
    - 首次调用 `GPUTrailRenderer::AllocateTrail()` 并缓存 `gpuTrailId`
    - 每帧调用 `GPUTrailRenderer::AppendPoint()` 而非 CPU 点列表
    - 跳过 CPU 渲染路径
  - 当 `trailEnabled = false` 时保持原有 CPU 路径
- [x] **修改** `src/engine/render/RenderSystem.cpp`
  - 在 VFXPass 回调中添加 `GPUTrailRenderer::Get().Render(camera)` 调用
- [ ] **验证**: 开启 GPU Trail 时渲染与 CPU Trail 视觉一致

**交付物**: TrailSystem 双路径支持

---

## Phase D: 力场系统 (4h)

> **前置依赖**: Phase A 完成
> **可并行**: 与 Phase C 并行

### Task D.1: ForceFieldManager 实现 (1.5h)
- [ ] **新建** `src/engine/render/particle/ForceFieldManager.hpp`
- [ ] **新建** `src/engine/render/particle/ForceFieldManager.cpp`
  - `Init()`: 创建 SSBO (32 × MAX_FORCE_FIELDS)
  - `AddForceField()`: 分配槽位，返回 ID
  - `RemoveForceField()`: 标记无效 (strength = 0)
  - `SyncToGPU()`: 批量上传到 SSBO
  - `BindSSBO()`: 绑定到指定 binding point
- [ ] **验证**: 添加/移除力场，SSBO 数据正确

**交付物**: ForceFieldManager 实现

### Task D.2: Compute Shader 力场采样 (2h)
- [ ] **修改** `assets/shaders/particle.compute`
  - 新增 `layout(std430, binding = 4) readonly buffer ForceFieldBuffer { ForceField fields[]; }`
  - 新增 `uniform int forceFieldCount` uniform
  - 在物理更新阶段遍历力场，按类型施加力：
    - **Radial**: `F = strength * dir / (dist^falloff)` 方向力
    - **Vortex**: 切线力 + 微弱向心力
    - **Noise**: 基于位置的伪随机扰动（`sin/cos` 噪声）
- [ ] **验证**: 单个径向力场使粒子排斥/吸引正常

**交付物**: particle.compute 力场集成

### Task D.3: GPUParticleSystem 力场绑定 (0.5h)
- [ ] **修改** `src/engine/render/GPUParticleSystem.cpp`
  - `Update()`: 检查 `RenderConfig::forceFieldEnabled`
    - 如果启用，在 compute dispatch 前绑定 `ForceFieldManager` SSBO 到 `ParticleCS::FORCE_FIELDS`
    - 设置 `forceFieldCount` uniform
  - `Init()`: 初始化 `ForceFieldManager`
  - `Shutdown()`: 清理 `ForceFieldManager`
- [ ] **验证**: 力场 + 粒子协同工作

**交付物**: 力场端到端可用

---

## Phase E: 子发射器 (5h)

> **前置依赖**: Phase B 完成（需要纹理粒子基础）
> **Tier 限制**: 仅 High / Ultra 启用

### Task E.1: 子发射缓冲区 (1h)
- [ ] **修改** `src/engine/render/GPUParticleSystem.hpp`
  - 新增 `ComputeBuffer m_subEmissionBuffer` （子发射输出缓冲）
  - 新增 `PersistentBuffer m_subEmitCountBuffer` （子发射原子计数）
- [ ] **修改** `src/engine/render/GPUParticleSystem.cpp`
  - `CreateBuffers()`: 创建子发射缓冲区 (MAX_SUB_EMISSIONS = 2048 × 64 bytes)
- [ ] **验证**: 缓冲区创建无 GL 错误

**交付物**: 子发射缓冲区基础设施

### Task E.2: Compute 死亡检测 (2h)
- [ ] **修改** `assets/shaders/particle.compute`
  - 新增 `layout(std430, binding = 5) buffer SubEmissionBuffer { Particle subEmissions[]; }`
  - 新增 `layout(std430, binding = 6) buffer SubEmitCounter { uint subEmitCount; }`
  - 新增 `uniform int subEmitterEnabled`
  - 在生命值归零时（`p.lifetime <= 0.0 && p.subEmitterType > 0`）：
    - 读取 `subEmitterType` 和 `subEmitterParam`
    - 生成 N 个新粒子写入 SubEmissionBuffer
    - 原子递增 `subEmitCount`
    - 强制新粒子 `subEmitterType = 0`（禁止递归）
- [ ] **验证**: 死亡粒子触发子发射，SubEmissionBuffer 中有数据

**交付物**: 死亡检测 + 子发射数据生成

### Task E.3: 子发射集成到主粒子池 (2h)
- [ ] **新建** `assets/shaders/particle_sub_emit.compute`
  - 读取 SubEmissionBuffer 中的新粒子
  - 追加到主粒子池的末尾（使用 aliveCounter 原子递增）
  - 更新 IndirectDraw 命令
- [ ] **修改** `src/engine/render/GPUParticleSystem.cpp`
  - `Update()`: 在主 compute 完成后、FinalizeFrame 前：
    - 读取 SubEmitCounter
    - 如果 > 0，dispatch `particle_sub_emit.compute`
    - 重置 SubEmitCounter
    - Memory Barrier
  - 受 `RenderConfig::subEmitterEnabled` 控制
- [ ] **验证**: 粒子死亡后在原位爆裂出新粒子

**交付物**: 子发射器端到端可用

---

## Phase F: 集成 & 测试 (4h)

> **前置依赖**: Phase B + C + D + E 全部完成

### Task F.1: VFXPass 总集成 (1h)
- [ ] **修改** `src/engine/render/passes/VFXPass.cpp`
  - 确保 Execute 回调中渲染顺序：
    1. `GPUParticleSystem::Render()` （粒子，含纹理粒子）
    2. `GPUTrailRenderer::Render()` （GPU 轨迹）
    3. 现有 HoloBlade / SkillEffect 渲染
  - 确保 HDR SceneColor 上下文正确（所有 VFX 输出到 HDR Buffer）
- [ ] **确认**: `RenderSystem` 中 VFXPass 回调已正确注册上述调用
- [ ] **验证**: 完整渲染管线无 GL 状态泄漏

**交付物**: VFXPass 完整集成

### Task F.2: 回归测试 (1.5h)
- [ ] **新建** `tests/unit/ParticleTextureTest.cpp`
  - 测试 GPUParticle 结构 ABI (sizeof, offsetof)
  - 测试 ParticleTextureManager 初始化/加载/释放
  - 测试 `textureIndex = -1` 默认行为向下兼容
- [ ] **新建** `tests/unit/TrailRendererTest.cpp`
  - 测试 GPUTrailRenderer 分配/释放/溢出
  - 测试 AppendPoint 环形写入逻辑
  - 测试 GPUTrailPoint 和 GPUTrailHeader ABI
- [ ] **新建** `tests/unit/ForceFieldTest.cpp`
  - 测试 ForceFieldManager 添加/移除/超限
  - 测试 GPUForceField ABI
- [ ] **运行** 现有全部测试用例确认零回归:
  ```powershell
  .\build\bin\Release\NoMoreDayTests.exe
  ```
- [ ] **验证**: 所有测试通过

**交付物**: 3 个新测试文件 + 全量通过报告

### Task F.3: 性能基准测试 (1h)
- [ ] **新建** `tests/performance/ParticleTrailBenchmark.cpp`
  - 场景 1: 纹理粒子 10k 个，VFXPass 耗时 < 0.8ms
  - 场景 2: 力场 16 个 + 粒子 50k 个，Compute 耗时 < 0.5ms
  - 场景 3: GPU Trail 256 条 × 48 点，Render 耗时 < 0.3ms
  - 场景 4: 子发射器 1k 死亡/帧，Sub-Emit Dispatch < 0.2ms
- [ ] **验证**: 所有场景通过阈值

**交付物**: 性能基准报告

### Task F.4: rendering_system_progress.md 更新 (0.5h)
- [ ] **修改** `conductor/rendering_system_progress.md`
  - Phase 3 状态更新为 ✅ 已完成
  - 填写验收记录清单
  - 记录归档位置
- [ ] **修改** `conductor/tracks.md`
  - 状态更新为 DONE
- [ ] **归档** track 文档到 `conductor/archive/particle_trail_enhancement_20260213/`

**交付物**: 进度追踪更新完成

---

## CMakeLists.txt 影响

以下新文件需加入 `CMakeLists.txt`：

```
# 源文件
src/engine/render/particle/ParticleTextureManager.cpp
src/engine/render/particle/ForceFieldManager.cpp
src/engine/render/trail/GPUTrailRenderer.cpp

# 测试文件
tests/unit/ParticleTextureTest.cpp
tests/unit/TrailRendererTest.cpp
tests/unit/ForceFieldTest.cpp
tests/performance/ParticleTrailBenchmark.cpp
```

---

## Shader 资产清单

| 文件 | 操作 | Phase |
|------|------|-------|
| `assets/shaders/particle.compute` | 修改 | B + D + E |
| `assets/shaders/particle.vert` | 修改 | B |
| `assets/shaders/particle.frag` | 修改 | B |
| `assets/shaders/particle_emit.compute` | 修改 | B |
| `assets/shaders/particle_sub_emit.compute` | 新建 | E |
| `assets/shaders/trail/trail.vert` | 新建 | C |
| `assets/shaders/trail/trail.frag` | 新建 | C |

---

*计划版本: 1.0*
*最后更新: 2026-02-13*
