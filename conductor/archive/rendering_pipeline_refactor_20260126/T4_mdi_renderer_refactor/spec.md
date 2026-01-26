# Track 4: MDIRenderer Refactor 规格说明书 (V1.0)

> **Track ID**: `T4_mdi_renderer_refactor`
> **依赖**: T1, T2
> **状态**: 🔵 待开始
> **预计工时**: 8h

---

## 1. 概述 (Overview)

本 Track 旨在规范化 `MDIRenderer`，使其成为纯粹的渲染执行器，移除所有底层 GL 扩展加载代码，统一使用 `RenderConstants` 和 `GPUUtils`。

### 1.1 问题陈述

**当前 MDIRenderer.cpp 的问题**:

1. **GL 扩展手动加载** (Line 32-75):
   ```cpp
   typedef void (*PFNGLDRAWARRAYSINDIRECTPROC)(...);
   static PFNGLDRAWARRAYSINDIRECTPROC glDrawArraysIndirect = nullptr;
   // ...
   glDrawArraysIndirect = (PFNGLDRAWARRAYSINDIRECTPROC)glfwGetProcAddress("glDrawArraysIndirect");
   ```

2. **魔法数字绑定** (Line 151-152, 212-217):
   ```cpp
   m_visibleBuffer.BindBase(1);
   m_commandBuffer.BindBase(2);
   // ...
   entities.BindPreviousNoSync(0);
   m_statsBuffer.BindPreviousNoSync(3);
   ```

3. **GL 宏重定义** (Line 8-46):
   ```cpp
   #ifndef GL_DRAW_INDIRECT_BUFFER
   #define GL_DRAW_INDIRECT_BUFFER 0x8F3F
   #endif
   // 多个宏定义重复存在于多个文件
   ```

4. **平台抽象不一致**:
   - 部分使用 rlgl (`rlEnableShader`, `rlComputeShaderDispatch`)
   - 部分使用原生 GL (`glDrawArraysIndirect`, `glBindBuffer`)
   - 混合模式使代码难以追踪

### 1.2 设计目标

1. **统一抽象层**: 所有 GL 调用通过 `GPUUtils` 进行。
2. **类型安全绑定**: 所有 Buffer Binding 使用 `RenderConstants::Binding`。
3. **消除宏重定义**: GL 常量统一定义在 `RenderConstants.hpp`。
4. **保持性能**: 重构不能引入性能回归。

---

## 2. 架构设计

### 2.1 重构后的 MDIRenderer 职责

```
┌─────────────────────────────────────────────────────────────────┐
│                        MDIRenderer                              │
│                     (纯渲染执行器)                               │
├─────────────────────────────────────────────────────────────────┤
│  职责:                                                          │
│  1. 管理 Quad VAO/VBO (顶点缓冲)                                 │
│  2. 管理 Visibility/Command/Stats PersistentBuffer              │
│  3. 执行 Cull Compute Shader                                    │
│  4. 执行 Indirect Draw                                          │
├─────────────────────────────────────────────────────────────────┤
│  不再负责:                                                       │
│  × GL 扩展函数加载 (由 GPUUtils 负责)                            │
│  × GL 常量定义 (由 RenderConstants 负责)                         │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 依赖关系变化

```
Before:
    MDIRenderer
        ├── glfwGetProcAddress (直接调用)
        ├── GL 宏定义 (本地)
        └── rlgl

After:
    MDIRenderer
        ├── RenderConstants::Binding
        ├── RenderConstants::Barrier
        ├── GPUUtils::*
        └── rlgl
```

---

## 3. 数据模型 (Data Model)

### 3.1 GL 常量迁移

需要从 `MDIRenderer.cpp` 迁移到 `RenderConstants.hpp` 的常量:

```cpp
// 在 RenderConstants.hpp 中添加
namespace NoMoreDay::RenderConstants::GL {

// Buffer Targets
constexpr uint32_t DRAW_INDIRECT_BUFFER = 0x8F3F;
constexpr uint32_t SHADER_STORAGE_BUFFER = 0x90D2;

// Texture Targets
constexpr uint32_t TEXTURE_2D_ARRAY = 0x8C1A;
constexpr uint32_t TEXTURE0 = 0x84C0;

// Draw Modes
constexpr uint32_t TRIANGLES = 0x0004;

} // namespace NoMoreDay::RenderConstants::GL
```

### 3.2 重构后的 MDIRenderer 接口

```cpp
// ============================================================
// src/engine/render/MDIRenderer.hpp (重构后)
// ============================================================
#pragma once

#include "engine/render/PersistentBuffer.hpp"
#include "engine/render/RenderConstants.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "raylib.h"
#include <cstdint>

namespace NoMoreDay::render {

class MDIRenderer {
public:
    static MDIRenderer& Get();

    /**
     * @brief 初始化 MDI 渲染器。
     * @pre GPUUtils::IsInitialized() == true
     */
    void Init(ResourceManager& rm, uint32_t maxEntities);

    void Update(ResourceManager& rm,
                const PersistentBuffer& entityBuffer,
                float alpha);

    void UpdateStats(const std::vector<components::GPUVisualStats>& stats,
                     int count = -1);

    /**
     * @brief 执行 GPU 视锥剔除。
     * @param viewBounds AABB 边界 (minX, minY, maxX, maxY)
     */
    void Cull(Vector4 viewBounds);

    /**
     * @brief 执行 Indirect Draw。
     * @param rm ResourceManager 引用 (用于获取纹理)
     * @param entities 实体数据缓冲区
     * @param renderAlpha 插值 Alpha
     */
    void Render(ResourceManager& rm, 
                const PersistentBuffer& entities,
                float renderAlpha);

    void Shutdown();
    void ResetCommand();

    bool IsInitialized() const { return m_quadVAO != 0; }
    int GetCurrentSlot() const { return m_commandBuffer.GetCurrentSlot(); }
    unsigned int GetId() const { return m_commandBuffer.GetId(); }
    size_t GetSize() const { return m_commandBuffer.GetSize(); }

private:
    MDIRenderer() = default;
    ~MDIRenderer();

    MDIRenderer(const MDIRenderer&) = delete;
    MDIRenderer& operator=(const MDIRenderer&) = delete;

    PersistentBuffer m_visibleBuffer;
    PersistentBuffer m_commandBuffer;
    PersistentBuffer m_statsBuffer;

    Shader m_cullShader;
    Shader m_renderShader;

    uint32_t m_quadVAO = 0;
    uint32_t m_quadVBO = 0;
    uint32_t m_maxEntities = 0;
};

} // namespace NoMoreDay::render
```

---

## 4. 实现细节

### 4.1 Init() 重构

```cpp
void MDIRenderer::Init(ResourceManager& rm, uint32_t maxEntities) {
    if (m_quadVAO != 0) return;

    // 前置检查
    if (!utils::GPUUtils::IsInitialized()) {
        LOG_ERROR("MDIRenderer::Init called before GPUUtils::Initialize!");
        return;
    }

    m_maxEntities = maxEntities;

    // 使用 RenderConstants 创建 Buffers
    // (Buffer 创建逻辑不变，但绑定使用常量)
    m_visibleBuffer.Create(maxEntities * sizeof(uint32_t), 3);
    m_commandBuffer.Create(sizeof(DrawArraysIndirectCommand), 3);
    m_statsBuffer.Create(maxEntities * sizeof(components::GPUVisualStats), 3);

    // 加载 Shaders (不变)
    m_cullShader = rm.loadComputeShader(...);
    m_renderShader = rm.loadShader(...);

    // 创建 Quad VAO (使用 rlgl，不变)
    // ...
}
```

### 4.2 Cull() 重构

```cpp
void MDIRenderer::Cull(Vector4 viewBounds) {
    using namespace RenderConstants;
    
    if (!m_cullShader.id) {
        LOG_LIMITED_ERROR(5.0f, "MDI Cull Fail: Shader ID is 0!");
        return;
    }

    ResetCommand();
    rlEnableShader(m_cullShader.id);

    // 使用类型安全的 Binding
    m_visibleBuffer.BindBase(static_cast<uint32_t>(Binding::SSBO_VISIBLE_ID));
    m_commandBuffer.BindBase(static_cast<uint32_t>(Binding::SSBO_COMMAND));

    // Set Uniforms (不变)
    int locView = rlGetLocationUniform(m_cullShader.id, "viewBounds");
    if (locView != -1)
        rlSetUniform(locView, &viewBounds, RL_SHADER_UNIFORM_VEC4, 1);

    int locMax = rlGetLocationUniform(m_cullShader.id, "maxEntities");
    if (locMax != -1) {
        int me = (int)m_maxEntities;
        rlSetUniform(locMax, &me, RL_SHADER_UNIFORM_INT, 1);
    }

    // Dispatch Compute (使用 GPUUtils)
    int groups = (m_maxEntities + 255) / 256;
    utils::GPUUtils::DispatchComputeNoBarrier(groups, 1, 1);

    // Memory Barrier (使用枚举)
    utils::GPUUtils::MemoryBarrier(Barrier::Command | Barrier::SSBO | Barrier::Buffer);

    m_visibleBuffer.Lock();
    m_commandBuffer.Lock();
}
```

### 4.3 Render() 重构

```cpp
void MDIRenderer::Render(ResourceManager& rm, 
                         const PersistentBuffer& entities,
                         float renderAlpha) {
    using namespace RenderConstants;
    
    if (m_renderShader.id == 0) return;

    rlDrawRenderBatchActive();

    Matrix modelview = rlGetMatrixModelview();
    Matrix projection = rlGetMatrixProjection();
    Matrix mvp = MatrixMultiply(modelview, projection);

    rlEnableShader(m_renderShader.id);

    // 绑定纹理 (使用 GPUUtils)
    unsigned int texArray = rm.getEntityTextureArray();
    if (texArray != 0) {
        int locTex = rlGetLocationUniform(m_renderShader.id, "entityTextures");
        if (locTex != -1) {
            utils::GPUUtils::ActiveTexture(TextureUnit::TEX_ENTITY_ARRAY);
            utils::GPUUtils::BindTexture(GL::TEXTURE_2D_ARRAY, texArray);
            int unit = 0;
            rlSetUniform(locTex, &unit, RL_SHADER_UNIFORM_INT, 1);
        }
    }

    // Set Uniforms (不变)
    int locVP = rlGetLocationUniform(m_renderShader.id, "viewProj");
    if (locVP != -1) rlSetUniformMatrix(locVP, mvp);

    int locInterp = rlGetLocationUniform(m_renderShader.id, "interpolationFactor");
    if (locInterp != -1) rlSetUniform(locInterp, &renderAlpha, RL_SHADER_UNIFORM_FLOAT, 1);

    // 绑定 SSBOs (使用类型安全的 Binding)
    entities.BindPreviousNoSync(static_cast<uint32_t>(Binding::SSBO_ENTITY_DATA));
    m_visibleBuffer.BindPreviousNoSync(static_cast<uint32_t>(Binding::SSBO_VISIBLE_ID));
    m_statsBuffer.BindPreviousNoSync(static_cast<uint32_t>(Binding::SSBO_VISUAL_STATS));

    // VAO & Indirect Buffer
    rlEnableVertexArray(m_quadVAO);

    int bufferCount = m_commandBuffer.GetBufferCount();
    int prevSlot = (m_commandBuffer.GetCurrentSlot() - 1 + bufferCount) % bufferCount;
    size_t offset = (size_t)prevSlot * m_commandBuffer.GetSize();

    utils::GPUUtils::BindBuffer(GL::DRAW_INDIRECT_BUFFER, m_commandBuffer.GetId());

    // Memory Barrier & Draw
    utils::GPUUtils::MemoryBarrier(Barrier::Command | Barrier::SSBO | Barrier::Buffer);
    utils::GPUUtils::DrawArraysIndirect(GL::TRIANGLES, offset);

    // Cleanup
    utils::GPUUtils::BindBuffer(GL::DRAW_INDIRECT_BUFFER, 0);
    rlDisableVertexArray();
    rlDisableShader();
}
```

---

## 5. 影响范围 (Affected Files)

| 文件 | 变更类型 | 说明 |
|---|---|---|
| `RenderConstants.hpp` | 修改 | 添加 `GL` 命名空间常量 |
| `MDIRenderer.hpp` | 修改 | 移除 `m_resetShader` (如果未使用) |
| `MDIRenderer.cpp` | 重构 | 移除所有手动 GL 加载，使用 `GPUUtils` |

---

## 6. 验收标准 (Acceptance Criteria)

- [ ] `MDIRenderer.cpp` 中无 `glfwGetProcAddress` 调用。
- [ ] `MDIRenderer.cpp` 中无 `#define GL_*` 宏定义。
- [ ] 所有 `BindBase(X)` / `BindPreviousNoSync(X)` 使用 `RenderConstants::Binding`。
- [ ] 所有 `glMemoryBarrier(X)` 替换为 `GPUUtils::MemoryBarrier(Barrier::*)`。
- [ ] 所有 `glDrawArraysIndirect` 替换为 `GPUUtils::DrawArraysIndirect`。
- [ ] 编译通过，无新警告。
- [ ] 所有测试通过。
- [ ] 视觉验收: 实体渲染、剔除效果与重构前一致。
- [ ] 性能验收: 无明显回归 (< 5% 差异)。

---

## 7. 风险与缓解

| 风险 | 影响 | 缓解措施 |
|---|---|---|
| GPUUtils 初始化顺序 | 渲染失败 | 在 `Init()` 添加前置检查 |
| 遗漏替换 | 运行时崩溃 | 代码审查 + 全量搜索验证 |
| 性能回归 | 帧率下降 | 基准测试对比 |

---

*规格版本: 1.0*
*最后更新: 2026-01-26*
