---
name: systematic-debugging
description: 专门针对 NoMoreDay (C++/EnTT) 架构的系统化调试框架。在处理任何 Bug、崩溃或非预期行为前使用。遵循“根因优先”原则，包含四阶段：根因调查、模式分析、假设测试、最终实现。
---

# Systematic Debugging (NoMoreDay Edition)

## 核心法则 (The Iron Law)
> **严禁在未确定根因的情况下尝试修复。任何基于猜想的补丁都是对项目质量的破坏。**

## 1. 根因调查 (Root Cause Investigation)

在动手修改代码前，必须回答：**它是如何、在何处、为什么崩溃/失败的？**

### 1.1 错误信息深度解析
- **EnTT 异常**: 检查是否在 `each()` 循环中删除了实体，或在持有组件引用时触发了 `emplace`/`erase`。
- **内存错误**: 使用 `search_file_content` 检查嫌疑组件是否为 POD。是否存在跨线程访问未保护的组件。
- **渲染故障**: 检查 SSBO 绑定点或 Compute Shader 的局部工作组大小 (Local Workgroup Size) 是否与 C++ 端定义的常量对齐。

### 1.2 证据搜集
- **Git 回溯**: `git diff HEAD --name-only` 查看最近变动。
- **日志分析**: 调用 `python .agent/skills/bug-fixer/scripts/analyze_logs.py`。
- **调用链追踪**: 必须使用 `cpp-analyzer` 的 `find_callers` 和 `find_callees`。

## 2. 模式分析 (Pattern Analysis)

### 2.1 寻找参照物
- 在 `src/` 中寻找实现相同逻辑的成功用例。
- 对比当前失败代码与成功用例在内存布局、组件组合上的差异。

### 2.2 验证假设
- 提出明确假设：“我认为 X 是根因，因为 Y。”
- **最小化改动**: 仅修改一处变量来验证假设。

## 3. 假设测试与复现 (Testing & Reproduction)

### 3.1 强制复现测试 (MANDATORY)
- 在 `tests/unit` 或 `tests/integration` 下编写最简化的复现代码。
- 确保运行 `.\build.bat && build/bin/tests/tests_runner.exe` 时该测试稳定失败。
- 如果无法编写测试，必须详细记录手动复现步骤及观测到的中间状态（通过 `spdlog`）。

## 4. 最终实现 (Implementation)

### 4.1 根因修复
- 只解决 Phase 1 确定的根因。
- 严禁“顺手”进行无关的重构。
- 确保修复后，之前的复现测试通过。

### 4.2 架构反思
- 如果修复需要对核心架构（如 `EntityManager` 或 `RenderSystem`）进行大规模改动，或者已经尝试了 2 次以上的修复均告失败：
  **必须停止修复，与用户讨论架构设计是否存在根本性缺陷。**

## 集成工具
- `cpp-analyzer`: 用于符号追踪。
- `auditor`: 用于修复后的安全性二次审计。
- `performance-hardening`: 若 Bug 涉及性能抖动，需咨询此技能。
