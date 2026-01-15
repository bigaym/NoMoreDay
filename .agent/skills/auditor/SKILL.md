---
name: auditor
description: 执行最终代码审查和技术审计。在实现完成后使用此技能，在提交前验证安全性、风格和文档。
---

# 项目审计员 (Project Auditor)

## 目标
作为最终质量关卡，确保代码严格符合内存安全、高性能且文档完善的 C++ 标准。

## 指令
1.  **智能审查与扫描**:
    -   **变更检测**: 使用 `analyze {mode:'git_status'}` 识别所有已修改、已暂存和未跟踪的文件。
    -   **安全扫描**: 对源代码运行自动化安全扫描器：
        `python .agent/skills/auditor/scripts/safety_scan.py src`
    -   如果发现任何违规（如原始 `new`、`printf` 等），立即驳回代码。

2.  **手动代码审计**:
    -   审查 `git diff` 以发现扫描器无法捕获的逻辑错误：
        -   **UAF**: EnTT 视图中是否存在使用后释放？
        -   **并发**: Taskflow 任务中是否存在竞态条件？
        -   **命名**: 类型使用 `PascalCase`，常量使用 `kPascalCase`？

3.  **构建验证**:
    -   运行 `build.bat` 并确保 0 警告。
    -   运行 `build/bin/tests/` 中相关的测试。

4.  **文档与提交**:
    -   更新 `conductor/tracks/` 下的文件。
    -   生成规范的提交信息（Conventional Commit，如 `feat:`, `fix:`, `refactor:`）。

## 约束
-   对内存安全违规零容忍。
-   构建必须洁净（无警告）。
-   提交前必须同步文档。
