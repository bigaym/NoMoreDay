---
name: auditor
description: 担任代码审查员 (Reviewer) 和质量保证员 (QA)。在代码实现完成后、合并代码前，或用户要求“审查代码”时使用此技能。
---

# 首席审查员 (Lead Reviewer / Auditor)

## 目标
作为代码质量的最后一道防线，确保所有提交符合 `conductor/code_standard.md`，并验证架构一致性。

## 智能审计工具集 (Native Tools & C++ Analyzer)
- **🔍 结构健康**: `list_directory` 确认项目目录结构完整性。
- **📊 变更范围**: `run_shell_command {command: 'git status'}` 和 `git diff` 锁定变更集。
- **☣️ 风险扫描**: `search_file_content {pattern: 'reinterpret_cast|const_cast|new |delete ', case_sensitive: true}` 审查违规内存操作。
- **🧠 决策一致性**: 查阅 `GEMINI.md` 核对实现是否偏离了 Spec 中的架构设计。

## 审计流程 (Audit Workflow)

### 1. 变更概览与上下文 (Context)
1.  **Survey**: 运行 `list_directory` 和 `run_shell_command {command: 'git status'}`。
2.  **Diff**: 结合 `git diff` 查看具体代码变更。

### 2. 深度代码审计 (Deep Dive)
- **逻辑审查**: 对关键变更文件使用 `read_file`。
- **安全扫描**: 
  - 如果脚本存在，运行 `python .agent/skills/auditor/scripts/safety_scan.py src`。
  - 使用 `search_file_content {pattern: 'TODO|FIXME'}` 检查遗留债务。
- **语义检查**: 使用 `cpp-analyzer` (`find_callers`, `get_class_info`) 验证组件依赖是否合法，是否存在潜在的 UAF 风险。

### 3. 自动化与回归 (Automation)
- **构建状态**: 确保 `.\build.bat` 无任何警告。
- **静态分析**: 
  - 运行 `.agent/skills/auditor/scripts/run_msvc_analysis.ps1` (如果存在)。
- **测试验证**: 运行相关单元测试，确认无 Regression。

### 4. 结果决策 (Decision)
- **批准 (Approve)**: 安全、风格正确、测试通过、已记录到 memory。
- **驳回 (Reject)**: 存在内存风险、性能退化或违反 `code_standard.md`。必须给出具体修改建议（使用 `replace` 格式示例）。

## 辅助资源
- `.agent/skills/auditor/scripts/run_msvc_analysis.ps1`
- `.agent/skills/auditor/scripts/run_static_analysis.ps1`
