---
name: code-risk-analyzer
description: 深度分析代码库风险，识别性能瓶颈、UB、UAF、内存泄漏及逻辑陷阱。
---

# 代码风险分析师 (Code Risk Analyzer)

## 目标
识别代码库中的深层技术债务和安全隐患，提供高性能、生产级别的修复方案。

## 增强型工具集 (Smart Tree Powered)
- **🔍 深度搜索**: 使用 `search {keyword:'reinterpret_cast|const_cast|malloc|free|new |delete ', case_sensitive:true}` 扫描危险代码模式。
- **📊 依赖分析**: 使用 `analyze {mode:'semantic', path:'src'}` 理解模块间的耦合，识别循环依赖风险。
- **⚡ 热点定位**: 使用 `find {type:'code', pattern:'*System.cpp'}` 快速定位每一帧都在运行的核心系统逻辑。
- **🧠 记忆检索**: 使用 `memory {operation:'find', keywords:['bug', 'performance', 'uaf']}` 检索过往的项目痛点。

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

## 执行流程

1.  **探索与发现**: 
    - 使用 `search` 和 `analyze` 工具扫描代码库。
    - 运行 `powershell -File .gemini/skills/code-risk-analyzer/scripts/deep_scan.ps1` 进行静态分析。
2.  **记录问题**: 将发现的每一个风险点记录下来。
3.  **生成报告**: 
    - 使用 Python 脚本生成标准化报告文件。
    - 报告位置: `conductor/analyzer/YYYY-MM-DD_HH-MM-SS_code_analyze.md`
    - 指令: `python .agent/skills/code-risk-analyzer/scripts/generate_report.py "报告的具体Markdown内容"`

## 报告格式内容

报告必须包含以下 Markdown 结构：

```markdown
# Code Risk Analysis Report
Date: YYYY-MM-DD HH:MM:SS

## Summary
Brief overview of the codebase health and major risks found.

## Critical Risks (High Priority)
### 1. [Risk Type] in `filename:line`
- **Description**: Why is this dangerous?
- **Consequence**: Crash / Corruption / Lag.
- **Recommendation**: Code snippet or specific refactoring advice.

## Performance Bottlenecks
...

## Refactoring Suggestions
...
```