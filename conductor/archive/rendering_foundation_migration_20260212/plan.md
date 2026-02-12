# Implementation Plan: Rendering Foundation Migration (Phase 0)

## 任务概览

| Task ID | 描述 | 优先级 | 估计工时 | 依赖 |
| :--- | :--- | :--- | :--- | :--- |
| **1. 基建** | **目录与基础类实现** | | | |
| 1.1 | 创建目录结构与 CMakeLists.txt 更新 | 高 | 0.5h | - |
| 1.2 | 实现 `QualityTierManager` 及 GPU 探测逻辑 | 高 | 1.5h | - |
| 1.3 | 实现 `TransientResourcePool` (基础 FBO 管理) | 中 | 2h | - |
| **2. 核心** | **RenderGraph 架构实现** | | | |
| 2.1 | 实现 `RenderGraph`, `RenderPass` 接口与 `RenderContext` | 高 | 2h | 1.1 |
| 2.2 | 在 `RenderSystem` 中集成 Graph 实例 | 高 | 1h | 2.1 |
| **3. 重构** | **Pass 拆解与迁移** | | | |
| 3.1 | 迁移 Scene 渲染逻辑到 `ScenePass` | 高 | 2h | 2.1 |
| 3.2 | 迁移 Particle/VFX 到 `VFXPass` | 高 | 2h | 3.1 |
| 3.3 | 迁移 UI/World HUD 到 `UIWorldPass` | 中 | 1.5h | 3.2 |
| 3.4 | 实现 `CompositePass` 进行最终合成 | 高 | 1h | 3.3 |
| 3.5 | 清理 `RenderSystem` 冗余代码 | 高 | 1h | 3.4 |
| **4. 验证** | **回归与基准测试** | | | |
| 4.1 | 编写 `ScopedGLState` 解决 Raylib 状态冲突 | 高 | 1h | 3.5 |
| 4.2 | 执行渲染对比与性能基准测试 | 高 | 2h | 4.1 |

## 详细任务定义

### Task 1.1: 目录结构
- 在 `src/engine/render/` 下创建 `graph`, `resources`, `core`, `passes` 子目录。
- 确保所有新文件被包含在 `CMakeLists.txt` 的 `NoMoreDayCore` 目标中。

### Task 1.2: QualityTierManager
- 检查 `GL_RENDERER` 字符串（如 "RTX", "Iris", "Radeon"）。
- 定义 `QualityTier` 枚举。
- 在 `RenderSystem::init` 中初始化。

### Task 3.1 - 3.4: Pass 拆解
- **原则**: 每次迁移一个模块，立即编译运行验证。
- 初始阶段，所有 Pass 共享默认 Framebuffer，直到 Phase 1 引入 HDR。

## 状态恢复
- 如遇中断，检查 `RenderSystem::render()` 中尚未被注释/移除的旧代码块。
