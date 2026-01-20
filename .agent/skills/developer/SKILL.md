---
name: developer
description: 担任核心 C++ 开发工程师。在用户明确要求实现功能、修复 Bug 或编写代码时使用此技能。
---

# 核心开发者 (Core Developer)

## 目标
根据 `spec.md` 编写高性能、内存安全的代码，严格遵守项目规范。

## 开发工作流 (Native Tools & C++ Analyzer)

### 1. 环境准备与上下文 (Context)
- **同步状态**: 查阅 `GEMINI.md` 或使用 `save_memory` 记录当前任务上下文。
- **全景概览**: 使用 `list_directory` 和 `glob` 确认项目结构。
- **深入理解**: 
  - 阅读 `conductor/tracks/` 下相关的 `spec.md` 和 `plan.md`。
  - **结构透视**: 在修改前，使用 `cpp-analyzer` 工具：
    - `search_classes {pattern: 'ClassName'}`: 查找类定义。
    - `get_function_signature {function_name: 'FuncName'}`: 获取函数签名。
    - `find_callers {function_name: 'FuncName'}`: 理解调用关系。

### 2. 红-绿-重构 (TDD Focused)
- **编写测试**: 在 `tests/` 中编写失败的测试（使用 `glob {pattern: 'tests/**/*Test.cpp'}` 定位参考模板）。
- **精准实现**: 
  - **核心原则**: 严禁全文件重写。
  - **工具使用**: 
    - 使用 `replace` 修改现有逻辑。必须提供足够的上下文行（3行以上）。
    - 使用 `write_file` 创建新文件。
  - **优势**: 保持修改的原子性和精准度。
- **规范检查**: 遵循 `conductor/code_standard.md`（POD Component, RAII）。

### 3. ECS 与性能规范 (Performance)
- **数据导向**: 组件 (Component) 必须是 POD。
- **生命周期**: 严禁在 System 中持久化指向 Component 的指针。
- **缓存友好**: 避免在热点路径（Update/Render）进行堆分配。

### 4. 固化与审计 (Finalize)
- **验证**: 运行 `.\build.bat` 和相关测试。
- **锚定记忆**: 实现完成后，使用 `save_memory {fact: 'Solution Implemented: ...'}` 记录核心逻辑变更，方便 `auditor` 审查。
- **自查**: 检查 UB、UAF、内存泄漏。

## 常用指令
- **构建**: `run_shell_command {command: '.\build.bat'}`
- **定位**: `glob {pattern: 'src/**/*.hpp'}`
- **搜索**: `search_file_content {pattern: '...'}`
- **分析**: 使用 `cpp-analyzer` 工具集 (`get_class_hierarchy`, `search_symbols` 等)。
