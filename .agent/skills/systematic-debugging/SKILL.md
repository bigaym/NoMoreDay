---
name: systematic-debugging
description: 专门针对 NoMoreDay (C++/EnTT) 架构的系统化调试框架。在处理任何 Bug、崩溃或非预期行为前使用。遵循“根因优先”原则，包含四阶段：根因调查、模式分析、假设测试、最终实现。
---

# Systematic Debugging (NoMoreDay Edition)

## 0. 必须遵守的协议 (Mandatory Protocol)

你必须严格按照以下顺序执行，**不可跳过任何步骤**：

1.  **代码感知初始化 (MANDATORY)**: 在开始调查前，必须调用 `set_project_directory` 工具初始化项目路径（**使用当前项目的根目录**）。这允许你使用 `cpp-analyzer` 像在 IDE 中一样快速定位符号和理解代码结构。
2.  **调查阶段 (Investigation)**: 使用只读工具 (read/search/cpp-analyzer) 和测试运行工具分析问题。
    - 使用 `get_function_signature` 确认参数类型。
    - 使用 `find_callers` 追踪崩溃函数的调用源。
3.  **报告阶段 (Reporting)**: 向用户输出一份包含根因、证据和修复计划的详细报告。
4.  **授权阶段 (Authorization)**: **显式停止**并询问用户：“是否授权执行此修复计划？”
5.  **执行阶段 (Execution)**: 只有在获得用户肯定回答后，才可调用 `replace`/`write` 修改业务代码。

---

## 1. 根因调查 (Root Cause Investigation)

在动手修改代码前，必须回答：**它是如何、在何处、为什么崩溃/失败的？**

### 1.1 错误信息深度解析
- **EnTT 异常**: 检查是否在 `each()` 循环中删除了实体，或在持有组件引用时触发了 `emplace`/`erase`。
- **内存错误**: 使用 `search_file_content` 检查嫌疑组件是否为 POD。是否存在跨线程访问未保护的组件。
- **渲染故障**: 检查 SSBO 绑定点或 Compute Shader 的局部工作组大小 (Local Workgroup Size) 是否与 C++ 端定义的常量对齐。

### 1.2 证据搜集 (Read-Only)
- **Git 回溯**: `git diff HEAD --name-only` 查看最近变动。
- **日志分析**: 阅读 `logs/` 目录下的最新日志。
- **调用链追踪**: 必须使用 `search_file_content` 或相关工具追踪调用链。

## 2. 模式分析 (Pattern Analysis)

### 2.1 寻找参照物
- 在 `src/` 中寻找实现相同逻辑的成功用例。
- 对比当前失败代码与成功用例在内存布局、组件组合上的差异。

### 2.2 验证假设
- 提出明确假设：“我认为 X 是根因，因为 Y。”
- **无损验证**: 通过添加日志 (`spdlog`) 或编写独立的单元测试来验证假设，**禁止直接修改业务逻辑**。

## 3. 假设测试与复现 (Testing & Reproduction)

### 3.1 强制复现测试 (MANDATORY)
- 在 `tests/unit` 或 `tests/integration` 下编写最简化的复现代码。
- 确保运行 `.\build.bat; build/bin/tests/NoMoreDayTests.exe` (或相应测试命令) 时该测试稳定失败。
- 如果无法编写测试，必须详细记录手动复现步骤及观测到的中间状态。

## 4. 报告与授权 (Report & Authorization)

**在此阶段，你必须输出如下格式的报告，并停止操作等待用户回复：**

```markdown
# 调试分析报告

## 1. 根因分析 (Root Cause)
（详细解释导致 Bug 的根本原因，引用具体代码行号）

## 2. 证据 (Evidence)
（日志片段、堆栈跟踪、测试失败结果或逻辑矛盾点）

## 3. 修复方案 (Proposed Solution)
（清晰描述将要进行的修改，包括将要修改的文件和逻辑）

## 4. 验证计划 (Verification Plan)
（如何证明 Bug 已被修复？例如：运行测试 X）
```

**结束语必须是：“请确认以上分析和计划是否准确。我是否可以开始执行修复？”**

## 5. 最终实现 (Implementation)

*(仅在获得授权后执行)*

### 5.1 根因修复
- 只解决报告中确定的根因。
- 严禁“顺手”进行无关的重构。
- 确保修复后，之前的复现测试通过。

### 5.2 架构反思
- 如果修复需要对核心架构（如 `EntityManager` 或 `RenderSystem`）进行大规模改动，或者已经尝试了 2 次以上的修复均告失败：
  **必须停止修复，与用户讨论架构设计是否存在根本性缺陷。**

## 集成工具
- `search_file_content`: 用于静态分析。
- `auditor`: 用于修复后的安全性二次审计。
- `performance-hardening`: 若 Bug 涉及性能抖动，需咨询此技能。