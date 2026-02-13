# HDR + 后处理管线 技术规格书 (Spec)

> **Track ID**: `hdr_postprocess_pipeline_20260212`
> **Parent Phase**: Phase 1 (GPU 渲染系统 2.0)
> **设计参考**: [GPU_Rendering_System_2.md](../../设计文档/特效和UI/GPU_Rendering_System_2.md) §5, §11.2, §17
> **前置依赖**: Phase 0 (`rendering_foundation_migration_20260212`) ✅ 已完成
> **状态**: DRAFT

---

## 1. 概述 (Overview)

### 1.1 目标

将当前 LDR 直出管线升级为 **HDR → 后处理 → LDR** 全链路，实现以下视觉能力：

| 能力 | 当前 (Phase 0) | 目标 (Phase 1) |
|------|----------------|----------------|
| **色彩空间** | RGBA8 (LDR, 0~1) | RGBA16F (HDR, 0~∞) |
| **Bloom** | ❌ 无 | ✅ Dual Kawase Blur, 3~7 Mip |
| **Tone Mapping** | ❌ 无 | ✅ ACES Filmic |
| **FXAA** | ❌ 无 | ✅ FXAA 3.11 Quality |
| **Vignette** | ❌ 无 | ✅ 参数化暗角 |
| **Low Tier 降级** | N/A | ✅ 完整 LDR 回退路径 |

### 1.2 非目标

- 不实现 Color Grading / LUT（Phase 5 范畴）
- 不实现 Screen Distortion（Phase 4 范畴）
- 不修改 `GPUParticleSystem`、`GPUEntitySystem` 等成熟子系统的渲染逻辑
- 不引入新的 VFX 资源或粒子纹理

### 1.3 核心约束

- **OpenGL 4.3+**: 所有 FBO 操作使用 raw OpenGL API (通过 `GPUUtils` 封装)
- **Raylib 互操作**: 遵循 §16 GL 状态契约，Pass 边界强制 `ScopedGLState`
- **RAII**: 所有 GPU 资源由智能指针/Pool 管理，禁止原生 `new/delete`
- **性能预算**: Bloom ≤ 0.5ms / Tonemap+FXAA+Vignette ≤ 0.4ms (标定机常规场景)

---

## 2. 系统架构

### 2.1 Phase 1 渲染管线流程

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        RenderGraph (Phase 1)                           │
│                                                                         │
│  ScenePass ──→ VFXPass ──→ UIWorldPass ──→ PostProcessPass ──→ CompositePass
│     │              │           │                 │                │     │
│     └──── 写入 HDR SceneBuffer ─┘                 │                │     │
│            (RGBA16F FBO)                          │                │     │
│                                                   │                │     │
│                                    ┌──────────────┘                │     │
│                                    │  BrightExtract               │     │
│                                    │      ↓                        │     │
│                                    │  Kawase Downsample Chain     │     │
│                                    │      ↓                        │     │
│                                    │  Kawase Upsample Chain       │     │
│                                    │      ↓                        │     │
│                                    │  Tonemapping (HDR→LDR)       │     │
│                                    │      ↓                        │     │
│                                    │  FXAA                        │     │
│                                    │      ↓                        │     │
│                                    │  Vignette                    │     │
│                                    └──────────────────────────────┘     │
│                                                                         │
│                                              CompositePass: LDR → Screen│
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Low Tier (bloomEnabled=false) 回退路径

```
ScenePass → VFXPass → UIWorldPass → CompositePass (直出, 与 Phase 0 完全一致)
```

**关键原则**: `PostProcessPass` 在 `bloomEnabled == false` 时整体跳过，管线回退到 Phase 0 行为，确保零退步。

### 2.3 与 Raylib 的边界

```
自定义 GPU 管线:
  - HDR FBO 创建/绑定/渲染/读取 → 全部通过 GPUUtils 原生 GL 调用
  - 后处理 shader 编译/执行 → 全部通过 GPUUtils

Raylib 负责:
  - 窗口/输入管理
  - UI 面板绘制 (rlgl batch flush 后进行)
  - 字体光栅化 (glyph instancing 通过 rlgl 纹理接口)
```

---

## 3. 数据模型

### 3.1 FramebufferHandle (新增)

```cpp
// ============================================================
// src/engine/render/resources/FramebufferHandle.hpp
// ============================================================
#pragma once
#include <cstdint>

namespace NoMoreDay::render::resources {

/// GPU 帧缓冲对象句柄，RAII 封装原生 OpenGL FBO + 颜色附件
struct FramebufferHandle {
    uint32_t fbo = 0;          // GL Framebuffer Object ID
    uint32_t colorTexture = 0; // GL Texture2D ID (颜色附件)
    uint32_t depthRbo = 0;     // GL Renderbuffer ID (可选深度附件)
    int width = 0;
    int height = 0;
    uint32_t internalFormat = 0; // GL_RGBA8 / GL_RGBA16F

    bool IsValid() const { return fbo != 0 && colorTexture != 0; }
};

} // namespace NoMoreDay::render::resources
```

### 3.2 RenderConfig 扩展

```cpp
// ============================================================
// src/engine/render/core/RenderConstants.hpp — RenderConfig 扩展
// ============================================================
struct RenderConfig {
    // --- Phase 0 (已有) ---
    bool bloomEnabled = false;
    bool dynamicLightingEnabled = false;
    int maxParticles = 20000;
    int shadowResolution = 0;

    // --- Phase 1 (新增) ---
    int bloomMipLevels = 0;        // 0=关闭, 3/5/7 = Low/Med/High/Ultra
    float bloomThreshold = 1.0f;   // 亮度提取阈值 (HDR 值)
    float bloomIntensity = 0.8f;   // Bloom 混合强度
    float bloomKnee = 0.1f;        // 阈值软过渡宽度

    bool fxaaEnabled = false;
    bool vignetteEnabled = false;
    float vignetteIntensity = 0.3f; // 暗角强度 (0=无, 1=全黑)
    float vignetteRadius = 0.75f;   // 暗角半径 (0=中心, 1=边缘)
};
```

### 3.3 QualityTier 配置矩阵

| 配置项 | Low | Medium | High | Ultra |
|--------|-----|--------|------|-------|
| `bloomEnabled` | `false` | `true` | `true` | `true` |
| `bloomMipLevels` | `0` | `3` | `5` | `7` |
| `bloomThreshold` | — | `1.2` | `1.0` | `0.8` |
| `bloomIntensity` | — | `0.6` | `0.8` | `1.0` |
| `fxaaEnabled` | `false` | `true` | `true` | `true` |
| `vignetteEnabled` | `false` | `true` | `true` | `true` |
| `vignetteIntensity` | — | `0.2` | `0.3` | `0.35` |

### 3.4 RenderContext 扩展

```cpp
// src/engine/render/graph/RenderContext.hpp — 新增字段
struct RenderContext {
    // --- Phase 0 (已有) ---
    entt::registry* registry = nullptr;
    const NoMoreDay::SharedContext* shared = nullptr;
    const Camera2D* camera = nullptr;
    resources::TransientResourcePool* transientPool = nullptr;
    core::QualityTierManager* qualityManager = nullptr;

    // --- Phase 1 (新增) ---
    resources::FramebufferHandle hdrSceneBuffer = {};  // 主 HDR 渲染目标
};
```

---

## 4. FBO 基础设施

### 4.1 GPUUtils FBO 扩展 (新增静态方法)

```cpp
// GPUUtils.hpp — 新增 FBO 操作
class GPUUtils {
public:
    // ... 现有方法 ...

    // === Framebuffer Operations (Phase 1 新增) ===
    static void GenFramebuffers(int n, uint32_t* framebuffers);
    static void DeleteFramebuffers(int n, const uint32_t* framebuffers);
    static void BindFramebuffer(uint32_t target, uint32_t framebuffer);
    static void FramebufferTexture2D(uint32_t target, uint32_t attachment,
                                     uint32_t textarget, uint32_t texture,
                                     int level);
    static uint32_t CheckFramebufferStatus(uint32_t target);

    // === Renderbuffer Operations ===
    static void GenRenderbuffers(int n, uint32_t* renderbuffers);
    static void DeleteRenderbuffers(int n, const uint32_t* renderbuffers);
    static void BindRenderbuffer(uint32_t target, uint32_t renderbuffer);
    static void RenderbufferStorage(uint32_t target, uint32_t internalformat,
                                    int width, int height);
    static void FramebufferRenderbuffer(uint32_t target, uint32_t attachment,
                                        uint32_t renderbuffertarget,
                                        uint32_t renderbuffer);

    // === Texture 2D Storage (Phase 1 新增) ===
    static void TexStorage2D(uint32_t target, int levels,
                             uint32_t internalformat,
                             int width, int height);
    static void TexImage2D(uint32_t target, int level,
                           int internalformat, int width, int height,
                           int border, uint32_t format, uint32_t type,
                           const void* pixels);

    // === Draw / Viewport ===
    static void Viewport(int x, int y, int width, int height);
    static void DrawArrays(uint32_t mode, int first, int count);

    // === Blend Mode ===
    static void Enable(uint32_t cap);
    static void Disable(uint32_t cap);
    static void BlendFunc(uint32_t sfactor, uint32_t dfactor);

private:
    // 新增函数指针 (Phase 1)
    static void* s_glGenFramebuffers;
    static void* s_glDeleteFramebuffers;
    static void* s_glBindFramebuffer;
    static void* s_glFramebufferTexture2D;
    static void* s_glCheckFramebufferStatus;
    static void* s_glGenRenderbuffers;
    static void* s_glDeleteRenderbuffers;
    static void* s_glBindRenderbuffer;
    static void* s_glRenderbufferStorage;
    static void* s_glFramebufferRenderbuffer;
    static void* s_glTexStorage2D;
    static void* s_glTexImage2D;
    static void* s_glViewport;
    static void* s_glDrawArrays;
    static void* s_glEnable;
    static void* s_glDisable;
    static void* s_glBlendFunc;
};
```

### 4.2 FramebufferManager (新增)

```cpp
// ============================================================
// src/engine/render/resources/FramebufferManager.hpp
// ============================================================
#pragma once
#include "engine/render/resources/FramebufferHandle.hpp"

namespace NoMoreDay::render::resources {

/// 管理 HDR/LDR Framebuffer 的创建、销毁和窗口尺寸响应
class FramebufferManager {
public:
    /// 创建指定格式的 FBO (RGBA8 / RGBA16F)
    static FramebufferHandle Create(int width, int height,
                                    uint32_t internalFormat,
                                    bool withDepth = false);

    /// 销毁 FBO 及其附件
    static void Destroy(FramebufferHandle& handle);

    /// 窗口尺寸变化时重建 (持久资源)
    static void Resize(FramebufferHandle& handle,
                       int newWidth, int newHeight);
};

} // namespace NoMoreDay::render::resources
```

### 4.3 全屏四边形 (FullscreenQuad)

```cpp
// ============================================================
// src/engine/render/resources/FullscreenQuad.hpp
// ============================================================
#pragma once
#include <cstdint>

namespace NoMoreDay::render::resources {

/// 惰性创建的全屏 Quad VAO，用于后处理 Pass
class FullscreenQuad {
public:
    /// 获取或创建全屏 Quad VAO，然后绘制
    static void Draw();

    /// 释放 GPU 资源
    static void Shutdown();

private:
    static uint32_t s_vao;
    static uint32_t s_vbo;
    static bool s_initialized;
};

} // namespace NoMoreDay::render::resources
```

**实现要点**:
- 顶点数据: 3 个顶点覆盖全屏 (oversized triangle trick, 比两个三角形更高效)
- 顶点着色器通过 `gl_VertexID` 计算 UV，无需传入

---

## 5. 后处理 Pass 详细设计

### 5.1 PostProcessPass (编排器)

```cpp
// ============================================================
// src/engine/render/passes/PostProcessPass.hpp
// ============================================================
#pragma once
#include "engine/render/graph/RenderPass.hpp"
#include "engine/render/resources/FramebufferHandle.hpp"
#include <vector>

namespace NoMoreDay::render::passes {

/// 后处理管线编排器 — 管理 Bloom/Tonemap/FXAA/Vignette 的资源和执行
class PostProcessPass final : public graph::RenderPass {
public:
    PostProcessPass();
    ~PostProcessPass();

    void Setup(graph::RenderGraphBuilder& builder) override;
    void Execute(graph::RenderContext& context) override;
    const char* GetName() const override { return "PostProcessPass"; }

    /// 窗口尺寸变化回调
    void OnResize(int width, int height);

    /// 初始化 shader 和资源
    bool Initialize();
    void Shutdown();

private:
    // --- Bloom Resources ---
    struct BloomMip {
        resources::FramebufferHandle fbo;
        int width;
        int height;
    };
    std::vector<BloomMip> m_bloomMips;  // Downsample chain

    // --- Shader Programs (Raylib Shader handles) ---
    Shader m_brightExtractShader = {0};
    Shader m_kawaseDownShader = {0};
    Shader m_kawaseUpShader = {0};
    Shader m_tonemapShader = {0};
    Shader m_fxaaShader = {0};
    Shader m_vignetteShader = {0};
    Shader m_compositeBlendShader = {0}; // HDR + Bloom → Tonemap

    // --- Intermediate FBOs ---
    resources::FramebufferHandle m_ldrBuffer;  // Tonemap 输出

    // --- Uniform Locations ---
    int m_bloomThresholdLoc = -1;
    int m_bloomKneeLoc = -1;
    int m_bloomIntensityLoc = -1;
    int m_tonemapExposureLoc = -1;
    int m_fxaaTexelSizeLoc = -1;
    int m_vignetteIntensityLoc = -1;
    int m_vignetteRadiusLoc = -1;

    // --- Internal Methods ---
    void ExecuteBloom(const graph::RenderContext& context);
    void ExecuteTonemap(const graph::RenderContext& context);
    void ExecuteFXAA(const graph::RenderContext& context);
    void ExecuteVignette(const graph::RenderContext& context);

    void RebuildBloomMips(int baseWidth, int baseHeight, int mipLevels);
    void DestroyBloomMips();

    bool m_initialized = false;
};

} // namespace NoMoreDay::render::passes
```

### 5.2 Bloom 算法: Dual Kawase Blur

```
输入: HDR Scene Texture
  │
  ├── [BrightExtract] → 提取 luma > threshold 的像素 (带 soft knee)
  │
  ├── [Downsample 0] → 1/2 分辨率
  ├── [Downsample 1] → 1/4 分辨率
  ├── [Downsample 2] → 1/8 分辨率
  │   ... (最多 7 级)
  │
  ├── [Upsample 2] → 1/4 (blend with Downsample 1)
  ├── [Upsample 1] → 1/2 (blend with Downsample 0)
  ├── [Upsample 0] → 全分辨率 Bloom Texture
  │
  └── 输出: Bloom Texture (additive blend 到 tonemap 阶段)
```

**Kawase Downsample Kernel**:
```glsl
// 5-tap downsample: 中心 + 4 个对角偏移
vec3 c = texture(uSource, uv).rgb * 4.0;
c += texture(uSource, uv + vec2(-halfpixel.x, halfpixel.y)).rgb;
c += texture(uSource, uv + vec2(halfpixel.x, halfpixel.y)).rgb;
c += texture(uSource, uv + vec2(halfpixel.x, -halfpixel.y)).rgb;
c += texture(uSource, uv + vec2(-halfpixel.x, -halfpixel.y)).rgb;
FragColor = vec4(c / 8.0, 1.0);
```

**Kawase Upsample Kernel**:
```glsl
// 9-tap upsample: 8 方向 + 中心
vec3 c = texture(uSource, uv + vec2(-halfpixel.x * 2.0, 0.0)).rgb;
c += texture(uSource, uv + vec2(-halfpixel.x, halfpixel.y)).rgb * 2.0;
c += texture(uSource, uv + vec2(0.0, halfpixel.y * 2.0)).rgb;
c += texture(uSource, uv + vec2(halfpixel.x, halfpixel.y)).rgb * 2.0;
c += texture(uSource, uv + vec2(halfpixel.x * 2.0, 0.0)).rgb;
c += texture(uSource, uv + vec2(halfpixel.x, -halfpixel.y)).rgb * 2.0;
c += texture(uSource, uv + vec2(0.0, -halfpixel.y * 2.0)).rgb;
c += texture(uSource, uv + vec2(-halfpixel.x, -halfpixel.y)).rgb * 2.0;
FragColor = vec4(c / 12.0, 1.0);
```

### 5.3 Tone Mapping: ACES Filmic

```glsl
vec3 ACESFilmic(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdr = texture(uHDRScene, vTexCoord).rgb;
    vec3 bloom = texture(uBloomTexture, vTexCoord).rgb;
    vec3 combined = hdr + bloom * uBloomIntensity;
    combined *= uExposure;
    vec3 ldr = ACESFilmic(combined);
    ldr = pow(ldr, vec3(1.0 / 2.2)); // Gamma correction
    FragColor = vec4(ldr, 1.0);
}
```

### 5.4 FXAA 3.11

使用标准 FXAA 3.11 Quality 实现，关键 uniform:
- `uTexelSize`: `vec2(1.0/screenWidth, 1.0/screenHeight)`
- 输入: Tonemapped LDR texture
- 输出: 抗锯齿后的 LDR texture

### 5.5 Vignette

```glsl
void main() {
    vec3 color = texture(uSource, vTexCoord).rgb;
    vec2 uv = vTexCoord * 2.0 - 1.0;
    float dist = length(uv);
    float vignette = smoothstep(uRadius, uRadius - 0.45, dist);
    color *= mix(1.0, vignette, uIntensity);
    FragColor = vec4(color, 1.0);
}
```

---

## 6. Shader 资产清单

### 6.1 新增 Shader 文件

```
assets/shaders/postprocess/
├── fullscreen.vert           // 全屏 Quad 顶点着色器 (gl_VertexID → UV)
├── bright_extract.frag       // 亮度提取 (threshold + soft knee)
├── kawase_down.frag          // Dual Kawase Downsample
├── kawase_up.frag            // Dual Kawase Upsample
├── tonemap.frag              // ACES Tonemap + Gamma + Bloom 混合
├── fxaa.frag                 // FXAA 3.11 Quality
└── vignette.frag             // 径向暗角
```

### 6.2 顶点着色器 (`fullscreen.vert`)

所有后处理 Pass 共用此顶点着色器:

```glsl
#version 430 core
out vec2 vTexCoord;
void main() {
    // Fullscreen triangle (3 vertices, no VBO needed)
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
    vTexCoord = gl_Position.xy * 0.5 + 0.5;
}
```

---

## 7. 文件变更清单

### 7.1 新增文件

| 路径 | 用途 |
|------|------|
| `src/engine/render/resources/FramebufferHandle.hpp` | FBO RAII 句柄 |
| `src/engine/render/resources/FramebufferManager.hpp/cpp` | FBO 创建/销毁/Resize |
| `src/engine/render/resources/FullscreenQuad.hpp/cpp` | 全屏 Quad VAO |
| `src/engine/render/passes/PostProcessPass.hpp/cpp` | 后处理编排器 |
| `assets/shaders/postprocess/fullscreen.vert` | 全屏顶点着色器 |
| `assets/shaders/postprocess/bright_extract.frag` | 亮度提取 |
| `assets/shaders/postprocess/kawase_down.frag` | Kawase Downsample |
| `assets/shaders/postprocess/kawase_up.frag` | Kawase Upsample |
| `assets/shaders/postprocess/tonemap.frag` | Tonemapping |
| `assets/shaders/postprocess/fxaa.frag` | FXAA |
| `assets/shaders/postprocess/vignette.frag` | 暗角 |
| `tests/unit/PostProcessTest.cpp` | 后处理单元测试 |

### 7.2 修改文件

| 路径 | 变更范围 |
|------|----------|
| `src/engine/render/core/RenderConstants.hpp` | `RenderConfig` 新增 Phase 1 字段 |
| `src/engine/render/core/QualityTierManager.cpp` | `UpdateConfigForTier` 新增 Phase 1 预设 |
| `src/engine/render/graph/RenderContext.hpp` | 新增 `hdrSceneBuffer` 字段 |
| `src/engine/render/GPUUtils.hpp/cpp` | 新增 FBO/RBO/Texture2D/Viewport 操作 |
| `src/engine/render/RenderSystem.cpp` | 集成 HDR FBO 和 PostProcessPass |
| `CMakeLists.txt` | 新增源文件 |

### 7.3 禁止修改的文件

| 路径 | 原因 |
|------|------|
| `src/engine/render/GPUEntitySystem.*` | 成熟子系统, Phase 1 不触碰 |
| `src/engine/render/GPUParticleSystem.*` | 成熟子系统, Phase 1 不触碰 |
| `src/engine/render/GPUSkillEffectSystem.*` | Phase 3 改造对象 |
| `src/engine/render/PopupRenderer.*` | 成熟子系统 |
| `src/engine/render/MDIRenderer.*` | 成熟子系统 |
| `src/engine/render/graph/RenderGraph.hpp/cpp` | Phase 0 产出, 接口冻结 |
| `src/engine/render/graph/RenderPass.hpp` | 接口冻结 |

---

## 8. 风险与缓解

| ID | 描述 | 影响 | 缓解 |
|----|------|------|------|
| **R-003** | Intel Iris Xe 上 RGBA16F 精度/性能问题 | Bloom 可能出现 banding 或帧率下降 | Low Tier 完全跳过 HDR, Medium Tier 使用 3 Mip 降低压力；启动时检测 `GL_RENDERER` 自动降档 |
| **R-004** | 后处理 shader 编译失败 (驱动兼容) | 黑屏或崩溃 | 所有 shader 加载后检查 `id != 0`，失败时 fallback 到 LDR 直出路径并打印日志 |
| **R-005** | Bloom "firefly" 伪影 (孤立亮像素) | 画面闪烁点 | `bright_extract.frag` 实现 soft knee (渐变阈值而非硬截断) |
| **R-006** | 窗口 Resize 时 FBO 泄漏 | 显存增长 | `FramebufferManager::Resize` 先 Destroy 再 Create；`TransientResourcePool` 回收时清理超龄 FBO |
| **R-007** | rlgl batch 与自定义 FBO 绑定冲突 | 渲染目标错乱 | 进入 HDR FBO 前执行 `rlDrawRenderBatchActive()` flush；退出时恢复默认 Framebuffer `BindFramebuffer(GL_FRAMEBUFFER, 0)` |

---

## 9. 验收标准 (Acceptance Criteria)

### 9.1 功能验收

- [ ] **AC-1**: 开启 Bloom 后, 粒子/技能特效中颜色值 >1.0 的部分产生自然的光晕扩散效果
- [ ] **AC-2**: Bloom 强度随 `bloomMipLevels` 变化呈现阶梯差异 (3/5/7 Mip 分别可见不同扩散范围)
- [ ] **AC-3**: Tonemapping 后高亮区域保留细节，不出现大面积纯白过曝
- [ ] **AC-4**: FXAA 开启后，实体/UI 边缘锯齿明显减少
- [ ] **AC-5**: Vignette 开启后，画面四角有自然渐变暗角效果
- [ ] **AC-6**: `QualityTier::Low` 时画面与 Phase 0 完全一致 (像素级回归)

### 9.2 性能验收 (标定机: RTX 4070S @ 2560x1440)

| 指标 | 门槛 |
|------|------|
| Bloom Pass 耗时 (Ultra, 7 Mip) | ≤ 0.5ms |
| Tonemap + FXAA + Vignette 合计 | ≤ 0.4ms |
| 常规场景总帧率 (Ultra) | ≥ 260 FPS (略低于 Phase 0 的 270 是可接受的) |
| 显存增量 (1440p RGBA16F 全链路) | ≤ 40MB |

### 9.3 稳定性验收

- [ ] 30 分钟压力战斗无崩溃
- [ ] 显存无持续增长 (反复 Resize 窗口 20 次后显存稳定)
- [ ] Debug 构建下 `ScopedGLState` 无报警

---

*规格版本: 1.0*
*最后更新: 2026-02-12*
