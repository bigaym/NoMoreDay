# 打磨与高级特性 规格说明书 (V1.0)

> **Track ID**: `polishing_advanced_features_20260213`
> **设计参考**: [GPU_Rendering_System_2.md](../../设计文档/特效和UI/GPU_Rendering_System_2.md) — §11.6, §17, §20
> **进度参考**: [rendering_system_progress.md](../../rendering_system_progress.md)
> **对应 Phase**: GPU 渲染系统 2.0 — Phase 5
> **前置依赖**: Phase 1/2/3/4 完成并可稳定运行
> **状态**: 🚧 IN_PROGRESS

---

## 1. 概述 (Overview)

Phase 5 目标是把当前可用的渲染系统提升到“可发布、可调优、可调试”的高级形态，覆盖 4 个核心能力：

1. `Color Grading LUT`：在后处理链增加 LUT 色彩校正。
2. `Volumetric Light Scattering`：Ultra 档体积光散射通路。
3. `Pass Profiler HUD`：按 Pass 展示 CPU/GPU 耗时与预算偏差。
4. `Shader Hot Reload`：开发期着色器热重载（失败可回退）。

### 1.1 设计目标

1. **可回退**: Low/Medium 档不引入视觉或性能回归；所有高级项可独立开关。
2. **不破坏帧序**: 保持现有 RenderGraph Pass 执行稳定，不改 Gameplay 主循环顺序。
3. **可诊断**: HUD 与日志同时提供 Pass 级证据，支持预算门禁。
4. **开发效率**: Shader 改动可热重载，失败不影响当前画面。

### 1.2 非目标

- 不引入新的 3D 体积雾系统（本 Track 仅做 2D 屏幕空间散射）
- 不重构 `RenderGraph` 核心调度模型
- 不改动 `GPUEntitySystem` / `MDIRenderer` / ECS 数据流
- 不替换现有 UI 框架

---

## 2. 数据模型 (Data Model)

### 2.1 RenderConfig 扩展

```cpp
// core/RenderConstants.hpp
struct RenderConfig {
  // ... existing fields ...

  // Phase 5 - Color Grading
  bool colorGradingEnabled = false;
  int colorGradingLutSize = 0;      // 0=off, 16 or 32
  float colorGradingIntensity = 1.0f;

  // Phase 5 - Volumetric Light
  bool volumetricLightEnabled = false;
  int volumetricSampleCount = 0;    // 0=off, Ultra defaults 48
  float volumetricScattering = 0.0f;
  float volumetricDecay = 0.0f;

  // Phase 5 - Debug/Dev
  bool profilerHudEnabled = false;
  bool shaderHotReloadEnabled = false;
};
```

### 2.2 Pass Profiling 数据

```cpp
// engine/render/debug/RenderProfiler.hpp
enum class RenderPassId : uint8_t {
  Scene = 0,
  Lighting = 1,
  Volumetric = 2,
  VFX = 3,
  UIWorld = 4,
  PostProcess = 5,
  Distortion = 6,
  Composite = 7,
  Count
};

struct PassTimingSample {
  float cpuMs = 0.0f;
  float gpuMs = 0.0f;
};

struct PassTimingStats {
  float cpuMeanMs = 0.0f;
  float cpuP95Ms = 0.0f;
  float gpuMeanMs = 0.0f;
  float gpuP95Ms = 0.0f;
  float budgetMs = 0.0f;
};
```

### 2.3 Shader 热重载清单项

```cpp
// engine/render/dev/ShaderHotReloadManager.hpp
struct ShaderWatchEntry {
  std::string debugName;
  std::string vertexPath;
  std::string fragmentPath;
  std::string computePath;
  uint64_t lastWriteHash = 0;
  bool enabled = true;
};
```

### 2.4 Quality Tier 配置矩阵（Phase 5）

| 配置项 | Low | Medium | High | Ultra |
|---|---|---|---|---|
| `colorGradingEnabled` | ❌ | ❌ | ✅ | ✅ |
| `colorGradingLutSize` | 0 | 0 | 16 | 32 |
| `volumetricLightEnabled` | ❌ | ❌ | ❌ | ✅ |
| `volumetricSampleCount` | 0 | 0 | 0 | 48 |
| `profilerHudEnabled` | ❌ | ❌ | ✅(Debug) | ✅(Debug) |
| `shaderHotReloadEnabled` | ❌ | ❌ | ✅(Debug) | ✅(Debug) |

---

## 3. 系统架构 (Architecture)

### 3.1 模块布局

```
src/engine/render/
├── core/
│   ├── RenderConstants.hpp          ← [修改] RenderConfig 扩展
│   └── QualityTierManager.cpp       ← [修改] Tier 映射扩展
├── passes/
│   ├── PostProcessPass.hpp/.cpp     ← [修改] Color Grading 扩展
│   ├── LightingPass.hpp/.cpp        ← [修改] 体积光输入参数
│   └── VolumetricLightPass.hpp/.cpp ← [新建] Ultra 体积光 Pass
├── debug/
│   ├── RenderProfiler.hpp           ← [新建]
│   ├── RenderProfiler.cpp           ← [新建]
│   └── ProfilerHudRenderer.cpp      ← [新建]
├── dev/
│   ├── ShaderHotReloadManager.hpp   ← [新建]
│   └── ShaderHotReloadManager.cpp   ← [新建]
└── RenderSystem.cpp                 ← [修改] Pass 计时、HUD/热重载接入

assets/shaders/
├── postprocess/
│   ├── color_grading.frag           ← [新建]
│   └── tonemap.frag                 ← [按需修改] uniform 对齐
└── lighting/
    └── volumetric_light.frag        ← [新建]

assets/luts/
├── neutral_16.png                   ← [新建]
├── cinematic_warm_16.png            ← [新建]
└── nightmare_32.png                 ← [新建]
```

### 3.2 RenderGraph 执行顺序（Phase 5）

```
ScenePass
  -> LightingPass
  -> VolumetricLightPass (Ultra only)
  -> VFXPass
  -> UIWorldPass
  -> PostProcessPass (Bloom -> Tonemap -> Vignette -> ColorGrading -> FXAA)
  -> DistortionPass (High+)
  -> CompositePass
```

### 3.3 关键约束

1. `VolumetricLightPass` 仅在 `volumetricLightEnabled=true` 且 HDR 路径启用时参与。
2. `ColorGrading` 仅处理 LDR 阶段输出，不直接写入 HDR scene buffer。
3. 热重载失败时必须保留“最后一次成功编译的 Shader Program”。
4. HUD 仅用于调试/开发构建，发布构建默认关闭。

---

## 4. 子系统规格

### 4.1 Color Grading LUT

1. 新增 `color_grading.frag`，输入 `uSceneTexture` + `uLutTexture` + `uIntensity`。
2. LUT 采用 `2D strip` 编码，支持 `16x16x16` 与 `32x32x32`。
3. 在 `PostProcessPass` 中新增 `ExecuteColorGrading()`，位于 `Vignette` 后、`FXAA` 前。
4. 若 LUT 资产缺失或加载失败，自动回退为 `neutral` LUT。

### 4.2 Volumetric Light Scattering

1. 新建 `VolumetricLightPass`，基于光源 SSBO 与屏幕坐标进行屏幕空间散射采样。
2. Shader 提供参数：`sampleCount`、`scattering`、`decay`、`exposure`。
3. 输出目标为 HDR 缓冲，保证后续 Bloom 可自然叠加。
4. Ultra 档默认开启；非 Ultra 不创建相关 FBO/Shader 资源。

### 4.3 Pass Profiler HUD

1. 引入 `RenderProfiler`（环形窗口，默认 120 帧）收集每个 Pass 的 CPU/GPU 样本。
2. HUD 显示：
   - Pass 名称
   - CPU/GPU Mean 与 P95
   - 预算值与超预算百分比
3. 数据来源：
   - CPU：`std::chrono` + scoped measurement
   - GPU：`GL_TIME_ELAPSED` query（不可用时自动降级为 CPU only）
4. 每 5 秒打印摘要日志，作为无屏环境证据。

### 4.4 Shader Hot Reload

1. 新建 `ShaderHotReloadManager` 统一管理 watch 列表：
   - `PostProcessPass` shaders
   - `LightingPass` / `VolumetricLightPass` shaders
   - `DistortionPass` shaders
2. 每 `0.5s` 轮询文件时间戳并判定变更。
3. 变更后执行“编译->链接->替换”三段式，失败仅告警不替换。
4. `renderConfig.shaderHotReloadEnabled` 为总开关；`NDEBUG` 下默认 false。

---

## 5. 边界约束 (Constraints)

### 5.1 可修改文件清单

| 文件 | 修改类型 |
|---|---|
| `src/engine/render/core/RenderConstants.hpp` | RenderConfig 扩展 |
| `src/engine/render/core/QualityTierManager.cpp` | Tier 映射扩展 |
| `src/engine/render/passes/PostProcessPass.hpp/cpp` | Color Grading 接入 |
| `src/engine/render/passes/LightingPass.hpp/cpp` | 体积光参数透传 |
| `src/engine/render/RenderSystem.cpp` | Pass 排序、Profiler/HUD/HotReload 接入 |
| `assets/shaders/postprocess/*` | 新增 color grading shader |
| `assets/shaders/lighting/*` | 新增 volumetric shader |
| `tests/performance/*` | Pass 预算基准扩展 |

### 5.2 禁止修改文件

| 文件 | 理由 |
|---|---|
| `src/engine/render/GPUEntitySystem.*` | 与 Phase 5 无关，避免引入实体渲染回归 |
| `src/engine/render/MDIRenderer.*` | 基础主渲染稳定，非本 Track 范围 |
| `src/game/systems/ai/*` | 禁止跨层改动 gameplay/AI 行为 |
| `src/game/systems/combat/*` | 禁止影响战斗逻辑与帧序 |

---

## 6. 风险与缓解 (Risks & Mitigations)

| 风险 | 影响 | 缓解措施 |
|---|---|---|
| LUT 采样坐标错误导致偏色/色带 | 画质明显劣化 | 增加 neutral LUT 基准对比截图；单测验证边界采样 |
| Volumetric Pass 过重 | Ultra 帧率下降 | `sampleCount` 档位化 + 超预算自动降级 |
| GPU Query 不可用 | HUD 数据不完整 | 自动回退 CPU only 并记录 capability 日志 |
| 热重载编译失败导致黑屏 | 运行时不可用 | 双 shader 句柄策略：失败不替换当前 program |
| Pass 顺序破坏 | Bloom/Distortion 结果异常 | 固定顺序检查 + RenderGraph 执行快照测试 |

---

## 7. 验收标准 (Acceptance Criteria)

### 7.1 功能验收

- [ ] High/Ultra 档开启 LUT 后，画面色调可见变化且可切换到 neutral 回退
- [ ] Ultra 档体积光可见且与动态光源位置一致
- [ ] HUD 可显示 Scene/Lighting/VFX/Post/Composite 的 CPU/GPU 指标
- [ ] Shader 文件修改后 0.5~1.0 秒内触发热重载并生效
- [ ] 热重载失败时画面保持上一有效 Shader，不崩溃

### 7.2 性能验收

| 场景 | 指标 | 阈值 |
|---|---|---|
| Color Grading @ 2560x1440 | Pass GPU 耗时 | `< 0.25ms` |
| Volumetric (Ultra, 48 samples) | Pass GPU 耗时 | `< 0.80ms` |
| Profiler HUD 开启 | 全帧额外 CPU 耗时 | `< 0.15ms` |
| Hot Reload 轮询 | 全帧额外 CPU 耗时 | `< 0.05ms` |

### 7.3 稳定性验收

- [ ] 30 分钟运行无显存持续增长
- [ ] 窗口 resize 后 LUT/Volumetric 输出尺寸正确，无拉伸/黑屏
- [ ] `bin/logs/NoMoreDay.log` 无持续 GL error 记录
- [ ] Debug 构建下反复热重载 20 次无崩溃

---

## 8. 已确认设计决策 (Confirmed Decisions)

| 问题 | 决策 |
|---|---|
| Color Grading 所在阶段 | 放在 LDR 后处理链，位于 Vignette 后、FXAA 前 |
| Volumetric 档位 | 仅 Ultra 默认启用，其他档位关闭 |
| Profiler 数据源 | GPU Query 优先，失败自动回退 CPU only |
| 热重载策略 | 轮询时间戳 + 安全替换，不做文件系统事件依赖 |
| 失败行为 | 所有高级特性失败时必须回退到 Phase 4 可运行状态 |

---

*规格版本: 1.0*  
*最后更新: 2026-02-13*
