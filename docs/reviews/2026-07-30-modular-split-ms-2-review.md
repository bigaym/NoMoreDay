# 模块拆分 MS-2 审查报告

**审查目标：** 将 Game-ECS 专属 `PhysicsUtils` 从 Core 迁移至 Game。
**结论：** 提交
**审查轮次：** 首次审查

## 输入

- 设计：`docs/designs/modular-split-exe-lib-dll-design.md`（受保护用户修改，排除）
- 计划：`docs/plans/modular-split-exe-lib-dll-implementation-plan.md`
- 证据：`docs/reports/modular-split-exe-lib-dll/ms-2/evidence.md`
- 标准：`docs/workflows/review.md`、`conductor/code_standard.md`

## 变更边界与范围对齐

`ApplyKnockback` 直接操作 Game ECS registry 及 `Common.hpp` 中的
`Position`/`Velocity`，因此归属于 Game，而不是 Core 或 Types。实现将其从
`src/core/math/PhysicsUtils.hpp` 移至
`src/game/systems/physics/PhysicsUtils.hpp`，保留命名空间、签名和行为；生产
消费者、focused test 与陈旧 include 已同步。未保留 Core forwarding header。

本包未修改 CMake target topology、`build.bat`、GPU/render、输入/Engine physics
归属或其他 Gameplay 行为。MS-0 ledger 与 MS-1 contract 只移除了已解决的
PhysicsUtils 项。用户拥有的设计文档未编辑、暂存或纳入提交。

## 验证

- 模块边界 checker：通过，`128/128` 条边、36 个文件。
- Core candidate contract checker：通过。
- Focused knockback doctest：通过，1 test case / 1 assertion。
- `build.bat check`：通过。
- `git diff --check`：通过。
- 完整构建在未修改的 `BloodSea.cpp:243` 既有技能模块错误处失败，已在 evidence
  中记录；该问题不属于本节点，未在本包修复。

## 发现项

无 Blocker、High、Medium 或 Low 发现。

## 最佳实践建议

`src/pch.hpp` 仍有一行注释掉的旧路径文字；它不是活动 include，不影响本包，宜在
后续独立文档或 PCH 整理节点处理。

## 接受的剩余风险

接受未修改的 `BloodSea.cpp` 完整构建阻塞，以及非活动的 PCH 旧路径注释。二者均不
影响 MS-2 的 ownership correction。

## 下一步

提交已审查的 MS-2 包；随后推进 MS-3 Input 与 ECS Physics 归属工作。
