# 掉落标签清晰度 MSDF 改造实施审查（首次）

**结论：提交**

- 审查日期：2026-08-15
- 审查轮次：首次
- 审查目标：核验「掉落标签清晰度 MSDF 改造」两阶段（A 止损 + B 根治，任务 A1/B1/B2/B3/B4/B5）是否满足设计、计划、代码标准及出口门禁；本审查仅产出报告，不修改生产代码。
- 复审轮次：2026-08-15 复审（定案），见下文「复审结论」与文末「处理记录」。

## 复审结论（2026-08-15 定案）

**最终结论：提交。** H-01/H-02 修复逐点核验合格，H-03 归属定案，无新增阻断项；「修改」依据全部关闭。

- **H-01 修复合格（engine→adapter 可用性回传）**：`glyphMsdfEngineReady` in-field 位于 `font`（GameplayRenderHooks.hpp:69）与 `glyphMsdfEnabled`（:83）之间；`ToHooksFrame()`（RenderSystem.cpp:176-184，全仓唯一 `GameplayRenderFrame{...}` 构造点）聚合初始化字段序与声明序一致，无错位面；`render()` 在聚合前填充 `(s_glyphMsdfShader.id != 0 && GPUTextSystem::Get().IsInitialized())`（RenderSystem.cpp:1186-1188），全部 6 处 `ToHooksFrame()` 调用（RenderSystem.cpp:585/:604/:635/:1194/:1405 及自身）共享同一 `RenderFrameData` 成员状态，in-field 逐帧一致；adapter `msdfAvailable` 唯一定义点（GameplayRenderAdapter.cpp:985-987）同时驱动模板构建分流（:1001-1009）、模板源缓存失效（:998/:1014）与 H-02 跳过分支（:1031）；引擎侧不可用时 adapter 走位图模板 + `glyphMsdfEnabled=false`（位图 else 分支 :1047-1053 保留），绘制行为与改造前一致；draw 分支 `msdfReady`（RenderSystem.cpp:721-723）含 `glyphMsdfEngineReady` + shader 指针/id 三重检查；跳过分支（:746-758）仅在 `glyphMsdfEnabled=true` 且引擎资源缺失时可达，而 `glyphMsdfEnabled` 只能由同帧 `msdfAvailable=true`（隐含 `glyphMsdfEngineReady=true`）产生，故为防御性不可达；日志措辞无 fallback/legacy 字样（Legacy Gate 安全）。
- **H-02 修复合格（标签级跳过）**：`else if (msdfAvailable)` 分支（GameplayRenderAdapter.cpp:1031-1046）——MSDF 模式已定且该标签模板为空（全部码点未命中图集）时不写任何 glyph 实例，背景标签照常绘制（:968-970 先于字形段）；一次性 LOG_WARN 措辞「label has no MSDF glyph coverage in MSDF mode; label glyphs skipped.」无 Legacy Gate 拦截词；绝不混用位图 UV：位图 `BatchString` else 分支（:1047-1053）仅在 `!msdfAvailable`（整帧位图模式）可达，帧级单 shader 不变量保持；`glyphMsdfEnabled` 判定正确：`= glyphMsdfUsedThisFrame`（:1063；函数级局部变量 :793 每帧归零），仅当 `msdfAvailable && cachedGlyphs.size()==glyphTemplates.size()`（实际输出 MSDF 实例，:1025-1029）置真。
- **H-03 归属定案（采信主控）**：视觉常量改动（bgColor Color{14,14,18,255}、borderColor 0.95、cornerRadius 3、padding 5、fSize 24/20、tSize 100/70）及 `assets/shaders/ui/glyph.frag` 掩码修复、`assets/shaders/ui/label_instanced.frag` 抗锯齿重写，归属 2026-08-14 会话既有未提交修复（与历史记忆记录一致），非本轮子代理越权；解除 H-03 作为「修改」依据，该批改动随行提交。
- **验证证据**：`bin\NoMoreDayTests.exe --test-case="[Unit] LootText*,[Unit] MSDFAtlasRegistry*"` → 14/14 用例、256/256 断言全绿；`ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` → 6/6 全绿；构建由主控验证（EXIT=0），本轮未重复全量构建。
- **保留条件**：B6 手测矩阵（headed 会话）与「剩余风险与正向证据」清单维持为后续跟踪项，不构成「提交」障碍。

**复审新增发现（建议级，不阻断）**：

1. H-02 分支注释「or has no cache entry」与事实不符——`get_or_emplace<LabelCacheComponent>`（GameplayRenderAdapter.cpp:859-860）保证同轮循环内字形段 `labelCachePtr` 非空，「无缓存实体丢字形」场景不存在。最小修复：删除该措辞（1 行注释）。
2. 全缺字形标签每帧触发模板重建（`templates.size()==0` 恒真 → 每帧 `BuildTemplatesMsdf` + `BatchString`），单标签级小成本；GB2312 覆盖下罕见，可接受，或后续加 `lastTemplatesEmpty` 短路位。

## 输入与变更文件边界

| 项目 | 证据 |
|---|---|
| 审查流程 | `docs/workflows/review.md`（完整阅读） |
| 设计规格 | `docs/designs/2026-08-15-loot-label-crispness-msdf-design.md`，重点 L9、L24、L32、L42、L58、L62、L71 |
| 实施计划 | `docs/plans/2026-08-15-loot-label-crispness-msdf-plan.md`，重点 §2 伪代码、§5 完成定义、§6 风险 |
| 代码标准 | `conductor/code_standard.md` §2.1、§5.2、§5.3、§6.1、§7.1、§7.2、§8.1；`conductor/code_styleguides/general.md` |
| 提交边界 | 工作区相对 HEAD `b2711c1a` 的全部改动（未 commit），以 `git status --short` 为准 |
| 文件边界 | `git diff --stat`：13 modified + 6 untracked，681 增 136 删。Modified：assets/shaders/ui/glyph.frag、assets/shaders/ui/label_instanced.frag、src/app/Game.cpp、src/engine/render/CMakeLists.txt、GPUTextSystem.hpp、GameplayRenderHooks.hpp、LootTextBatcher.cpp/.hpp、RenderSystem.cpp/.hpp、GameplayRenderAdapter.cpp、Common.hpp、tests/unit/LootTextBatcherTests.cpp。Untracked：glyph_msdf.frag、设计/计划 md、nmd_atlas_spike.obj、MSDFAtlasRegistry.cpp/.hpp、tests/unit/MSDFAtlasRegistryTests.cpp |
| 已检查变更 | `git diff`（全部 13 个 modified 文件）、`git show HEAD`（基线对照）、`git status --short`、图谱调用链检索（WriteInstances/MSDFAtlasRegistry 使用点）、focused doctest、Legacy Gate |

## 审查方法与验证证据

1. 以 `D-PRJ-NoMoreDay` 代码图谱检索定义、调用链和影响面（WriteInstances 调用点仅 `GameplayRenderAdapter.cpp:1013-1016` 与 tests；`MSDFAtlasRegistry` 使用点仅 Game.cpp / adapter / tests）；对图谱不足项用 grep/read/diff 补查。
2. 本轮执行 focused doctest：
   ```text
   bin\NoMoreDayTests.exe --test-case="[Unit] LootText*,[Unit] MSDFAtlasRegistry*"
   14 test cases | 256 assertions | 0 failed
   ```
   覆盖 LootText 8 用例（含新增 4 个 MSDF 模板用例与 OnZoomGrid 吸附用例）+ MSDFAtlasRegistry 6 用例。
3. `python scripts\check_legacy_reintroduction.py` → PASS（baseline 133/31 = current 133/31，无 marker 回归）。
4. 构建由主控验证 `build.bat` EXIT=0（含 Legacy Gate PASS），本轮按指示未重复全量构建。
5. `git show HEAD:src/game/application/render/GameplayRenderAdapter.cpp` 用于核验设计/计划基线描述（发现基线描述失实，见 L-01）。

## 范围对齐与完成度

| 任务 | 审查结果 | 依据 |
|---|---|---|
| A1 逐字形整像素吸附 + 半纹素 UV 内缩 + 整数 scaleFactor | 通过 | `SnapToPixelGrid`（LootTextBatcher.cpp:14-17）zoom<=1e-4 护栏 + round(value*zoom)/zoom；`BuildTemplates` 半纹素内缩（LootTextBatcher.cpp:146-151）；`WriteInstances` 吸附 position/size、UV 从模板覆盖、尺寸不匹配早退（LootTextBatcher.cpp:232-255）；`WriteInstances` 签名变更全部调用点已同步（唯一外部调用点 GameplayRenderAdapter.cpp:1013-1016；BatchString/MeasureText 未动，DamagePopup 等其他调用方不受影响）；整数 scaleFactor 量化（GameplayRenderAdapter.cpp:872-878） |
| B1 MSDFAtlasRegistry + 图集唯一 owner | 通过 | `Register` 拷贝 glyphs 向量（MSDFAtlasRegistry.cpp:5-19）、`m_available` 防护、Game.cpp:136-138 在 `SetAtlasTexture` 前注册且 `Unload` 前拷贝安全；图集加载失败路径 Game.cpp:85-88 早退 → registry 不可用 → adapter 位图路径（设计 L62 满足）；`GetAtlasTexture()`（GPUTextSystem.hpp:49）；CMakeLists.txt 已加源文件。Registry 不接管纹理所有权，符合「GPUTextSystem 唯一 owner」硬约束 |
| B2 BuildTemplatesMsdf | 通过 | LootTextBatcher.cpp:178-230：scale=fontSize/emSize、命中 offset={currentX+bearing*scale}、miss 游标 +=fontSize*0.5+spacing、空格跳模板游标照推、UV 直接取 uvRect 不内缩，与计划 §2 伪代码一致；单测手算值全部正确 |
| B3 glyph_msdf.frag | 通过 | 25 行（任务描述称 34 行，实际 25，记录差异）；median3 解码 + uScreenPxRange clamp + discard；`#version 430 core` 与 glyph.vert 一致；输入接口 fragTexCoord/fragColor 与 glyph.vert 输出匹配。GLSL 运行时编译未被构建验证，归入 B6 手测 |
| B4 RenderSystem 三路分支 | 部分通过（见 H-01） | 独立 uniform loc 落实（GetShaderLocation(msdf.id) 三处 + rlGetLocationUniform(msdf.id,"uScreenPxRange")）；msdfReady = glyphMsdfEnabled && shader.id!=0 && GPUTextSystem::IsInitialized()（RenderSystem.cpp:714-717）；slot 3/texUnit 3 两分支一致（:729-734 与 :760-764）；BlendMode/depth 两分支共用外层状态（pass 尾 :779 统一 rlSetBlendMode(RL_BLEND_ALPHA)）；加载（Initialize）/卸载（Shutdown）/接线（render()）完整；ToHooksFrame 聚合顺序与 GameplayRenderFrame 字段序一致 |
| B5 模板分流 + 缓存失效 + pxRange 发布 | 通过（H-02 例外） | `LabelCacheComponent.lastUsedMsdf`（Common.hpp:207-210）纳入失效且不变式保持：外层 lastFontSize/lastRarityHash 失效清空模板（adapter:879-903），内层重建条件 size()==0 || lastUsedMsdf!=msdfAvailable（adapter:991-993）；glyphMsdfEnabled = msdfAvailable && 尺寸一致；pxRange 代表值 = distanceRange*(maxGlyphFSize*zoom/emSize)（adapter:1036-1050），金币 20px 档相对物品 24px 档的 20/24≈0.83 偏差已记录于计划 §5/§6 与代码注释（仅混合帧的金币标签受影响，纯金币帧代表值正确） |

## 质量与风险评估

- **回退安全性（重点审查项）**：图集不可用（registry 未注册 / 加载失败早退）→ adapter 位图路径，行为与 B5 前一致，安全；图集可用但引擎侧 MSDF shader 未加载 / GPUTextSystem 未初始化 → 实现选择跳过 glyph draw + 一次性日志（RenderSystem.cpp:740-752），而非设计 L71 要求的位图回退——该偏差见 H-01。跳过分支无 BeginShaderMode/无悬挂 GL 状态（EndShaderMode/rlActiveTextureSlot(0) 仅在 drawGlyphs=true 时执行，与状态对称）。
- **模板缓存失效正确性**：判别键 {lastFontSize, lastRarityHash, lastUsedMsdf} 完备；模式切换必清模板，不存在 MSDF/位图 UV 跨模式复用；内层重复写入 lastFontSize（adapter:1008）冗余无害。
- **内存/所有权**：Registry 拷贝一次 ~7537 字形×44B 一次性成本；无裸 new/delete；无跨 mutator EnTT 指针持有（LabelCacheComponent 指针使用与 B5 前模式一致，且无组件增删）。
- **性能**：每帧新增成本为 1 次 unordered_map 查找/标签（Find）+ 既有模板缓存路径；无热路径堆分配新增（glyphBuffer 复用）。
- **合规**：Legacy Gate PASS；无 dynamic_cast/裸线程；日志措辞避开 fallback 词；代码英文、注释英文，符合规范。

## 发现项（按严重度）

### 一般（High）

#### H-01：引擎侧 MSDF 资源不可用时「跳过绘制」违背设计验收标准 L42/L71

- **位置**：`src/engine/render/RenderSystem.cpp:740-752`；设计 `docs/designs/2026-08-15-loot-label-crispness-msdf-design.md` L42（验收：「MSDF 不可用时回退位图路径且无视觉回归」）、L71（「确保 shader 加载失败时回退位图 shader」）。
- **问题**：adapter 依据 registry 可用性启用 MSDF 模板（glyphMsdfEnabled=true），但引擎侧 shader 未加载（id==0）或 GPUTextSystem 未初始化时，整批 glyph draw 被跳过——标签只剩背景框无文字，且持续整个会话。设计要求的是位图回退，实现未满足。跳过而非错画（MSDF UV 配位图图集=错纹素）在技术上是安全选择，且已由主控在交付报告记录为「谨慎偏差」，但设计文档未修订、验收标准未满足。
- **为何是问题**：直接违背设计验收标准；引擎侧 shader 加载失败（GLSL 驱动编译失败等）是运行时可达场景，用户可见功能退化（全部标签文字消失）。设计 L71 的位图回退并非不可实现：正确做法是引擎把 MSDF 资源可用性回传 adapter，adapter 据此选择位图模板 + glyphMsdfEnabled=false，引擎自然走位图分支——完全满足设计 L42/L71。
- **修复建议**（二选一，重审前必须定案）：(1) 推荐：`GameplayRenderFrame` 增 engine→adapter 可用性 in-field（如 `glyphMsdfEngineReady`），`RenderSystem` 在 `ToHooksFrame` 填充（s_glyphMsdfShader.id!=0 && GPUTextSystem::IsInitialized()），adapter:981-982 的 `msdfAvailable` 改为 `IsAvailable() && frame.glyphMsdfEngineReady`；现有跳过分支保留为兜底保险（正常不可达）。约 10-15 行。(2) 修订设计文档 L42/L71 为「跳过+一次性日志」，由主控签字。推荐 (1)。

#### H-02：混合来源实例在 MSDF 帧内以位图 UV 采样 MSDF 图集（错字形）

- **位置**：`src/game/application/render/GameplayRenderAdapter.cpp:1026-1030`（模板为空的 else 分支直写 `BatchString` 位图 UV 实例）、`src/engine/render/RenderSystem.cpp:768-771`（帧级单一 glyphMsdfEnabled，整批同 shader）。
- **问题**：某标签全部字符缺失于 MSDF 图集（GBK 扩展字符等）→ glyphTemplates.size()==0 → else 分支把位图 UV 实例写入共享 glyphBuffer；若同帧其他标签正常 MSDF（glyphMsdfEnabled=true），该标签实例被 MSDF shader 以 MSDF 图集采样 → 错误纹素。相邻场景：标签部分字符缺失 → templates/cachedGlyphs 尺寸不等 → `WriteInstances` 早退 → 整标签字形静默消失（只剩背景）。
- **为何是问题**：帧级单模式 + 共享实例缓冲 + 每标签独立失效判定的架构下，任何「部分标签位图」都会污染整批或静默丢字；当前仅靠 MSDF 图集 GB2312 覆盖常用字的概率保证，属渲染正确性漏洞（低概率触发）。
- **修复建议**：最小方案——msdfAvailable 且模板为空时该标签不写任何 glyph 实例（跳过该标签字形 + 一次性日志，与引擎跳过分支哲学一致），约 5 行；完整方案——收集阶段对全部候选文本做图集覆盖预检，任一标签无法 MSDF 则整帧位图模式。

#### H-03：标签背景视觉参数改动无设计/计划依据（越权改动）

- **位置**：`src/game/application/render/GameplayRenderAdapter.cpp:963-967`（bgColor 由 0.7 半透明黑改为 {14,14,18,255} 不透明、borderColor alpha 0.5→0.95、cornerRadius 4→3、padding 4→5）、`:719/:750`（fSize 18→24 / 16→20）、CollectVisibleItemProxies 内 tSize 80→100 / 60→70。
- **问题**：设计/计划未提及任何标签背景颜色、透明度、圆角、内边距、tSize 变更（grep 设计+计划无 14,14,18 / cornerRadius / 0.95 / tSize 依据）。fSize 24/20 与设计 L9 假定值一致（见 L-01，设计基线描述失实），可视为对齐设计；颜色/圆角/内边距/tSize 属本轮任务清单之外的越权改动。
- **为何是问题**：违反「不修改无关内容」边界原则；视觉档位影响整体观感且未记录，可能覆盖 08-14 或其他工作线的既定视觉设定。
- **修复建议**：主控确认归属——若刻意保留，补设计文档「视觉档位调整」小节并列出参数表；否则回退至 HEAD 值。fSize/tSize 量化对 MSDF 档位有实际意义，建议保留并补设计记录。

### 建议（Low / Best Practice）

#### L-01：设计/计划基线描述与 HEAD 事实不符

- **位置**：`docs/designs/2026-08-15-loot-label-crispness-msdf-design.md:9`、`docs/plans/2026-08-15-loot-label-crispness-msdf-plan.md:15`（「仅矩形原点吸附 L917-932」「fSize=round(24*scale/zoom)」描述为现状）。
- **问题**：`git show HEAD` 证实基线（HEAD）无任何 rect 原点吸附、fSize 基线为 18/16。本轮才新增 rect 吸附（adapter:938-953）。文档基线描述不可信，影响后续对比审计。
- **修复建议**：修订设计 L9 / 计划 L15 基线描述（或标注「08-14 未提交工作树状态」）。

#### L-02：glyph 批次外层 guard 依赖位图 shader 状态

- **位置**：`src/engine/render/RenderSystem.cpp:696-698`（`frame.glyphShader->id != 0` 门禁整个 glyph 块，含 MSDF 路径）。
- **问题**：位图 glyph shader 加载失败时，即使 MSDF 资源全部就绪也不绘制（罕见、与 H-01 反向的耦合）。
- **修复建议**：该 guard 仅作位图分支门禁，MSDF 分支独立判定（msdfReady 已含自身 shader 检查）。

#### L-03：等价性断言放宽后精确等价保证消失

- **位置**：`tests/unit/LootTextBatcherTests.cpp`（原 byte-for-byte 等价用例改为半像素容差断言）。
- **问题**：snap 有意量化使语义变更合理，非作弊弱化；但精确断言消失，无法捕获吸附公式回归（如 round 改 trunc）。
- **修复建议**：补「位置差 == 精确 snap 增量（round 解析值）」断言，把容差收敛到解析值而非半像素区间。

#### L-04：小项合集（Best Practice）

- `MSDFAtlasRegistry::Register` 恒返回 true、无失败路径，bool 返回值无意义，可改 void 或加 [[nodiscard]]（MSDFAtlasRegistry.cpp:5-19）。
- MSDF 模板命中路径的 offset/size/advanceX 仅用于游标推进，渲染几何来自 cachedGlyphs（位图 BatchString 布局）——「MSDF 图集与位图字体同字体同度量」的隐式耦合未在代码注释说明（LootTextBatcher.cpp:178-230、adapter:1005-1007），建议在 BuildTemplatesMsdf 注释补一句，防止未来误用。
- `GameplayRenderAdapter.cpp:18` include 顺序：resource/MSDFAtlasRegistry.hpp 应在 core/QualityTierManager.hpp 之后（字母序）。
- 整数 scaleFactor 量化副作用：round-half-away 使 scale 0.5 → fSize 24（12px 地板被覆盖，低缩放档位失效）、scale 1.5 → 48（较 36 偏 33%），粗粒度档位行为建议在设计记录。

## 最佳实践建议

1. 建立「引擎资源可用性回传」机制（H-01 修复即顺带落地）：GPU 资源在引擎侧、字形布局在 adapter 侧的两段式分流必须由引擎主动发布能力位，避免 adapter 单方面假设引擎状态。
2. 为「帧级单 shader + 共享实例缓冲」的 batch 渲染建立不变量测试：断言同一 glyphBuffer 内所有实例的 UV 来源一致（可通过 adapter 单测模拟「一标签 MSDF 一标签模板为空」场景）。
3. 把 B6 手测矩阵升级为常态化验收门禁（headed 会话执行、结果归档 docs/reports/）。

## 剩余风险与正向证据

- **B6 未执行（计划 §5/§6 待办）**：手测矩阵 zoom 1.2/1.5/2.0 × LootFilter scale 1.0/1.5 × 中文物品名/金币/悬停 + 图集禁用回退，均待 headed 会话；GLSL 运行时编译、pxRange 视觉粗细（金币 20/24≈0.83 偏差）、MSDF 与位图布局视觉一致性均未人工确认。
- 工作树内 `assets/shaders/ui/glyph.frag`（08-14 掩码修复遗留）与 `assets/shaders/ui/label_instanced.frag`（51 行边框 AA 重写）不在本轮任务清单内，归属已定案（2026-08-14 会话既有未提交修复，随行提交，见 H-03）；`nmd_atlas_spike.obj` 为 A2 spike 残留，勿提交。
- 正向证据：focused 测试 14/256 全绿、Legacy Gate PASS、构建 EXIT=0（主控验证）；回退主路径（图集不可用 → 位图）实现正确且有失败早退保护；pxRange 偏差已在计划记录；三路分支 GL 状态（blend/depth/slot3）一致。

## 下一步动作与出口门禁

1. ✅ H-01 已关闭：engine→adapter 可用性回传已实现并复审合格（见「复审结论」与「处理记录」）。
2. ✅ H-02 已关闭：标签级跳过已实现并复审合格（见「复审结论」与「处理记录」）。
3. ✅ H-03 已定案：归属 2026-08-14 会话既有未提交修复，随行提交（见「复审结论」与「处理记录」）。
4. 酌情关闭 L-01/L-02（低成本），L-03/L-04 可随下轮处理。
5. 复跑 `bin\NoMoreDayTests.exe --test-case="[Unit] LootText*,[Unit] MSDFAtlasRegistry*"` + `python scripts\check_legacy_reintroduction.py` + build.bat check。
6. headed 会话执行 B6 手测矩阵并归档结果；确认 glyph.frag/label_instanced.frag 归属与 nmd_atlas_spike.obj 清理。
7. 复审（2026-08-15）已定案：「提交」。

## 处理记录（2026-08-15 复审前修复）

| 条目 | 状态 | 修复说明 |
|---|---|---|
| H-01 | 已修复 | 采用修复建议 (1) engine→adapter 可用性回传：`GameplayRenderFrame` 新增 in-field `glyphMsdfEngineReady`（GameplayRenderHooks.hpp:71-76，位于 `font` 与 `glyphMsdfEnabled` 之间）；`RenderSystem::render()` 在接线点填充 `(s_glyphMsdfShader.id != 0 && GPUTextSystem::Get().IsInitialized())`（RenderSystem.cpp:1186-1188），经 `ToHooksFrame()` 聚合初始化（RenderSystem.cpp:181，字段序与结构体声明一致）随帧下发；adapter 的 `msdfAvailable` 改为 `MSDFAtlasRegistry::IsAvailable() && frame.glyphMsdfEngineReady`（GameplayRenderAdapter.cpp:985-987），引擎侧 MSDF 资源不可用时 adapter 自动走位图模板 + `glyphMsdfEnabled=false`，引擎自然走位图分支，满足设计 L42/L71 位图回退验收。原「跳过绘制」分支保留为防御兜底（RenderSystem.cpp:746-758），理论上不可达，日志措辞标注 defensive/unreachable（无 fallback/legacy 字样，Legacy Gate 安全）。glyph draw 的 `msdfReady` 同步含 `glyphMsdfEngineReady`（RenderSystem.cpp:721-723）。 |
| H-02 | 已修复 | 采用最小方案（标签级跳过）：adapter 字形段新增 `else if (msdfAvailable)` 分支（GameplayRenderAdapter.cpp:1031-1046）——MSDF 模式已定（registry 可用且 `glyphMsdfEngineReady`）而某标签模板为空（全部码点未命中图集）时，跳过该标签的字形实例写入（不产生实例、绝不调用位图 `BuildTemplates`/`BatchString` 填充，避免位图 UV 被 MSDF shader 错采样），一次性 `LOG_WARN` 记录；`glyphMsdfEnabled` 判定（仅当实际输出 MSDF 实例）不受影响；位图模式（新条件为假）行为与改造前一致（原 else 分支保留，GameplayRenderAdapter.cpp:1047-1053）。 |
| H-03 | 已定案 | 主控定案：视觉常量（bgColor Color{14,14,18,255} / borderColor 0.95 / cornerRadius 3 / padding 5 / fSize 24/20 / tSize 100/70）与 glyph.frag 掩码修复、label_instanced.frag 抗锯齿重写归属 2026-08-14 会话既有未提交修复（与历史记忆记录一致），非本轮越权；解除作为「修改」依据。 |
| L-01..L-04 | 未处理 | 低成本项可随下轮处理（本轮范围外）。 |
| 复审定案 | 结论：提交 | H-01/H-02 修复逐点核验合格（字段序一致、唯一定义点、位图行为一致、防御不可达；标签级跳过、一次性日志、绝不混 UV、判定正确）；focused 14/256 全绿；ctest integration 6/6；B6 手测与剩余风险保留为后续跟踪项。 |

复审（2026-08-15）已定案：「提交」。结论行与新增「复审结论」段已同步；B6 手测矩阵与剩余风险清单保留为后续跟踪项。
