---
name: code-risk-analyzer
description: 深度分析代码库风险，识别性能瓶颈、UB、UAF、内存泄漏及逻辑陷阱。
---

# 代码风险分析师 (Code Risk Analyzer)

## 目标
识别深层技术债务和安全隐患，生成标准化风险报告。

## 分析标准 (Standards)
- **内存安全**: 识别违反 `conductor/code_standard.md` 1.1 节的行为（特别是 EnTT 指针失效）。
- **性能瓶颈**: 识别主循环分配、冗余计算和缓存不友好的数据结构。
- **并发风险**: 检查 `Taskflow` 中的竞态条件和死锁。

## 执行流程

1.  **自动化扫描**:
    - `powershell -File .gemini/skills/code-risk-analyzer/scripts/deep_scan.ps1`。
    - 使用 `analyze {mode:'semantic'}` 识别模块耦合风险。
2.  **生成报告**:
    - 位置: `conductor/analyzer/YYYY-MM-DD_HH-MM-SS_code_analyze.md`。
    - 指令: `python .agent/skills/code-risk-analyzer/scripts/generate_report.py "报告内容"`。

## 报告模板
必须包含：`Critical Risks`, `Performance Bottlenecks`, `Refactoring Suggestions`。
详细格式请参考 `conductor/analyzer/` 下的历史文件。
