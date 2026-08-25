# 工作包 05 — reviewer（审阅）

**Role**: 架构审阅
**Depends**: 1-4
**Files Owned**: `docs/reviews/2026-08-24-coordinate-system-review.md`（新增）

## Mission
对照设计 §3 六条铁律逐条审阅全部 diff，产出可归档结论 `提交` 或 `修改`。

## 必做条款
1. R1-R6 各列一项证据（文件/行/测试）。
2. 检查「等价改动」是否真的零行为变化（MVP 逐位等价、UI 像素位置一致）。
3. 检查是否有新增两可注释（`// Screen/World coords` 之类）。
4. 结论与理由写入 review 文档；若为 `修改`，列出阻塞项并指定负责人。
