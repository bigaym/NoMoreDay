---
name: bug-fixer
description: 定位、分析并修复 NoMoreDay 项目中的 Bug。当用户报告 Bug、崩溃、逻辑错误或提供错误日志时使用。
---

# Bug 修复专家 (Bug Fixer)

## 核心工作流 (Native Tools & C++ Analyzer)

### 1. 诊断与溯源 (Diagnosis)
- **全景扫视**: `list_directory` 确认环境状态。
- **上下文调取**: 查阅 `GEMINI.md` 寻找相似记录或已知问题。
- **锁定嫌疑人**: 
  - 使用 `run_shell_command {command: 'git diff HEAD --name-only'}` (或 `git log`) 找出最近修改的代码。
  - 如果存在日志脚本，运行 `python .agent/skills/bug-fixer/scripts/analyze_logs.py`。
- **数据流追踪**: 
  - 使用 `search_file_content {pattern: '<VariableOrFunction>', context: 5}` 追踪故障变量。
  - 使用 `find_callers` 和 `find_callees` (cpp-analyzer) 理解调用链。

### 2. 方案规划 (Planning)
- **复现测试**: 
  - 使用 `glob {pattern: 'tests/**/*Test.cpp'}` 寻找测试模板。
  - 在 `tests/` 中编写复现用例，确保 Bug 可被稳定触发。
- **语义审计**: 使用 `get_class_info` 检查导致崩溃的类结构，确认是否存在非线程安全操作或组件生命周期管理错误。

### 3. 实现与验证 (Action)
- **精准修复**: 
  - **强制使用**: `replace` 修改代码，`write_file` 创建新文件。
  - **优势**: 避免全文件覆盖带来的风险。
- **回归测试**: 
  - 编译: `run_shell_command {command: '.\build.bat'}`。
  - 验证: `run_shell_command {command: 'build/bin/tests/tests_runner.exe'}`。

### 4. 知识沉淀 (Memory)
- **根因锚定**: 修复完成后，**必须**使用 `save_memory {fact: 'Bug Fixed: [Description], Root Cause: [Cause]'}` 记录该 Bug 的根因模式。

## 相关工具
- **全景搜索**: `search_file_content {pattern: 'TODO|FIXME'}`。
- **依赖分析**: `cpp-analyzer` 工具集。
- **高级调试**: [advanced_debugging.md](references/advanced_debugging.md) (ASan, Cppcheck)
