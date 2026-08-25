# 工作包 02 — ui-coord（UI 坐标收口）

**Role**: UI/Raylib 坐标专家
**Depends**: Phase 1（`CoordSystem` 可用）
**Files Owned**: `src/game/application/ui/MonsterHealthBarController.{hpp,cpp}`、`src/game/application/ui/GameUiHost.cpp`、可能含 `UISystem` 相关逻辑坐标遗留

## Mission
让 UI 消费端只相信一个坐标面：`UiViewport`（logical↔native）+ `CoordSystem`（world↔scenePx），删除手写换算。

## 必做条款
1. MonsterHealthBar 的 world→screen→`ToLogical`→`ToPixel` 连环换算改为**单次** world→uiLogical 语义（可用两次已知调用组合，但必须经由 `CoordSystem`/`UiViewport`，禁止自写 `(x-target)*zoom+offset`）。
2. `GameUiHost` drag phantom 改用 `UiViewport::ToLogical(GetMousePosition())`（已有的 viewport 实例），移除 legacy logical helper 在 UI 绘制的消费。
3. 审计 `ui/` 下 `GetScreenToWorld2D / GetWorldToScreen2D` 残留，需要时交由 `CoordSystem`。
4. 不改变视觉行为；不提交。

## Acceptance
- 无新增裸物理缩放公式于 controller。
- 单测（`MonsterHealthBarControllerTests`、`UiRaylibBackendTests`）保持通过。
- 16:9 / 21:9 / 4:3 下 HUD/panel 像素位置证据（截图或 test）。
