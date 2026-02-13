# 材质与 VFX 序列器 实施计划 (V1.0)

> **Track ID**: `material_vfx_sequencer_20260213`
> **依赖 Spec**: `spec.md` (V1.0)
> **Phase**: GPU 渲染系统 2.0 — Phase 4
> **预计工时**: 5~7 天

---

## 阶段总览

| 阶段 | 名称 | 核心产出 | 预估工时 | 状态 |
|------|------|----------|----------|------|
| **A** | 材质数据层 | `GPUMaterialData`, `MaterialInstance`, `MaterialDefs.hpp` | 2h | ✅ |
| **B** | 材质管理器 | `MaterialManager`, SSBO 上传/绑定, 预设注册 | 3h | ✅ |
| **C** | 材质 JSON 管线 | JSON 解析, 热重载, Schema 校验 | 2h | 🚧 |
| **D** | 材质 Shader 集成 | 粒子/实体 Shader 消费 materialId | 3h | 🚧 |
| **E** | VFX 数据模型 | `VFXEvent`, `VFXSequenceAsset`, `VFXPlayerComponent` | 2h | ⏳ |
| **F** | VFX 序列管理器 | `VFXSequenceManager`, JSON 加载, 播放 API | 3h | ⏳ |
| **G** | VFX 序列器系统 | `VFXSequencerSystem`, 事件分发, 子系统调用 | 4h | ⏳ |
| **H** | Screen Distortion Pass | `DistortionPass`, FBO, Shader, Pipeline 插入 | 4h | ⏳ |
| **I** | RenderConfig 扩展 | QualityTier Phase 4 配置, Binding 注册 | 1h | 🚧 |
| **J** | 预制 VFX 序列库 | 10+ JSON 序列资产, 游戏集成 | 4h | ⏳ |
| **K** | 单测与性能验证 | MaterialTest, VFXSequencerTest, DistortionBenchmark | 3h | 🚧 |
| **L** | 文档归档 | 进度更新, Track 关闭 | 0.5h | ⏳ |

**依赖图:**
```
A → B → C → D
              ↘
E → F → G ──→ J → K → L
              ↗
H ← I ───────┘
```

---

## Phase A: 材质数据层 (Material Data Layer)

### Task A.1: GPU 材质数据结构定义
- [x] 在 `GPUData.hpp` 中新增 `GPUMaterialData` 结构 (64 byte, std430 aligned)
- [x] 添加 `static_assert(sizeof(GPUMaterialData) == 64)`
- [x] 定义字段: baseColor(4f), emissive(3f+intensity), distortion, blendMode, shaderVariant, flags, textureSlots(4i)

### Task A.2: CPU 材质实例与枚举定义
- [x] 新建 `src/engine/render/MaterialDefs.hpp`
- [x] 定义 `ShaderVariant` 枚举 (Default/Ink/Hologram/Fire/Ice/Lightning/Dissolve)
- [x] 定义 `BlendMode` 枚举 (Alpha/Additive/Multiply)
- [x] 定义 `MaterialInstance` 结构

### Task A.3: 预定义材质常量
- [x] 在 `MaterialDefs.hpp` 的 `MaterialPresets` 命名空间中定义 constexpr 预设:
  - `Default`, `InkSplash`, `FireGlow`, `IceCrystal`, `LightningArc`, `HoloBlade`, `ShadowVoid`, `DistortionShockwave`
- [x] 验证: 编译通过，结构大小断言正确

---

## Phase B: 材质管理器 (Material Manager)

### Task B.1: MaterialManager 核心实现
- [x] 新建 `src/engine/render/MaterialManager.hpp` 和 `.cpp`
- [x] 实现单例 `Get()`
- [x] 实现 `Initialize()`: 创建 SSBO, 注册所有 constexpr 预设
- [x] 实现 `Shutdown()`: 销毁 SSBO
- [x] 实现 `RegisterMaterial()`: 分配 ID, 标记 dirty

### Task B.2: GPU 同步
- [x] 实现 `SyncToGPU()`: 仅 dirty 时 glBufferSubData 上传整组材质数据
- [x] 实现 `BindSSBO()`: 绑定到 `Binding::MATERIAL_SSBO`
- [x] 实现 `MaterialInstance → GPUMaterialData` 转换函数

### Task B.3: 查询接口
- [x] 实现 `GetMaterial(int id)`, `GetMaterialId(std::string name)`, `GetMaterialCount()`
- [x] 验证: 注册 8 个预设, ID 分配正确, SSBO 绑定无 GL 报错

---

## Phase C: 材质 JSON 管线 (Material Asset Pipeline)

### Task C.1: JSON 解析
- [x] 实现 `LoadFromJson(path)`: 解析 `materials_vfx.json`
- [x] 校验 `material_schema_version`
- [x] 支持字段: name, baseColor(array4), emissive(array3), emissiveIntensity, distortion, blendMode(string), shader(string), textureSlots(array4)
- [x] 回退: 解析失败使用 Default 材质 + 结构化日志

### Task C.2: 热重载支持
- [x] 实现 `TryHotReload()`: 检查文件修改时间, 仅在 `RenderConfig::hotReloadEnabled` 时生效
- [ ] 双缓冲策略: 新材质解析成功后原子替换, 失败不影响当前

### Task C.3: 创建初始 JSON 资产
- [x] 新建 `assets/data/materials_vfx.json`
- [x] 写入至少 5 个数据驱动材质: FireExplosion, IceShatter, PoisonCloud, ShadowMist, HolyLight
- [x] 验证: 加载后 MaterialManager count 增加, ID 查询正确

---

## Phase D: 材质 Shader 集成 (Material Shader Integration)

### Task D.1: GLSL 材质 ABI 定义
- [x] 新建 `assets/shaders/generated/material_abi.glslinc`
- [x] 定义 `MaterialData` 结构 (与 `GPUMaterialData` 镜像)
- [x] 定义 `MaterialBuffer` SSBO layout

### Task D.2: 粒子 Shader 材质采样
- [x] 修改 `particle.frag`: `#include` 材质 ABI, 当 `materialId > 0` 时从 SSBO 采样
- [x] 提取 materialId 从 `flags` 高 16 位: `int matId = int(flags >> 16) & 0xFFFF;`
- [x] 应用: baseColor 乘算, emissive 叠加到 HDR 输出, blendMode 控制混合

### Task D.3: GPUParticle materialId 打包
- [x] 修改 `GPUParticleSystem::Emit()` 或发射调用方: 将 materialId 打包入 flags 高 16 位
- [x] 辅助函数: `PackMaterialId(uint32_t& flags, int materialId)` / `UnpackMaterialId(uint32_t flags)`
- [ ] 验证: 发射带 materialId 的粒子, 渲染颜色由材质驱动

### Task D.4: RenderSystem 集成 MaterialManager
- [x] 在 `RenderSystem::Initialize()` 中调用 `MaterialManager::Get().Initialize()`
- [x] 在渲染主循环中调用 `SyncToGPU()` + `BindSSBO()`
- [x] 在 `RenderSystem::Shutdown()` 中调用 `MaterialManager::Get().Shutdown()`

---

## Phase E: VFX 数据模型 (VFX Data Model)

### Task E.1: VFX 核心结构体定义
- [ ] 新建 `src/engine/vfx/VFXTypes.hpp`
- [ ] 定义 `AnchorType` 枚举
- [ ] 定义 `EventType` 枚举
- [ ] 定义各参数结构: `ParticleEventParams`, `TrailEventParams`, `LightEventParams`, `ShakeEventParams`, `DistortionEventParams`, `SoundEventParams`, `MaterialSwapParams`
- [ ] 定义 `EventParams = std::variant<...>`
- [ ] 定义 `VFXEvent` 和 `VFXSequenceAsset`

### Task E.2: VFX 播放器组件
- [ ] 新建 `src/engine/vfx/VFXPlayerComponent.hpp`
- [ ] 定义 `VFXPlayerComponent` ECS 组件
- [ ] 验证: 编译通过, 所有结构体大小合规

---

## Phase F: VFX 序列管理器 (VFX Sequence Manager)

### Task F.1: VFXSequenceManager 核心实现
- [ ] 新建 `src/engine/vfx/VFXSequenceManager.hpp` 和 `.cpp`
- [ ] 实现单例 `Get()`
- [ ] 实现 `Initialize()` / `Shutdown()`

### Task F.2: JSON 加载
- [ ] 实现 `LoadFromJson(path)`: 扫描 `assets/vfx/` 目录下所有 `.json` 文件
- [ ] 解析 `vfx_schema_version`, `name`, `duration`, `minTier`, `events[]`
- [ ] 事件参数解析: 根据 `type` 字段构造对应 variant
- [ ] materialId 字段: 支持字符串名称 (通过 MaterialManager 查找 ID)
- [ ] 回退: 解析失败跳过该序列 + 日志

### Task F.3: 播放控制 API
- [ ] 实现 `Play(registry, entity, sequenceName, target, loop)`: 挂载 `VFXPlayerComponent`
- [ ] 实现 `Stop(registry, entity)`: 移除 `VFXPlayerComponent`
- [ ] 实现 `GetSequence()`, `GetSequenceId()`

### Task F.4: 热重载
- [ ] 实现 `TryHotReload()`: 监听文件修改时间, 重新解析变更文件
- [ ] 验证: 手动修改 JSON 后调用热重载, 序列数据更新

---

## Phase G: VFX 序列器系统 (VFX Sequencer System)

### Task G.1: VFXSequencerSystem 核心循环
- [ ] 新建 `src/engine/vfx/VFXSequencerSystem.hpp` 和 `.cpp`
- [ ] 实现 `Update(registry, dt)`:
  1. 遍历所有 `VFXPlayerComponent`
  2. 递增 `elapsed`
  3. 检查 `events[nextEventIdx].time <= elapsed` → 触发
  4. 序列结束: loop 则重置, 否则移除组件

### Task G.2: 事件分发
- [ ] 实现 `DispatchEvent()`: 计算世界坐标 (基于 AnchorType + 实体 Position)
- [ ] QualityTier 检查: 事件的 `minTier > currentTier` 则跳过
- [ ] `vfxSequenceDetail` 检查: minimal 模式只触发 Low tier 事件

### Task G.3: 各事件执行器
- [ ] `ExecuteParticle()`: 构建 `GPUParticle`, 打包 materialId, 调用 `GPUParticleSystem::Emit()`
- [ ] `ExecuteTrail()`: 调用 `TrailSystem` GPU 路径
- [ ] `ExecuteLight()`: 调用 `LightManager::AddTransientLight()` (需确认现有接口)
- [ ] `ExecuteShake()`: 调用 `RenderSystem::AddScreenShake()`
- [ ] `ExecuteDistortion()`: 调用 `DistortionPass::AddDistortionSource()`
- [ ] `ExecuteSound()`: 调用现有音频接口

### Task G.4: GameplayState 集成
- [ ] 在 `GameplayState::Update()` 中调用 `VFXSequencerSystem::Update()`
- [ ] 在 `GameplayState::Initialize()` 中调用 `VFXSequenceManager::Get().Initialize()` 和 `LoadFromJson()`
- [ ] 验证: 手动触发 Play(), 事件按时间线正确执行

---

## Phase H: Screen Distortion Pass

### Task H.1: DistortionPass 核心实现
- [ ] 新建 `src/engine/render/passes/DistortionPass.hpp` 和 `.cpp`
- [ ] 实现 `Initialize()`: 创建 Distortion FBO (RG16F), 创建 SSBO
- [ ] 实现 `Shutdown()` / `OnResize()`
- [ ] 实现 `AddDistortionSource()`: 追加到 `m_sources[]`, 递增 `m_activeCount`

### Task H.2: Distortion 写入 Shader
- [ ] 新建 `assets/shaders/postprocess/distortion_write.frag`
- [ ] 逻辑: 对每个扭曲源画环形渐变扭曲到 Distortion Buffer
- [ ] 使用 SSBO 传入 GPUDistortionSource[]

### Task H.3: Distortion 应用 Shader
- [ ] 新建 `assets/shaders/postprocess/distortion_apply.frag`
- [ ] 逻辑: 采样 LDR + Distortion Buffer, 偏移 UV 输出最终画面

### Task H.4: Pipeline 集成
- [ ] `RenderSystem` 中在 PostProcessPass 之后、CompositePass 之前插入 DistortionPass
- [ ] 仅当 `RenderConfig::distortionEnabled` 为 true 时执行
- [ ] 每帧开始清零 `m_activeCount`
- [ ] 验证: 手动 AddDistortionSource, 屏幕出现环形扭曲效果

---

## Phase I: RenderConfig 扩展

### Task I.1: Phase 4 配置字段
- [x] `RenderConstants.hpp`: 新增 `distortionEnabled`, `maxMaterials`, `materialSystemEnabled`, `vfxSequenceDetail`, `hotReloadEnabled`
- [x] `QualityTierManager::UpdateConfigForTier()`: 添加四档配置

### Task I.2: Binding Point 注册
- [ ] `RenderConstants.hpp`: 新增 `Binding::MATERIAL_SSBO = 8`, `Binding::DISTORTION_SSBO = 9`
- [ ] 验证: 不与已有绑定点冲突

---

## Phase J: 预制 VFX 序列库 (Prefab Library)

### Task J.1: 创建 VFX JSON 资产
- [ ] 新建 `assets/vfx/` 目录
- [ ] 编写 10 个 JSON 序列文件:
  - `sword_slash.json`
  - `fire_explosion.json`
  - `ice_shatter.json`
  - `lightning_strike.json`
  - `heal_pulse.json`
  - `shadow_nova.json`
  - `blade_formation.json`
  - `critical_hit.json`
  - `death_dissolve.json`
  - `item_drop_legendary.json`

### Task J.2: 技能系统集成
- [ ] 在 `VisualFXSystem::Initialize()` 的 CombatEvent 回调中, 替换硬编码粒子为 `VFXSequenceManager::Get().Play()`
- [ ] 在技能行为 (FlowingThrust, RendingWave 等) 中, 替换硬编码 ScreenShake/Particle 为 VFX 序列
- [ ] 保留兼容路径: 若对应 VFX 序列不存在, 回退到原有硬编码逻辑

### Task J.3: 验证
- [ ] 逐个验证 10 个序列在游戏中的视觉表现
- [ ] 确认 QualityTier 降级正确 (Low 模式跳过 Distortion 事件)

---

## Phase K: 单测与性能验证

### Task K.1: 材质系统单测
- [x] 新建 `tests/unit/MaterialTest.cpp`
- [x] 测试: 预设注册 ID 分配、JSON 加载解析、名称查询、越界安全
- [x] 测试: GPUMaterialData 布局 (sizeof/offsetof 断言)

### Task K.2: VFX 序列器单测
- [ ] 新建 `tests/unit/VFXSequencerTest.cpp`
- [ ] 测试: JSON 加载、事件排序、播放器推进、循环/结束、QualityTier 过滤
- [ ] 测试: 无效 JSON 回退 (不崩溃)

### Task K.3: Distortion Pass 测试
- [ ] 新建 `tests/unit/DistortionTest.cpp`
- [ ] 测试: DistortionSource 添加/清零、最大数量限制

### Task K.4: 性能基准
- [ ] 扩展 `tests/performance/` 或新建 `MaterialVFXBenchmark.cpp`
- [ ] 基准: MaterialManager SSBO 同步 < 0.05ms
- [ ] 基准: VFXSequencerSystem 100 活跃播放器 < 0.1ms
- [ ] 基准: DistortionPass 2K@8 活跃源 < 0.3ms

### Task K.5: 全量回归
- [ ] 运行 `NoMoreDayTests.exe`, 全量测试通过
- [ ] 短时实机运行, 日志无 GL 错误/泄漏

---

## Phase L: 文档归档

### Task L.1: 更新进度
- [ ] 更新 `conductor/rendering_system_progress.md`: Phase 4 验收记录
- [ ] 更新 `conductor/tracks.md`: Track 状态 → COMPLETED

### Task L.2: 归档
- [ ] 归档到 `conductor/archive/material_vfx_sequencer_20260213/`
- [ ] 关闭 Track

---

## 关键文件清单 (Impact Scope)

### 新建文件
| 文件路径 | 用途 |
|---------|------|
| `src/engine/render/MaterialDefs.hpp` | 材质枚举、结构、预设常量 |
| `src/engine/render/MaterialManager.hpp` / `.cpp` | 材质管理器 |
| `src/engine/vfx/VFXTypes.hpp` | VFX 事件类型与参数结构 |
| `src/engine/vfx/VFXPlayerComponent.hpp` | VFX 播放器 ECS 组件 |
| `src/engine/vfx/VFXSequenceManager.hpp` / `.cpp` | VFX 序列资产管理器 |
| `src/engine/vfx/VFXSequencerSystem.hpp` / `.cpp` | VFX 序列器每帧驱动系统 |
| `src/engine/render/passes/DistortionPass.hpp` / `.cpp` | 屏幕扭曲渲染通路 |
| `assets/shaders/generated/material_abi.glslinc` | GLSL 材质 ABI |
| `assets/shaders/postprocess/distortion_write.frag` | 扭曲源写入 Shader |
| `assets/shaders/postprocess/distortion_apply.frag` | 扭曲应用 Shader |
| `assets/data/materials_vfx.json` | 数据驱动材质定义 |
| `assets/vfx/*.json` (×10) | 预制 VFX 序列资产 |
| `tests/unit/MaterialTest.cpp` | 材质系统单测 |
| `tests/unit/VFXSequencerTest.cpp` | VFX 序列器单测 |
| `tests/unit/DistortionTest.cpp` | 扭曲通路单测 |

### 修改文件
| 文件路径 | 修改内容 |
|---------|---------|
| `src/engine/render/GPUData.hpp` | 新增 `GPUMaterialData`, `GPUDistortionSource` |
| `src/engine/render/core/RenderConstants.hpp` | 新增 Phase 4 RenderConfig 字段 + Binding 常量 |
| `src/engine/render/core/QualityTierManager.cpp` | 添加 Phase 4 Tier 配置 |
| `src/engine/render/RenderSystem.hpp` / `.cpp` | 集成 MaterialManager + DistortionPass |
| `assets/shaders/particle.frag` | 添加材质 SSBO 采样 |
| `src/game/states/GameplayState.cpp` | 集成 VFXSequencerSystem |
| `src/game/systems/combat/VisualFXSystem.cpp` | 迁移硬编码 VFX → 序列驱动 |
| `CMakeLists.txt` | 新增源文件 |

### 禁止修改
| 文件路径 | 理由 |
|---------|------|
| `src/engine/render/graph/RenderPass.hpp` | Phase 0 稳定接口 |
| `src/engine/render/graph/RenderGraph.hpp` | Phase 0 稳定接口 |
| `src/engine/render/GPUParticleSystem.hpp` (核心接口) | 仅允许新增 helper, 不修改 Emit 签名 |

---

*计划版本: 1.0*
*最后更新: 2026-02-13*
