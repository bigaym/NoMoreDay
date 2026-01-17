---
name: bug-fixer
description: 定位、分析并修复 NoMoreDay 项目中的 Bug。当用户报告 Bug、崩溃、逻辑错误或提供错误日志时使用。
---

# Bug 修复专家 (Bug Fixer)

此技能指导你完成 NoMoreDay 代码库的诊断和修复过程，确保严格遵守项目规范和安全性协议。

## 核心工作流

### 1. 分析与定位
*   **输入**: 分析用户报告、日志和堆栈跟踪（Stack Traces）。
*   **自动日志分析**: 运行 `python .gemini/skills/bug-fixer/scripts/analyze_logs.py` 快速提取错误。
*   **工具箱**:
    *   `search_file_content`: 定位错误字符串、函数名或逻辑模式。
    *   `smart-tree` (find/search): 进行高级模式匹配和文件变动分析。
    *   **更多工具**: 参阅 [advanced_debugging.md](references/advanced_debugging.md) 获取 ASan、静态分析等高级技巧。
*   **根因分析**: 明确失败的**根本原因**（例如：UAF、竞态条件、空指针解引用、逻辑漏洞）。

### 2. 方案规划
*   **规范检查**: 查阅 `conductor/code_standard.md` 以确保方案符合项目标准。
*   **策略制定**: 制定一个既能修复根因又不会引入副作用（Side Effects）的方案。
*   **测试规划**: 在 `tests/` 中寻找现有测试，或规划一个新的复现测试。

### 3. 代码实现
*   **修改**: 使用 `replace` 或 `edit_file` 应用修复方案。
*   **关键安全准则**:
    *   **EnTT**: 在执行任何会修改注册表的操作（创建/销毁/添加组件）时，**绝不能**持有组件指针（如 `auto* c = reg.get(...)`）。这是常见的 UAF 来源。
    *   **内存**: 必须使用 `std::unique_ptr` / `std::shared_ptr`。禁止使用原始 `new`/`delete`。
    *   **并发**: 避免使用静态可变变量。使用 `Taskflow` 处理并行任务。

### 4. 验证与交付
*   **编译**: 务必运行 `.\build.bat` 确保编译通过。
*   **测试**: 运行项目的统一测试运行器：
    *   **GCC**: `.\build\bin\tests_runner.exe`
    *   **MSVC**: `.\build\bin\Release\tests_runner.exe`
*   **日志验证**: 确认修复方案确实解决了报告的问题。

## 相关资源

*   **安全检查清单**: [debugging_checklist.md](references/debugging_checklist.md) - 修复完成前的强制核对项。
*   **高级调试指南**: [advanced_debugging.md](references/advanced_debugging.md) - 包含 ASan、Smart-Tree 和日志分析技巧。
*   **代码规范**: `conductor/code_standard.md` (Project root) - 风格与安全性的唯一真相来源。

## 最佳实践

*   **不要回滚**更改，除非用户明确要求或更改引入了新的 Bug。
*   **精简注释**: 注释应只解释“为什么”这么做，而不是“做了什么”。
*   **安全性优先**: 崩溃（Crash）比行为错误更严重，修复时必须优先考虑系统稳定性。