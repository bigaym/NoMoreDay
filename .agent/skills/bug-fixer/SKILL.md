---
name: bug-fixer
description: 定位、分析并修复 NoMoreDay 项目中的 Bug。当用户报告 Bug、崩溃、逻辑错误或提供错误日志时使用。
---

# Bug 修复专家 (Bug Fixer)

## 核心工作流 (Smart Tree Powered)

### 1. 诊断与溯源
- **上下文调取**: `memory {operation:'find', keywords:['bug', 'crash', 'error']}` 寻找相似记录。
- **日志与变量追踪**: 
  - 运行 `python .gemini/skills/bug-fixer/scripts/analyze_logs.py`。
  - 使用 `search {keyword:'...', context_lines:5}` 追踪故障变量的读写流转。
- **语义审计**: 使用 `analyze {mode:'semantic'}` 检查导致崩溃的类是否存在非线程安全操作或组件生命周期管理错误（如 UAF）。

### 2. 方案规划 (遵循 Code Standard)
- **合规性检查**: 所有修复必须符合 `conductor/code_standard.md`。
- **复现测试**: 使用 `find {type:'tests'}` 寻找模板，并在 `tests/` 中编写复现用例。

### 3. 实现与验证
- **精准修复**: 使用 `edit {operation:'smart_edit'}` 针对性修改故障函数。
- **回归测试**: 
  - 编译: 运行 `.\build.bat`。
  - 验证: 运行 `build/bin/tests/tests_runner.exe`。
- **知识沉淀**: 修复完成后，必须使用 `memory {operation:'anchor', anchor_type:'pattern'}` 记录该 Bug 的根因模式，防止在其他模块复现。

## 相关工具
- **全景搜索**: `search {keyword:'TODO|FIXME', include_content:true}`。
- **依赖分析**: `analyze {mode:'directory', path:'src/systems'}`。
- **高级调试**: [advanced_debugging.md](references/advanced_debugging.md) (ASan, Cppcheck)
