---
name: bug-fixer
description: 定位、分析并修复 NoMoreDay 项目中的 Bug。当用户报告 Bug、崩溃、逻辑错误或提供错误日志时使用。
---

# Bug 修复专家 (Bug Fixer)

## 核心工作流 (Smart Tree Powered)

### 1. 诊断与溯源 (Diagnosis)
- **全景扫视**: `overview {mode:'quick'}` 确认环境状态。
- **上下文调取**: `memory {operation:'find', keywords:['bug', 'crash', 'error']}` 寻找相似记录或已知问题。
- **锁定嫌疑人**: 
  - 使用 `find {type:'recent', days:2, pattern:'*.cpp'}` 快速找出最近修改的代码（Bug 高发区）。
  - 运行 `python .agent/skills/bug-fixer/scripts/analyze_logs.py`。
- **数据流追踪**: 使用 `search {keyword:'<VariableOrFunction>', context_lines:5, include_content:true}` 追踪故障变量的读写流转。

### 2. 方案规划 (Planning)
- **复现测试**: 
  - 使用 `find {type:'tests', pattern:'*Test.cpp'}` 寻找测试模板。
  - 在 `tests/` 中编写复现用例，确保 Bug 可被稳定触发。
- **语义审计**: 使用 `analyze {mode:'semantic'}` 检查导致崩溃的类是否存在非线程安全操作或组件生命周期管理错误（如 UAF）。

### 3. 实现与验证 (Action)
- **精准修复 (Smart Edit)**: 
  - **强制使用**: `edit {operation:'smart_edit', file_path:'...', edits:[...]}`。
  - **优势**: 相比全文件重写，AST 感知的编辑能节省 90% Token 并避免意外覆盖无关代码。
- **回归测试**: 
  - 编译: 运行 `.\build.bat`。
  - 验证: 运行 `build/bin/tests/tests_runner.exe`。

### 4. 知识沉淀 (Memory)
- **根因锚定**: 修复完成后，**必须**使用 `memory {operation:'anchor', anchor_type:'pattern', keywords:['bug_fix', 'root_cause']}` 记录该 Bug 的根因模式，防止在其他模块复现。

## 相关工具
- **全景搜索**: `search {keyword:'TODO|FIXME', include_content:true}`。
- **依赖分析**: `analyze {mode:'directory', path:'src/systems'}`。
- **高级调试**: [advanced_debugging.md](references/advanced_debugging.md) (ASan, Cppcheck)