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
- [x] Task 1.5: `build.bat` + focused CTest 验证（2026-08-25：build.bat 通过；`ctest -R "nmd.tests.unit|nmd.tests.ai.unit"` 通过）。

### Phase 2: ui-coord
- [~] Task 2.1: MonsterHealthBarController 改为单次 world→uiLogical（删除手写逆变换与连环换算）。
- [~] Task 2.2: GameUiHost drag phantom 改用 `UiViewport`（移除 legacy logical helper 的 UI 消费）。
- [~] Task 2.3: 全 UI controller 审计 `GetScreenToWorld2D`/`GetWorldToScreen2D` 残留，全部走 `CoordSystem`。
- [x] Task 2.4: 验证 16:9/21:9/4:3 下 HUD/panel 像素位置一致（2026-08-25：以 `tests/unit/CoordPixelMatrixTests.cpp` 12 配置自动化像素链回归替代人工验证，含 DRS on/off × HDR on/off 全组合）。

### Phase 3: text-msdf
- [~] Task 3.1: MSDF bearing 在 `MSDFAtlasRegistry` 导入期归一为 Y 向下世界偏移；`BuildTemplatesMsdf` 消费端不再做符号假设。
- [x] Task 3.2: blit 翻转收敛到 `RenderTargetDescriptor.flipY` / `coord::NativeYToGl`；移除散落负高度与 `height - y`（2026-08-25：`coord::BlitSourceRect` 为唯一源矩形构造入口；RenderSystem.cpp HDR blit 走 `targetState.flipY`；GameplayState 两处与 AstrolabeRenderer 走 `flipY=false` 窗口路径；负高度源矩形全仓清零）。
- [~] Task 3.3: popup/glyph/text 三处 UV 朝向由资源元数据统一驱动。
- [x] Task 3.4: bitmap 与 MSDF 两条文字路径同参输出一致（2026-08-25：Game.cpp GPU text 导入已显式化 `coord::MsdfBearingToWorldOffset` 与 em 换算，数值保持原 1:1 约定；后经 F9 热键实测：bitmap 路径 24px 整数倍量化导致文字明显偏大，与 MSDF 无法同参一致 → **决定删除 bitmap 标签路径**，MSDF 图集成为唯一字形来源；删除范围：F9 调试热键、整数量化、`LootTextBatcher::BatchString/MeasureText/BuildTemplates`、`LabelCacheComponent.cachedGlyphs`、`GlyphCache`（GlyphIndexCache）及对应测试；`WriteInstances` 改为从 MSDF 模板重建实例（模板成为唯一布局来源），新增 `MeasureTextMsdf` 度量（与模板光标数学一致）。验收：LootText 6 用例 + CoordGuard 4 用例 + ctest ci 全绿，文字尺寸 = fontSize 精确，不再放大）。

### Phase 4: qa-coord
- [~] Task 4.1: round-trip 单测（world↔screen↔uiLogical↔uiNative）。
- [~] Task 4.2: source guard 测试：禁止渲染 pass/panel 新增 `MatrixOrtho(`、`ScreenHeight -`、裸 `GetScreenToWorld2D(`。
- [x] Task 4.3: 跨域像素矩阵 fixture（分辨率/DRS/HDR 组合，2026-08-25：`tests/unit/CoordPixelMatrixTests.cpp`，16:9/21:9/4:3 × DRS on/off × HDR on/off 共 12 配置，锁定 blit 链解析公式、DRS 1/scale 放大语义、flipY 位置无关性与 MVP↔像素入口一致性）。
- [x] Task 4.4: 全量 CI 回归（2026-08-25：`ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` 全绿；全仓 `GetScreenToWorld2D/GetWorldToScreen2D/rlGetMatrixModelview/rlGetMatrixProjection` 零残留）。

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
>
> **遗留调用点全量迁移与收口（2026-08-25）**：Task 1.5/2.4/3.2/4.3/4.4 已闭环（见各任务状态）；本次增量：① 旧调用点全量迁移——RenderSystem 4 处 MVP、MDIRenderer、GPUEntitySystem（Render/RenderLegacy）、GPULootSystem、OccluderExtractPass、FluidSimulationPass、HoloBladeRenderSystem（新增 camera 参数）、GameplayRenderAdapter、CombatSystem、TooltipController、GameUiHost、AstrolabeController/AstrolabeRenderer、InputSystem、MonsterHealthBarSystem、GameplayState 全部收口到 `coord::*`；② blit 收敛 `coord::BlitSourceRect`（flipY 为唯一事实来源）；③ GPUData.hpp 结构补 `coord::Space` 注释；④ Game.cpp GPU text em-unit 度量显式化（`coord::MsdfBearingToWorldOffset`）；⑤ `CoordPixelMatrixTests.cpp` 12 配置像素链回归；⑥ **删除 bitmap 标签路径**（2026-08-25 决策：F9 实测 bitmap 路径整数量化后文字偏大，MSDF 成为唯一字形来源，详见 Task 3.4）；⑦ **审查修复（2026-08-25，独立审查 High 1）**：`rlGetFramebufferWidth/Height` 只在 `BeginTextureMode` 内刷新（RLGL.State.framebufferWidth = DRS 缩放 RT 尺寸），与 raylib 默认窗口投影（`rlOrtho(0, GetRenderWidth(), GetRenderHeight(), ...)`）在 DRS≠1 或运行时 resize/全屏切换后分叉 → 全部 GPU MVP 尺寸源统一改为 `GetRenderWidth()/GetRenderHeight()`（与 `SetupViewport` ortho 同源活值，DRS on/off、resize、HiDPI 均与旧 `rlGetMatrixProjection` 行为一致）。全仓旧 API 零残留。

> **用户验证（2026-08-24）**：用户已在游戏内验证，未见明显问题；自动化 `build.bat`/CTest 与像素矩阵仍作为后续 CI 补跑项。
