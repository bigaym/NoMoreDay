# 模块拆分 MS-0 审查报告

## 首次审查

**审查目标：** Modular Static Target Split, MS-0 ownership ledger and boundary guard
**结论：** 修改
**审查轮次：** 首次审查

### 输入

- 设计：`docs/designs/modular-split-exe-lib-dll-design.md`
- 实施计划：`docs/plans/modular-split-exe-lib-dll-implementation-plan.md`
- 审查标准：`docs/workflows/review.md`
- 验证：`python scripts/check_module_boundaries.py`、`python -m unittest tests/python/CheckModuleBoundariesTest.py`、`build.bat check`、`git diff --check`

### 变更文件边界

审查前 `git status --short`：

```text
 M build.bat
 M docs/designs/modular-split-exe-lib-dll-design.md
?? docs/plans/modular-split-exe-lib-dll-implementation-plan.md
?? docs/reports/modular-split-exe-lib-dll/
?? scripts/check_module_boundaries.py
?? tests/python/CheckModuleBoundariesTest.py
```

`docs/designs/modular-split-exe-lib-dll-design.md` 是本包开始前已有的用户修改；本包不得编辑、暂存或提交它。审查的 MS-0 变更为 `build.bat`、计划、MS-0 ledger/evidence、boundary checker 与其 Python 测试。

### 范围对齐

包没有改动 CMake target graph、C++ 源目录、GPU 生命周期或 RenderGraph，且把 GPU/RenderSystem 相关反向边列为 P0 track blocker，符合设计的 Phase 0 范围。当前 guard 覆盖直接引用的 `game/` 和 `app/` quoted includes 及 PCH 泄漏；它不宣称已解决这些边。

### 质量与风险评估

脚本的基线命令与 `build.bat check` 可运行，但测试所声明的 API 与实际 checker 不一致，因此当前无法证明 synthetic regression/stale entry 处理。按 `docs/workflows/review.md` 的必要验证与测试真实性规则，此问题阻止提交。

### 发现项

1. **Blocker** `tests/python/CheckModuleBoundariesTest.py:19`, `tests/python/CheckModuleBoundariesTest.py:38`, `tests/python/CheckModuleBoundariesTest.py:73`, `tests/python/CheckModuleBoundariesTest.py:98` 调用 `check_module_boundaries.main(argv)`，但 `scripts/check_module_boundaries.py:348` 的 `main()` 不接收参数；此外测试使用未实现的 `--write-current` 参数（`tests/python/CheckModuleBoundariesTest.py:41`, `tests/python/CheckModuleBoundariesTest.py:76`）。实际运行该测试得到四个 `TypeError`，与 evidence 的通过陈述冲突。应统一 CLI/`main` 接口和 fixture 生成方式，并重新运行所有 checker 测试后更新证据。
2. **High** `scripts/check_module_boundaries.py:150-156`, `scripts/check_module_boundaries.py:189-208`, `scripts/check_module_boundaries.py:307-317` 仅检查 `future_owner_layer` 与 `p0_blocking` 的类型，未验证允许的 owner、P0 track 标识或 P0-blocked disposition。candidate target/layer 由 ledger scope 提供后再和同一 scope 比较，不能提供计划要求的 target-aware metadata protection。应把允许的 target/layer/owner/P0 policy 固化在 checker，并为无效 owner、移除 P0 blocker 与错误 candidate metadata 补测试。

### 最佳实践建议

- 让 `main(argv: Sequence[str] | None = None)` 调用 `parse_args(argv)`，使单元测试和命令行共用同一个入口；或只测试稳定的公开函数，但不要留下无法执行的测试。
- fixture ledger 应显式写入完整 schema，而不是依赖尚不存在的生成模式；这样能独立证明未登记边和 stale entry 的 exit code。
- P0 track 字符串和 render/GPU edge policy 应由 checker 的常量验证，避免账本编辑静默解除阻塞。

### 剩余风险

尚未接受任何剩余风险。当前范围只防护直接 quoted includes；transitive、generated 和 angle-bracket dependencies 仍是后续审计范围。

### 下一步动作

修复上述 Blocker 和 High 项，运行完整 Python checker 测试、baseline guard、`build.bat check` 与 `git diff --check`，然后在本文件追加跟进审查。

## 跟进审查

**审查目标：** Modular Static Target Split, MS-0 ownership ledger and boundary guard
**结论：** 修改
**审查轮次：** 跟进审查

### 输入

- 设计：`docs/designs/modular-split-exe-lib-dll-design.md`
- 实施计划：`docs/plans/modular-split-exe-lib-dll-implementation-plan.md`
- 审查标准：`docs/workflows/review.md`
- 验证：`python scripts/check_module_boundaries.py`、`python -m unittest tests/python/ModuleBoundaryCheckerTest.py`、`python -m py_compile scripts/check_module_boundaries.py tests/python/ModuleBoundaryCheckerTest.py`、`build.bat check`、`git diff --check`

### 变更文件边界

本轮复查 `build.bat`、计划、MS-0 ledger/evidence、checker 与 focused Python test。工作区仍包含包外的用户设计文档修改，未由本包修改或作为提交候选。

### 范围对齐

首次审查的测试 API/metadata validation 问题已修复：唯一 focused test 可运行，checker 的根目录、target/layer、owner、future owner 和 P0 identifier 都由代码常量验证；本包仍未进入 C++、CMake topology 或 P0 rendering 实现范围。

### 质量与风险评估

边证据双向一致性、synthetic untracked/stale cases 和 malformed metadata 的退出码均已验证。但现有 P0 条目没有不可变的 required-P0 policy；把某个已有 P0 条目的字段改为 `null` 会通过 schema validation，从而失去设计要求的 P0 可见阻塞。

### 发现项

1. **High** `scripts/check_module_boundaries.py:231-242` 只约束非空 `p0_blocking` 值，允许把已有 P0-protected ledger 条目改为 `null`，随后 P0 disposition/milestone 验证不再执行。`tests/python/ModuleBoundaryCheckerTest.py:138-180` 未覆盖移除 P0 blocker 的情形。这样可静默解除当前 66 条 P0 反向边的阻塞，违反计划 `docs/plans/modular-split-exe-lib-dll-implementation-plan.md:56-57` 的“remain observable, not exempted”约束。应由 checker 固定所需 P0 evidence policy（精确 evidence key 或经审计的受保护 source/include policy），并测试移除该字段返回输入错误。

### 最佳实践建议

- 将当前已审计 P0 evidence key 或其最窄稳定匹配策略放入 checker 常量；仅在后续经审查的 P0 coordinator 变更中更新此 policy 和 ledger。
- 增加 synthetic case：源 evidence 属于 required-P0 policy 但 ledger `p0_blocking` 为 `null`，期望 exit code `2`。

### 剩余风险

尚未接受剩余风险。P0 tracking bypass 必须先修复。

### 下一步动作

补充 required-P0 policy 和回归测试，重跑 MS-0 验证命令后进行最终通过审查。

## 最终审查

**审查目标：** Modular Static Target Split, MS-0 ownership ledger and boundary guard
**结论：** 提交
**审查轮次：** 最终审查

### 输入

- 设计：`docs/designs/modular-split-exe-lib-dll-design.md`
- 实施计划：`docs/plans/modular-split-exe-lib-dll-implementation-plan.md`
- 审查标准：`docs/workflows/review.md`
- 验证：`python scripts/check_module_boundaries.py`、`python -m unittest tests/python/ModuleBoundaryCheckerTest.py`、`python -m py_compile scripts/check_module_boundaries.py tests/python/ModuleBoundaryCheckerTest.py`、`build.bat check`、`git diff --check`

### 变更文件边界

审查包仅包含 `build.bat`、实施计划、MS-0 ledger/evidence、boundary checker 与 focused Python test。`docs/designs/modular-split-exe-lib-dll-design.md` 是包外既有用户修改，不得暂存或提交。

### 范围对齐

本包只建立直接 quoted `game/`/`app/` include 的逆向依赖台账和增量防护。未修改 C++、CMake target graph、RenderGraph、GPU 资源生命周期、ResourceManager 或 RenderSystem 合约。全部 66 条 P0 渲染/GPU 边由 checker 的固定策略追踪，继续阻塞 MS-6。

### 质量与风险评估

首次审查指出的测试入口不一致和 metadata validation 缺失已修复；跟进审查指出的 P0 `null` 旁路已由固定 required-P0 source policy 和回归测试修复。checker 基线通过 `129/129` 条目、37 个文件；focused test 通过 6 项；语法编译和 `build.bat check` 通过，且 precheck 在 CMake 配置前执行。

### 发现项

无。

### 最佳实践建议

- 后续 ownership migration 在同一变更中更新源码和 ledger，使双向证据校验持续收敛。
- 任何 P0 source policy 调整均应与渲染轨道协调，并经独立审查后修改 checker 常量和 ledger。

### 剩余风险

接受以下剩余风险：

- Guard 仅覆盖候选 `engine/core` 目录及 `src/pch.hpp` 中直接引用的 quoted `game/`/`app/` includes；传递、生成和 angle-bracket 依赖留待后续审计。
- 66 条渲染/GPU P0 反向边仍存在，但已固定策略追踪并保持 MS-6 阻塞状态。

### 下一步动作

仅暂存本审查通过的 MS-0 包文件并创建重要节点提交；下一项待办为 MS-1 的 minimal Types/Core candidate contract。
