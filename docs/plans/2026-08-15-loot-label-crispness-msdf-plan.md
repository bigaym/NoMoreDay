# 掉落标签清晰度两阶段实施计划（Pixel Snap + MSDF）

> **Status:** ready for implementation
>
> **设计依据:** [2026-08-15-loot-label-crispness-msdf-design.md](../designs/2026-08-15-loot-label-crispness-msdf-design.md)
>
> **范围:** 阶段 A（位图路径逐字形吸附止损）+ 阶段 B（标签字形采样迁移 MSDF）。CPU 标签路径（gpuLootEnabled=false 默认激活）。伤害弹字、GPUText 计算排版、GPU loot 路径不在范围内。
>
> **前序计划:** [2026-08-09-loot-label-rendering-fix-plan.md](2026-08-09-loot-label-rendering-fix-plan.md)（A/B/C 已完成：预算优先级、GlyphCache、模板缓存）。

## 1. 实施思路/原理

### 阶段 A：逐字形整像素吸附 + 半纹素 UV 内缩

原理：现有吸附只作用于 `currentRect` 原点（GameplayRenderAdapter.cpp:917-932），字形 quad 在世界空间全 float。将吸附下推到 `LootTextBatcher::WriteInstances`：以 zoom 为参数，把每个字形实例的 position/size 量化到"整屏幕像素"（世界单位网格 = 1/zoom）。模板缓存（BuildTemplates）保持不变——吸附是每帧按原点的廉价运算，模板只承载字体无关信息。scaleFactor 量化：fSize 计算后对齐到图集纹素整数倍，避免非整数缩放。半纹素内缩在 BuildTemplates 生成 UV 时一次性内缩（模板缓存），shader 不动。

### 阶段 B：MSDF 采样替换（in-place，B2 方案）

原理（设计 §1.3）：标签渲染管线（收集→预算→排序→重叠→背景/边框 quad→字形 quad）全部不动；仅字形 quad 的数据来源从"24px 位图图集度量"换成"MSDF 度量"，采样着色器换成 median 解码。数据流：

```
MSDFAtlasRegistry(新, 只读查找) 
  ← Game.cpp InitializeGPUTextBootstrap 装载图集+度量后注册
  → LootTextBatcher::BuildTemplatesMsdf(codepoint→uvRect/bearing/size/advance)
  → LabelCacheComponent.glyphTemplates(复用缓存)
  → WriteInstances(阶段 A 吸附, 位置/颜色写入 glyphBuffer)
  → RenderSystem glyph draw: MSDF 模式绑 GPUTextSystem 图集 + glyph_msdf.frag + pxRange uniform
```

screenPxRange = distanceRange × (fSize × zoom / emSize)，由 adapter 每帧写入 frame（新 out-field），引擎透传 uniform。

## 2. 伪代码引导

### A1. WriteInstances 整像素吸附（签名变更）

```
// LootTextBatcher.hpp
static void WriteInstances(
    const std::vector<GlyphTemplate>& templates,
    const std::vector<GPUGlyphInstance>& cachedRelative,
    Vector2 origin, uint32_t color, float zoom,       // ← 新增 zoom
    std::vector<GPUGlyphInstance>& outBuffer);

// LootTextBatcher.cpp
// 像素网格 = 1/zoom 世界单位。吸附：世界坐标先乘 zoom 得屏幕像素，round 后除回。
float snap(float world, float zoom) {
  return roundf(world * zoom) / zoom;               // zoom>0 已由调用方保证
}
// 每字形：position = {snap(src.position.x + origin.x), snap(...y)}；
//         size = {snap(src.size.x), snap(src.size.y)}（非整数像素尺寸量化后边缘清晰）
```

### A2-spike. 字体图集升级可行性（只测量）

```
// 测量脚本/日志（不改渲染代码）：
// 1) UISystem 加载字体的 font.texture.width/height 实测值
// 2) 以 48px 重光栅化 20902 码点的打包面积估算（字符面积 ~4x，图集上限 2048²/4096²）
// 3) 结论写入 A2 决策段：可行→单独任务；不可行/被 B 取代→关闭
```

### B1. MSDFAtlasRegistry

```
// src/engine/render/resource/MSDFAtlasRegistry.hpp（新）
class MSDFAtlasRegistry {
 public:
  static MSDFAtlasRegistry& Get();
  bool Register(Texture2D texture /*不接管所有权*/, const std::vector<MSDFGlyphMetric>& glyphs,
                float distanceRange, float emSize);   // Game.cpp bootstrap 调用
  void Clear();                                       // 关机/重载
  bool IsAvailable() const;
  const MSDFGlyphMetric* Find(uint32_t codepoint) const;   // 未命中→nullptr
  Texture2D GetTexture() const; float GetDistanceRange() const; float GetEmSize() const;
 private:
  Texture2D m_texture{}; std::unordered_map<uint32_t, size_t> m_index;
  const std::vector<MSDFGlyphMetric>* m_glyphs = nullptr; float m_distanceRange = 0; float m_emSize = 0;
};
// Game.cpp: 原有局部 codepointToMetric 构建后追加 registry.Register(atlasData.texture,
//   atlasData.glyphs, atlasData.distanceRange, kEmSize)；GPUTextSystem 仍 SetAtlasTexture(...,true) 唯一 owner。
// GPUTextSystem.hpp: 新增 [[nodiscard]] Texture2D GetAtlasTexture() const { return m_atlasTexture; }
```

B1 内必须核实：`LoadMetricsBinary` 是否解析 emSize；若 metrics.bin 无该字段，从生成脚本常量引入并单测锁定。

### B2. BuildTemplatesMsdf

```
// LootTextBatcher 新增（位图版保留）：
static void BuildTemplatesMsdf(const std::vector<MSDFGlyphMetric>& glyphs,
                               const MSDFGlyphRegistryView& lookup,   // codepoint→metric 索引
                               const std::string& text, float fontSize, float atlasSize,
                               std::vector<GlyphTemplate>& out);
// scale = fontSize / emSize；
// tpl.offset = {currentX + bearing[0]*scale, bearing[1]*scale}
// tpl.size   = {size[0]*scale, size[1]*scale}
// tpl.uvMin/Max = 直接取 uvRect（MSDF 图集各字形有 margin，不内缩）
// tpl.advanceX = advance*scale + spacing；游标推进同位图版
```

### B3. glyph_msdf.frag（新文件）

```
#version 430 core
uniform sampler2D uFontAtlas; uniform float uScreenPxRange;
in vec2 fragTexCoord; in vec4 fragColor;
// median-of-3 解码：
// vec3 m = texture(uFontAtlas, fragTexCoord).rgb;
// float sd = median(m.r, m.g, m.b);
// float a = clamp((sd - 0.5) * uScreenPxRange + 0.5, 0.0, 1.0);
// finalColor = vec4(fragColor.rgb, fragColor.a * a); a<0.01 discard
```

### B4. 引擎 glyph draw MSDF 模式

```
// GameplayRenderHooks/GameplayRenderFrame: 新增 out-fields
//   bool glyphMsdfEnabled = false; float glyphMsdfPxRange = 1.0f;
// RenderSystem glyph draw（RenderSystem.cpp:679-711）：
//   加载 glyph_msdf.frag（与 glyph.frag 同处初始化，失败→msdf 不可用，日志）
//   if (frame.glyphMsdfEnabled && msdfShader 就绪 && GPUTextSystem::Get().IsInitialized()) {
//     bind GPUTextSystem::Get().GetAtlasTexture().id；SetShaderValue(msdfPxRange)
//   } else { 现有位图路径（frame.font.texture + glyph.frag） }
```

### B5. GameplayRenderAdapter 集成

```
// BuildCpuLootLabels 字形段（现 BuildTemplates+WriteInstances 调用处）：
// if (MSDFAtlasRegistry::Get().IsAvailable()) {
//   失效条件同现状 → BuildTemplatesMsdf(...) → 缓存
//   WriteInstances(..., frame.camera.zoom)   // 阶段 A 吸附，两路径共用
//   frame.glyphMsdfEnabled = true;
//   frame.glyphMsdfPxRange = distanceRange * (fSize * zoom / emSize);  // 每标签一致时整帧取代表值；不一致则逐标签画到同值上限
// } else { 位图路径 + WriteInstances(..., zoom) }
```

pxRange 简化：同帧内 fSize 分物品(24)/金币(20)两档，取每标签实际值写入按标签分组提交；若实现复杂，允许取较大档位值（视觉偏差 ≤ 字号比 24/20，验收手测确认）。

## 3. 原子任务拆分

依赖：A1 → B2（复用吸附）；B1 → B2/B5；B3/B4 → B5；B6 最后。A1 与 B1 文件不相交，可并行。

- `[x] A1: 逐字形整像素吸附 + 半纹素 UV 内缩`
  - LootTextBatcher：WriteInstances 加 zoom 吸附（位置+尺寸）；BuildTemplates UV 内缩 0.5/texSize。
  - GameplayRenderAdapter：调用点传 frame.camera.zoom；fSize 计算后量化到整数 scaleFactor。
  - 单测 tests/unit/LootTextBatcherTests.cpp：吸附函数（含 zoom=1.5/2.0）、UV 内缩断言。
- `[x] A2: 字体图集升级 spike（只测量）`
  - 测量 24px 图集纹理尺寸；48px 重光栅化面积/内存估算；写决策结论（大概率关闭，由 B 取代）。
  - **A2 结论（2026-08-15，spike 实测）**：UISystem 全局字体 simhei.ttf（codepoint 集 21395 个，24px，padding=4，raylib basic packing）实测图集 **8192×4096**（128MB RGBA8，打包 21133/21395 字形）；48px 重光栅化实测 **16384×8192**（512MB RGBA8）。**2048² 放不下**（48px 需 16384×8192，是 2048² 面积的 16 倍），内存开销 512MB 不可接受。**结论：位图升级不可行，由阶段 B（MSDF）取代后关闭**——MSDF 以 24px 量级图集承载距离场，任意缩放清晰且内存远小于位图。
- `[x] B1: MSDFAtlasRegistry + GPUTextSystem 访问器 + Game.cpp 注册`
  - 新类 engine/render/resource/MSDFAtlasRegistry.hpp/.cpp；Game.cpp bootstrap 注册；GPUTextSystem::GetAtlasTexture()。
  - 核实 emSize 来源（metrics.bin 或生成脚本常量）；单测 tests/unit/MSDFAtlasRegistryTests.cpp（命中/未命中/Clear/重复注册）。
- `[x] B2: LootTextBatcher::BuildTemplatesMsdf`
  - 位图版保留；单测：合成 MSDFGlyphMetric 集（含 '0' 0.5em advance 校验）断言 offset/size/uv/advance 与 scale。
  - **B2 完成（2026-08-15）**：`BuildTemplatesMsdf` 已实现（scale=fontSize/emSize；bearing(left,bottom)*scale 定位；uv 直接取 uvRect 不内缩；advanceX=advance*scale+1.0f；未命中游标 += fontSize*0.5+1.0f；IsAvailable()==false 时 return 空，调用方回退）。单测新增 4 用例（`[Unit] LootText - MSDF templates ...`）：度量换算（"A0" fontSize=2*emSize，手算 offset/size/uv/advanceX 一致，含 '0' 0.5em 校验）、未注册输出为空、未命中码点跳过+游标推进、WriteInstances zoom 网格衔接。`bin\NoMoreDayTests.exe --test-case="[Unit] LootText*"` 8 cases/224 assertions 全绿；Registry 用例后恢复 Clear，MSDFAtlasRegistry 6 cases 独立全绿。
- `[x] B3: glyph_msdf.frag 新增`
  - assets/shaders/ui/glyph_msdf.frag（median 解码 + uScreenPxRange）。
  - **B3 完成（2026-08-15）**：新文件 assets/shaders/ui/glyph_msdf.frag 已创建。接口与 glyph.vert/glyph.frag 一致（in fragTexCoord/fragColor、uniform sampler2D uFontAtlas、uniform float uScreenPxRange、out finalColor）；median3 解码 MSD，alpha=clamp((sd-0.5)*uScreenPxRange+0.5,0,1)，a<0.01 discard。GL 编译验证归 B4（引擎加载路径）；本任务仅语法一致性检查（无 glslangValidator 环境）。
- `[x] B4: 引擎 glyph draw MSDF 模式 + 回退`
  - GameplayRenderHooks/RenderFrame 新增 out-fields；RenderSystem.cpp:679-711 分流绑定；shader 加载失败回退位图。
  - **B4 完成（2026-08-15）**：GameplayRenderFrame 增 out-fields `glyphMsdfEnabled`/`glyphMsdfPxRange`（沿 frame.font 回传模式，ExecuteUIWorldPass 处 `frame.glyphMsdfEnabled = hooksFrame.glyphMsdfEnabled`）；RenderSystem 新增 s_glyphMsdfShader/s_glyphMsdfMvpLoc/s_glyphMsdfTexLoc/s_glyphMsdfPxRangeLoc（加载 glyph.vert+glyph_msdf.frag，失败 LOG_WARN 一次且 id==0）；glyph draw 三路分支（MSDF ready → 绑定 GPUTextSystem 图集 + pxRange uniform；glyphMsdfEnabled 但引擎资源不可用 → 跳过 draw + 一次性日志（防 MSDF UV 错绑定位图图集）；否则位图原路径）。blend/depth/vao/instanced 计数共用不动。
- `[x] B5: GameplayRenderAdapter 集成 MSDF 路径`
  - BuildCpuLootLabels 字形段分流；金币路径同覆盖；MSDF 不可用→位图。
  - **B5 完成（2026-08-15）**：字形段按 `MSDFAtlasRegistry::IsAvailable()` 分流 BuildTemplatesMsdf/BuildTemplates；LabelCacheComponent 增 `lastUsedMsdf` 把模板来源纳入失效（位图/MSDF UV 不可互换）；WriteInstances 保持 zoom 吸附；循环后发布 out-fields：`glyphMsdfPxRange = distanceRange * (maxFSize * zoom / emSize)`（代表值 = 本帧最大字号，物品 24 档 > 金币 20 档；已知偏差：金币/小号标签边缘粗细略异，属 B6 手测范围）；IsAvailable()==false 时 glyphMsdfEnabled=false，位图路径与改造前一致。
- `[x] B6: 回归 + 手测验收`
  - build.bat + analyze + ctest unit/integration/ci；手测矩阵：zoom 1.2/1.5/2.0 × filter scale 1.0/1.5 × 中文名/金币/悬停；MSDF 回退验证（临时禁用图集）。
  - **B6 回归完成（2026-08-15）**：build.bat 全量 RelWithDebInfo EXIT=0（Legacy Gate / ABI 治理 / 资产校验 PASS）；`[Unit] LootText*,[Unit] MSDFAtlasRegistry*` 14 cases/256 assertions 全绿；ctest ci 1/1、integration 6/6 通过；全量 1174 cases 无新增失败（performance 13 个既有失败为无 GL 基线）。审查两轮：初审「修改」（H-01 回退偏差/H-02 混合来源错 UV/H-03 归属），H-01/H-02 已修复（glyphMsdfEngineReady in-field 同步、缺字形标签跳过），H-03 定案归属 2026-08-14 既有未提交修复；复审定案「提交」（docs/reviews/2026-08-15-loot-label-crispness-msdf-review.md）。**手测矩阵（视觉验收）待用户在本机执行**——无 GL 环境无法自动验证 glyph_msdf.frag 运行时编译与画面效果。

## 4. 测试方法

| 层级 | 用例 | 落点 | 命令 |
| --- | --- | --- | --- |
| unit | 吸附（zoom 网格）、UV 内缩 | tests/unit/LootTextBatcherTests.cpp | `ctest -C RelWithDebInfo -L unit` |
| unit | Registry 命中/未命中/Clear | tests/unit/MSDFAtlasRegistryTests.cpp | 同上 |
| unit | BuildTemplatesMsdf 度量换算 | tests/unit/LootTextBatcherTests.cpp | 同上 |
| integration | RenderGraph/instanced draw 回归 | tests/integration/* | `ctest -C RelWithDebInfo -L integration` |
| functional/manual | B6 手测矩阵 | 运行 build.bat 产物 | 见 §5 |

新增测试经 tests/CMakeLists.txt GLOB 自动纳入。

## 5. 验证任务完成（完成定义 / 退出标准）

- **A1**：单测绿；`LootTextBatcher` 位图路径行为等价（同输入不同 zoom 输出仍像素对齐）；手测 zoom 1.3/1.5/1.7 文字边缘无软边（截图对照）。
- **A2**：决策结论记录进本计划（任务勾选/关闭）。
- **B1**：单测绿；emSize 来源有证据（脚本常量或 bin 字段），数字字形 advance 校验 ≤0.6em；Game.cpp 重构后 GPUTextSystem 初始化行为不变（log 无新增告警）。
- **B2**：单测绿；位图 API 零回归。
- **B3/B4**：shader 编译通过（启动日志无 fallback 告警）；位图回退路径手测可触发（临时屏蔽 registry）。
- **B5**：MSDF 路径中文物品名/金币/悬停边框正常；LootFilter scale 1.5 清晰。
- **B6**：`build.bat` + `build.bat analyze` + `ctest -C RelWithDebInfo -L unit|integration|ci` 全绿；手测矩阵截图留档；`git diff --check` 干净。
- **整体**：不新增纹理资产/binding/pass；settings.json 与 tier 开关不变；性能不劣化（标签收集 ScopedTimer 不告警，MSDF 路径无 per-frame 字符串解析）。

## 6. 风险与回退

- 引擎 glyph shader 切换失败 → 位图回退（B4 内置）；registry 空 → 位图回退（B5 判断）。
- pxRange 取代表值若产生视觉粗细差 → B5 按标签分组提交（备选实现）。
- MSDF 图集所有权：Registry 只读引用、GPUTextSystem 唯一 owner；B1 单测锁定二次 Register 不转移所有权。
- 设计缺口时暂停回设计流程更新（planning.md 纪律）。
