---
name: bug-fixer
description: 定位、分析并修复 NoMoreDay 项目中的 Bug。当用户报告 Bug、崩溃、逻辑错误或提供错误日志时使用。
---

# Bug 修复专家 (Bug Fixer)

## 核心工作流

### 1. 诊断与定位
- **日志分析**: 运行 `python .gemini/skills/bug-fixer/scripts/analyze_logs.py`。
- **根因分析**: 区分表面现象与核心缺陷（如架构问题导致的 UAF）。
- **参考**: 使用 `smart-tree` 的 `search` 功能追踪变量流转。

### 2. 方案规划 (遵循 Code Standard)
- **合规性**: 所有修复必须符合 `conductor/code_standard.md`。
- **测试先行**: 在 `tests/` 中编写复现用例。

### 3. 实现与验证
- **安全检查**: 修复完成后，必须对照 `conductor/code_standard.md` 第 8 节进行自查。
- **编译**: 运行 `.\build.bat`。
- **验证**: 运行 `build/bin/tests/tests_runner.exe`。

## 相关资源
- **高级调试**: [advanced_debugging.md](references/advanced_debugging.md) (ASan, Cppcheck)
- **代码规范**: `conductor/code_standard.md` (唯一事实来源)
