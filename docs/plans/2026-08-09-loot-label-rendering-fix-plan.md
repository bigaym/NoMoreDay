# 掉落装备标签渲染修复 — 实施计划

> **Status:** approved → ready for implementation
>
> **设计依据:** [2026-08-09-loot-label-rendering-fix-design.md](../designs/2026-08-09-loot-label-rendering-fix-design.md)
>
> **范围:** CPU 标签路径（`gpuLootEnabled=false` 默认激活）的三项修复 A/B/C。GPU loot 路径不修改。

## 1. 实施思路/原理

运行期默认路径为 CPU 标签收集（`settings.json` 关 gpuLoot）。缺陷根因（设计 §1）：`GameplayRenderAdapter::ExecuteUIWorldPass` 在 `SIMDSpatialGrid` 行主序遍历回调中，于 `labelCount>=64` 处 `return false` 中止整棵查询，左上区域先耗尽预算，导致右下掉落不显示。次要性能缺陷：每帧逐字符 `GetGlyphIndex` 线性扫描 N≈21k。

三项改动，各独立可交付、可验证：

- **A（功能根因）**：把"边遍历边扣预算"改为"先收集后按优先级分配预算"。查询回调只做视口过滤 + 收集，不再在 64 处中止；收集完毕对候选按 强调 > 稀有度 > 距离 排序，再顺序应用既有预算规则（64/32/48）。预算数字与规则语义完全保留，仅改变"预算被谁消费"。
- **B（性能）**：新增 `GlyphCache`，一次构建 `codepoint→glyphIndex` 哈希，替换 raylib `GetGlyphIndex` 的线性扫描。语义一致（未命中回退 `?`）。
- **C（性能）**：`LabelCacheComponent` 扩展字形模板缓存，每帧只做"模板 + 位置偏移"。

改动文件清单：
| 文件 | 改动 |
| --- | --- |
| `src/game/render/GameplayRenderAdapter.cpp` | A：收集段(648-761)重构、排序段(762-766)重构；C：使用模板 API |
| `src/game/components/Common.hpp` | C：`LabelCacheComponent` 扩展 `glyphTemplates`/`cachedGlyphs` |
| `src/engine/render/LootTextBatcher.hpp/.cpp` | B：接入 GlyphCache；C：新增 `BuildTemplates`/`WriteInstances` |
| `src/engine/render/GlyphCache.hpp/.cpp` | B：新增类 |
| `tests/unit/LootTextBatcherTests.cpp` | B/C：字形缓存 + 模板 API 单测 |
| `tests/unit/GlyphCacheTests.cpp` | B：哈希命中/回退/重建单测 |
| `tests/unit/LootLabelBudgetPriorityTests.cpp` | A：预算分配优先级单测 |

不新增依赖、不改 `SIMDSpatialGrid`、不改 shader、不改 `settings.json`。

## 2. 伪代码引导

### A. 收集 → 优先级预算 → painter 排序 → 重叠消解 → 实例

```
// GameplayRenderAdapter::ExecuteUIWorldPass 收集段（替换 648-761）
labelCount = 0;  // 复用，作为"已选中标签数"
s_candidates.clear();

kMaxCollectCandidates = 256;   // 安全上限：防高密度场景无限增长
collected = 0;

if (UiShared::s_itemGrid) {
  s_itemGrid->query(center=frame.camera.target, radius=1000,
    [&](entity, pos) -> bool {
      if (collected >= kMaxCollectCandidates) return false;  // 仅安全上限中止
      if (!CheckCollisionPointRec({pos.x,pos.y}, viewRect)) return true;
      if (item = try_get<ItemComponent>) {
        if (filterResult && !filterResult->visible) return true;   // 提前过滤（不变）
        ++collected;
        // 计算 rarityColor/scale/emphasized/fSize/cachedSize（沿用 669-704 逻辑，不移除）
        s_candidates.push_back({entity, pos, tSize, rarityColor, scale,
                                item->name, false, bg, /*新增*/ item->rarity,
                                /*distSq*/ (pos-player)^2, 0, emphasized});
        return true;   // ← 不再在 labelCount>=64 处 return false（根因修复）
      }
      if (gold = try_get<GoldComponent>) {
        ++collected;
        s_candidates.push_back({entity, pos, tSize, GOLD, 1.0f, text, true, bg,
                                /*rarity*/ Rarity::Common(占位, 金币不走稀有度),
                                distSq, gold->amount, false});
        return true;
      }
      return true;
    });
}

// 优先级排序（替换 762-766 的仅 pos.y 排序；先按优先级，再 painter 序）
// key: 物品优先级 = (emphasized?, -rarity, distSq) ; 金币 = (amount desc, distSq)
// 排序目标：物品(强调>稀有度>近) 先于 金币(量大>近)，同类内距离近优先
sort(s_candidates, priority_key);

// 顺序应用预算（等价原 652/664/724 的规则，但流已有序）
for cand in s_candidates:
  if labelCount >= 64: break;
  if cand.isGold:
    if labelCount > 48 && cand.amount < 100: continue;
  else:  // item
    if labelCount > 32 && cand.rarity < Rarity::Rare
       && (!filterResult || cand.scale<=1.0): continue;
  ++labelCount;
  selected.push_back(cand);

// painter 序（原 763-766）→ 重叠消解（原 768-787）→ 实例构造（原 789-814）
sort(selected, pos.y desc);
... 其余沿用现状 ...
```

关键不变式：**预算判断从"遍历序"迁移到"优先级序"；中止整树只发生在安全上限 256，远超预算 64，不引入偏置。**

### B. GlyphCache

```
// src/engine/render/GlyphCache.hpp
class GlyphIndexCache {
 public:
  // 一次性构建：遍历 font.glyphs 填哈希；记录 fallback('?') 索引与 glyphCount
  static const GlyphIndexCache& Get(const Font& font);
  int GetIndex(int codepoint) const;          // 未命中 → fallbackIndex
  bool IsValidFor(const Font& font) const;    // glyphCount 变化 → 重建
 private:
  std::unordered_map<uint32_t,int> m_index;
  int m_fallbackIndex; size_t m_glyphCount;
};

// LootTextBatcher::BatchString / MeasureText 内替换：
//   int gi = GlyphIndexCache::Get(font).GetIndex(codepoint);
//   (原 GetGlyphIndex(font, codepoint))
```

语义等价性：raylib `GetGlyphIndex`（rtext.c:1339-1365）在 `SUPPORT_UNORDERED_CHARSET` 下扫描全部 glyph，未命中回退 `?`(63)；哈希版完全复刻，仅在 font 重建（glyphCount 变化）时重建。

### C. 标签字形模板缓存

```
// LabelCacheComponent 扩展（Common.hpp:778-787）
struct GlyphTemplate {
  Vector2 size, uvMin, uvMax;  // 绘制尺寸与 UV（含 scaleFactor）
  Vector2 offset;              // glyph.offsetX/Y * scaleFactor（相对文本原点）
  float  advanceX;
};
struct LabelCacheComponent {
  ... 现有字段不变 ...
  std::vector<GlyphTemplate> glyphTemplates;
  std::vector<GPUGlyphInstance> cachedGlyphs;  // 模板化实例（position=相对原点）
};

// LootTextBatcher 新增（保留既有 BatchString/MeasureText 签名不变）
static void BuildTemplates(const Font&, const char* text, float fontSize,
                           std::vector<GlyphTemplate>& out);
static void WriteInstances(const std::vector<GlyphTemplate>&,
                           const std::vector<GPUGlyphInstance>&,
                           Vector2 origin, uint32_t color,
                           std::vector<GPUGlyphInstance>& outBuffer);

// ExecuteUIWorldPass：
//  失效条件（与现 cachedSize 失效一致）→ 仅在此重建 templates+cachedGlyphs
//    item:  !isValid || lastFontSize!=fSize || lastRarityHash!=rarity
//    gold:  !isValid || lastFontSize!=fSize   // 补齐 lastFontSize 跟踪（原仅 !isValid）
//  每帧：  WriteInstances(templates, cachedGlyphs, {rect.x+4, rect.y+2}, color, *glyphBuffer)
```

金币路径补齐 `lastFontSize`：原代码（735-750）仅 `!isValid` 重建文本、但尺寸判断用 `lastFontSize!=fSize`；模板缓存要求文本与模板同步失效，故失效条件统一为 `!isValid || lastFontSize != fSize`（文本重建仍仅 `!isValid`，与现状一致）。

## 3. 原子任务拆分

依赖序：T1 → T2 → T3 → T4 → T5。B 与 C 依赖 LootTextBatcher，A 依赖候选结构字段扩展（可在 T2 一并完成）。

- `[ ] T1: GlyphCache 实现 + 单测`（B 基础；无依赖，可最先）
  - 新增 `src/engine/render/GlyphCache.hpp/.cpp`；接入 `LootTextBatcher` 的 `GetGlyphIndex` 调用点（`BatchString`/`MeasureText`）。
  - 单测 `tests/unit/GlyphCacheTests.cpp`：构造合成 Font（少量 glyph + `?`），断言命中/未命中回退/`glyphCount` 变化触发重建。
  - 完成定义：`ctest -C RelWithDebInfo -L unit` 通过；构建通过。

- `[~] T2: LootTextBatcher 模板 API + LabelCacheComponent 扩展`（C 基础）
  - 新增 `BuildTemplates`/`WriteInstances`；扩展 `LabelCacheComponent`（`glyphTemplates`/`cachedGlyphs`）。
  - 单测 `tests/unit/LootTextBatcherTests.cpp`：合成 Font 下 `BuildTemplates` 的 UV/size/offset/advance 正确、`WriteInstances` 相对→绝对位置平移正确；`BatchString` 与 `BuildTemplates+WriteInstances` 产出等价（同输入同位置同 buffer）。
  - 完成定义：单测通过；既有调用点签名不变（编译期验证）。

- `[ ] T3: 收集→优先级预算重构（GameplayRenderAdapter.cpp 648-766）`（A 功能根因）
  - `LabelCandidate` 扩展 `rarity`/`distSq`/`amount`/`emphasized` 字段。
  - 收集段移除 64 中止、加 256 安全上限；新增优先级排序 + 预算分配；保留 painter 排序/重叠/实例逻辑。
  - 完成定义：手测全屏掉落一致显示；现有渲染集成测试通过。

- `[ ] T4: 标签字形模板缓存接入 ExecuteUIWorldPass`（C 集成）
  - 收集段失效重建 `glyphTemplates`+`cachedGlyphs`；实例段用 `WriteInstances` 替换 `BatchString`（804-813）。
  - 金币路径失效条件补齐 `lastFontSize`。
  - 完成定义：手测中英文标签无缺字/乱码、无跳动；`ScopedTimer` 对照达标。

- `[ ] T5: A 优先级预算单测 + 性能对照 + 回归全绿`
  - 单测 `tests/unit/LootLabelBudgetPriorityTests.cpp`：构造固定候选集（含 Common/Rare/金币/强调/距离差异），断言"预算子集与优先级一致、与遍历序无关"（注入按左上→右下顺序的输入，验证选中子集不变）。
  - 性能对照：记录修复前后 `BatchString` 单标签耗时 + "Loot Label Collection" ScopedTimer。
  - 回归：`build.bat` + `build.bat analyze` + `ctest -C RelWithDebInfo -L unit|integration|ci`。

## 4. 测试方法

| 层级 | 用例 | 落点 | 命令 |
| --- | --- | --- | --- |
| unit | GlyphCache 命中/回退/重建 | `tests/unit/GlyphCacheTests.cpp` | `ctest -C RelWithDebInfo -L unit` |
| unit | BuildTemplates/WriteInstances 等价性 | `tests/unit/LootTextBatcherTests.cpp` | 同上 |
| unit | 预算分配与遍历序无关 | `tests/unit/LootLabelBudgetPriorityTests.cpp` | 同上 |
| integration | 渲染集成回归（现网） | `tests/integration/*`（MDI/RenderGraph 系列） | `ctest -C RelWithDebInfo -L integration` |
| performance | BatchString 单标签耗时对照 | `tests/performance/`（如新增场景或复用） | `ctest -C RelWithDebInfo -L performance` |
| functional/manual | 城镇 60+ 掉落全屏显示、中文正确、移动重排 | 手测 | 运行 `build.bat` 产物 |

新增测试文件经 `tests/CMakeLists.txt` 的 `GLOB_RECURSE "*.cpp"` 自动纳入（无需改 CMake）。

## 5. 验证任务完成（完成定义 / 退出标准）

- **T1/T2 完成**：单测绿；`LootTextBatcher` 既有 API 签名零变化（A/B 编译期兼容）。
- **T3 完成**：
  - 手测：城镇地面 ≥60 掉落（物品+金币混排），玩家居中 → **四角全屏均有标签**，不再"仅左上角完整"。
  - 同屏 >64 掉落时标签总数 ≤64；Rare+/强调优先；大额金币优先（对照设计 §5.1）。
- **T4 完成**：中文物品名无缺字/乱码；移动时标签无跳动/重叠；金币金额文本随 amount 正确重建。
- **T5 完成**：
  - `ctest -C RelWithDebInfo -L unit|integration|ci` 全绿；`build.bat analyze` 静态检查通过。
  - 性能对照表：修复前后 `BatchString` 单标签耗时（≥20× 提升）与 "Loot Label Collection" ScopedTimer（≤100μs 不告警）留档。
  - `git diff --check` 无空白错误。
- **整体**：不改变 GPU loot 路径与 `settings.json` 默认开关；无新增依赖/资产/binding。

## 6. 风险与回退

- `kMaxCollectCandidates=256` 需手测校准：若极端场景可见掉落 >256，仍按优先级取前 256 收集再裁至 64，标签质量不劣化。
- GlyphCache 构建失败 → 回退 raylib `GetGlyphIndex`（性能退化、行为不变）。
- `std::sort` 不稳定：同级候选次序不定；若手测出现跳动，改 `std::stable_sort`（T3 内决策点）。
- 设计缺口时暂停回设计流程更新（planning.md 纪律）。
