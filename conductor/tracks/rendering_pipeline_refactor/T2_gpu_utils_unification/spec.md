# Track 2: GPU Utils Unification 规格说明书 (V1.0)

> **Track ID**: `T2_gpu_utils_unification`
> **依赖**: T1 (`render_constants`)
> **状态**: 🔵 待开始
> **预计工时**: 8h

---

## 1. 概述 (Overview)

本 Track 旨在统一所有 OpenGL 扩展函数的加载与调用，消除代码重复，建立单一 GPU 抽象入口。

### 1.1 问题陈述

当前代码中存在多处重复的 GL 扩展函数加载：

**MDIRenderer.cpp (Line 32-42)**:
```cpp
typedef void (*PFNGLDRAWARRAYSINDIRECTPROC)(...);
static PFNGLDRAWARRAYSINDIRECTPROC glDrawArraysIndirect = nullptr;
// ... 手动 glfwGetProcAddress
```

**GPUUtils.hpp (Line 70-72)**:
```cpp
static PFNGLMEMORYBARRIERPROC glMemoryBarrier_ptr = nullptr;
glMemoryBarrier_ptr = (PFNGLMEMORYBARRIERPROC)glfwGetProcAddress("glMemoryBarrier");
```

**ComputeBuffer.hpp (Line 100-107)**:
```cpp
static PFNGLBINDBUFFERPROC glBindBufferFn = nullptr;
glBindBufferFn = (PFNGLBINDBUFFERPROC)glfwGetProcAddress("glBindBuffer");
```

这种模式的问题：
1. **代码冗余**: 同一函数指针在多处定义和加载。
2. **维护困难**: 添加新 GL 函数需在多处修改。
3. **抽象泄漏**: 业务代码直接接触底层 GL 调用。

### 1.2 设计目标

1. **单一加载点**: 所有 GL 扩展函数在 `GPUUtils` 初始化时一次性加载。
2. **类型安全 API**: 暴露高级封装接口，隐藏原始 GL 调用。
3. **Barrier 枚举化**: 使用 `RenderConstants::Barrier` 替代 GL 宏。
4. **零业务侵入**: `MDIRenderer`, `ComputeBuffer` 等仅调用 `GPUUtils` 接口。

---

## 2. 数据模型 (Data Model)

### 2.1 GPUUtils 扩展接口

```cpp
// ============================================================
// src/engine/render/GPUUtils.hpp (扩展)
// ============================================================
#pragma once
#include "engine/render/RenderConstants.hpp"
#include "raylib.h"
#include "rlgl.h"
#include "GLFW/glfw3.h"
#include "core/logging/Logger.hpp"
#include <cstdint>

namespace NoMoreDay::utils {

using namespace NoMoreDay::RenderConstants;

struct GPUSupportInfo {
    int majorVersion = 0;
    int minorVersion = 0;
    bool computeShaderSupported = false;
    bool indirectDrawSupported = false;
    bool persistentMappingSupported = false;
    int maxComputeWorkGroupCount[3] = {0};
    int maxComputeWorkGroupSize[3] = {0};
    int maxComputeWorkGroupInvocations = 0;
};

class GPUUtils {
public:
    // === 初始化 ===
    /**
     * @brief 检测 GPU 能力并加载所有 GL 扩展函数。
     * 必须在 OpenGL Context 创建后、任何渲染操作前调用。
     */
    static GPUSupportInfo Initialize();
    
    /**
     * @brief 检查是否已初始化。
     */
    static bool IsInitialized();

    // === Memory Barriers ===
    /**
     * @brief 发出内存屏障，确保 GPU 操作顺序。
     * @param barriers 使用 Barrier 枚举组合 (如 Barrier::SSBO | Barrier::Command)
     */
    static void MemoryBarrier(Barrier barriers);
    
    // 便捷重载，接受原始 uint32_t (用于兼容)
    static void MemoryBarrier(uint32_t barriers);

    // === Compute Shader ===
    /**
     * @brief 分派 Compute Shader，自动添加 SSBO Barrier。
     */
    static void DispatchCompute(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ);
    
    /**
     * @brief 分派 Compute Shader，不添加 Barrier (性能敏感场景)。
     */
    static void DispatchComputeNoBarrier(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ);

    // === Buffer Operations ===
    /**
     * @brief 绑定缓冲区到指定 GL Target。
     * @param target GL_SHADER_STORAGE_BUFFER, GL_DRAW_INDIRECT_BUFFER, etc.
     * @param bufferId OpenGL Buffer ID
     */
    static void BindBuffer(uint32_t target, uint32_t bufferId);
    
    /**
     * @brief 绑定缓冲区到 SSBO Binding Point (glBindBufferBase)。
     */
    static void BindBufferBase(Binding binding, uint32_t bufferId);
    
    // 便捷重载
    static void BindBufferBase(uint32_t binding, uint32_t bufferId);

    // === Indirect Draw ===
    /**
     * @brief 执行 Indirect Draw Arrays。
     * @param mode GL_TRIANGLES, GL_TRIANGLE_STRIP, etc.
     * @param indirectOffset Byte offset into the bound GL_DRAW_INDIRECT_BUFFER
     */
    static void DrawArraysIndirect(uint32_t mode, size_t indirectOffset = 0);

    // === Texture Operations ===
    /**
     * @brief 激活指定纹理单元。
     */
    static void ActiveTexture(TextureUnit unit);
    
    // 便捷重载
    static void ActiveTexture(uint32_t unit);

    /**
     * @brief 绑定纹理到指定 Target。
     */
    static void BindTexture(uint32_t target, uint32_t textureId);

    // === Image Binding (Compute Shader) ===
    static void BindImageTexture(uint32_t unit, uint32_t textureId, 
                                  int level = 0, bool layered = false, 
                                  int layer = 0, uint32_t access = 0x88B9, // GL_WRITE_ONLY
                                  uint32_t format = 0x8058); // GL_RGBA8

private:
    GPUUtils() = delete; // 静态类，禁止实例化
    
    static bool s_initialized;
    
    // 缓存的函数指针 (内部使用)
    static void* s_glMemoryBarrier;
    static void* s_glDrawArraysIndirect;
    static void* s_glBindBuffer;
    static void* s_glBindBufferBase;
    static void* s_glActiveTexture;
    static void* s_glBindImageTexture;
};

} // namespace NoMoreDay::utils
```

### 2.2 实现细节

```cpp
// ============================================================
// src/engine/render/GPUUtils.cpp (新增或扩展)
// ============================================================
#include "engine/render/GPUUtils.hpp"

namespace NoMoreDay::utils {

bool GPUUtils::s_initialized = false;
void* GPUUtils::s_glMemoryBarrier = nullptr;
void* GPUUtils::s_glDrawArraysIndirect = nullptr;
void* GPUUtils::s_glBindBuffer = nullptr;
void* GPUUtils::s_glBindBufferBase = nullptr;
void* GPUUtils::s_glActiveTexture = nullptr;
void* GPUUtils::s_glBindImageTexture = nullptr;

GPUSupportInfo GPUUtils::Initialize() {
    if (s_initialized) {
        return CheckSupport(); // 已初始化，返回缓存信息
    }
    
    GPUSupportInfo info = CheckSupport();
    
    // 加载所有扩展函数
    s_glMemoryBarrier = glfwGetProcAddress("glMemoryBarrier");
    s_glDrawArraysIndirect = glfwGetProcAddress("glDrawArraysIndirect");
    s_glBindBuffer = glfwGetProcAddress("glBindBuffer");
    s_glBindBufferBase = glfwGetProcAddress("glBindBufferBase");
    s_glActiveTexture = glfwGetProcAddress("glActiveTexture");
    s_glBindImageTexture = glfwGetProcAddress("glBindImageTexture");
    
    // 验证关键函数
    info.indirectDrawSupported = (s_glDrawArraysIndirect != nullptr);
    if (!info.indirectDrawSupported) {
        LOG_WARN("glDrawArraysIndirect not supported! MDI rendering will fail.");
    }
    
    s_initialized = true;
    LOG_INFO("GPUUtils initialized. IndirectDraw: {}, Compute: {}", 
             info.indirectDrawSupported, info.computeShaderSupported);
    
    return info;
}

bool GPUUtils::IsInitialized() {
    return s_initialized;
}

void GPUUtils::MemoryBarrier(Barrier barriers) {
    MemoryBarrier(static_cast<uint32_t>(barriers));
}

void GPUUtils::MemoryBarrier(uint32_t barriers) {
    if (!s_glMemoryBarrier) return;
    using FnType = void (*)(uint32_t);
    reinterpret_cast<FnType>(s_glMemoryBarrier)(barriers);
}

void GPUUtils::DispatchCompute(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) {
    rlComputeShaderDispatch(groupsX, groupsY, groupsZ);
    MemoryBarrier(Barrier::SSBO);
}

void GPUUtils::DispatchComputeNoBarrier(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) {
    rlComputeShaderDispatch(groupsX, groupsY, groupsZ);
}

void GPUUtils::BindBuffer(uint32_t target, uint32_t bufferId) {
    if (!s_glBindBuffer) return;
    using FnType = void (*)(uint32_t, uint32_t);
    reinterpret_cast<FnType>(s_glBindBuffer)(target, bufferId);
}

void GPUUtils::BindBufferBase(Binding binding, uint32_t bufferId) {
    BindBufferBase(static_cast<uint32_t>(binding), bufferId);
}

void GPUUtils::BindBufferBase(uint32_t binding, uint32_t bufferId) {
    if (!s_glBindBufferBase) {
        rlBindShaderBuffer(bufferId, binding); // Fallback to rlgl
        return;
    }
    using FnType = void (*)(uint32_t, uint32_t, uint32_t);
    reinterpret_cast<FnType>(s_glBindBufferBase)(0x90D2, binding, bufferId); // GL_SHADER_STORAGE_BUFFER
}

void GPUUtils::DrawArraysIndirect(uint32_t mode, size_t indirectOffset) {
    if (!s_glDrawArraysIndirect) {
        LOG_LIMITED_ERROR(5.0f, "glDrawArraysIndirect not available!");
        return;
    }
    using FnType = void (*)(uint32_t, const void*);
    reinterpret_cast<FnType>(s_glDrawArraysIndirect)(mode, reinterpret_cast<const void*>(indirectOffset));
}

void GPUUtils::ActiveTexture(TextureUnit unit) {
    ActiveTexture(0x84C0 + static_cast<uint32_t>(unit)); // GL_TEXTURE0 + unit
}

void GPUUtils::ActiveTexture(uint32_t unit) {
    if (!s_glActiveTexture) return;
    using FnType = void (*)(uint32_t);
    reinterpret_cast<FnType>(s_glActiveTexture)(unit);
}

void GPUUtils::BindTexture(uint32_t target, uint32_t textureId) {
    glBindTexture(target, textureId); // raylib 已导出
}

void GPUUtils::BindImageTexture(uint32_t unit, uint32_t textureId, 
                                 int level, bool layered, 
                                 int layer, uint32_t access, uint32_t format) {
    if (!s_glBindImageTexture) {
        LOG_WARN("glBindImageTexture not available!");
        return;
    }
    using FnType = void (*)(uint32_t, uint32_t, int, unsigned char, int, uint32_t, uint32_t);
    reinterpret_cast<FnType>(s_glBindImageTexture)(unit, textureId, level, layered ? 1 : 0, layer, access, format);
}

} // namespace NoMoreDay::utils
```

---

## 3. 影响范围 (Affected Files)

| 文件 | 变更类型 | 说明 |
|---|---|---|
| `GPUUtils.hpp` | 重构 | 拆分为 `.hpp` + `.cpp`，扩展接口 |
| `GPUUtils.cpp` | **新增** | 实现所有函数 |
| `MDIRenderer.cpp` | 修改 | 移除本地函数指针，调用 `GPUUtils` |
| `ComputeBuffer.hpp` | 修改 | 移除 `Bind()` 内的手动加载，调用 `GPUUtils` |
| `PersistentBuffer.cpp` | 修改 | 使用 `GPUUtils::BindBufferBase` |
| `Game.cpp` | 修改 | 在窗口创建后调用 `GPUUtils::Initialize()` |

---

## 4. 使用示例

### 4.1 MDIRenderer 重构后

```cpp
void MDIRenderer::Render(...) {
    // 替换前:
    // glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
    // glDrawArraysIndirect(GL_TRIANGLES, (void*)offset);
    
    // 替换后:
    GPUUtils::MemoryBarrier(Barrier::Command | Barrier::SSBO);
    GPUUtils::DrawArraysIndirect(GL_TRIANGLES, offset);
}
```

### 4.2 ComputeBuffer 重构后

```cpp
void ComputeBuffer::Bind(unsigned int target) const {
    // 替换前:
    // static PFNGLBINDBUFFERPROC glBindBufferFn = ...
    // glBindBufferFn(target, m_id);
    
    // 替换后:
    GPUUtils::BindBuffer(target, m_id);
}
```

---

## 5. 验收标准 (Acceptance Criteria)

- [ ] `GPUUtils.cpp` 已创建，包含所有函数实现。
- [ ] `MDIRenderer.cpp` 无 `glfwGetProcAddress` 调用。
- [ ] `ComputeBuffer.hpp` 无 `glfwGetProcAddress` 调用。
- [ ] `Game.cpp` 在 OpenGL Context 创建后调用 `GPUUtils::Initialize()`。
- [ ] `Select-String -Path 'src/**/*.cpp' -Pattern 'glfwGetProcAddress' -Recurse` 仅在 `GPUUtils.cpp` 中有结果。
- [ ] 项目编译通过，所有测试通过。
- [ ] 运行时无 GL 函数加载失败的警告日志。

---

## 6. 风险与缓解

| 风险 | 影响 | 缓解措施 |
|---|---|---|
| 函数指针 Cast 错误 | 崩溃 | 使用 `static_assert` 验证函数签名大小 |
| 初始化顺序问题 | GL 调用失败 | 在 `GPUUtils::Initialize()` 中添加断言 |
| 驱动兼容性 | 某些 GPU 不支持 | 保留 Fallback 路径 (如 `rlBindShaderBuffer`) |

---

*规格版本: 1.0*
*最后更新: 2026-01-26*
