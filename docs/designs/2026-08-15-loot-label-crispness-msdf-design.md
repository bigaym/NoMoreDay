# 掉落标签清晰度两阶段设计（Loot Label Crispness: Pixel Snap + MSDF）

> **Status:** proposed
>
> **Purpose:** 在 2026-08-14 已有修复（1:1 像素映射、矩形原点吸附、glyph.frag 掩码、不透明背景）基础上，解决掉落物品标签文本仍存的模糊问题。分两阶段：A) 位图路径逐字形整像素吸附止损；B) 标签字形采样从 24px 位图图集迁移到 MSDF 图集（根治任意 zoom/scale 下的模糊）。本设计扩展 `2026-08-09-loot-label-rendering-fix-design.md` §2.2 的范围（原设计明确 MSDF 迁移不在范围内）。
>
> **Primary evidence:**
> - 用户上报（2026-08-15）："掉落物的物品标签文本显示有些模糊，应该是标签渲染流程的问题"。
> - 代码定位：`GameplayRenderAdapter::BuildCpuLootLabels`（src/game/application/render/GameplayRenderAdapter.cpp:784-991，fSize=round(24*scale/zoom)、仅矩形原点吸附 L917-932）、`LootTextBatcher::BuildTemplates/WriteInstances`（src/engine/render/LootTextBatcher.cpp:97-176，全 float 无吸附）、`assets/shaders/ui/glyph.frag`（bilinear 采样无半纹素内缩）、引擎 glyph draw（src/engine/render/RenderSystem.cpp:679-711，绑定 frame.font.texture）。
> - MSDF 资产已存在：`assets/textures/fonts/msdf/v4_msdf_gb2312_4096.png` + `.metrics.bin`（GB2312+ASCII+符号，4096²，pxrange 6），启动时由 `InitializeGPUTextBootstrap`（src/app/Game.cpp:72-135）无条件加载并上传 GPUTextSystem。
> - RenderGraph 通道顺序（RenderSystem.cpp:1408-1430）：VFXPass → GPUTextPass(1415, gpuTextEnabled 门控) → GPULootPass(1423) → UIWorldPass(1430) → PostProcess → Composite。

## 1. 决策摘要

### 1.1 剩余模糊根因（已确认）

1. **1:1 映射仅 zoom=1.5 且 scale=1 时成立**：fSize=round(24*scale/zoom) 取整后，屏幕字号 ≠ 24px 图集纹素尺寸，bilinear 重采样发糊；LootFilter 强调 scale>1 同样破坏 1:1。
2. **只吸附了矩形原点**：字形 quad 的世界坐标与尺寸仍是小数（WriteInstances 全 float），亚像素偏移被 bilinear 采成软边。
3. **图集本身限制**：24px 位图图集无 mipmap，任何放大（zoom>1.5、强调 scale）即糊；UV 无半纹素内缩，邻字形渗色。

### 1.2 阶段 A：位图路径止损（短期）

- 逐字形整像素吸附：`WriteInstances` 按当前 zoom 把每个字形 quad 的位置与尺寸量化到整屏幕像素（世界单位网格 1/zoom），并只允许 scaleFactor 为整数倍。
- 半纹素 UV 内缩：在 `BuildTemplates` 生成模板时内缩 0.5/texSize（模板缓存，一次性成本）。
- 图集升级（24→48px）降级为 **spike 任务**：先测量 24px 图集实际纹理尺寸与 48px 重光栅化内存/打包可行性，再决定是否执行；阶段 B 落地后大概率不需要。

### 1.3 阶段 B：标签字形采样迁移 MSDF（根治）

**方案取舍（B1 vs B2）**：

- B1（走 GPUTextPass + GPUTextSystem 计算排版）：GPUTextPass 在 RenderGraph 中**早于** UIWorldPass（RenderSystem.cpp:1415 < 1430），而标签背景矩形在 UIWorldPass 内绘制且不透明——GPUTextPass 的文字会被其后绘制的标签背景覆盖。需要 pass 重排或背景搬移，侵入大。且依赖 BUG-20260219-002（动态字符串表）与 BUG-20260219-001（counter 同步回读）的修复。
- **B2（选定）**：保持现有 UIWorldPass 内"标签背景 → 字形"绘制顺序与全部标签管线（预算/重叠/吸附/边框）不变，仅替换字形四边形的**采样纹理与着色器**：字形 quad 的 UV/size/bearing/advance 改从 MSDF 度量生成（CPU 侧模板，复用现有缓存机制），fragment 用 median RGB 解码 + screenPxRange。引擎 glyph draw 增加 MSDF 模式（绑定 MSDF 图集 + pxRange uniform）。回退：MSDF 图集不可用时自动落回现有位图路径，行为与今日完全一致。

B2 的理由：标签是"背景+边框+文字"复合渲染，B1 需要拆开跨 pass 渲染；B2 零 pass 改动、零布局计算依赖、复用全部既有标签逻辑与阶段 A 吸附成果。

## 2. 目标、非目标与硬约束

### 2.1 目标

1. 阶段 A 后：位图路径在 zoom=1.5±0.3、filter scale≤1.5 下字形边缘无 bilinear 软边（对比修复前后截图）。
2. 阶段 B 后：任意 zoom（1.0~2.5）与 filter scale（≤2.0）下标签文字锐利；不依赖 fSize 取整；中文物品名、金币数字、悬停边框全部覆盖。
3. MSDF 不可用时回退位图路径且无视觉回归（对比 08-14 版本）。
4. 不新增纹理资产、不新增 SSBO binding；MSDF 图集复用现有已加载资源。

### 2.2 非目标

- 不迁移伤害弹字（仍 DrawTextEx/GPUText 现状）。
- 不修复 BUG-20260219-001/002/003（GPUText 计算排版链路），本设计不依赖它们。
- 不迁移标签背景/边框到 GPU 计算（继续 label_instanced）。
- 不做标签动画/渐隐（沿用现状）。

### 2.3 硬约束

- 渲染路径满足既有 RenderGraph/回退约束；不改 pass 顺序、不新增 pass。
- `LootTextBatcher` 既有公共签名（BatchString/MeasureText/BuildTemplates/WriteInstances）外部调用点仅 GameplayRenderAdapter，签名变更须同步更新其调用点与单测。
- 纹理所有权：GPUTextSystem 继续是 MSDF 图集纹理唯一 owner（SetAtlasTexture takeOwnership=true 现状），Registry 只读引用，禁止二次释放。
- 构建 `RelWithDebInfo`（AGENTS.md）；代码规则遵循 implementation.md / code_standard.md。
- 不改 settings.json 开关；MSDF 模式与 tier 无关（图集无条件已加载），但保留 `IsInitialized` 失败回退。

## 3. 已验证基线

- 图集加载无条件执行（Game.cpp:354），失败时 `InitializeGPUTextBootstrap` 直接 return，游戏继续以位图路径运行。
- 字形 draw：RenderSystem.cpp:679-711，绑定 frame.font.texture.id 至 slot 3，shader 由引擎持有（glyphShader/glyphMvpLoc/glyphTexLoc）。
- MSDF 度量字段：`MSDFGlyphMetric{codepoint, uvRect[4], bearing[2]=left/bottom, size[2], advance}`（src/engine/render/resource/MSDFAtlasLoader.hpp:17-23），单位为图集生成时的像素；`MSDFAtlasData.distanceRange` 由 metrics.bin 解析。
- 待验证（B1 任务内确认）：emSize（px/em）常量来源——检查 `scripts/` 下 MSDF 生成脚本与 `MSDFAtlasLoader::LoadMetricsBinary` 是否解析 emSize；若无则从生成脚本常量引入，并以 ASCII 字形 advance 和（≈0.5~0.7 em）做一致性校验。

## 4. 风险

- emSize/pxRange 单位错误会导致 MSDF 边缘过粗或过细——B1 用数字字形（0-9，已知 0.5em 宽）做视觉/数值校验。
- MSDF 图集 4096² RGB8（约 50MB VRAM）已常驻，B2 不新增显存；BC4/BC5 压缩为后续独立优化（MSDFAtlasCompression 已预留）。
- 引擎 glyph shader 切换引入第三组 shader（glyph.frag / glyph_msdf.frag）——确保 shader 加载失败时回退位图 shader（与 BUG-20260221-001 的 CPU 回退保护同模式）。
