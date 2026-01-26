# Track 4: MDIRenderer Refactor 实施计划 (V1.0)

> **Track ID**: `T4_mdi_renderer_refactor`
> **依赖 Spec**: `spec.md` (V1.0)
> **依赖 Track**: T1, T2
> **预计工时**: 8h

---

## 阶段总览

| 阶段 | 名称 | 核心产出 | 状态 |
|---|---|---|---|
| **Phase 1** | 常量迁移 | GL 常量移到 RenderConstants | 🟢 已完成 |
| **Phase 2** | 函数指针移除 | 删除本地函数指针和加载代码 | 🟢 已完成 |
| **Phase 3** | 绑定替换 | 使用 RenderConstants::Binding | 🟢 已完成 |
| **Phase 4** | GPU 调用替换 | 使用 GPUUtils 接口 | 🟢 已完成 |
| **Phase 5** | 验证 | 测试与性能基准 | 🟢 已完成 |

---

## Phase 1: 常量迁移 (Constants Migration)

### Task 1.1: 扩展 RenderConstants.hpp
- [x] 添加 `GL` 子命名空间
- [x] 添加 `DRAW_INDIRECT_BUFFER = 0x8F3F`
- [x] 添加 `SHADER_STORAGE_BUFFER = 0x90D2`
- [x] 添加 `TEXTURE_2D_ARRAY = 0x8C1A`
- [x] 添加 `TEXTURE0 = 0x84C0`
- [x] 添加 `TRIANGLES = 0x0004`

### Task 1.2: 验证编译
- [x] 编译 `RenderConstants.hpp` 使用者
- [x] 确保无命名冲突

---

## Phase 2: 函数指针移除 (Function Pointer Removal)

### Task 2.1: 删除 typedef 和静态变量
- [x] 删除 `typedef void (*PFNGLDRAWARRAYSINDIRECTPROC)(...)` (Line 32-33)
- [x] 删除 `static PFNGLDRAWARRAYSINDIRECTPROC glDrawArraysIndirect = nullptr` (Line 34)
- [x] 删除 `typedef void (*PFNGLBINDBUFFERPROC)(...)` (Line 36)
- [x] 删除 `static PFNGLBINDBUFFERPROC glBindBuffer = nullptr` (Line 37)
- [x] 删除 `static PFNGLACTIVETEXTUREPROC glActiveTexture = nullptr` (Line 41)
- [x] 删除 `static PFNGLMEMORYBARRIERPROC glMemoryBarrier = nullptr` (Line 42)

### Task 2.2: 删除 Init() 中的加载代码
- [x] 删除 `if (!glDrawArraysIndirect) { ... }` 块 (Line 59-66)
- [x] 删除 `if (!glBindBuffer) { ... }` 块 (Line 67-69)
- [x] 删除 `if (!glActiveTexture) { ... }` 块 (Line 70-72)
- [x] 删除 `if (!glMemoryBarrier) { ... }` 块 (Line 73-75)

### Task 2.3: 删除 GL 宏定义
- [x] 删除 `#ifndef GL_DRAW_INDIRECT_BUFFER` ... `#endif` (Line 8-10)
- [x] 删除 `#ifndef GL_SHADER_IMAGE_ACCESS_BARRIER_BIT` ... (Line 12-14)
- [x] 删除 `#ifndef GL_COMMAND_BARRIER_BIT` ... (Line 16-18)
- [x] 删除 `#ifndef GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT` ... (Line 20-22)
- [x] 删除 `#ifndef GL_TEXTURE_2D_ARRAY` ... (Line 24-26)
- [x] 删除 `#ifndef GL_TEXTURE0` ... (Line 28-30)
- [x] 删除 `#ifndef GL_SHADER_STORAGE_BUFFER` ... (Line 44-46)

### Task 2.4: 添加必要的 include
- [x] 添加 `#include "engine/render/RenderConstants.hpp"`
- [x] 添加 `#include "engine/render/GPUUtils.hpp"`

---

## Phase 3: 绑定替换 (Binding Replacement)

### Task 3.1: 替换 Cull() 中的绑定
- [x] Line 151: `m_visibleBuffer.BindBase(1)` → `BindBase(static_cast<uint32_t>(Binding::SSBO_VISIBLE_ID))`
- [x] Line 152: `m_commandBuffer.BindBase(2)` → `BindBase(static_cast<uint32_t>(Binding::SSBO_COMMAND))`

### Task 3.2: 替换 Render() 中的绑定
- [x] Line 212: `entities.BindPreviousNoSync(0)` → `BindPreviousNoSync(static_cast<uint32_t>(Binding::SSBO_ENTITY_DATA))`
- [x] Line 215: `m_visibleBuffer.BindPreviousNoSync(1)` → 使用 `Binding::SSBO_VISIBLE_ID`
- [x] Line 217: `m_statsBuffer.BindPreviousNoSync(3)` → 使用 `Binding::SSBO_VISUAL_STATS`

### Task 3.3: 添加 using 声明
- [x] 在 Cull(), Render() 开头添加: `using namespace RenderConstants;`

---

## Phase 4: GPU 调用替换 (GPU Call Replacement)

### Task 4.1: 替换 Memory Barrier 调用
- [x] Cull() Line 169:
  ```cpp
  // 替换前:
  glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
  // 替换后:
  utils::GPUUtils::MemoryBarrier(Barrier::Command | Barrier::SSBO | Barrier::Buffer);
  ```
- [x] Render() Line 229: 同上

### Task 4.2: 替换 DrawArraysIndirect 调用
- [x] Render() Line 231:
  ```cpp
  // 替换前:
  glDrawArraysIndirect(GL_TRIANGLES, (void*)offset);
  // 替换后:
  utils::GPUUtils::DrawArraysIndirect(GL::TRIANGLES, offset);
  ```

### Task 4.3: 替换 BindBuffer 调用
- [x] Render() Line 226:
  ```cpp
  // 替换前:
  m_commandBuffer.Bind(GL_DRAW_INDIRECT_BUFFER);
  // 替换后:
  utils::GPUUtils::BindBuffer(GL::DRAW_INDIRECT_BUFFER, m_commandBuffer.GetId());
  ```
- [x] Render() Line 234-235:
  ```cpp
  // 替换前:
  if (glBindBuffer) glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
  // 替换后:
  utils::GPUUtils::BindBuffer(GL::DRAW_INDIRECT_BUFFER, 0);
  ```

### Task 4.4: 替换 Texture 操作
- [x] Render() Line 192-197:
  ```cpp
  // 替换前:
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, texArray);
  // 替换后:
  utils::GPUUtils::ActiveTexture(TextureUnit::TEX_ENTITY_ARRAY);
  utils::GPUUtils::BindTexture(GL::TEXTURE_2D_ARRAY, texArray);
  ```

### Task 4.5: 添加 GPUUtils 初始化检查
- [x] 在 `Init()` 开头添加:
  ```cpp
  if (!utils::GPUUtils::IsInitialized()) {
      LOG_ERROR("MDIRenderer::Init called before GPUUtils::Initialize!");
      return;
  }
  ```

---

## Phase 5: 验证 (Verification)

### Task 5.1: 编译验证
- [x] 执行 `.\build.bat`
- [x] 确保无编译错误、无新警告

### Task 5.2: 代码搜索验证
- [x] `Select-String -Path 'MDIRenderer.cpp' -Pattern 'glfwGetProcAddress'` → 空
- [x] `Select-String -Path 'MDIRenderer.cpp' -Pattern '#define GL_'` → 空
- [x] `Select-String -Path 'MDIRenderer.cpp' -Pattern 'Bind.*\([0-3]\)'` → 空 (仅绑定点 0-3)

### Task 5.3: 单元测试
- [x] 运行 `.\build\bin\Release\NoMoreDayTests.exe`
- [x] 确保所有测试通过

### Task 5.4: 视觉验收
- [x] 运行游戏，进入战斗场景
- [x] 验证实体渲染正常 (位置、大小、纹理)
- [x] 验证视锥剔除正常 (屏幕外实体不渲染)
- [x] 验证状态效果视觉反馈正常

### Task 5.5: 性能验收
- [x] 使用 RenderDoc 捕获帧
- [x] 验证 Indirect Draw 调用正确
- [x] 对比重构前后帧时间 (差异应 < 5%)

### Task 5.6: 提交与标记
- [x] Git Commit: `refactor(render): standardize MDIRenderer with GPUUtils and RenderConstants`
- [x] Git Tag: `render_refactor_T4_complete`

---

## 任务依赖图

```
T1, T2 Complete
        │
        ▼
    Task 1.* (常量迁移)
        │
        ▼
    Task 2.* (函数指针移除)
        │
        ▼
    Task 3.* (绑定替换)
        │
        ▼
    Task 4.* (GPU 调用替换)
        │
        ▼
    Task 5.* (验证)
```

---

## 快速参考: 替换映射表

| 原代码 | 新代码 |
|---|---|
| `glMemoryBarrier(GL_XXX_BIT \| ...)` | `GPUUtils::MemoryBarrier(Barrier::X \| Barrier::Y)` |
| `glDrawArraysIndirect(mode, ptr)` | `GPUUtils::DrawArraysIndirect(GL::TRIANGLES, offset)` |
| `glBindBuffer(target, id)` | `GPUUtils::BindBuffer(GL::XXX, id)` |
| `glActiveTexture(GL_TEXTURE0)` | `GPUUtils::ActiveTexture(TextureUnit::X)` |
| `BindBase(0)` | `BindBase(static_cast<uint32_t>(Binding::SSBO_ENTITY_DATA))` |
| `BindPreviousNoSync(1)` | `BindPreviousNoSync(static_cast<uint32_t>(Binding::SSBO_VISIBLE_ID))` |

---

*计划版本: 1.0*
*最后更新: 2026-01-26*