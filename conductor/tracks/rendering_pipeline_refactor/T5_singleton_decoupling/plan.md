# Track 5: Singleton Decoupling 实施计划 (V1.0)

> **Track ID**: `T5_singleton_decoupling`
> **依赖 Spec**: `spec.md` (V1.0)
> **依赖 Track**: T3, T4
> **预计工时**: 16h
> **风险等级**: 🔴 高

---

## 阶段总览

| 阶段 | 名称 | 核心产出 | 状态 |
|---|---|---|---|
| **Phase 1** | 基础设施 | 创建 RenderContext | 🟢 已完成 |
| **Phase 2** | 单例过渡 | 标记 Get() 废弃，添加实例注册 | 🟢 已完成 |
| **Phase 3** | 核心迁移 | 迁移 Game, GameplayState | 🟢 已完成 |
| **Phase 4** | 扩展迁移 | 迁移其他调用点 | 🟢 已完成 |
| **Phase 5** | 测试支持 | 验证非单例测试 (Integration) | 🟢 已完成 |
| **Phase 6** | 验证 | 编译、测试、视觉验收 | 🟢 已完成 |

---

## Phase 1: 基础设施 (Infrastructure)

### Task 1.1: 创建 RenderContext.hpp
- [x] 在 `src/engine/render/` 创建 `RenderContext.hpp`
- [x] 定义 `RenderContext` 结构体
- [x] 添加前向声明 (`GPUEntitySystem`, `MDIRenderer`, `ResourceManager`)
- [x] 实现 `IsValid()` 验证方法
- [x] 实现便捷访问器 (`GPU()`, `MDI()`, `Resources()`)

### Task 1.2: 编译验证
- [x] 确保 header 独立编译通过

---

## Phase 2: 单例过渡 (Singleton Transition)

### Task 2.1: 修改 GPUEntitySystem.hpp
- [x] 添加 `[[deprecated("Use RenderContext injection instead")]]` 到 `Get()`
- [x] 添加静态成员声明 `static GPUEntitySystem* s_instance`
- [x] 确保构造函数为 public (支持外部实例化)

### Task 2.2: 修改 GPUEntitySystem.cpp
- [x] 添加静态成员定义 `GPUEntitySystem* GPUEntitySystem::s_instance = nullptr`
- [x] 修改 `Get()` 实现: 返回 `s_instance` 或 fallback
- [x] 在 `Init()` 中注册实例: `s_instance = this`
- [x] 在 `Shutdown()` 中注销实例

### Task 2.3: 修改 MDIRenderer.hpp
- [x] 添加 `[[deprecated]]` 到 `Get()`
- [x] 添加静态成员声明

### Task 2.4: 修改 MDIRenderer.cpp
- [x] 添加静态成员定义
- [x] 修改 `Get()` 实现
- [x] 在 `Init()` / `Shutdown()` 中注册/注销

### Task 2.5: 编译验证
- [x] 编译项目，观察废弃警告数量
- [x] 记录所有警告位置 (用于后续迁移)

---

## Phase 3: 核心迁移 (Core Migration)

### Task 3.1: 修改 Game.hpp
- [x] 添加成员变量:
  ```cpp
  systems::GPUEntitySystem m_gpuEntitySystem;
  render::MDIRenderer m_mdiRenderer;
  RenderContext m_renderContext;
  ```
- [x] 添加 `#include "engine/render/RenderContext.hpp"`

### Task 3.2: 修改 Game.cpp - 初始化
- [x] 在 `InitializeRendering()` 中:
  - 调用 `m_gpuEntitySystem.Init(...)`
  - 调用 `m_mdiRenderer.Init(...)`
  - 构建 `m_renderContext`:
    ```cpp
    m_renderContext.gpuEntitySystem = &m_gpuEntitySystem;
    m_renderContext.mdiRenderer = &m_mdiRenderer;
    m_renderContext.resources = &m_resources;
    ```

### Task 3.3: 修改 Game.cpp - 清理
- [x] 在析构/Shutdown 中:
  - 调用 `m_gpuEntitySystem.Shutdown()`
  - 调用 `m_mdiRenderer.Shutdown()`

### Task 3.4: 修改 GameplayState.hpp
- [x] 添加 `RenderContext* m_renderCtx` 成员 (或引用)
- [x] 修改构造函数签名接收 `RenderContext&`

### Task 3.5: 修改 GameplayState.cpp
- [x] 构造函数存储 `RenderContext` 引用
- [x] 替换 `GPUEntitySystem::Get()` 调用:
  - `Update()` 中的调用
  - `Render()` 中的调用
- [x] 替换 `MDIRenderer::Get()` 调用 (如有)

### Task 3.6: 更新 Game.cpp - State 创建
- [x] 修改 `GameplayState` 的创建，传入 `m_renderContext`

### Task 3.7: 验证
- [x] 编译通过
- [x] 运行游戏，验证渲染正常

---

## Phase 4: 扩展迁移 (Extended Migration)

### Task 4.1: 搜索剩余调用点
- [x] 执行: `Select-String -Path 'src/**/*.cpp' -Pattern 'GPUEntitySystem::Get\(\)' -Recurse`
- [x] 执行: `Select-String -Path 'src/**/*.cpp' -Pattern 'MDIRenderer::Get\(\)' -Recurse`
- [x] 记录所有结果

### Task 4.2: 迁移 InventoryState (如需)
- [x] 添加 `RenderContext` 参数
- [x] 替换 `Get()` 调用

### Task 4.3: 迁移其他 State 类 (如需)
- [x] 逐个迁移

### Task 4.4: 迁移 UI 相关代码 (如需)
- [x] 审计 `UISystem`, `UIRenderer` 等
- [x] 按需迁移

### Task 4.5: 验证
- [x] 编译无新错误
- [x] 废弃警告数量显著减少

---

## Phase 5: 测试支持 (Testing Support)

### Task 5.1: 验证集成测试支持
- [x] 验证 `RenderingBenchmark.hpp` 使用依赖注入
- [x] 确认 `MDIRenderBenchmark` 可独立实例化 `GPUEntitySystem`
- [x] 验证 `RenderContext` 在测试环境中的构建

### Task 5.2: (Optional) 创建 Mock 类
- [ ] (跳过) 创建 MockGPUEntitySystem.hpp
- [ ] (跳过) 创建 MockMDIRenderer.hpp
- [ ] (跳过) 创建 MockRenderContext.hpp
- [ ] (跳过) 编写示例测试

*注: 由于项目主要依赖高性能集成测试，且 `MDIRenderBenchmark` 已成功演示了非单例模式的独立测试能力，因此跳过专门的 Mock 类创建。*

---

## Phase 6: 验证 (Verification)

### Task 6.1: 编译验证
- [x] 执行 `.\build.bat`
- [x] 确保无编译错误

### Task 6.2: 废弃警告审计
- [x] 记录剩余 `[[deprecated]]` 警告数量
- [x] 确保主要生产代码无警告

### Task 6.3: 单元测试
- [x] 运行 `.\build\bin\Release\NoMoreDayTests.exe`
- [x] 确保所有测试通过
- [x] 验证新增的 Mock 测试通过 (Integration Tests Passed)

### Task 6.4: 视觉验收
- [x] 运行游戏完整流程
- [x] 验证所有渲染功能正常

### Task 6.5: 文档更新
- [x] 更新 `GEMINI.md` 如需
- [x] 更新 `code_standard.md` 添加依赖注入约定

### Task 6.6: 提交与标记
- [x] Git Commit: `refactor(render): decouple singletons with RenderContext DI`
- [x] Git Tag: `render_refactor_T5_complete`
- [x] Git Tag: `render_pipeline_refactor_complete` (里程碑)

---

## 任务依赖图

```
T3, T4 Complete
        │
        ▼
    Task 1.* (RenderContext)
        │
        ▼
    Task 2.* (Singleton 过渡)
        │
        ├─────────────────────────────────┐
        ▼                                 ▼
    Task 3.* (Game/GameplayState)    Task 5.* (Mock 类)
        │                                 │
        ▼                                 │
    Task 4.* (扩展迁移) ◄─────────────────┘
        │
        ▼
    Task 6.* (验证)
```

---

## 迁移检查清单

### 已迁移文件
- [x] `Game.cpp`
- [x] `GameplayState.cpp`
- [x] `RenderSystem.cpp`
- [x] `MDIRenderBenchmark.hpp`
- [x] `RenderingBenchmark.hpp`

### 保留 Get() 调用的位置
- [ ] 测试代码 (可接受)
- [ ] (其他特殊情况)

---

## 回滚计划

如果迁移出现严重问题:

1. **Phase 2-3**: 移除 `[[deprecated]]` 属性，恢复原单例行为。
2. **Phase 4-5**: Git revert 到 Phase 3 Complete。
3. **紧急回滚**: `git checkout render_refactor_T4_complete`

---

## 后续工作 (V2.0)

完成 V1.0 后，在稳定运行一段时间后考虑:

1. **移除 `Get()` 方法**: 完全删除单例接口。
2. **编译时强制**: 使用 `= delete` 禁止单例访问。
3. **接口抽象**: 如需多态，引入 `IRenderSystem` 接口。

---

*计划版本: 1.0*
*最后更新: 2026-01-26*
