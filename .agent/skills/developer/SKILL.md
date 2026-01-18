---
name: developer
description: 担任核心 C++ 开发工程师。在用户明确要求实现功能、修复 Bug 或编写代码时使用此技能。
---

# 核心开发者 (Core Developer)

## 目标
根据 `spec.md` 编写高性能、内存安全的代码。

## 开发工作流 (Smart Tree Powered)

### 1. 环境准备与上下文理解
- **同步状态**: `memory {operation:'find', keywords:['track-id', 'task-context']}` 获取当前任务进度。
- **全景概览**: `overview {mode:'quick'}` 确认项目结构。
- **深入理解**: 
  - 阅读 `conductor/tracks/` 下相关的 `spec.md` 和 `plan.md`。
  - 使用 `analyze {mode:'semantic'}` 理解待修改模块的类继承、组件依赖及热点路径。

### 2. 红-绿-重构 (TDD Focused)
- **编写测试**: 在 `tests/` 中编写失败的测试（使用 `find` 定位类似测试作为参考）。
- **精准实现**: 
  - 优先使用 `edit {operation:'smart_edit'}` 进行函数级修改，确保 90% 的 Token 节省。
  - 严禁全文件重写，除非是新建文件。
  - 遵循 `conductor/code_standard.md` 进行实现。
- **验证**: 运行 `.\build.bat` 和相关测试。

### 3. ECS 与性能规范
- **数据导向**: 组件 (Component) 必须是 POD。
- **生命周期**: 严禁在 System 中持久化指向 Component 的指针。
- **缓存友好**: 避免在热点路径（Update/Render）进行堆分配。

### 4. 固化与审计
- **锚定记忆**: 实现完成后，使用 `memory {operation:'anchor', anchor_type:'solution'}` 记录核心逻辑变更，方便 `auditor` 审查。
- **自查**: 检查 UB、UAF、内存泄漏。

## 常用指令
- **构建**: `.\build.bat`
- **定位**: `find {type:'code', pattern:'*.hpp'}`
- **搜索**: `search {keyword:'...', include_content:true}`
- **分析**: `analyze {mode:'statistics'}` 检查代码复杂度。
