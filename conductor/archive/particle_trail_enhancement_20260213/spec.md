# 粒子与轨迹增强 规格说明书 (V1.0)

> **Track ID**: `particle_trail_enhancement_20260213`
> **设计参考**: [GPU_Rendering_System_2.md](../../设计文档/特效和UI/GPU_Rendering_System_2.md) — §6
> **对应 Phase**: GPU 渲染系统 2.0 — Phase 3
> **前置依赖**: Phase 0 (RenderGraph), Phase 1 (HDR + PostProcess), Phase 2 (Lighting)
> **状态**: 🚧 IN_PROGRESS

---

## 1. 概述 (Overview)

升级 GPU 粒子系统与轨迹渲染器，使其从"纯 SDF 形状 + CPU 线段"进化为"纹理粒子 + GPU 轨迹"的完整 VFX 基础设施。本 Track 是后续 Phase 4（材质 & VFX 序列器）的必要前提。

### 1.1 核心特性

| 特性 | 规格 |
|------|------|
| **纹理粒子** | Texture2DArray 采样，向下兼容 SDF 形状（textureIndex = -1） |
| **序列帧动画** | 基于 lifetime 自动播放，支持 NxM 子区域网格 |
| **子发射器** | 粒子死亡时 GPU 端链式触发新粒子 |
| **力场系统** | GPU SSBO 力场：径向力、涡旋力、噪声力 |
| **GPU TrailRenderer** | 环形 PersistentBuffer，GPU 展宽三角带渲染 |

### 1.2 设计目标

1. **向下兼容**: `textureIndex = -1`（默认值）时行为与当前完全一致，零回归。
2. **高性能**: 纹理粒子/轨迹渲染均通过 Instanced Draw / GPU-driven 完成，主循环零堆分配。
3. **Tier 驱动**: 所有新特性均可通过 `QualityTierManager` 按档位启停。
4. **ABI 安全**: GPUParticle 结构保持 64 字节不变，复用现有 `padding[3]` 字段。

### 1.3 非目标

- 不实现材质系统（Phase 4 范围）
- 不实现 VFX 序列器（Phase 4 范围）
- 不实现 Screen Distortion Pass（Phase 4 范围）
- 不实现子发射器的碰撞检测（仅处理死亡触发）

---

## 2. 数据模型 (Data Model)

### 2.1 GPUParticle 结构改造

**策略**: 复用现有 `padding[3]`（12 字节），保持 64 字节 ABI 不变。

```cpp
// ============================================================
// GPUData.hpp — GPUParticle 改造 (Phase 3)
// ============================================================
struct GPUParticle {
  Vector2 position = {0.0f, 0.0f};             // 8
  Vector2 velocity = {0.0f, 0.0f};             // 8
  Vector2 acceleration = {0.0f, 0.0f};         // 8
  Color color = {0, 0, 0, 0};                  // 4
  float lifetime = 0.0f;                       // 4
  float maxLifetime = 0.0f;                    // 4
  float scale = 0.0f;                          // 4
  uint32_t flags = 0;                          // 4
  float growthRate = 0.0f;                     // 4
  float rotation = 0.0f;                       // 4

  // --- Phase 3: 纹理粒子扩展 (替代原 padding[3]) ---
  int16_t textureIndex = -1;                   // 2  纹理数组层索引, -1 = SDF
  uint16_t subUV = 0;                          // 2  序列帧：rows<<8 | cols (atlas grid)
  uint16_t animFrameCount = 0;                 // 2  总帧数 (0 = 无动画)
  uint8_t blendMode = 0;                       // 1  0=Alpha, 1=Additive
  uint8_t subEmitterType = 0;                  // 1  0=None, 1=Burst, 2=Sparks
  float subEmitterParam = 0.0f;                // 4  子发射器参数 (粒子数或强度)

  GPUParticle() = default;
};

// ABI 守卫
static_assert(sizeof(GPUParticle) == 64,
              "GPUParticle struct must be exactly 64 bytes for SSBO alignment");
```

**字段说明**:

| 字段 | 偏移 | 大小 | 说明 |
|------|------|------|------|
| `textureIndex` | 52 | 2 | Texture2DArray 层索引，`-1` 保持 SDF 渲染 |
| `subUV` | 54 | 2 | 序列帧网格配置：`(rows << 8) | cols`，最大 16x16 |
| `animFrameCount` | 56 | 2 | 动画总帧数，0 表示静态纹理 |
| `blendMode` | 58 | 1 | 混合模式：0=Alpha Blend, 1=Additive |
| `subEmitterType` | 59 | 1 | 子发射器类型：0=禁用，1=死亡爆裂，2=火花 |
| `subEmitterParam` | 60 | 4 | 子发射器参数：爆裂粒子数或火花强度 |

**GLSL 镜像** (particle.compute / particle.vert):

```glsl
// 必须与 C++ GPUParticle 严格对应 (64 bytes)
struct Particle {
    vec2 position;      // 8
    vec2 velocity;      // 8
    vec2 acceleration;  // 8
    uint color;         // 4
    float lifetime;     // 4
    float maxLifetime;  // 4
    float scale;        // 4
    uint flags;         // 4
    float growthRate;   // 4
    float rotation;     // 4

    // Phase 3 extensions (replaces padding[3])
    int  textureIndex;        // 4 (int16 + uint16 packed as int)
    uint animData;            // 4 (animFrameCount:16 | blendMode:8 | subEmitterType:8)
    float subEmitterParam;    // 4
}; // Total: 64 bytes
```

> **注**: GLSL 中 `int16` 不可用，采用 `int` 保持 4 字节对齐。C++ 侧的 `textureIndex(i16) + subUV(u16)` 共 4 字节映射到 GLSL 的 `int textureIndex`，在着色器中通过位操作拆分。

### 2.2 GPUTrailPoint（新增）

```cpp
// ============================================================
// GPUData.hpp — GPU 轨迹控制点 (Phase 3)
// ============================================================
struct GPUTrailPoint {
    float posX = 0.0f;            // 4
    float posY = 0.0f;            // 4
    float dirX = 0.0f;            // 4  运动方向 (用于法线展宽)
    float dirY = 0.0f;            // 4
    float width = 0.0f;           // 4
    float lifetime = 0.0f;        // 4  剩余生命
    uint32_t colorPacked = 0;     // 4  RGBA8
    uint32_t flags = 0;           // 4  bit[0:7]=textureIdx, bit[8:9]=blendMode
};

static_assert(sizeof(GPUTrailPoint) == 32,
              "GPUTrailPoint struct must be exactly 32 bytes for SSBO alignment");
```

### 2.3 GPUTrailHeader（新增）

```cpp
// 轨迹头部：描述环形缓冲区状态
struct GPUTrailHeader {
    int32_t headIndex = 0;       // 4  环形写入头
    int32_t pointCount = 0;      // 4  当前有效点数
    int32_t maxPoints = 64;      // 4  最大控制点数
    float maxLifetime = 0.5f;    // 4  最大生命时长
    float widthStart = 8.0f;     // 4  头部宽度
    float widthEnd = 1.0f;       // 4  尾部宽度
    uint32_t colorStart = 0xFFFFFFFF; // 4  头部颜色
    uint32_t colorEnd = 0x00000000;   // 4  尾部颜色 (淡出)
};

static_assert(sizeof(GPUTrailHeader) == 32,
              "GPUTrailHeader struct must be exactly 32 bytes");
```

### 2.4 GPU 力场数据结构（新增）

```cpp
// ============================================================
// GPUData.hpp — GPU 力场 (Phase 3)
// ============================================================
enum class ForceFieldType : uint32_t {
    Radial = 0,    // 径向力 (正=排斥, 负=吸引)
    Vortex = 1,    // 涡旋力
    Noise = 2,     // 噪声湍流
};

struct GPUForceField {
    float posX = 0.0f;           // 4
    float posY = 0.0f;           // 4
    float radius = 100.0f;       // 4  影响半径
    float strength = 50.0f;      // 4  力度
    uint32_t type = 0;           // 4  ForceFieldType
    float falloff = 1.0f;        // 4  衰减指数
    float noiseFrequency = 1.0f; // 4  噪声频率 (仅 Noise 类型)
    float padding = 0.0f;        // 4  对齐
};

static_assert(sizeof(GPUForceField) == 32,
              "GPUForceField struct must be exactly 32 bytes");

namespace Constants::GPU {
    constexpr int MAX_FORCE_FIELDS = 16;
    constexpr int MAX_TRAILS = 512;
    constexpr int MAX_TRAIL_POINTS_PER_TRAIL = 64;
}
```

### 2.5 RenderConfig 扩展

```cpp
// core/RenderConstants.hpp — RenderConfig 新增字段
struct RenderConfig {
    // ... 现有字段 ...

    // Phase 3: 粒子增强
    bool particleTexturesEnabled = false;  // 纹理粒子开关
    bool subEmitterEnabled = false;        // 子发射器开关
    bool forceFieldEnabled = false;        // 力场系统开关
    int  maxForceFields = 0;               // 最大力场数

    // Phase 3: 轨迹渲染
    bool trailEnabled = false;             // GPU 轨迹开关
    int  trailMaxPoints = 0;               // 每条轨迹最大控制点
    int  maxTrails = 0;                    // 最大同时轨迹数
};
```

### 2.6 Quality Tier 配置矩阵

| 配置项 | Low | Medium | High | Ultra |
|--------|-----|--------|------|-------|
| `particleTexturesEnabled` | ❌ | ✅ | ✅ | ✅ |
| `subEmitterEnabled` | ❌ | ❌ | ✅ | ✅ |
| `forceFieldEnabled` | ❌ | ❌ | ✅ | ✅ |
| `maxForceFields` | 0 | 0 | 8 | 16 |
| `trailEnabled` | ❌ | ✅ | ✅ | ✅ |
| `trailMaxPoints` | 0 | 32 | 48 | 64 |
| `maxTrails` | 0 | 128 | 256 | 512 |

---

## 3. 系统架构 (Architecture)

### 3.1 模块布局

```
src/engine/render/
├── GPUParticleSystem.hpp/cpp    ← [修改] 纹理绑定、力场分发
├── GPUData.hpp                  ← [修改] 新增结构体定义
├── core/
│   ├── RenderConstants.hpp      ← [修改] RenderConfig 扩展
│   └── QualityTierManager.cpp   ← [修改] Tier 映射
├── trail/                       ← [新建] GPU 轨迹模块
│   ├── GPUTrailRenderer.hpp
│   ├── GPUTrailRenderer.cpp
│   └── TrailManager.hpp
├── particle/                    ← [新建] 粒子增强模块
│   ├── ParticleTextureManager.hpp
│   ├── ParticleTextureManager.cpp
│   ├── ForceFieldManager.hpp
│   └── ForceFieldManager.cpp
└── passes/
    └── VFXPass.cpp              ← [修改] 集成新渲染路径

assets/shaders/
├── particle.compute             ← [修改] 力场采样、子发射器检测
├── particle.vert                ← [修改] 纹理 UV 计算、传递
├── particle.frag                ← [修改] Texture2DArray 采样
├── particle_sub_emit.compute    ← [新建] 子发射器计算着色器
├── trail/                       ← [新建]
│   ├── trail.vert
│   └── trail.frag
└── textures/
    └── particles/               ← [新建] 粒子纹理资产

src/game/
├── systems/vfx/
│   └── TrailSystem.cpp          ← [修改] GPU 路径切换
└── components/vfx/
    └── MotionTrailComponent.hpp ← [修改] 增加 GPU Trail 配置字段
```

### 3.2 数据流图

```
┌─────────────── CPU 每帧 ───────────────────────────────────────┐
│                                                                 │
│  MotionTrailComponent  ForceFieldManager  ParticleEmission    │
│        │                    │                    │               │
│  Write头部控制点        Update SSBO         Lock-free Emit     │
│        ↓                    ↓                    ↓               │
│  TrailManager       ForceField SSBO      EmissionBuffer       │
│  (CPU→GPU Ring)         (binding 4)        (binding 0)        │
│                                                                 │
├─────────────── GPU Compute ────────────────────────────────────┤
│                                                                 │
│  particle.compute                                               │
│  ├── Read ForceField SSBO → Apply forces to each particle      │
│  ├── Detect death → Write SubEmission Buffer                    │
│  └── Stream compaction → Indirect Draw                         │
│                                                                 │
│  particle_sub_emit.compute (Tier >= High)                       │
│  └── Read SubEmission → Generate new particles into main pool  │
│                                                                 │
├─────────────── GPU Render (VFXPass) ───────────────────────────┤
│                                                                 │
│  1. GPUParticleSystem::Render()                                 │
│     ├── Bind Texture2DArray (unit 1)                            │
│     └── DrawArraysIndirect (with texture sampling in frag)     │
│                                                                 │
│  2. GPUTrailRenderer::Render()                                  │
│     ├── Bind trail SSBO                                         │
│     └── DrawArrays per trail (triangle strip)                  │
│                                                                 │
│  Output → HDR SceneColor (自然参与 Bloom)                      │
└─────────────────────────────────────────────────────────────────┘
```

### 3.3 Binding 点分配

| Binding | 用途 | 系统 | 生命周期 |
|---------|------|------|----------|
| `ParticleCS::4` | ForceField SSBO (只读) | particle.compute | 临时 |
| `ParticleCS::5` | SubEmission Buffer (写) | particle.compute | 临时 |
| `Binding::SSBO_RESERVED_10` | Trail Header SSBO (全局) | GPUTrailRenderer | 持久 |
| `Binding::SSBO_RESERVED_11` | Trail Points SSBO (全局) | GPUTrailRenderer | 持久 |
| `TextureUnit::TEX_PARTICLE_ATLAS (1)` | Particle Texture2DArray | GPUParticleSystem | 持久 |

> **注**: 粒子系统的 Binding 0-3 保持不变（ParticleCS 命名空间）。新增 4/5 仅在 particle.compute dispatch 期间有效。

---

## 4. 着色器改造规格 (Shader Specification)

### 4.1 particle.vert 改造

```glsl
#version 430

layout(location = 0) in vec2 vertexPos;

struct Particle {
    vec2 position;
    vec2 velocity;
    vec2 acceleration;
    uint color;
    float lifetime;
    float maxLifetime;
    float scale;
    uint flags;
    float growthRate;
    float rotation;
    int  texInfo;       // lower 16 = textureIndex, upper 16 = subUV
    uint animData;      // [15:0] = frameCount, [23:16] = blendMode, [31:24] = subEmitterType
    float subEmitParam;
};

layout(std430, binding = 0) readonly buffer CompactBuffer {
    Particle particles[];
};

uniform mat4 mvp;

out vec4 fragColor;
out flat uint vFlags;
out vec2 vTexCoord;
out flat int vTextureIndex;
out vec2 vAtlasUV;       // 序列帧 UV 偏移
out flat uint vBlendMode;

void main() {
    uint id = gl_InstanceID;
    Particle p = particles[id];
    
    if (p.lifetime <= 0.0 || p.scale <= 0.0) {
        gl_Position = vec4(-9999.0, -9999.0, 0.0, 1.0);
        return;
    }
    
    // 解包颜色
    vec4 col = vec4(
        float(p.color & 0xFFu) / 255.0,
        float((p.color >> 8) & 0xFFu) / 255.0,
        float((p.color >> 16) & 0xFFu) / 255.0,
        float((p.color >> 24) & 0xFFu) / 255.0
    );
    
    float lifetimeRatio = clamp(p.lifetime / p.maxLifetime, 0.0, 1.0);
    col.a *= lifetimeRatio;
    fragColor = col;
    vFlags = p.flags;
    
    // === Phase 3: 纹理索引解包 ===
    vTextureIndex = (p.texInfo << 16) >> 16;  // Sign extend lower 16 bits
    int subUVPacked = (p.texInfo >> 16) & 0xFFFF;
    
    int subRows = (subUVPacked >> 8) & 0xFF;
    int subCols = subUVPacked & 0xFF;
    
    uint frameCount = p.animData & 0xFFFFu;
    vBlendMode = (p.animData >> 16) & 0xFFu;
    
    // === 序列帧 UV 计算 ===
    if (vTextureIndex >= 0 && frameCount > 0u && subRows > 0 && subCols > 0) {
        float progress = 1.0 - lifetimeRatio; // 0→1 over lifetime
        int currentFrame = int(progress * float(frameCount - 1u));
        currentFrame = clamp(currentFrame, 0, int(frameCount) - 1);
        
        int row = currentFrame / subCols;
        int col_idx = currentFrame % subCols;
        
        float cellW = 1.0 / float(subCols);
        float cellH = 1.0 / float(subRows);
        
        vec2 uvBase = vec2(float(col_idx) * cellW, float(row) * cellH);
        vAtlasUV = uvBase + (vertexPos + 0.5) * vec2(cellW, cellH);
    } else if (vTextureIndex >= 0) {
        // 静态纹理，直接使用完整 UV
        vAtlasUV = vertexPos + 0.5;
    } else {
        vAtlasUV = vec2(0.0);
    }
    
    vTexCoord = vertexPos + 0.5;
    
    // 旋转 + 缩放 + 位移
    float cosR = cos(p.rotation);
    float sinR = sin(p.rotation);
    mat2 rotMat = mat2(cosR, sinR, -sinR, cosR);
    
    float lifetimeScale = sqrt(lifetimeRatio);
    float finalScale = p.scale * lifetimeScale;
    vec2 worldPos = rotMat * vertexPos * finalScale + p.position;
    
    gl_Position = mvp * vec4(worldPos, 0.0, 1.0);
}
```

### 4.2 particle.frag 改造

```glsl
#version 430

in vec4 fragColor;
in flat uint vFlags;
in vec2 vTexCoord;
in flat int vTextureIndex;
in vec2 vAtlasUV;
in flat uint vBlendMode;

// Phase 3: Particle Texture Atlas
uniform sampler2DArray particleAtlas;

out vec4 finalColor;

void main() {
    float alpha = fragColor.a;
    
    if (vTextureIndex >= 0) {
        // === 纹理粒子 ===
        vec4 texColor = texture(particleAtlas, vec3(vAtlasUV, float(vTextureIndex)));
        
        // 调制: 纹理颜色 × 顶点颜色
        finalColor = vec4(fragColor.rgb * texColor.rgb, alpha * texColor.a);
    } else {
        // === SDF 形状 (原有逻辑，完全保留) ===
        uint shapeId = vFlags & 0xFFu;
        float d = distance(vTexCoord, vec2(0.5));
        
        if (shapeId == 0u) {
            alpha *= smoothstep(0.5, 0.2, d);
        } else if (shapeId == 1u) {
            alpha *= smoothstep(0.5, 0.1, d);
        } else if (shapeId == 2u) {
            float spark = 1.0 - (abs(vTexCoord.x - 0.5) + abs(vTexCoord.y - 0.5)) * 2.5;
            alpha *= clamp(spark * 3.0, 0.0, 1.0);
        }
        // ... 其余 SDF 形状保持不变 ...
        
        finalColor = vec4(fragColor.rgb, alpha);
    }
    
    if (finalColor.a < 0.01) discard;
}
```

### 4.3 trail.vert / trail.frag（新建）

```glsl
// trail.vert
#version 430

struct TrailPoint {
    float posX, posY;
    float dirX, dirY;
    float width;
    float lifetime;
    uint colorPacked;
    uint flags;
};

struct TrailHeader {
    int headIndex;
    int pointCount;
    int maxPoints;
    float maxLifetime;
    float widthStart;
    float widthEnd;
    uint colorStart;
    uint colorEnd;
};

layout(std430, binding = 10) readonly buffer TrailHeaders {
    TrailHeader headers[];
};
layout(std430, binding = 11) readonly buffer TrailPoints {
    TrailPoint points[];
};

uniform mat4 mvp;
uniform int trailIndex;

out float vProgress;     // 0=头, 1=尾
out vec4 vColor;
out vec2 vTexCoord;

void main() {
    TrailHeader h = headers[trailIndex];
    if (h.pointCount < 2) {
        gl_Position = vec4(-9999.0, -9999.0, 0.0, 1.0);
        return;
    }
    
    // 每个控制点产生 2 个顶点 (左/右)
    int pointIdx = gl_VertexID / 2;
    int side = (gl_VertexID % 2 == 0) ? 1 : -1;
    
    // Ring buffer 索引
    int ringIdx = (h.headIndex - pointIdx + h.maxPoints) % h.maxPoints;
    int globalIdx = trailIndex * 64 + ringIdx; // MAX_TRAIL_POINTS = 64
    
    TrailPoint pt = points[globalIdx];
    
    float progress = float(pointIdx) / float(h.pointCount - 1);
    vProgress = progress;
    
    // 宽度插值
    float w = mix(h.widthStart, h.widthEnd, progress) * 0.5;
    
    // 法线展宽
    vec2 normal = vec2(-pt.dirY, pt.dirX);
    vec2 worldPos = vec2(pt.posX, pt.posY) + normal * w * float(side);
    
    // 颜色插值
    vec4 startCol = unpackUnorm4x8(h.colorStart);
    vec4 endCol = unpackUnorm4x8(h.colorEnd);
    vColor = mix(startCol, endCol, progress);
    
    // UV
    vTexCoord = vec2(progress, float(side) * 0.5 + 0.5);
    
    gl_Position = mvp * vec4(worldPos, 0.0, 1.0);
}
```

```glsl
// trail.frag
#version 430

in float vProgress;
in vec4 vColor;
in vec2 vTexCoord;

out vec4 finalColor;

void main() {
    // 边缘柔化
    float edgeFade = smoothstep(0.0, 0.1, vTexCoord.y) *
                     smoothstep(1.0, 0.9, vTexCoord.y);
    
    // 尾部淡出
    float tailFade = smoothstep(1.0, 0.7, vProgress);
    
    float alpha = vColor.a * edgeFade * tailFade;
    
    if (alpha < 0.01) discard;
    
    finalColor = vec4(vColor.rgb, alpha);
}
```

---

## 5. 核心系统 API

### 5.1 ParticleTextureManager

```cpp
// ============================================================
// engine/render/particle/ParticleTextureManager.hpp
// ============================================================
namespace NoMoreDay::render {

class ParticleTextureManager {
public:
    static ParticleTextureManager& Get();
    
    void Init(int maxLayers = 64, int layerSize = 128);
    void Shutdown();
    
    // 加载纹理到指定层
    // 返回层索引 (用于 GPUParticle::textureIndex)
    int LoadLayer(const std::string& path);
    
    // 绑定 Texture2DArray 到指定纹理单元
    void Bind(uint32_t textureUnit) const;
    void Unbind(uint32_t textureUnit) const;
    
    bool IsInitialized() const;
    int GetLayerCount() const;
    
private:
    unsigned int m_textureArrayId = 0;
    int m_maxLayers = 64;
    int m_layerSize = 128;
    int m_loadedLayers = 0;
    bool m_initialized = false;
};

} // namespace NoMoreDay::render
```

### 5.2 ForceFieldManager

```cpp
// ============================================================
// engine/render/particle/ForceFieldManager.hpp
// ============================================================
namespace NoMoreDay::render {

class ForceFieldManager {
public:
    static ForceFieldManager& Get();
    
    void Init(int maxForceFields = 16);
    void Shutdown();
    
    // 添加力场，返回 ID
    int AddForceField(const components::GPUForceField& field);
    void RemoveForceField(int id);
    void ClearAll();
    
    // 每帧同步到 GPU
    void SyncToGPU();
    
    // 绑定 SSBO
    void BindSSBO(uint32_t bindingPoint) const;
    
    int GetActiveCount() const;
    
private:
    core::ComputeBuffer m_ssbo;
    std::vector<components::GPUForceField> m_fields;
    int m_maxFields = 16;
    bool m_dirty = false;
};

} // namespace NoMoreDay::render
```

### 5.3 GPUTrailRenderer

```cpp
// ============================================================
// engine/render/trail/GPUTrailRenderer.hpp
// ============================================================
namespace NoMoreDay::render {

class GPUTrailRenderer {
public:
    static GPUTrailRenderer& Get();
    
    void Init(int maxTrails = 512, int maxPointsPerTrail = 64);
    void Shutdown();
    
    // 分配/释放轨迹槽
    int AllocateTrail(const components::GPUTrailHeader& config);
    void FreeTrail(int trailId);
    
    // 每帧从 CPU 追加头部控制点
    void AppendPoint(int trailId, Vector2 position, Vector2 direction,
                     float width, uint32_t color);
    
    // 每帧更新 (衰减生命、同步 GPU)
    void Update(float dt);
    
    // 渲染 (在 VFXPass 中调用)
    void Render(const Camera2D& camera);
    
    // 清理
    void ClearAll();
    
    bool IsInitialized() const;
    int GetActiveTrailCount() const;
    
private:
    Shader m_trailShader = {0};
    int m_mvpLoc = -1;
    int m_trailIndexLoc = -1;
    unsigned int m_quadVAO = 0; // For triangle strip rendering
    
    core::ComputeBuffer m_headerSSBO;   // GPUTrailHeader[MAX_TRAILS]
    core::ComputeBuffer m_pointsSSBO;   // GPUTrailPoint[MAX_TRAILS * MAX_POINTS]
    
    struct TrailSlot {
        bool active = false;
        components::GPUTrailHeader header;
        std::vector<components::GPUTrailPoint> points; // CPU ring buffer
    };
    std::vector<TrailSlot> m_slots;
    
    int m_maxTrails = 512;
    int m_maxPointsPerTrail = 64;
    bool m_initialized = false;
};

} // namespace NoMoreDay::render
```

### 5.4 RenderConstants 扩展

```cpp
// RenderConstants.hpp 新增
namespace ParticleCS {
    // ... 现有 0-3 ...
    constexpr uint32_t FORCE_FIELDS = 4;     // 力场 SSBO (只读)
    constexpr uint32_t SUB_EMISSION = 5;     // 子发射 Buffer (写)
}

namespace TrailBinding {
    constexpr uint32_t HEADERS = 10;   // Binding::SSBO_RESERVED_10
    constexpr uint32_t POINTS = 11;    // Binding::SSBO_RESERVED_11
}
```

---

## 6. 边界约束 (Constraints)

### 6.1 可修改文件清单

| 文件 | 修改类型 |
|------|----------|
| `src/engine/render/GPUData.hpp` | 修改 GPUParticle padding → 纹理字段 |
| `src/engine/render/GPUParticleSystem.hpp/cpp` | 纹理绑定、力场分发、子发射逻辑 |
| `src/engine/render/core/RenderConstants.hpp` | RenderConfig 扩展 |
| `src/engine/render/core/QualityTierManager.cpp` | Tier 配置映射 |
| `src/engine/render/passes/VFXPass.cpp` | 集成 TrailRenderer |
| `src/engine/render/RenderConstants.hpp` | Binding 扩展 |
| `src/game/systems/vfx/TrailSystem.cpp` | GPU 路径切换 |
| `src/game/components/vfx/MotionTrailComponent.hpp` | 增加 GPU 配置字段 |
| `assets/shaders/particle.*` | 纹理采样、力场、子发射 |

### 6.2 禁止修改文件

| 文件 | 理由 |
|------|------|
| `GPUEntitySystem.*` | 实体系统已成熟，不在 Phase 3 范围 |
| `MDIRenderer.*` | MDI 管线独立 |
| `PostProcessPass.*` | 后处理管线无关 |
| `LightingPass.*` | 光照系统无关 |
| `RenderGraph.*` | RenderGraph 核心不应被修改 |

---

## 7. 风险与缓解 (Risks & Mitigations)

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| **GPUParticle ABI 破坏** | 所有现有粒子效果失效 | padding 替换保持 64 字节不变；static_assert 守卫；默认值保持 SDF 行为 |
| **Texture2DArray 显存占用** | 低端 GPU 显存不足 | Low Tier 禁用纹理粒子；按需加载纹理层 |
| **子发射器级联爆炸** | 粒子数失控 | 每帧子发射总量硬性上限 (2048)；子发射不可递归 |
| **轨迹渲染 DrawCall 增多** | 512 条轨迹 = 512 DrawCall | 优先实现 Multi-Draw 合批；Low Tier 禁用轨迹 |
| **GLSL int16 对齐** | 结构体跨驱动不一致 | 使用 int (4 字节) 打包，位操作拆分 |

---

## 8. 验收标准 (Acceptance Criteria)

### 8.1 功能验收

- [ ] `textureIndex = -1` 时所有现有粒子效果完全无回归
- [ ] 纹理粒子可正常加载 Texture2DArray 并在 Fragment Shader 中采样
- [ ] 序列帧动画根据 lifetime 自动播放完整循环
- [ ] 子发射器在粒子死亡时触发新粒子（Tier >= High）
- [ ] 力场系统可对粒子施加径向力/涡旋力/噪声力
- [ ] GPU TrailRenderer 正确渲染宽度/颜色渐变的轨迹
- [ ] 旧 CPU TrailSystem 在 `trailEnabled = true` 时切换到 GPU 路径

### 8.2 性能验收

| 场景 | 指标 | 阈值 |
|------|------|------|
| 纹理粒子 10k 个 | VFXPass 耗时 | < 0.8ms |
| 力场 16 个 + 粒子 50k | Compute 耗时 | < 0.5ms |
| GPU Trail 256 条 × 48 点 | Trail Render 耗时 | < 0.3ms |
| 子发射器 1k 死亡/帧 | Sub-Emit Dispatch | < 0.2ms |

### 8.3 稳定性验收

- [ ] 30 分钟战斗无显存持续增长
- [ ] 纹理加载失败回退到 SDF 渲染（不崩溃）
- [ ] 力场数量超限时静默忽略新增（不崩溃）
- [ ] Trail 槽位耗尽时返回 -1，不影响其他系统

---

## 9. 已确认设计决策 (Confirmed Decisions)

| 问题 | 决策 |
|------|------|
| **GPUParticle 结构 ABI** | 复用 `padding[3]` 的 12 字节，保持 64 字节不变 |
| **纹理格式** | Texture2DArray，固定 128x128 每层 |
| **子发射器递归** | 禁止递归：子发射产生的粒子 `subEmitterType = 0` |
| **GLSL 对齐策略** | 小于 4 字节的字段合并为 int/uint，位操作拆分 |
| **Trail 渲染方式** | CPU 写控制点 → GPU 展宽三角带（非 Compute 生成） |
| **向下兼容** | 所有新字段默认值保持原有行为（-1 / 0） |

---

*规格版本: 1.0*
*最后更新: 2026-02-13*
