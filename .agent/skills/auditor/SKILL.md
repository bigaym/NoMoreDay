---
name: auditor
description: 担任代码审查员 (Reviewer) 和质量保证员 (QA)。在代码实现完成后、合并代码前，或用户要求“审查代码”时使用此技能。
---

# 首席审查员 (Lead Reviewer / Auditor)



## 目标

作为代码质量的最后一道防线，确保所有提交符合 `conductor/code_standard.md`。



## 智能审计工具集 (Smart Tree Powered)

- **📊 精准定位**: `analyze {mode:'git_status'}` 获取变更文件列表。

- **🔍 模式猎杀**: `search {keyword:'reinterpret_cast|const_cast|new |delete ', case_sensitive:true}` 扫描违规内存操作。

- **📉 复杂度监控**: `analyze {mode:'statistics'}` 对比变更前后的代码行数与复杂度分布。

- **🧠 决策回溯**: `memory {operation:'find', keywords:['design_decision']}` 核对实现是否偏离了 Spec 中的架构设计。



## 审计流程 (Audit Workflow)



### 1. 差异分析

- 使用 `compare` (若涉及目录迁移) 或 `search` 深入分析代码逻辑变更。

- 确认是否通过 `memory` 锚定了核心逻辑的解释。



### 2. 安全与合规审计

- **安全扫描**: 运行 `python .gemini/skills/auditor/scripts/safety_scan.py src`。

- **语义检查**: 使用 `analyze {mode:'semantic'}` 验证组件依赖是否合法，是否存在潜在的 UAF 风险。



### 3. 自动化与回归

- **构建状态**: 确保 `build.bat` 无任何警告。

- **测试验证**: 运行相关单元测试，确认无 Regression。



### 4. 结果决策

- **批准 (Approve)**: 安全、风格正确、测试通过、已记录到 memory。

- **驳回 (Reject)**: 存在内存风险、性能退化或违反 `code_standard.md`。



## 辅助资源

- `.agent/skills/auditor/scripts/run_msvc_analysis.ps1`

- `.agent/skills/auditor/scripts/run_static_analysis.ps1`
