# 工作包 04 — qa-coord（测试与门禁）

**Role**: 测试/质量工程师
**Depends**: Phase 1-3 落地
**Files Owned**: `tests/unit/CoordSystemTests.cpp`、`tests/tech/CoordGuardTests.cpp`（新增）、`tests/integration/`（像素矩阵）、`docs/plans/2026-08-24-coordinate-system-implementation-plan.md`（进度同步）

## Mission
把「坐标契约」变成可验证、防回退的门禁。

## 必做条款
1. round-trip 单测：world↔screen↔uiLogical↔uiNative，误差 < 0.01。
2. source guard（仿现有 `UiR5RemediationGuardTests`）：
   - 渲染 pass/panel 新增代码中禁止出现 `MatrixOrtho(`、`ScreenHeight -`、裸 `GetScreenToWorld2D(`；
   - `CoordSystem` 之外不再是坐标重写点。
3. 跨域像素矩阵：16:9/21:9/4:3 × DRS × HDR，断言画面四角与中心一致（可复用的 fixture/截图）。
4. 同步更新 `plan.md` 完成状态。

## Acceptance
- focused + 全量 CI CTest 通过。
- guard 测试可证明「新代码绕过 `CoordSystem` 会被源码断言抓住」。
