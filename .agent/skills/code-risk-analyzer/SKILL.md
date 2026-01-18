---
name: code-risk-analyzer
description: 深度分析代码库风险，识别性能瓶颈、UB、UAF、内存泄漏及逻辑陷阱。
---

# 代码风险分析师 (Code Risk Analyzer)

## 目标
识别深层技术债务和安全隐患，生成标准化风险报告。

## 分析标准 (Smart Tree Powered)
- **结构基准**: `overview {mode:'quick'}` 建立当前代码库的基准视图。
- **内存安全**: 使用 `search {keyword:'reinterpret_cast|delete ', include_content:true}` 与 `analyze {mode:'semantic'}` 识别违反 `conductor/code_standard.md` 1.1 节的行为（特别是 EnTT 指针失效）。
- **性能瓶颈**: 使用 `analyze {mode:'statistics'}` 识别主循环分配、冗余计算和缓存不友好的数据结构。
- **并发风险**: 检查 `Taskflow` 中的竞态条件，利用 `search` 扫描多线程资源竞争点。

## 执行流程

1.  **全量扫描 (Deep Scan)**:
    - **结构概览**: `overview {mode:'project'}`。
    - **深度语义**: `analyze {mode:'semantic', path:'src/'}`。
    - **静态脚本**: `powershell -File .agent/skills/code-risk-analyzer/scripts/deep_scan.ps1`。
2.  **生成报告 (Reporting)**:
    - 位置: `conductor/analyzer/YYYY-MM-DD_HH-MM-SS_code_analyze.md`。
    - 工具: `python .agent/skills/code-risk-analyzer/scripts/generate_report.py "报告内容"`。
3.  **记忆锚定 (Memory)**: 
    - 使用 `memory {operation:'anchor', anchor_type:'pattern', keywords:['risk_pattern', 'bottleneck']}` 记录发现的高危代码模式，供未来自动识别。

## 报告内容要求
必须包含：`Critical Risks`, `Performance Bottlenecks`, `Refactoring Suggestions`。
详细格式请参考 `conductor/analyzer/` 下的历史文件。