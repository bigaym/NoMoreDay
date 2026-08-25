# 工作包 03 — text-msdf（MSDF / UV / Y 翻转收口）

**Role**: 文字/字形/UV 专家
**Depends**: Phase 1（`CoordSystem`）+ Phase 2（UI 收口后更稳）
**Files Owned**: `src/engine/render/LootTextBatcher.cpp/.hpp`、`src/engine/render/resource/MSDFAtlasRegistry.{hpp,cpp}`、`assets/shaders/ui/glyph*.frag`、`assets/shaders/text/text_quad.*`、`assets/shaders/vfx/popup.vert`、`assets/shaders/ui/label_instanced.*`

## Mission
统一 MSDF/FreeType 度量与 Y/UV 朝向，让 bitmap 与 MSDF 两条文字路径同参输出一致。

## 必做条款
1. MSDF bearing（`MsdfMetric`）在 `MSDFAtlasRegistry` 导入时归一为 **Y 向下世界偏移**；`BuildTemplatesMsdf` 消费端删除符号假设，与 bitmap 路径 `offsetY` 语义一致。
2. 渲染边界处 Y 翻转只经 `coord::NativeYToGl` / `RenderTargetDescriptor.flipY`；删除散落负高度与 `height - y`。
3. popup/glyph/text 三处 UV 的 V 朝向由资源元数据驱动，禁止 shader 内手写 row 翻转假设。
4. 不改变视觉行为（除修复既有 MSDF 偏移/UV 缺陷外）；不提交。

## Acceptance
- `MSDFAtlasDataTests`、`LootTextBatcherTests` 通过。
- bitmap 与 MSDF 同参输出一致（测试/截图）。
