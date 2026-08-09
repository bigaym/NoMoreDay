# 掉落装备标签渲染修复设计（Loot Label Rendering Fix）

> **Status:** proposed
>
> **Purpose:** 修复掉落装备/金币标签的两类缺陷：① 世界内标签出现"仅左上角完整、其余区域多数不显示"的偏置（用户 BUG-20260809 上报）；② CPU 标签收集/字形查找的每帧线性开销。修复不改动 GPU 侧 3 个 instanced draw 的渲染结构，不引入新字体资产。
>
> **Primary evidence:**
> - 用户上报（2026-08-09）："装备渲染的视锥剔除似乎有很大问题，有时候装备标签不显示在屏幕，只在地图上显示了装备占位的紫色图标……左上角的显示比较完全，其余部分则多数时候不显示。"
> - 代码定位：`GameplayRenderAdapter::ExecuteUIWorldPass`（src/game/render/GameplayRenderAdapter.cpp:569-819）、`SIMDSpatialGrid::queryImpl`（src/engine/physics/SIMDSpatialGrid.hpp:153-212）、`LootTextBatcher::BatchString`（src/engine/render/LootTextBatcher.cpp:6-68）、raylib `GetGlyphIndex`（third_party/raylib/src/rtext.c:1339-1365）。
> - 历史 Track：`v4_gpu_loot_rendering_20260219`（已归档，GPU 路径），`v4_gpu_text_rendering_20260219`（活跃，GPU 文字）。

## 1. 决策摘要

运行期默认走 **CPU 标签路径**（`settings.json` 中 `render.gpuLoot.enabled=false`，`QualityTierManager::UpdateConfigForTier` override 强制关闭，见 QualityTierManager.cpp:1715-1723）。当前"左上角完整、其余缺失"的根因是 **CPU 标签预算在空间遍历过程中被按顺序消耗**：

1. `ExecuteUIWorldPass` 通过 `UiShared::s_itemGrid->query(玩家位置, 1000, callback)` 收集可见掉落（GameplayRenderAdapter.cpp:648-651）。
2. `SIMDSpatialGrid::queryImpl` 按 `gy` 外层、`gx` 内层、从 `(minGx,minGy)` 到 `(maxGx,maxGy)` 的**行主序遍历**（SIMDSpatialGrid.hpp:176-177）——等价于从屏幕左上到右下。
3. 回调在 `labelCount >= 64` 时 `return false`（GameplayRenderAdapter.cpp:652-654），**立即终止整棵查询**。屏幕左上区域先被遍历，先消耗 64 个标签预算；预算耗尽后右下区域的掉落永远不会被检查到 → 精确复现"仅左上角显示"。

次要缺陷：每帧对每个可见标签逐字符调用 `GetGlyphIndex`（raylib 线性扫描，N≈21,000 字形），64 标签 × ~12 字符 ≈ 每帧 16M 次比较（`LootTextBatcher::BatchString` + `GetGlyphIndex`）。

**修复三件事**：
- **A. 收集后按优先级分配预算**：查询回调不再在 64 处中止整棵查询；先收集全部视口内候选（带安全上限），按优先级（强调/稀有度/距离）排序后再应用 64/32/48 预算规则。**这是对用户两个怀疑的结论：根因是显示策略的实现缺陷（预算作用于空间有序流 + 整树中止），而非 GPU 视锥剔除本身**（GPU 路径剔除已验证正确）。
- **B. 字形索引缓存**：一次性构建 `codepoint → glyphIndex` 哈希表，把逐字符 O(21k) 线性扫描降为 O(1)（不新增字体资产；全局中文字体已含全部 CJK 字形）。
- **C. 标签字形模板缓存**：在 `LabelCacheComponent` 缓存与屏幕位置无关的字形模板（uv/size/offset/advance），每帧仅做"模板 + 标签位置偏移"，不做字符串解析与字形查找。

执行序：A → B → C，每步独立可交付、可验证。

## 2. 目标、非目标与硬约束

### 2.1 目标

1. 世界内掉落标签在任意屏幕区域一致显示，消除"左上角完整、其余缺失"偏置。
2. 标签预算规则保留原意图（同屏 64 上限、非 Rare 数量节制、金币数量节制），但作用于**按重要性排序**的候选流而非空间遍历流。
3. 每帧标签收集的 CPU 成本从"逐字符 O(21k) 字形扫描"降到"O(可见标签 × 字符数) 的偏移拷贝"。
4. GPU 侧渲染保持 3 个 instanced draw（beams/labels/glyphs）不变；不改变视觉顺序与质量策略。
5. 不引入新字体资产、新纹理、新 SSBO binding。

### 2.2 非目标

- 不修复 GPU loot 路径（`GPULootSystem`）本身——其剔除已验证正确（Phase 4-6）；仅确保启用 `gpuLoot` 时本设计不与其冲突（`ExecuteUIWorldPass` 在 `gpuLootEnabled` 时提前返回，两条路径天然互斥）。
- 不把标签渲染迁移到 MSDF/GPU 排版（归属 `v4_gpu_text_rendering_20260219`，独立 Track）。
- 不做标签渐隐/缩放动画、不做稀有度闪烁等视觉增强。
- 不改 minimap/掉落地面图标渲染（非本缺陷范围）。

### 2.3 硬约束

- 保持 `LootTextBatcher` 公共接口签名不变（外部调用点仅 `GameplayRenderAdapter`）。
- `LabelCacheComponent` 的字段兼容现有读写（DropSystem/InventorySystem/FragmentDropSystem 的 emplace/invalidate 点保持不变）。
- `SIMDSpatialGrid` 不修改（其行主序遍历是既定空间查询语义；修复在回调侧完成）。
- 保持 `check_module_boundaries.py` 0 违反；`src/` 禁写 `legacy` 字样。
- 构建配置 `RelWithDebInfo`（AGENTS.md），不用 debug。
- 不改 `settings.json` 的 gpuLoot/gpuText 开关（默认关闭 CPU 路径即当前路径）。

## 3. 已验证基线

### 3.1 运行时路径（已确认）

- `settings.json`（D:/PRJ/NoMoreDay/settings.json）：`render.gpuLoot.enabled=false`、`render.gpuText.enabled=false`、DRS 关闭（`dynamicResolutionEnabled=false`, `renderScale=1.0`）。
- `QualityTierManager::UpdateConfigForTier`（QualityTierManager.cpp:1495-1713）：Ultra 默认开 gpuLoot/gpuText，但 override（:1715-1723）读 `settings.json` 强制为 false。→ 运行期 `frame.gpuLootEnabled=false`，CPU 标签路径激活。
- 引擎侧 `ExecuteUIWorldPass`（RenderSystem.cpp:615-715）：`onUIWorld` 填 label/glyph/beam 缓冲 → 3 次 instanced draw，MVP = `rlGetMatrixModelview() * rlGetMatrixProjection()`（gameplay 内 = `GetCameraMatrix2D(camera) * ortho(0,RT.w,RT.h)`，世界坐标正确）。**引擎侧非缺陷**。

### 3.2 CPU 收集路径（缺陷所在）

- `GameplayRenderAdapter::ExecuteUIWorldPass`（GameplayRenderAdapter.cpp:569-819）：
  - 626-632：`viewRect` = `GetScreenToWorld2D({0,0})` / `({screenW,screenH})` 各 ±100/200 填充。
  - 648-651：`s_itemGrid->query({camera.target.x, camera.target.y}, 1000, callback)`。
  - 652-654：**`if (labelCount >= 64) return false;` ← 整树中止（根因①）**。
  - 655-657：视口外跳过。
  - 664-667：`labelCount > 32 && rarity < Rare && (!filterResult || scale <= 1.0)` → 跳过（预算规则 ①）。
  - 673-675：`LootFilterResultComponent.visible == false` → 跳过。
  - 724：金币 `labelCount > 48 && amount < 100` → 跳过（预算规则 ②）。
  - 762-766：候选按 `pos.y` 降序排序（painter 序）。
  - 768-787：重叠消解，仅查最近 16 个候选，最多 4 次安全迭代。
  - 789-814：构造 `GPULabelInstance` + 字形 `BatchString`。
- `SIMDSpatialGrid::queryImpl`（SIMDSpatialGrid.hpp:153-212）：行主序 `gy` 外层 / `gx` 内层；回调返回 `false` 即整体中止（:205）。

### 3.3 字形成本（缺陷②）

- `GetGlyphIndex`（rtext.c:1339-1365）：`SUPPORT_UNORDERED_CHARSET` 下对全部 `glyphCount`（≈21,000）线性扫描。
- `LootTextBatcher::BatchString`（LootTextBatcher.cpp:6-68）：每帧、每可见标签、每字符调用 `GetCodepointNext` + `GetGlyphIndex` 并构造 `GPUGlyphInstance`；`MeasureText`（:70-94）在缓存失效时同样逐字符。
- 量级：64 标签 × ~12 字符 × 21,000 ≈ 16M 比较/帧（含全角/中文）。

### 3.4 缓存基建（可复用）

- `LabelCacheComponent`（src/game/components/Common.hpp:778-787）：`cachedText[64]`、`cachedSize`、`lastFontSize`、`lastRarityHash`、`isValid` + `Invalidate()`。写入点：DropSystem.cpp:124/140/442/487/517、InventorySystem.cpp:268/308、FragmentDropSystem.cpp:84；读取点：GameplayRenderAdapter.cpp:685/729、UISystem.cpp:590（经 `VisibleItemCache`）。
- `VisibleItemCache`（UiShared.hpp:32-39）：`visibleItems = {entity, worldRect}`，render 域写、ui 域读（UISystem hover）。
- 字体：`UiShared::GlobalFont()` = 全局中文 UI 字体（UISystem.cpp:118-156，含 ASCII + 0x4E00-0x9FFF 全量 CJK + 全角区），`frame.font` 由此填充。

## 4. 设计

### 4.1 A：收集后按优先级分配预算（根因修复）

**改造点：`GameplayRenderAdapter::ExecuteUIWorldPass` 的收集段（648-761）与排序段（762-766）。**

流程改为：

```
1. 查询 s_itemGrid，回调仅做过滤与收集，不再在预算处中止：
     - viewRect 外 → 跳过
     - LootFilterResultComponent.visible == false → 跳过
     - 加入临时候选表（复用 s_candidates，先 clear）
     - 安全上限：收集到 kMaxCollectCandidates（默认 256）即 return false
       （防止高密度场景无限增长；256 远超 64 预算，不引入偏置）
2. 候选排序（新优先级序，替换原仅 pos.y 排序的前置阶段）：
     sort key（降序重要性）：
       a. emphasized（filterResult.scale > 1）优先
       b. 稀有度降序（按 `Rarity` 枚举底层值 `static_cast<uint8_t>` 降序；与既有 `rarity < Rarity::Rare` 阈值判断语义一致。注意枚举实际顺序为 Common < Magic < Rare < Uncommon < Set < Epic < Legendary < Mythic < Ancient，ItemComponent.hpp:71-81）
       c. 与玩家距离升序（pos → m_playerPos 的 distSq）
       d. 金币按 amount 降序
     —— 物品候选与金币候选共用同一排序流（物品凭 (a,b,c)，金币凭 (a,d,c)），
        排序后物品自然先于金币进入预算。
3. 顺序应用预算规则（与原规则一一对应，但作用于排序后的流）：
     labelCount 递增；物品：
       - labelCount >= 64 → 停止
       - labelCount > 32 && rarity < Rare && (!filterResult || scale<=1.0) → 跳过
       金币：
       - labelCount > 48 && amount < 100 → 跳过
       - labelCount >= 64 → 停止
4. 选中候选随后走既有 pos.y 降序排序（painter 序）→ 重叠消解 → 实例构造。
```

**行为差异**：预算总量（64）、非 Rare 节制（32）、金币节制（48/100）三个数字全部保留；变化仅在"预算被谁消费"——从"空间遍历先到者"改为"最重要者"。这就是显示策略的意图修正。

**边界**：`kMaxCollectCandidates=256` 需大于任何预算上限（64）且足够覆盖同屏可见掉落；超过 256 的极端场景（同屏 >256 可见掉落）按优先级取前 256 收集再裁到 64，仍保证高质量标签。

### 4.2 B：字形索引缓存（性能根因）

**改造点：`LootTextBatcher` 内部，新增 `GlyphIndexCache`。**

```
新增 src/engine/render/GlyphCache.hpp/.cpp：
  class GlyphCache {
    // 一次构建：遍历 font.glyphs，填 unordered_map<uint32_t codepoint, int index>
    static const GlyphIndexCache& Get(const Font& font);
    int GetIndex(int codepoint) const;  // 未命中回退到 '?'(63) 的索引
    bool IsValidFor(const Font& font) const; // font.glyphCount/字体变化时重建
  };
```

- 键 = `font.id`（同一全局字体稳定）；缓存生命周期与全局字体一致。
- `LootTextBatcher::BatchString` / `MeasureText` 内的 `GetGlyphIndex(font, codepoint)` 替换为 `GlyphCache::Get(font).GetIndex(codepoint)`。
- 行为完全一致：找不到时回退 `?`（与原 raylib 语义一致，原实现即回退 fallbackIndex）。
- 复杂度：构建 O(N) 一次（~21k）；查询 O(1)。
- 不做 raylib 修改（第三方不动）。

**此步单独即可消除 16M 比较/帧的主体。**

### 4.3 C：标签字形模板缓存（进一步消除逐字符每帧开销）

**改造点：`LabelCacheComponent` 扩展 + `LootTextBatcher` 新增模板 API + `ExecuteUIWorldPass` 使用。**

```
LabelCacheComponent 新增字段（Common.hpp）：
  struct GlyphTemplate {
    Vector2 size;      // 字形绘制尺寸（世界单位，含 scaleFactor）
    Vector2 uvMin, uvMax;
    Vector2 offset;    // glyph.offsetX/Y * scaleFactor（相对文本原点）
    float  advanceX;   // 缩放后前进量
  };
  std::vector<GlyphTemplate> glyphTemplates;   // 失效时重建
  std::vector<GPUGlyphInstance> cachedGlyphs;  // 每帧复用（仅偏移 position）

LootTextBatcher 新增（保持既有接口不变）：
  static void BuildTemplates(const Font&, const char* text, float fontSize,
                             std::vector<GlyphTemplate>& out);
  static void WriteInstances(const std::vector<GlyphTemplate>&,
                             const std::vector<GPUGlyphInstance>&,
                             Vector2 origin, uint32_t color,
                             std::vector<GPUGlyphInstance>& outBuffer);
```

- **失效条件与现有文本/尺寸缓存一致**：`!isValid || lastFontSize != fSize || lastRarityHash != rarity`（物品路径 691-704 已具）；金币路径补齐 `lastFontSize` 跟踪（原金币路径仅 `!isValid` 重建文本，缩放/相机 zoom 变化时缓存尺寸会陈旧，本次一并修正）。
- 每帧：`cachedGlyphs[i].position = cachedGlyphs[i].position (模板基准) + (labelOrigin 增量)`——即模板存相对文本原点的 position，WriteInstances 将全部 glyph 平移到标签实际原点；不做字符串解析、不做字形查找、不做 UV/advance 计算。
- `GPUGlyphInstance`（GPUData.hpp:678-692）无需改结构；position 语义改为"模板相对基准 + 每次平移"，由 WriteInstances 在拷贝进 glyphBuffer 时落成绝对位置。

### 4.4 数据所有权与生命周期

| 数据 | 写方 | 读方 | 生命周期 |
| --- | --- | --- | --- |
| `s_candidates`（候选表） | render 域（ExecuteUIWorldPass） | 同帧内 | 帧内 clear/reuse（现状不变） |
| `LabelCacheComponent.glyphTemplates` | render 域（失效时重建） | render 域 | 随实体；Invalidate() 重建 |
| `LabelCacheComponent.cachedGlyphs` | render 域（WriteInstances 拷贝） | render 域 | 随实体；与模板同失效 |
| `GlyphCache` | LootTextBatcher 内部 | LootTextBatcher | 随全局字体；`IsValidFor` 校验重建 |
| `VisibleItemCache.visibleItems` | render 域 | ui 域（UISystem hover） | 现状不变 |

- 组件新增字段不改既有 emplace/invalidate 调用点；`NLOHMANN_DEFINE` 序列化不涉及（LabelCacheComponent 未注册序列化）。

### 4.5 回退路径

- 若 `GlyphCache` 构建失败（字体未加载），`BatchString` 回退到 raylib `GetGlyphIndex`（行为不变，仅性能退化）。
- 若候选收集异常（查询为空），沿现状路径返回（无标签绘制）。
- GPU loot 启用时（`gpuLootEnabled=true`），`ExecuteUIWorldPass` 于 606-608 提前返回，A/B/C 全部不生效——两条路径互斥，无冲突。

## 5. 验收标准

### 5.1 功能（手测，`build.bat` 后运行）

1. 城镇地面密集摆放 60+ 掉落（物品 + 金币混排），玩家站中：**全屏各区域（含右下/底部）均有标签**，不再"仅左上角完整"。
2. 同屏掉落 >64 时，标签总数 ≤64；Rare 及以上与强调(scale>1) 物品优先于 Common；金币数量大的优先于小额。
3. 玩家在密集掉落中移动（触发候选重排/重叠消解），无标签跳动、无重叠文本、无渲染 glitch。
4. 中文物品名正常显示（无缺字/乱码），与修复前字形一致。

### 5.2 性能（`ScopedTimer` 证据）

1. `LootTextBatcher::BatchString` 单标签成本：修复后相比修复前至少 20× 提升（字形查找 O(21k)→O(1) + 模板偏移）。
2. `ExecuteUIWorldPass` 的 "Loot Label Collection" ScopedTimer（现阈值 100μs）在 64 标签场景不触发超时告警；修复前需记录基线值作为对照。

### 5.3 回归

1. 现有单测通过：`ctest -C RelWithDebInfo -L unit|integration|ci`（重点 LootFilterTests、QualityTierManagerTest、网格相关）。
2. `build.bat` 构建通过；`build.bat analyze`（静态检查）通过。
3. GPU loot 开关开启（临时改 settings.json）验证两条路径互斥不冲突；随后恢复默认。

### 5.4 证据留存

- 修复前后各截图（左上角对照 + 全屏密集掉落）。
- 性能对照表（修复前后 BatchString 单标签耗时、Loot Label Collection 耗时）。

## 6. 未决问题、风险与取舍

| 项 | 说明 | 风险 | 处置 |
| --- | --- | --- | --- |
| `kMaxCollectCandidates` 取值 | 256 需覆盖同屏可见掉落 + 预算 64 | 低 | 手测密集场景校准；超限仍按优先级 |
| 金币 `lastFontSize` 补齐 | 修正既有陈旧缓存 | 低 | 单测/手测覆盖 |
| GlyphCache 内存 | unordered_map ~21k 条目 | 低 | 与全局字体生命周期一致，一次构建 |
| 优先级排序稳定性 | `std::sort` 不稳定，同级候选次序不定 | 低 | 排序 key 含距离，同级极少；若需稳定用 `std::stable_sort`（计划期定） |
| 用户原始怀疑"视锥剔除" | 结论：GPU 剔除正确，根因在 CPU 预算策略实现 | — | 设计文档明示结论，避免误修 GPU 路径 |

## 7. 验证方式

- 单元：为 `LootTextBatcher`（模板 API）、`GlyphCache`（命中/回退/重建）补测试；`GameplayRenderAdapter` 收集逻辑属渲染适配器，主验证走集成 + 手测。
- 集成：在既有渲染集成测试域内，验证">64 候选时预算分配不依赖遍历序"（注入固定候选集合，断言选中子集与优先级一致）。
- 性能：`ScopedTimer` 基线对照（修复前记录 → 修复后对比）。
- 手测：见 §5.1。

## 8. 下一步

- 本设计评审通过后，进入计划流程（`docs/workflows/planning.md`）产出 `*-plan.md`（含伪代码、分步实施、测试与完成标准）。
- 计划按 A → B → C 分阶段，每阶段独立可交付。
