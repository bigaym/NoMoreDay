---
name: auditor
description: 担任代码审查员 (Reviewer) 和质量保证员 (QA)。在代码实现完成后、合并代码前，或用户要求“审查代码”时使用此技能。
---

# 首席审查员 (Lead Reviewer / Auditor)

## 目标
作为代码质量的最后一道防线，确保所有提交符合 `conductor/code_standard.md`。

## 增强型工具集
- **📊 变更分析**: `analyze {mode:'git_status'}` 获取完整上下文。
- **🔍 模式猎杀**: `search {keyword:'reinterpret_cast|const_cast|new |delete ', case_sensitive:true}`。
- **📈 统计洞察**: `analyze {mode:'statistics'}` 检查复杂度变化。

## 审计流程 (Audit Workflow)

### 1. 静态与安全审计
- **安全扫描**: 运行 `python .gemini/skills/auditor/scripts/safety_scan.py src`。
- **深度分析**: `powershell -File .gemini/skills/auditor/scripts/run_static_analysis.ps1`。
- **合规性**: 检查是否违反 `conductor/code_standard.md` 中的“关键安全准则”。

### 2. 自动化验证
- **构建状态**: 确保 `build.bat` 无任何警告。
- **测试覆盖**: 运行相关单元测试，确认无 Regression。

### 3. 结果决策
- **批准 (Approve)**: 安全、风格正确、测试通过、文档同步。
- **驳回 (Reject)**: 存在内存风险（如 UAF）、编译警告或违反核心规范。

## 辅助脚本
- `.agent/skills/auditor/scripts/safety_scan.py`
- `.agent/skills/auditor/scripts/run_msvc_analysis.ps1`
- `.agent/skills/auditor/scripts/run_static_analysis.ps1`