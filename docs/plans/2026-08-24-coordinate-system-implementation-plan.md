# 坐标系单一来源实施计划

> **Track ID**: refactor_coordinate_system_20260824
> **依赖 Spec**: [spec.md](../../plandocs/designs/2026-08-24-coordinate-system-refactor/spec.md)
> **状态**: [~] In Progress — 2026-08-24

---

## 实施思路

分五个「子代理工作包」串行落地，每包都为独立可验证的原子切片：

1. **engine-coord**：先建 `CoordSystem`（header-only），再把引擎 MVP 与 FRAGCOORD 翻转接到唯一入口——这是全部后续的地基，且不改变任何视觉行为。
2. **ui-coord**：把 UI 层所有坐标消费收敛到 `UiViewport`，删掉手写 world→screen→logical 连环换算。
3. **text-msdf**：把 MSDF/FreeType bearing 在导入期归一为 Y 向下世界偏移，并把 Y/UV 翻转收敛到 target descriptor / helper。
4. **qa-coord**：补齐 round-trip、source guard、跨域像素矩阵。
5. **reviewer**：对照设计文档与 spec 审全部 diff，产出 `docs/reviews/2026-08-24-coordinate-system-review.md`。

## 阶段总览

| 阶段 | 负责人（子代理） | 核心产出 | 状态 |
| --- | --- | --- | --- |
| 0 | planner（本 Track） | 设计文档 + Track/spec/plan + 工作包 | [x] |
| 1 | engine-coord | `CoordSystem` + MVP/FRAGCOORD 收口 + 单测 | [~] |
| 2 | ui-coord | UI 坐标收口到 `UiViewport` | [ ] |
| 3 | text-msdf | MSDF/UV/Y 翻转收口 | [ ] |
| 4 | qa-coord | round-trip + guard + 像素矩阵 | [ ] |
| 5 | reviewer | 审阅与 `docs/reviews` 报告 | [ ] |

## 原子任务拆分

### Phase 1: engine-coord（地基）
- [~] Task 1.1: 新增 `src/engine/render/CoordSystem.hpp`（header-only，`coord::Space` / `Camera2DTransform` / `WorldToScenePixel` / `ScenePixelToWorld` / `Build2DMvp` / `NativeYToGl`）。
- [~] Task 1.2: `GPUParticleSystem::BuildMVP` 委托 `coord::Build2DMvp`（行为不变）。
- [~] Task 1.3: `GameplayState.cpp:1115` FRAGCOORD 翻转改调用 `coord::NativeYToGl`。
- [~] Task 1.4: 新增 `tests/unit/CoordSystemTests.cpp`（world↔screen、y-flip、2D MVP 一致性）。
- [ ] Task 1.5: `build.bat` + focused CTest 验证。

### Phase 2: ui-coord
- [~] Task 2.1: MonsterHealthBarController 改为单次 world→uiLogical（删除手写逆变换与连环换算）。
- [~] Task 2.2: GameUiHost drag phantom 改用 `UiViewport`（移除 legacy logical helper 的 UI 消费）。
- [~] Task 2.3: 全 UI controller 审计 `GetScreenToWorld2D`/`GetWorldToScreen2D` 残留，全部走 `CoordSystem`。
- [ ] Task 2.4: 验证 16:9/21:9/4:3 下 HUD/panel 像素位置一致。

### Phase 3: text-msdf
- [~] Task 3.1: MSDF bearing 在 `MSDFAtlasRegistry` 导入期归一为 Y 向下世界偏移；`BuildTemplatesMsdf` 消费端不再做符号假设。
- [ ] Task 3.2: blit 翻转收敛到 `RenderTargetDescriptor.flipY` / `coord::NativeYToGl`；移除散落负高度与 `height - y`。
- [~] Task 3.3: popup/glyph/text 三处 UV 朝向由资源元数据统一驱动。
- [ ] Task 3.4: bitmap 与 MSDF 两条文字路径同参输出一致。

### Phase 4: qa-coord
- [~] Task 4.1: round-trip 单测（world↔screen↔uiLogical↔uiNative）。
- [~] Task 4.2: source guard 测试：禁止渲染 pass/panel 新增 `MatrixOrtho(`、`ScreenHeight -`、裸 `GetScreenToWorld2D(`。
- [ ] Task 4.3: 跨域像素矩阵 fixture（分辨率/DRS/HDR 组合）。
- [ ] Task 4.4: 全量 CI 回归。

### Phase 5: reviewer
- [~] Task 5.1: 对照设计文档 §3 六条铁律逐条审 diff。
- [~] Task 5.2: 产出 `docs/reviews/2026-08-24-coordinate-system-review.md`，结论 `提交` 或 `修改`。

## 测试方法

| 层级 | 覆盖内容 | 证据 |
| --- | --- | --- |
| Unit | CoordSystem round-trip、UiViewport、MSDF | `./bin/NoMoreDayTests.exe --test-case="[Unit]*"` |
| Tech guard | R2/R3 防回退 | `./bin/NoMoreDayTests.exe --test-case="[Tech]*Guard*"` |
| Integration | 像素矩阵 / DRS/HDR | `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` |
| CI | 全量非性能 | `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` |

每次 C++ 变更先运行 `build.bat`（RelWithDebInfo），再由窄到宽跑测试。

> **Phase 2-5 现状（2026-08-24）**：ui-coord（MonsterHealthBar/GameUiHost）、text-msdf（MSDF helper）、qa-coord（CoordGuardTests）、reviewer（初步审查文档）均已落地；剩余为 blit/像素矩阵/全量构建验证（Task 1.5/2.4/3.2/3.4/4.3/4.4）将在统一 build/CI 阶段闭环。
>
> **构建阻塞（2026-08-24）**：本 dsh 沙箱未检测到 MSVC/Visual Studio，无法运行 `build.bat`；需在用户开发机/CI 执行构建与测试。

> **用户验证（2026-08-24）**：用户已在游戏内验证，未见明显问题；自动化 `build.bat`/CTest 与像素矩阵仍作为后续 CI 补跑项。
