# 打磨与高级特性 实施计划 (V1.0)

> **Track ID**: `polishing_advanced_features_20260213`
> **依赖 Spec**: `spec.md` (V1.0)
> **预计工时**: 4~6 天
> **前置依赖**: Phase 1/2/3/4 已完成并可运行

---

## 阶段总览

| 阶段 | 名称 | 核心产出 | 预估工时 | 状态 |
|---|---|---|---|---|
| **Phase A** | 配置层与资源基线 | RenderConfig/Tier 扩展，LUT 资产基线 | 2h | ✅ |
| **Phase B** | Color Grading | PostProcess 增量链路与 shader | 5h | ⏳ |
| **Phase C** | Volumetric Light | VolumetricLightPass + shader + RenderGraph 接入 | 7h | ⏳ |
| **Phase D** | Profiler HUD | Pass 级 CPU/GPU 计时 + HUD 展示 | 6h | ⏳ |
| **Phase E** | Shader Hot Reload | Shader 轮询热重载与安全替换 | 5h | ⏳ |
| **Phase F** | 集成验收 | 性能基准、运行证据、文档回写 | 4h | ⏳ |

**关键路径**: Phase A -> Phase B -> Phase C -> Phase F  
**可并行**: Phase D 与 Phase E 可在 Phase B 后并行推进

```
Phase A -> Phase B -> Phase C ----\
                   \-> Phase D ----> Phase F
                   \-> Phase E ----/
```

---

## Phase A: 配置层与资源基线 (2h)

### Task A.1: RenderConfig 扩展 (1h)
- [x] **修改** `src/engine/render/core/RenderConstants.hpp`
  - 新增 `colorGradingEnabled/colorGradingLutSize/colorGradingIntensity`
  - 新增 `volumetricLightEnabled/volumetricSampleCount/volumetricScattering/volumetricDecay`
  - 新增 `profilerHudEnabled/shaderHotReloadEnabled`
- [x] **验证**: 编译通过，默认值不改变 Low 档行为

**交付物**: RenderConfig Phase 5 字段就位

### Task A.2: Tier 配置映射 (1h)
- [x] **修改** `src/engine/render/core/QualityTierManager.cpp`
  - Low/Medium: 关闭 Phase 5 所有功能
  - High: 启用 color grading（LUT16），可选 profiler/debug
  - Ultra: 启用 color grading（LUT32）+ volumetric
- [x] **验证**: `QualityTierManager::GetConfig()` 各档位字段正确

**交付物**: Phase 5 配置矩阵可查询

---

## Phase B: Color Grading (5h)

> **前置依赖**: Phase A 完成

### Task B.1: LUT 资产与加载 (1.5h)
- [x] **新建** `assets/luts/neutral_16.png`
- [x] **新建** `assets/luts/cinematic_warm_16.png`
- [x] **新建** `assets/luts/nightmare_32.png`
- [x] **修改** `src/engine/render/passes/PostProcessPass.hpp/cpp`
  - 增加 LUT 纹理句柄与 uniform location 缓存
  - 实现 `LoadColorGradingLUT()`（失败回退 neutral）
- [ ] **验证**: LUT 可正常绑定、切换且无 GL error

**交付物**: LUT 资源与加载路径可用

### Task B.2: Color Grading Shader 与 Pass (2h)
- [x] **新建** `assets/shaders/postprocess/color_grading.frag`
  - 输入 `uSceneTexture`, `uLutTexture`, `uIntensity`, `uLutSize`
  - 支持 16/32 LUT 采样
- [x] **修改** `src/engine/render/passes/PostProcessPass.cpp`
  - 新增 `ExecuteColorGrading(const RenderContext&)`
  - 顺序更新为 `Bloom -> Tonemap -> Vignette -> ColorGrading -> FXAA`
- [ ] **验证**: 开关切换后色调变化可见，关闭后恢复原始色调

**交付物**: Color Grading 端到端链路

### Task B.3: 回归与单测补充 (1.5h)
- [x] **新建** `tests/unit/PostProcessColorGradingTest.cpp`
  - 配置开关路径测试
  - LUT 尺寸参数边界测试（0/16/32）
- [x] **运行** `NoMoreDayTests.exe` 确认零回归

**交付物**: Color Grading 单测与回归证明

---

## Phase C: Volumetric Light (7h)

> **前置依赖**: Phase A 完成  
> **说明**: 仅 Ultra 默认启用

### Task C.1: Volumetric Pass 基础框架 (2h)
- [x] **新建** `src/engine/render/passes/VolumetricLightPass.hpp`
- [x] **新建** `src/engine/render/passes/VolumetricLightPass.cpp`
  - Initialize/Shutdown/OnResize
  - 输入 HDR scene、输出 HDR result
  - 受 `renderConfig.volumetricLightEnabled` 控制
- [ ] **验证**: Pass 创建/销毁/resize 全流程稳定

**交付物**: VolumetricLightPass 骨架

### Task C.2: Volumetric Shader 与参数接入 (2.5h)
- [x] **新建** `assets/shaders/lighting/volumetric_light.frag`
  - 支持 `sampleCount/scattering/decay/exposure`
  - 基于光源数据执行屏幕空间散射近似
- [x] **修改** `VolumetricLightPass.cpp`
  - uniform 与 SSBO 绑定
  - 完成 HDR 混合输出
- [ ] **验证**: Ultra 档可见体积光条带，非 Ultra 不执行

**交付物**: Volumetric Shader 可运行

### Task C.3: RenderGraph 集成与顺序校验 (2.5h)
- [x] **修改** `src/engine/render/RenderSystem.cpp`
  - 在 `LightingPass` 后插入 `VolumetricLightPass`
  - 只在 HDR 路径与 Ultra 档挂载
- [ ] **验证**:
  - Distortion 与 PostProcess 输出保持正确
  - 关闭 volumetric 后回退到既有路径

**交付物**: Phase 5 Pass 顺序稳定

---

## Phase D: Profiler HUD (6h)

> **前置依赖**: Phase B 完成  
> **可并行**: 与 Phase E 并行

### Task D.1: RenderProfiler 数据层 (2h)
- [x] **新建** `src/engine/render/debug/RenderProfiler.hpp`
- [x] **新建** `src/engine/render/debug/RenderProfiler.cpp`
  - 环形缓冲记录 `PassTimingSample`
  - 输出 `PassTimingStats`（mean/p95/budget）
- [ ] **验证**: 无 UI 条件下数据记录与统计正确

**交付物**: Pass 统计核心

### Task D.2: RenderSystem Pass 计时埋点 (2h)
- [x] **修改** `src/engine/render/RenderSystem.cpp`
  - 每个 Pass 执行前后写入 CPU 计时
  - 可用时写入 GPU query 计时
  - 每 5 秒输出日志摘要
- [ ] **验证**: 日志可看到每个 Pass 的 mean/p95 与 budget 差值

**交付物**: Pass 级计时证据

### Task D.3: HUD 渲染 (2h)
- [x] **新建** `src/engine/render/debug/ProfilerHudRenderer.cpp`
  - 绘制 Pass 名称、CPU/GPU 时间、预算超限提示
  - 受 `profilerHudEnabled` 控制
- [ ] **验证**: Debug 构建 HUD 可开关且不影响主画面

**交付物**: Profiler HUD 可见

---

## Phase E: Shader Hot Reload (5h)

> **前置依赖**: Phase B 完成  
> **可并行**: 与 Phase D 并行

### Task E.1: Hot Reload 管理器 (2h)
- [x] **新建** `src/engine/render/dev/ShaderHotReloadManager.hpp`
- [x] **新建** `src/engine/render/dev/ShaderHotReloadManager.cpp`
  - 维护 watch 列表与时间戳哈希
  - 实现 `PollAndReload()`（最小轮询间隔 0.5 秒）
- [ ] **验证**: 文件变更可被检测

**交付物**: Shader 文件变更检测能力

### Task E.2: Pass 级重载接入 (2h)
- [x] **修改** `src/engine/render/passes/PostProcessPass.cpp`
- [x] **修改** `src/engine/render/passes/LightingPass.cpp`
- [x] **修改** `src/engine/render/passes/DistortionPass.cpp`
  - 暴露安全重载入口（不破坏已有 program）
  - 编译失败时保持旧 shader 并打日志
- [ ] **验证**: 修改 shader 文件后可在线生效

**交付物**: 多 Pass 热重载闭环

### Task E.3: RenderSystem 调度与开关 (1h)
- [x] **修改** `src/engine/render/RenderSystem.cpp`
  - 在 render tick 前执行 `PollAndReload()`
  - 受 `shaderHotReloadEnabled` 控制
- [ ] **验证**: Release 默认关闭；Debug 可启用

**交付物**: 热重载调度集成完成

---

## Phase F: 集成验收与文档回写 (4h)

> **前置依赖**: Phase B/C/D/E 全部完成

### Task F.1: build 与测试 (1h)
- [x] **执行** `.\build.bat`
- [x] **执行** `.\build\bin\Release\NoMoreDayTests.exe`
- [x] **验证**: 构建通过，测试零回归

**交付物**: 编译与测试证据

### Task F.2: 性能基准 (1.5h)
- [x] **新建/扩展** `tests/performance/RenderingBenchmark.cpp`
  - 增加 ColorGrading/Volumetric/ProfilerHUD 三项量化场景
- [x] **验证**:
  - ColorGrading `<0.25ms`
  - Volumetric `<0.80ms`
  - HUD overhead `<0.15ms`

**交付物**: Phase 5 基准报告

### Task F.3: 运行验收证据 (1h)
- [ ] **运行** 游戏并保存画面证据（LUT 开关、Volumetric on/off、Profiler HUD）
- [ ] **检查日志** `bin/logs/NoMoreDay.log`
  - 热重载成功/失败日志
  - Pass 预算日志
  - 备注：本轮自动化运行日志落在 `logs/NoMoreDay.log`，`bin/logs/NoMoreDay.log` 未更新
  - 当前状态：已确认 Debug 运行时启用 GPU profiler path；尚未采集到 ShaderHotReload 成功/失败日志

**交付物**: 画面 + 日志双证据

### Task F.4: 追踪文档更新 (0.5h)
- [x] **修改** `conductor/rendering_system_progress.md`
  - Phase 5 状态更新为进行中并补充验收清单入口
- [x] **修改** `conductor/tracks.md`
  - Track 状态从 IN_PROGRESS -> DONE 时同步

**交付物**: 项目追踪文档一致

---

## CMakeLists.txt 影响

以下新增源文件需加入构建系统：

```
src/engine/render/passes/VolumetricLightPass.cpp
src/engine/render/debug/RenderProfiler.cpp
src/engine/render/debug/ProfilerHudRenderer.cpp
src/engine/render/dev/ShaderHotReloadManager.cpp
tests/unit/PostProcessColorGradingTest.cpp
tests/performance/RenderingBenchmark.cpp
```

---

## Shader 资产清单

| 文件 | 操作 | Phase |
|---|---|---|
| `assets/shaders/postprocess/color_grading.frag` | 新建 | B |
| `assets/shaders/lighting/volumetric_light.frag` | 新建 | C |
| `assets/shaders/postprocess/tonemap.frag` | 按需修改 | B |
| `assets/shaders/postprocess/fxaa.frag` | 按需修改 | B |

---

*计划版本: 1.0*  
*最后更新: 2026-02-13*
