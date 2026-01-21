# Phase 1: GPU-Driven MDI Rendering 规格说明书

**Track ID**: `performance_optimization/phase1_mdi_rendering`  
**优先级**: P0 (极致性能)  
**预计收益**: CPU Draw Call 开销降低 90%+  
**依赖**: 无

---

## 1. 问题陈述 (Problem Statement)

### 当前实现分析
```
GPUEntitySystem::Render()
├── CPU 遍历所有实体决定可见性 (O(N))
├── 为每个实体类型调用 rlDrawVertexArrayInstanced
└── 每帧多次 Buffer Bind/Uniform Set
```

### 性能瓶颈
| 操作 | 10k 实体开销 | 目标 |
|------|-------------|------|
| 可见性判断 | ~0.5ms CPU | 0 (GPU 完成) |
| Draw Call 提交 | ~1.2ms CPU | < 0.1ms (单次 MDI) |
| Buffer Binding | ~0.3ms | 0 (一次绑定) |

---

## 2. 技术方案 (Technical Design)

### 2.1 架构概览

```
┌─────────────────────────────────────────────────────────────────┐
│                         CPU (每帧一次)                           │
├─────────────────────────────────────────────────────────────────┤
│  1. 上传实体变换 → InstanceBuffer (SSBO)                         │
│  2. 调用 glMultiDrawArraysIndirect (命令数=1)                    │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                     GPU Compute Shader                          │
├─────────────────────────────────────────────────────────────────┤
│  1. 视锥剔除 (Frustum Culling)                                   │
│  2. 写入 DrawArraysIndirectCommand 缓冲区                        │
│  3. 输出可见实体索引到紧凑数组                                    │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                     Vertex/Fragment Shader                       │
├─────────────────────────────────────────────────────────────────┤
│  gl_InstanceID → 可见实体索引 → 读取实例数据 → 渲染               │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 数据结构

```cpp
// OpenGL 标准间接绘制命令
struct DrawArraysIndirectCommand {
    uint32_t count;         // 顶点数 (e.g., 4 for quad)
    uint32_t instanceCount; // 可见实体数 (GPU 填充)
    uint32_t first;         // 起始顶点 (0)
    uint32_t baseInstance;  // 基础实例 (0)
};

// 扩展的实体实例数据 (SSBO)
struct alignas(16) GPUInstanceData {
    float position[2];      // 世界坐标
    float scale[2];         // 尺寸
    float rotation;         // 旋转角度
    uint32_t textureIndex;  // Atlas 纹理索引
    uint32_t flags;         // 状态标志位
    float _padding;
};
```

### 2.3 Shader 设计

#### `cull.compute` (视锥剔除)
```glsl
#version 430 core
layout(local_size_x = 256) in;

struct InstanceData {
    vec2 position;
    vec2 scale;
    float rotation;
    uint textureIndex;
    uint flags;
    float _padding;
};

layout(std430, binding = 0) readonly buffer Entities { InstanceData entities[]; };
layout(std430, binding = 1) buffer VisibleIndices { uint visibleIndices[]; };
layout(std430, binding = 2) buffer DrawCommand { 
    uint count;
    uint instanceCount;
    uint first;
    uint baseInstance;
};

uniform vec4 frustum[4];  // 左/右/上/下平面
uniform uint maxEntities;

void main() {
    uint id = gl_GlobalInvocationID.x;
    if (id >= maxEntities) return;
    
    InstanceData e = entities[id];
    if (e.scale.x <= 0.0) return;  // 无效实体
    
    // 简化的 2D 视锥检测 (AABB vs Frustum)
    vec2 pos = e.position;
    bool visible = (pos.x >= frustum[0].x && pos.x <= frustum[1].x &&
                    pos.y >= frustum[2].y && pos.y <= frustum[3].y);
    
    if (visible) {
        uint idx = atomicAdd(instanceCount, 1);
        visibleIndices[idx] = id;
    }
}
```

#### `entity_mdi.vert` (顶点着色器)
```glsl
#version 430 core

layout(location = 0) in vec2 aPos;  // 单位四边形顶点

struct InstanceData { ... };  // 同上
layout(std430, binding = 0) readonly buffer Entities { InstanceData entities[]; };
layout(std430, binding = 1) readonly buffer VisibleIndices { uint visibleIndices[]; };

uniform mat4 viewProj;

out vec2 vTexCoord;
flat out uint vTextureIndex;

void main() {
    uint entityId = visibleIndices[gl_InstanceID];
    InstanceData e = entities[entityId];
    
    // 变换
    vec2 worldPos = e.position + aPos * e.scale;
    gl_Position = viewProj * vec4(worldPos, 0.0, 1.0);
    
    // UV (基于顶点位置)
    vTexCoord = aPos + 0.5;
    vTextureIndex = e.textureIndex;
}
```

---

## 3. 实现计划 (Implementation Plan)

### Task 1: 创建 MDIRenderer 类
**文件**: `src/engine/render/MDIRenderer.hpp/cpp`

```cpp
class MDIRenderer {
public:
    void Init(uint32_t maxEntities);
    void UpdateInstances(const std::vector<GPUInstanceData>& data);
    void Cull(const glm::vec4 frustum[4]);
    void Render(const glm::mat4& viewProj);
    void Shutdown();

private:
    ComputeBuffer m_instanceBuffer;     // 所有实体
    ComputeBuffer m_visibleBuffer;      // 可见索引
    ComputeBuffer m_commandBuffer;      // DrawIndirectCommand
    Shader m_cullShader;
    Shader m_renderShader;
    uint32_t m_quadVAO;
};
```

### Task 2: 编写 cull.compute
**文件**: `assets/shaders/cull.compute`

### Task 3: 改造 entity.vert/frag
**文件**: `assets/shaders/entity_mdi.vert`, `assets/shaders/entity_mdi.frag`

### Task 4: 集成到 GPUEntitySystem
**文件**: `src/engine/render/GPUEntitySystem.cpp`
- 替换原有 `Render()` 实现
- 保留原实现作为 `RenderLegacy()` 回退

### Task 5: 验证与测试
- 创建 `MDIRenderTest.hpp` 验证渲染正确性
- 使用 RenderDoc 对比 Draw Call 数量

---

## 4. 接口契约 (API Contract)

```cpp
// 使用示例
void GameplayState::Render() {
    auto& renderer = MDIRenderer::Get();
    
    // GPU Culling (异步，上一帧的 frustum)
    renderer.Cull(m_cachedFrustum);
    
    // 渲染可见实体
    renderer.Render(camera.GetViewProjection());
    
    // 为下一帧缓存 frustum
    m_cachedFrustum = camera.GetFrustumPlanes();
}
```

---

## 5. 风险与缓解

| 风险 | 缓解 |
|------|------|
| rlgl 状态冲突 | 完全绕过 rlgl，使用原生 GL 调用 |
| atomicAdd 争用 | 使用 Wave-level prefix sum 优化 |
| Indirect Buffer 驱动兼容 | 运行时检测 ARB_draw_indirect |

---

*设计者: Gemini (Skill: designer)*
