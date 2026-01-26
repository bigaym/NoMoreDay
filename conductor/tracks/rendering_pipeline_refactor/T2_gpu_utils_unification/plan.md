# Track 2: GPU Utils Unification 实施计划 (V1.0)

> **Track ID**: `T2_gpu_utils_unification`
> **依赖 Spec**: `spec.md` (V1.0)
> **依赖 Track**: T1 (`render_constants`)
> **预计工时**: 8h

---

## 阶段总览

| 阶段 | 名称 | 核心产出 | 状态 |
|---|---|---|---|
| **Phase 1** | 接口定义 | 扩展 `GPUUtils.hpp` | 🔵 待开始 |
| **Phase 2** | 实现分离 | 创建 `GPUUtils.cpp` | 🔵 待开始 |
| **Phase 3** | 依赖重构 | 更新 `MDIRenderer`, `ComputeBuffer` 等 | 🔵 待开始 |
| **Phase 4** | 初始化集成 | 在 `Game.cpp` 添加初始化调用 | 🔵 待开始 |
| **Phase 5** | 验证 | 测试与清理 | 🔵 待开始 |

---

## Phase 1: 接口定义 (Interface Definition)

### Task 1.1: 扩展 GPUUtils.hpp
- [ ] 添加 `#include "engine/render/RenderConstants.hpp"`
- [ ] 添加静态成员 `s_initialized` 和函数指针缓存声明
- [ ] 添加 `Initialize()` 函数签名
- [ ] 添加 `IsInitialized()` 函数签名
- [ ] 扩展 `MemoryBarrier()` 支持 `Barrier` 枚举
- [ ] 添加 `DispatchCompute()` / `DispatchComputeNoBarrier()`
- [ ] 添加 `BindBuffer()` / `BindBufferBase()` (支持 `Binding` 枚举)
- [ ] 添加 `DrawArraysIndirect()`
- [ ] 添加 `ActiveTexture()` (支持 `TextureUnit` 枚举)
- [ ] 添加 `BindTexture()`
- [ ] 保留现有 `BindImageTexture()` 并标准化

**验证**: Header-only 编译检查通过。

### Task 1.2: 更新 GPUSupportInfo
- [ ] 添加 `indirectDrawSupported` 字段
- [ ] 添加 `persistentMappingSupported` 字段

---

## Phase 2: 实现分离 (Implementation Separation)

### Task 2.1: 创建 GPUUtils.cpp
- [ ] 在 `src/engine/render/` 创建 `GPUUtils.cpp`
- [ ] 定义所有静态成员变量
- [ ] 实现 `Initialize()`: 一次性加载所有函数指针
- [ ] 实现 `IsInitialized()`
- [ ] 实现 `MemoryBarrier()` (两个重载)
- [ ] 实现 `DispatchCompute()` / `DispatchComputeNoBarrier()`

### Task 2.2: 实现 Buffer 操作
- [ ] 实现 `BindBuffer()`
- [ ] 实现 `BindBufferBase()` (两个重载)
- [ ] 实现 Fallback 路径 (使用 `rlBindShaderBuffer`)

### Task 2.3: 实现 Draw 操作
- [ ] 实现 `DrawArraysIndirect()`
- [ ] 添加错误处理和日志

### Task 2.4: 实现 Texture 操作
- [ ] 实现 `ActiveTexture()` (两个重载)
- [ ] 实现 `BindTexture()`
- [ ] 迁移 `BindImageTexture()` 实现到 `.cpp`

### Task 2.5: 更新 CMakeLists.txt
- [ ] 将 `GPUUtils.cpp` 添加到 engine 库的源文件列表

**验证**: 编译 engine 库通过。

---

## Phase 3: 依赖重构 (Dependency Refactor)

### Task 3.1: 重构 MDIRenderer.cpp
- [ ] 移除所有本地 `typedef` 函数指针定义 (Line 32-42)
- [ ] 移除本地 `glfwGetProcAddress` 调用
- [ ] 在 `Init()` 中: 移除 `glDrawArraysIndirect`, `glBindBuffer`, `glActiveTexture`, `glMemoryBarrier` 加载
- [ ] 替换 `glMemoryBarrier(...)` → `GPUUtils::MemoryBarrier(Barrier::...)`
- [ ] 替换 `glDrawArraysIndirect(...)` → `GPUUtils::DrawArraysIndirect(...)`
- [ ] 替换 `glBindBuffer(...)` → `GPUUtils::BindBuffer(...)`
- [ ] 替换 `glActiveTexture(...)` → `GPUUtils::ActiveTexture(...)`

**验证**: `Select-String -Path 'MDIRenderer.cpp' -Pattern 'glfwGetProcAddress'` 返回空。

### Task 3.2: 重构 ComputeBuffer.hpp
- [ ] 在 `Bind()` 方法中移除手动 `glBindBuffer` 加载
- [ ] 调用 `GPUUtils::BindBuffer(target, m_id)`
- [ ] 在 `OrphanAndUpload()` 中考虑是否需要类似重构 (可选, 保留原逻辑以保证性能)

### Task 3.3: 重构 PersistentBuffer.cpp
- [ ] 检查是否有手动 GL 函数加载
- [ ] 替换为 `GPUUtils` 调用

### Task 3.4: 重构 GPUParticleSystem.cpp (如适用)
- [ ] 审计是否有手动 GL 调用
- [ ] 替换为 `GPUUtils` 调用

### Task 3.5: 重构 GPUSkillEffectSystem.cpp (如适用)
- [ ] 审计是否有手动 GL 调用
- [ ] 替换为 `GPUUtils` 调用

---

## Phase 4: 初始化集成 (Initialization Integration)

### Task 4.1: 修改 Game.cpp
- [ ] 在 `InitializeRendering()` 或窗口创建后添加:
  ```cpp
  auto gpuInfo = NoMoreDay::utils::GPUUtils::Initialize();
  LOG_INFO("GPU: OpenGL {}.{}, Compute: {}, IndirectDraw: {}",
           gpuInfo.majorVersion, gpuInfo.minorVersion,
           gpuInfo.computeShaderSupported, gpuInfo.indirectDrawSupported);
  ```

### Task 4.2: 添加初始化检查 (可选)
- [ ] 在 `MDIRenderer::Init()` 开头添加:
  ```cpp
  if (!GPUUtils::IsInitialized()) {
      LOG_ERROR("GPUUtils must be initialized before MDIRenderer!");
      return;
  }
  ```

---

## Phase 5: 验证 (Verification)

### Task 5.1: 编译验证
- [ ] 执行 `.\build.bat`
- [ ] 确保无编译错误

### Task 5.2: 代码搜索验证
- [ ] 执行: `Select-String -Path 'src/**/*.cpp' -Pattern 'glfwGetProcAddress' -Recurse`
- [ ] 确认结果仅包含 `GPUUtils.cpp`

### Task 5.3: 运行时验证
- [ ] 运行游戏，检查启动日志中的 GPU 能力输出
- [ ] 确认无 "Failed to load", "not supported" 等警告 (除非硬件限制)

### Task 5.4: 单元测试
- [ ] 运行 `.\build\bin\Release\NoMoreDayTests.exe`
- [ ] 确保所有测试通过

### Task 5.5: 视觉验收
- [ ] 验证实体渲染、粒子效果、技能特效正常
- [ ] 使用 RenderDoc 验证 GL 调用正确

### Task 5.6: 提交与标记
- [ ] Git Commit: `refactor(render): unify GL extension loading in GPUUtils`
- [ ] Git Tag: `render_refactor_T2_complete`

---

## 任务依赖图

```
T1 Complete
    │
    ▼
Task 1.1 ─── Task 1.2
    │
    ▼
Task 2.1 ─── Task 2.2 ─── Task 2.3 ─── Task 2.4 ─── Task 2.5
    │
    ├────────────────────────────────────┐
    ▼                                    ▼
Task 3.1 (MDIRenderer)              Task 3.2 (ComputeBuffer)
    │                                    │
    ├──────────── Task 3.3 ─── Task 3.4 ─┴── Task 3.5
    │
    ▼
Task 4.1 ─── Task 4.2
    │
    ▼
Task 5.* (验证)
```

---

*计划版本: 1.0*
*最后更新: 2026-01-26*
