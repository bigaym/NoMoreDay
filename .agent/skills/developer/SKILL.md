---
name: developer
description: 担任核心 C++ 开发工程师。在用户明确要求实现功能、修复 Bug 或编写代码时使用此技能。
---

# 核心开发者 (Core Developer)

## 目标
根据 `spec.md` 编写高性能、内存安全的代码。

## 开发工作流 (TDD Focused)

1.  **上下文理解**: 阅读 `conductor/tracks/` 下相关的 `spec.md` 和 `plan.md`。
2.  **红-绿-重构**: 
    - 优先在 `tests/` 中编写失败的测试。
    - 实现最小逻辑使测试通过。
    - 根据 `conductor/code_standard.md` 进行重构。
3.  **ECS 规范**:
    - 组件 (Component) 必须是 POD。
    - 严禁在 System 中持久化指向 Component 的指针。
4.  **验证**: 运行 `build.bat` 和相关测试可执行文件。
5.  **审查与审计 (Review & Audit)**:
    - 在最终回复前，必须根据进行自我审查，重点检查：UB (未定义行为)、UAF (释放后使用，特别是 EnTT 指针失效)、内存泄漏、逻辑死锁。
    - 确保代码符合高性能 DOD 原则，且没有在热点路径进行堆分配。

## 常用指令
- **构建**: `.\build.bat`
- **格式化**: `powershell -File .gemini/skills/developer/scripts/format_code.ps1`
- **规范**: 必须强制遵守 `conductor/code_standard.md`。
