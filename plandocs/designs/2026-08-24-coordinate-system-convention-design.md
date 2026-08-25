# 坐标系单一来源契约设计（Coordinate System Convention）

> **Status:** proposed (Phase 0 contract)
> **Date:** 2026-08-24
> **Purpose:** 终结渲染体系里「坐标语义不一致」反复引爆 bug 的局面。把世界、屏幕、UI、NDC、FBO/MSDF 的坐标系、朝向与唯一转换入口写成仓库契约，作为后续全部坐标改造（CoordSystem 收口）的权威依据。
> **关联调研:** 本文件基于对 `src/`、`docs/`、`conductor/`、`设计文档/` 的坐标调用点全量扫描（见 §4）。

---

## 1. 问题复盘（为什么这里值得立 contract）

历史上与坐标相关的故障与审计反复出现：

| 时间/位置 | 现象 | 根因 |
| --- | --- | --- |
| `gpu_hardware_validation_gate` W6.9 | HDR buffer 尺寸错、场景进不了 HDR | 6 处渲染调用点缺 viewport/投影/`BeginMode2D`，hooks **世界坐标错位** |
| M2-E DRS 审计 | 缩放 scene target 后 UI/坐标错乱 | 用 scaled extent 重建 target，却仍用 native-size camera，world/screen 互转不同步 |
| HDR external target 审计 | 黑帧/翻转风险 | blit 的坐标原点、Y 翻转、format 无显式契约 |
| commit `768a695a` | 天剑共鸣按钮缩放错位 | UI logical 与 physical 混乘 `scaleFactor` |
| commit `873b5f31` | MSDF 字形 UV 错位 | atlas UV 朝向与采样约定不一致 |
| 计划 U1 | 「坐标语义不一致」被列为最大风险 | 因此引入 `UiViewport`，但未覆盖全仓库 |

共同根因只有一个：**同一个数学被每个子系统各自重写一遍，且结构字段从不声明自己处在哪个坐标空间。**

## 2. 坐标系定义（唯一事实来源）

| Space | 原点 | 朝向 | 单位 | 参考实现/使用方 |
| --- | --- | --- | --- | --- |
| `World` | 屏幕内容左上角（raylib 2D 视域） | X 右、Y 下 | 世界单位 + `Camera2D(target, offset, zoom)` | `Position` 组件、`BeginMode2D`、`Camera2DTransform` |
| `ScenePixel` | 场景 RT 左上角 | X 右、Y 下 | 像素（fbo/target 像素） | `BeginTextureMode(m_sceneRT)`、`Build2DMvp` |
| `UiLogical` | UI 内容左上角 | X 右、Y 下 | 逻辑像素（基准 2560×1440） | `UiViewport`（`ToLogical/ToPixel`） |
| `UiNative` | 窗口左上角 | X 右、Y 下 | 设备像素 | `UiRaylibBackend::Render` |
| `Ndc` | OpenGL 默认（左下） | X 右、Y 上 | [-1,1] | raylib/rlgl 内建；**不对外直接编码** |
| `FboTexel / Texture` | 取决于 target descriptor 的 origin；raylib 纹理 V=0 为图像顶部（上传时翻转） | 采样坐标 | 纹素 | `DrawTexturePro`、glyph/popup shader |
| `MsdfMetric` | 基线、bearing 左下（FreeType 约定） | 上升为正 | em/像素 | `MSDFAtlasRegistry`、`text_layout.compute` |

**注意**：`World/ScenePixel/UiLogical/UiNative` 都是 **Y 向下**；`Ndc` 与 GL 片段坐标是 **Y 向上**；`MsdfMetric` 是**基线向上**。三套语义必须互斥地由 `CoordSystem` 负责互相换算，禁止调用方自行实现。

## 3. 铁律（RULES）

1. **R1 — 一个转换一个入口**：世界 ↔ 屏幕 ↔ UI logical ↔ native 只允许调用 `NoMoreDay::render::coord::*`；禁止在 panel / render pass / shader 侧手写 `(x - target)*zoom + offset` 或 `height - y`。
2. **R2 — MVP 单一来源**：任何自定义 GPU pass 的 `mvp` 只能由 `coord::Build2DMvp(camera, targetW, targetH)` 生成；禁止再自行构造正交矩阵。
3. **R3 — Y 翻转只在 target descriptor 边界出现**：`DrawTexturePro(..., -h, ...)`、`screenPlayer.y = GetScreenHeight() - y`、fragment 坐标翻转让 `coord::NativeYToGl` / `RenderTargetDescriptor.flipY` 统一承担。
4. **R4 — 字段必须标注 Space**：所有携带位置/尺寸/UV 的结构在声明处注明所属 `coord::Space`，禁止 `// Screen/World coords` 这类两可注释。
5. **R5 — 度量在导入期归一**：MSDF/FreeType 的 bearing（`MsdfMetric`）在进入 `MSDFAtlasRegistry` 时就换算为 `World`（Y 向下）偏移；消费端禁止再做符号假设。
6. **R6 — 跨域必有 round-trip 测试**：任何新增坐标换算必须带 round-trip 单测；任何改动渲染 target 尺寸/DRS/HDR 的路径必须带跨域像素矩阵回归。

## 4. 现状调用点清单（Phase 0 基线，评审使用）

### 4.1 屏幕 ↔ 世界（应在 Phase 1 收口到 `CoordSystem`）
- `src/game/application/states/GameplayState.cpp:762,1114`（1114 处做了 FRAGCOORD 翻转）
- `src/game/application/input/InputSystem.cpp:61,80`
- `src/game/application/ui/GameUiHost.cpp:1056`
- `src/game/application/ui/TooltipController.cpp:84`
- `src/game/application/ui/AstrolabeController.cpp:133,187`
- `src/game/application/ui/AstrolabeRenderer.cpp:192-193`
- `src/game/application/ui/MonsterHealthBarController.cpp:105-130（手写逆变换）、161-173（world→screen→logical 连环换算）`
- `src/game/application/render/GameplayRenderAdapter.cpp:688-690`
- `src/engine/render/GPUEntitySystem.cpp:176-177,209`
- `src/engine/render/GPULootSystem.cpp:635-636`
- `src/engine/render/passes/OccluderExtractPass.cpp:407`

### 4.2 MVP / 投影（Phase 1 收口到 `coord::Build2DMvp`）
- `src/engine/render/GPUParticleSystem.cpp:821-835`（中央 `BuildMVP`，建议作为委托源）
- `src/engine/render/RenderSystem.cpp:679,702,744,775`（`rlGetMatrixModelview()*rlGetMatrixProjection()`）
- `src/engine/render/trail/GPUTrailRenderer.cpp:238`
- `src/engine/render/GPUSkillEffectSystem.cpp:1392`
- `src/game/systems/vfx/HoloBladeRenderSystem.cpp:207-209`

### 4.3 Y/UV 翻转（Phase 3 收口）
- `GameplayState.cpp:1115`（FRAGCOORD 翻转）
- `GameplayState.cpp:1124-1134`、`AstrolabeRenderer.cpp:194`（`DrawTexturePro` 负高度）
- HDR blit：`gpu_production_hdr_gi_closure_20260726/spec.md:82`（坐标原点/Y 翻转/format），`plan.md:56`
- `tests/unit/MSDFAtlasDataTests.cpp:93-95`（v 轴翻转测试）

### 4.4 UI 逻辑/物理混用（Phase 2 收口）
- `src/game/application/ui/GameUiHost.cpp`（drag phantom 仍读 legacy `UISystem::GetMousePositionLogic()`）
- `src/game/application/ui/MonsterHealthBarController.cpp`（self 计算）
- 已收敛的正面样板：`UiViewport.hpp/.cpp`、`UiRaylibBackend::ToNativePoint/Rect`、`tests/unit/UiViewportTests.cpp`

### 4.5 MSDF/文字度量（Phase 3 收口）
- `src/engine/render/LootTextBatcher.cpp:203-213`（BuildTemplatesMsdf 直接用 bearing[1]，未归一化到 Y 向下）
- `assets/shaders/text/text_layout.compute`（`cmd.worldPosY + gm.offsetY`，y-down 排版）
- `assets/shaders/ui/glyph.vert`、`text_quad.vert`（`aPos.xy * size`）
- `assets/shaders/vfx/popup.vert`（PIL row0 在图像顶部，`vStart` 手写）

## 5. 落地路径（与实施工作包对应）

- **Phase 1 — Engine 坐标单一来源**：新增 `src/engine/render/CoordSystem.hpp`（header-only），`Build2DMvp` 收口、`WorldToScenePixel/ScenePixelToWorld` 统一、`NativeYToGl` 只此一处；`GPUParticleSystem::BuildMVP` 委托给它；`GameplayState` FRAGCOORD 翻转改调用 helper。
- **Phase 2 — UI 坐标收口**：所有 panel/overlay 只走 `UiViewport`；移除 legacy logical helper 的 UI 消费；`MonsterHealthBarController` 改为单次 world→ui 换算。
- **Phase 3 — Y/UV/MSDF 收口**：blit 翻转只经 target descriptor；MSDF bearing 导入期归一为 Y 向下世界偏移；popup/glyph/text 三处 UV 朝向统一由资源元数据驱动。
- **Phase 4 — 测试与门禁**：round-trip 单测、跨域像素矩阵、source guard 防回退。

## 6. 验收标准

- [ ] 全仓库不再出现新增的 `(x - cam.target)*zoom+offset` / `ScreenHeight - y` / 自建 `MatrixOrtho` 于渲染 pass 中（source guard 阻断）。
- [ ] `CoordSystem` 的 world/screen/ui 换算 round-trip 误差 < 0.01。
- [ ] 16:9 / 21:9 / 4:3 × DRS on/off × HDR on/off 下 HUD、场景、文字像素位置一致。
- [ ] MSDF/bitmap 两套文字路径在相同参数下输出一致。
- [ ] `build.bat` + 相关 CTest 通过，坐标改动无性能回归。
