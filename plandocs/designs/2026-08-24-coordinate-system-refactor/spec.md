# 坐标系单一来源（Coordinate System Single Source）规格说明书

> **Track ID**: refactor_coordinate_system_20260824
> **类型**: refactor / governance（防复发）
> **设计输入**: [坐标系单一来源契约设计](../2026-08-24-coordinate-system-convention-design.md)
> **状态**: 🚧 In Progress — 2026-08-24 立项
> **子代理**: engine-coord / ui-coord / text-msdf / qa-coord / reviewer

---

## 1. 问题与目标

坐标语义在 `World / ScenePixel / UiLogical / UiNative / Ndc(FBO) / MsdfMetric` 六套空间之间被各子系统分别重写，历史上造成 HDR 黑帧、世界坐标错位、UI 缩放错乱、MSDF 字形偏移等反复事故。

本 Track 目标：
1. 新增 `NoMoreDay::render::coord::CoordSystem` 作为**唯一坐标转换入口**（MVP、world↔screen、Y 翻转）。
2. 取消所有「自建 `MatrixOrtho` / `GetScreenToWorld2D` / 手写逆变换 / `height - y`」的散落实现。
3. UI 全部走 `UiViewport`，移除 legacy logical helper 的 UI 消费。
4. Y/UV 翻转只在 `RenderTargetDescriptor` 与 `coord::NativeYToGl` 边界出现。
5. MSDF/FreeType 度量在导入期归一为 Y 向下世界偏移。
6. 建立 round-trip 单测与跨域像素门禁，防止坐标问题复发。

## 2. 范围

### 交付
- `coord::Space` 枚举 + `Camera2DTransform` + `WorldToScenePixel/ScenePixelToWorld/Build2DMvp/NativeYToGl`。
- 引擎侧 `GPUParticleSystem::BuildMVP` 委托给 `coord::Build2DMvp`。
- 游戏侧 FRAGCOORD 相关翻转改走 `coord::NativeYToGl`。
- UI 侧归一 `UiViewport`，MonsterHealthBar 单次换算。
- MSDF 度量导入归一 + 三处 UV 朝向统一。
- 测试与 source guard。

### 非目标
- 不改 shader 视觉风格、不重做布局、不移动文件目录结构（除非子代理确认零风险）。
- 不提交/不建 worktree（遵循 AGENTS.md）；仅修改工作树。

## 3. 契约要求

见设计文档 §2（空间表）与 §3（六条铁律 R1-R6）。实现必须逐条满足。

## 4. 验收标准

- [ ] 编译：`build.bat`（RelWithDebInfo）通过。
- [ ] 单测：`bin/NoMoreDayTests.exe --test-case="[Unit]*Coord*"`、`[Unit]*UiViewport*`、`[Unit]*MSDF*` 通过。
- [ ] CI：`ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` 通过。
- [ ] source guard：`tests/tech/CoordGuardTests.cpp` 存在并覆盖 R2/R3。
- [ ] 跨域矩阵（16:9/21:9/4:3 × DRS × HDR）无坐标错位证据（截图或集成 fixture）。
- [ ] 无新增 `MatrixOrtho(` / `ScreenHeight -` / 裸 `GetScreenToWorld2D(` 于渲染 pass / panel。
