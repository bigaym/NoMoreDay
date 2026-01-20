---
name: code-risk-analyzer
description: 深度分析代码库风险，识别性能瓶颈、UB、UAF、内存泄漏及逻辑陷阱。
---

# 代码风险分析师 (Code Risk Analyzer)

## 目标
识别深层技术债务和安全隐患，生成标准化风险报告。

## 分析标准 (Native Tools & C++ Analyzer)
- **结构基准**: 使用 `list_directory` 建立当前代码库的基准视图。
- **内存安全**: 
  - 使用 `search_file_content {pattern: 'reinterpret_cast|delete '}` 识别高危操作。
  - 使用 `find_callers {function_name: 'destroy'}` (或其他 EnTT 相关销毁函数) 识别潜在的指针失效风险。
- **性能瓶颈**: 
  - 检查主循环 (`Game::Update`, `Game::Draw`) 中的分配。
  - 使用 `get_class_info` 检查组件大小（POD 检查）。
- **并发风险**: 检查 `Taskflow` 中的竞态条件，利用 `search_file_content` 扫描多线程资源竞争点。

## 执行流程

1.  **全量扫描 (Deep Scan)**:
    - **结构概览**: `list_directory` / `glob`.
    - **深度语义**: 使用 `cpp-analyzer` 工具 (`search_classes`, `get_function_signature`).
    - **静态脚本**: 如果存在，运行 `.agent/skills/code-risk-analyzer/scripts/deep_scan.ps1` (需先确认存在)。
2.  **生成报告 (Reporting)**:
    - 位置: `conductor/analyzer/YYYY-MM-DD_HH-MM-SS_code_analyze.md`。
    - 工具: 使用 `write_file` 写入报告。
3.  **记忆锚定 (Memory)**: 
    - 使用 `save_memory {fact: 'Risk Pattern Identified: ...'}` 记录发现的高危代码模式。

## 报告内容要求
必须包含：`Critical Risks`, `Performance Bottlenecks`, `Refactoring Suggestions`。
详细格式请参考 `conductor/analyzer/` 下的历史文件。
