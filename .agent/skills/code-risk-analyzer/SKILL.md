---
name: code-risk-analyzer
description: 深度分析代码库风险，识别性能瓶颈、UB、UAF、内存泄漏及逻辑陷阱。
---

# 代码风险分析师 (Code Risk Analyzer)

## 目标
识别代码库中的深层技术债务和安全隐患，提供高性能、生产级别的修复方案。

## 审计标准

### 1. 内存与安全 (Safety)
- **UB (Undefined Behavior)**: 
    - 检查解引用可能为 null 的 `try_get` 结果。
    - 检查数组越界或未初始化的变量使用。
- **UAF (Use-After-Free)**:
    - **EnTT 陷阱**: 检查在遍历视图过程中是否销毁实体或移除组件导致迭代器失效。
    - **Lambda 捕获**: 检查异步任务 (Taskflow) 中按引用捕获局部变量。
- **内存泄漏**:
    - 验证 RAII 覆盖率，识别遗留的 `new`/`delete`。
    - 检查循环引用（`shared_ptr`）。

### 2. 性能分析 (Performance)
- **分配热点**: 识别每帧循环（Main Loop）中的 `std::vector` 动态扩容或 `std::string` 拷贝。
- **缓存命中**: 检查组件布局是否符合 POD 要求，避免在 ECS 系统中使用虚函数。
- **冗余计算**: 识别未被烘焙（Bake）的静态数据查询或重复的标签解析。

### 3. 并发风险 (Threading)
- **竞态条件**: 检查 `registry` 在非线程安全上下文下的并发写入。
- **死锁**: 检查互斥锁的嵌套使用顺序。

## 执行指令
1.  **扫描危险模式**: 使用 `search` 查找 `reinterpret_cast`、`const_cast`、`raw pointer`、`static T var` 等。
2.  **数据流追踪**: 跟踪关键资源（如 `Texture`、`Shader`）的生命周期，确保在 `CloseWindow` 前释放。
3.  **ECS 架构核查**: 
    - 检查 `view<A, B>` 是否可以优化为 `group<A, B>`。
    - 验证 `registry.patch` 的使用是否正确触发了事件。

## 报告格式
对于发现的每个风险，按以下格式报告：
- **风险等级**: [Critical/High/Medium/Low]
- **位置**: `file:line`
- **问题描述**: 详细解释风险来源（如：为何会导致 UB）。
- **解决方案**: 提供符合 C++20 标准的重构建议。
