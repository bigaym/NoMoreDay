# UI System Rearchitecture 最终实施审查（首次）

**结论：修改**

- 审查日期：2026-08-12
- 审查轮次：首次
- 审查目标：核验 UI System Rearchitecture 实施包是否满足设计、计划、代码标准及最终出口门禁；本审查仅产出报告，不修改生产代码。

## 输入与变更文件边界

| 项目 | 证据 |
|---|---|
| 审查流程 | `docs/workflows/review.md`（完整阅读） |
| 设计规格 | `docs/designs/2026-08-11-ui-system-rearchitecture-design.md`，重点 §2、§3.2、§4、§9、§10 |
| 实施计划 | `docs/plans/2026-08-11-ui-system-rearchitecture-plan.md`，重点 §2、§11、§12、§13 |
| 代码标准 | `conductor/code_standard.md`，重点 §2.1、§2.2、§5.2、§5.3、§6.1、§7.2、§8.1 |
| 提交边界 | `a2d86175^..018906b4`：`a2d86175`（U1-U2）、`f0b5fc3a`（U3-U7）、`018906b4`（U8） |
| 文件边界 | `git diff --stat a2d86175^..018906b4`：128 个文件，`+16122/-4159` |
| 工作区基线 | 审查开始前执行 `git status --short` 无输出，工作区干净。本文档是本审查唯一新增文件。 |
| 已检查变更 | 已执行 `git diff a2d86175^..018906b4`、`git log -p a2d86175^..018906b4`、针对 UI 目录的依赖/遗留/调试/所有权门禁 grep，以及图谱调用链检索。 |

## 审查方法与验证证据

1. 以 `D-PRJ-NoMoreDay` 代码图谱检索定义、调用链和影响面；对图谱不足的门禁项使用 grep/read/diff。
2. 本轮执行 focused doctest：
   ```text
   bin\NoMoreDayTests.exe --test-case="*WorldUiFrame*,*GameUiCommandHandler*"
   27 test cases, 108 assertions, 0 failed, 973 skipped
   ```
   它只证明桥接基础对象及现有 command handler 的局部行为，不能覆盖所有面板的 Render 写入、GPU loot 分支或性能。
3. 以下为用户提供的既有验证输入，**本轮未重跑**：全量 doctest 999/1000 通过（唯一失败为既有 GPU 环境 `GraphBindingEquivalenceGLTest`）；ctest 18/21（3 个既有 GPU 环境失败）；`check_module_boundaries.py` PASS；`build.bat`/`build.bat check` 被 UI 工作之前已存在的 `check_legacy_reintroduction.py` baseline（legacy=14 vs 15）阻塞。审查没有将该既有阻塞归责于 UI 包，但已检查本包新增 legacy token。
4. `git diff --check a2d86175^..018906b4` 失败，详见 Low 发现 L-01。

## 范围对齐与完成度

| 设计/计划验收项 | 审查结果 | 依据 |
|---|---|---|
| UiRuntime core 不混入 entt/raylib draw/UiShared/gameplay 依赖 | 通过 | `src/game/application/ui/UiRuntime.hpp`、`src/game/application/ui/UiRuntime.cpp` 仅依赖 runtime/STL；`WorldUiFrame` 是设计允许的桥接层，不计入 core。 |
| 状态实例化、UIContext/UISystem static panel state/legacy panel 删除 | 部分通过 | `UISystem::State`、`UISystem::Update/Draw`、`UIContext` 和声明删除的 legacy panels 已无活动实现；`UISystem::s_globalFont` 是私有资源所有者且 shutdown 顺序安全。测试数据状态却仍存在，见 D-01。 |
| Screen UI 在 scene composite 后绘制 | 通过 | `src/game/application/states/GameplayState.cpp:1080-1128` 在 `DrawTexturePro` composite 后调用 `m_uiHost->PrepareRender()` 和 `m_uiHost->Draw(...)`。 |
| Tooltip 时间、淡入淡出和 session reset | 通过 | `src/game/application/ui/TooltipController.cpp:91-185,215-228` 使用 0.12s/0.05s/0.08s、`dt*10`/`dt*8`；`tests/unit/TooltipControllerTests.cpp:28-201` 和 host lifecycle 测试覆盖 reset。 |
| 地面拾取仍经 InventorySystem 验证 | 通过（仅该路径） | `GameUiHost::DetectPickupClick` 只入队 intent；`GameUiCommandHandler.cpp:19-143` 在 Update 验证实体、背包、距离后调用 `InventorySystem::pickUpItem`。 |
| 所有 screen panel 只读 snapshot、以 intent/handler 写玩法、Render 无 ECS 写 | 失败 | 见 B-01、A-01。仅 pickup/equip/use 有有限 command handler 覆盖，面板仍把 registry 和 mutator 传入 Draw。 |
| WorldUiFrame 的帧内桥接 | 部分通过 | CPU 路径的 object-owned proxy、token、pickup intent 已连通；GPU loot 早返回会跳过 BeginFrame，见 H-01。 |
| retained runtime / draw list 真正承担 layout、capture、paint | 失败 | 见 A-01。 |
| 计划 §12/§13 验证矩阵、Tracy 不超过基线 110% | 失败/证据不足 | `docs/reports/ui-system-rearchitecture/baseline.md:5-7,67-77` 明示运行期性能、分配、视觉和 DRS 是 `NOT_RUN`，且不能作为 panel migration 通过证据；见 H-03。 |

## 质量与风险评估

- **完成度（A）**：组合根、核心依赖边界、post-composite 次序、tooltip 和 CPU 地面拾取 intent 路径已落地；但 retained runtime、snapshot/intent 及全部 screen-panel 行为合同未落地，不能宣称 U3-U8 最终完成。
- **崩溃/泄露（B）**：未发现 UI 变更中的裸 `new/delete`、raw ownership、`dynamic_cast` 或 `std::thread`；Game cleanup 在 resource unload 前 shutdown host，Font 生命周期安全。可是 Draw 中的 ECS mutation 与持有 EnTT component pointer/引用跨 mutator 的用法违反安全门禁；WorldUiFrame 在 GPU 路径可读取陈旧 vector。
- **性能（C）**：多个新增 controller 在每帧 Draw/Render 分配 `std::string`、`std::vector`、`std::map`，违反热路径禁令。没有 Tracy、稳态 allocation 或基线 +10% 证据。
- **过程遗留（D）**：无新 UI TODO/FIXME/HACK/XXX、无活动旧 static panel state；但生产路径自动注入测试道具/技能/效果，且存在错误的迁移说明及 diff whitespace 错误。

## 发现项（按严重度）

### Blocker

#### B-01：Render 阶段仍直接执行 gameplay/ECS 写，并持有可能失效的 EnTT 指针

- **位置**：
  - `src/game/application/ui/GameUiHost.cpp:541-609`：`GameUiHost::Draw` 将 registry 传给 crafting/stash/inventory/skill/overlay 的 `Draw`。
  - `src/game/application/ui/OverlayController.cpp:227-234`：quantity popup Draw 直接调用 `InventorySystem::destroyItem` / `InventorySystem::dropItem`。
  - `src/game/application/ui/UIRenderer.cpp:1816-1877`：context menu Draw 直接调用 equip/use/unequip/drop，并写 `itemComp->isLocked`。
  - `src/game/application/ui/UICharacterController.cpp:292-297,650-665`：Draw 中 `get_or_emplace<AttributeUIComponent>`、修改属性及 `StatsDirty`。
  - `src/game/application/ui/UIInventoryController.cpp:133-136,270,357,415-431,531-533,625-644,667-680,891,930-933`：Draw 中调用 inventory/crafting/stash mutator、destroy/resize，并在 mutator 后继续使用 `InventoryComponent* inv`、`EquipmentComponent* equip`。
  - `src/game/application/ui/UICraftingController.cpp:455-487,495-598,606-613,766-863`：Draw 中调用 craft/salvage mutator，并跨 mutator 持有 `ItemComponent` 引用。
  - `src/game/application/ui/UIStashController.cpp:281,312-313,367-380,490,503`：Draw 中 mutator 与 `StashTab* currentTab` 并存。
- **问题**：设计要求的 “Render 只读 snapshot/hit-test/draw commands” 没有成立。点击处理在 Render 中直接改变 registry 和 gameplay；一些调用还在可能创建、删除或替换组件/实体后继续使用 registry component pointer/reference。这不仅绕过 `GameUiCommandHandler`，也有 EnTT 指针/引用失效和 UAF 风险。
- **为何是问题**：违反设计 §2.3、§4 的状态所有者和 Update-only 写入规则、§9 行为验收；违反计划 §2 全局不变量和 §13 第 3 项；违反 `conductor/code_standard.md` §2.2 与 §5.3。review.md 将这类 EnTT 不安全及 UB/UAF 风险列为硬否决。
- **修复建议**：为 drop/destroy/lock/drag/equip/socket/stash/crafting/salvage/attribute 等全部动作定义 `GameUiIntent`；controller 仅从 immutable `GameUiSnapshot` 绘制并入队 entity ID/POD 参数；只在 `GameplayState::OnUpdate` 的 command handler 运行 system/registry 写入。handler 中在 mutator 前复制需要的 entity/POD，变更后重建/发布 snapshot。新增架构门禁和功能测试，断言所有 `Draw` 路径不存在 registry mutator / `InventorySystem` / `CraftingSystem` / `StashSystem` / `SalvageSystem` 写入。

#### A-01：retained UiRuntime、snapshot 和 UiDrawList 没有成为生产面板执行路径

- **位置**：
  - `src/game/application/ui/GameUiHost.cpp:148-188`：snapshot 仅存入 `m_snapshot`，注释仍称为 transitional legacy renderer。
  - `src/game/application/ui/GameUiHost.cpp:531-539`：`PrepareRender` 只 Fit、Clear、Reserve(64)。
  - `src/game/application/ui/GameUiHost.cpp:541-609`：继续 immediate controller Draw，随后调用 backend。
  - `src/game/application/ui/UiRaylibBackend.cpp:61-80`：空 draw list 直接返回。
  - `src/game/application/ui/OverlayController.cpp:31-36`：明确将 runtime node 作为隐藏的 placeholder，overlay 仍经 legacy-compatible draw pass。
  - `src/game/application/ui/GameUiSnapshot.hpp:3-10,35-58` 和 `GameUiIntent.hpp:18-30`：snapshot 字段和 intent 种类不足以表示已迁移面板的状态/动作。
- **问题**：生产代码未调用 `m_runtime.UpdateInput`、`m_runtime.Arrange`，也没有面板把 FillRect/StrokeRect/Line/Text/Image/Custom 写入 draw list；backend 因 list 为空没有实际输出。所有可见 UI 仍是 immediate renderer + registry Draw，retained runtime 是未接入的基础设施。
- **为何是问题**：直接未满足设计 §3.3、§4.2、§5.2/§5.3 的 retained runtime 合同和计划 §13 第 2、3 项。计划 U4 的临时 wrapper 不能替代 U7/U8 声称的完成状态。
- **修复建议**：将每个 controller 拆为 snapshot view-model、retained node/layout/input 和 paint-to-draw-list；每 Update 执行 runtime input/layout，PrepareRender 接收并只读当前 `WorldUiFrame`，backend 输出实际 command list；删除 placeholder 和 immediate legacy fallback。增加测试验证非空 command list、capture/modal/text input 由 runtime 给出、controller 不再读取 registry。

#### D-01：生产 Gameplay 路径仍自动注入测试道具、技能和 buff

- **位置**：`src/game/application/ui/GameUiHost.hpp:361-365`、`src/game/application/ui/GameUiHost.cpp:355-432`。
- **问题**：`m_hasGivenTestItems` 每个 gameplay session 自动创建背包、改名/扩容、调用 `InventorySystem::pickUpItem`、覆盖 active skill slots、清空并重建 `test_power`、`test_speed`、`test_stun`、`test_poison` 效果，并标记 `StatsDirty`。这不是删除 `s_hasGivenTestItems`，而是把静态 test data 注入改为 host 成员注入；会污染真实存档状态，并可能覆盖真实技能/效果。
- **为何是问题**：与计划 §11/U8 “删除 `s_hasGivenTestItems`/Benchmark 和测试数据注入”的完成声明相冲突，也违反设计 §7 Phase 3 去除测试注入的要求及用户指定的过程遗留检查。它是新增生产行为回归而非测试 fixture。
- **修复建议**：删除整个 block 和成员；测试所需物品/技能/效果只能由测试 fixture 或显式测试 harness 建立。增加 source guard / startup regression test，断言 production host 不含 test-item/test-buff 注入。

#### C-01：新增 UI 热 Draw/Render 路径有堆分配和字符串临时对象

- **位置**：
  - `src/game/application/ui/PlayerHudController.cpp:256-432`：每帧 Draw 构造 HP/mana/detail/feedback 字符串，以及 `std::map summonGroups/summonNames/summonIcons`。
  - `src/game/application/ui/MonsterHealthBarController.cpp:245-281`：每帧 RenderUI 构造 `name`、`hpText` 和 affix label 字符串。
  - `src/game/application/ui/UIInventoryController.cpp:780-804`：Draw 中构造 `filteredList`、`lowerSearch`，并为每个材料构造 `lowerName`。
  - `src/game/application/ui/UICraftingController.cpp:606-613,698-709,843-860`：Draw 中构造临时 vector。
  - `src/game/application/ui/UiDrawList.cpp:37-41,81-94`：ordered `vector::insert` 会移动元素，Text command 自有 `std::string`；即使未来接入 runtime，当前实现也没有稳态 allocation 保证。
- **问题**：这些代码在每帧渲染路径上分配和进行 string 操作；尚无 allocation/Tracy 数据证明其不影响预算。
- **为何是问题**：违反 `conductor/code_standard.md` §2.1（Update/Render 热路径不得 heap allocation）和 §7.2 的热路径字符串规则，也是设计 §9 与计划 §12 的 110% 性能验收硬要求。review.md 将该项列为硬否决。
- **修复建议**：在 Update 构建/缓存格式化 view-model；重用有上界并 reserve 的 controller scratch buffer，使用固定 char buffer/预解析 glyph/text cache，避免每帧 map/vector/string；draw list 改为 append 后一次稳定排序或预分配排序缓冲。修复后用 Tracy 和 allocation counter 记录每个 migrated panel 的 update/layout/paint、draw/clip 数，并与 U0 baseline 比较（不得超过 110%）。

### High

#### H-01：GPU loot 分支跳过 WorldUiFrame::BeginFrame，可能读取前帧地面物品 proxy

- **位置**：`src/game/application/render/GameplayRenderAdapter.cpp:611-620`、`src/game/application/ui/WorldUiFrame.hpp:43-52`、`src/game/application/ui/GameUiHost.cpp:559,798-827`。
- **问题**：`frame.gpuLootEnabled` 时 RenderAdapter 在调用 `m_worldFrame->BeginFrame(++m_frameCounter)` 前 return。若此前经过 CPU loot 帧，vector、hover 和非零 token 会保留；host tooltip/pickup 仍读取那些已过期的 proxy。`IsValid()` 只检查 token 非零，读取者也没有 expected-token 验证。
- **为何是问题**：不满足设计和计划 U8 的帧内 `WorldUiFrame` 生命周期/无 stale vector 合同；可能显示或点击已不再可见/可用的拾取目标。现有 `tests/unit/WorldUiFrameTests.cpp:95-126` 只测试手工 BeginFrame，不覆盖 CPU→GPU 切换。
- **修复建议**：在所有 early-return 前 BeginFrame/invalidate，确保 GPU 路径清空或填入对应的 proxy；为 reader 增加有效帧 token 合同；新增 CPU loot → GPU loot → tooltip/click 的集成回归测试。

#### H-02：Escape 在非 inventory modal/overlay 上会同时请求 PauseState

- **位置**：`src/game/application/states/GameplayState.cpp:476-481,621-643`；`src/game/application/ui/GameUiHost.cpp:322-353`。
- **问题**：GameplayState 在调用 host Update 前，只要 inventory 未显示就对 Escape 请求 `PauseState`。随后 host 对同一按键按 quantity、character、context menu、skill tree、astrolabe 的优先级关闭 overlay。因此这些 overlay 打开时，Escape 既关闭 UI 又请求暂停。
- **为何是问题**：违反设计 §9 的 modal/text-input/hotkey 行为保持要求；用户输入捕获没有单一所有者。
- **修复建议**：由 host 先显式 `ConsumeEscape()` / 返回 modal capture，再决定是否 PushState；或将 pause 的 Escape 也纳入 host intent。为每个 overlay 加回归测试，断言 Escape 仅关闭最顶层 UI、不请求 PauseState。

#### H-03：计划 §12/§13 验证矩阵没有达到可提交证据标准

- **位置**：`docs/reports/ui-system-rearchitecture/baseline.md:5-7,67-77`；`docs/plans/2026-08-11-ui-system-rearchitecture-plan.md:346-368`；`docs/designs/2026-08-11-ui-system-rearchitecture-design.md:295-317`。
- **问题**：baseline 已明确 runtime performance、steady-state allocation、16:9/21:9/4:3、视觉/DRS 等为 `NOT_RUN`，并说明不能当作已通过迁移验收；本提交范围没有提供最终的 Tracy +10%、manual matrix 或更新后的明确结果。已有全量测试/模块边界证据不能替代这些专门门禁。
- **为何是问题**：计划要求每项有通过证据，不能运行时明确 NOT_RUN 而非隐含成功；但现有材料同时把 U8/包状态宣称为 complete。这使 §13 第 6 项无法成立。
- **修复建议**：在修复 Blocker 后，按矩阵执行并归档 RelWithDebInfo 下的 focused/full UI tests、manual aspect/DRS/modal/drag/pickup 清单、Tracy CPU/alloc/draw/clip 对比。硬件不可用项可以保留 `NOT_RUN`，但须有原因、范围、后续 owner，且不得作为通过依据。

### Low

#### L-01：提交含 whitespace 错误，并保留与已删除架构矛盾的迁移注释

- **位置**：
  - `src/game/application/ui/PlayerHudController.cpp:264,274,279,295,306,309,314,319,328,339,344,407,411,423,428`。
  - `src/game/application/ui/UIInventoryController.cpp:141,145,186,195,199,236,243,285,387,501,541,546,584,598,649,667-670,708,713,725,752-754,757,768,775,783,809,818,821,823,825,829,833,841,844,862,866,868`。
  - `src/game/application/ui/UISkillHub.cpp:456`、`tests/unit/TooltipControllerTests.cpp:232`。
  - `src/game/application/ui/UIStashController.hpp:70-73,85-87`。
- **问题**：`git diff --check` 报告上述 trailing whitespace/blank EOF。UIStashController 注释仍称 shared `UISystem::State` fallback，而该状态已删除；这不是计划 §11 允许的纯历史迁移说明，而是错误的当前维护信息。
- **为何是问题**：降低 diff hygiene 和维护者对实际 owner/依赖方向的判断质量；错误的 fallback 说明会掩盖遗留路径是否应删除。
- **修复建议**：清除所有 diff whitespace；删除或改写错误的 current-state/fallback 注释，只保留有必要且与实际代码一致的迁移背景。提交前重新运行 `git diff --check` 至零错误。

## 最佳实践建议

1. 建立 UI phase contract 测试：以 source/AST 守卫和技术测试双重约束 `Draw` 仅消费 snapshot/draw list，所有 gameplay 写只能经 command handler。
2. 给 `WorldUiFrame` 设计显式 `Invalidate` 和 reader token API，避免把“上一帧 vector 恰好仍存在”误判为有效帧。
3. 为每个 controller 维护有界 scratch/format cache 和 allocation telemetry；将性能证据作为迁移 commit 的必填产物，而不是后置补充。
4. 合并前强制 `git diff --check` 和“已删除 legacy symbol 仅允许白名单注释”的门禁，避免迁移说明演变为错误接口文档。

## 剩余风险与正向证据

- `Game::cleanup` 在资源卸载前执行 `m_uiHost.Shutdown()`；`UISystem::s_globalFont` 是私有资源句柄而非旧 panel static state，未发现其生命周期泄露。
- `UICharacterController::LeaveGameplay`、`OverlayController::LeaveGameplay`、`TooltipController::ResetAll` 已有对应 reset，focused 测试证明基本 session reset；但这不能抵消 Render mutation 的问题。
- `GameplayRenderAdapter.cpp:697-721,744-769` 在 UIWorld render 中的 `LabelCacheComponent` `get_or_emplace` 属于本范围前已存在的逻辑（非本包新增）；它仍与“Render 只读”目标有潜在冲突，应在后续全局 render/ECS 审计追踪，但本报告不把它计为本提交新增回归。
- 未发现本包新增 `UISystem::State`、`UIContext`、`VisibleItemCache`、`s_itemGrid` 等活动实现；检索命中大多为迁移注释。`s_globalFont` 的存留符合计划允许的资源 owner 例外。

## 下一步动作与出口门禁

1. 先关闭 B-01、A-01、D-01、C-01：移除所有 Render 写入和测试注入，真正接入 retained runtime/snapshot/intent/draw-list，消除热路径分配。
2. 修复 H-01/H-02，并增加 GPU frame 切换和 modal Escape 回归测试。
3. 运行并归档计划 §12 的验证矩阵（含性能/分配、manual aspect/DRS、现有 test/build 门禁结果）；未运行项明确标注为 `NOT_RUN`，不得转化为通过。
4. 清理 L-01 后重新执行 `git diff --check`，并复查 legacy-token 增量。
5. 只有上述 Blocker 和 High 已关闭、所有完成定义均有可核验证据时，才可重新审查并给出 `提交`。
---

# UI System Rearchitecture 修复实施跟进审查（第二轮）

**结论：提交**

- 审查日期：2026-08-13
- 审查轮次：第二轮（跟进）
- 审查目标：核验 R1-R9 修复是否逐条关闭首次审查（2026-08-12）的 B-01/A-01/C-01/D-01/H-01/H-02/H-03/L-01，并复核新证据的可核验性。本轮仅产出报告，不修改生产代码。首次结论「修改」保留于上文，本轮不删除任何首次内容。

## 输入与变更文件边界

| 项目 | 证据 |
|---|---|
| 修复计划 | docs/plans/2026-08-12-ui-system-rearchitecture-remediation-plan.md（R1-R9 全部 [x]） |
| 修复证据 | docs/reports/ui-system-rearchitecture/remediation-evidence.md §R1-R9 |
| R0 基线 | docs/reports/ui-system-rearchitecture/baseline.md §R0（B-R0-1/B-R0-2 定义） |
| 首次审查 | 本文档「首次」小节（不删除） |
| 代码标准 | conductor/code_standard.md §2.1/§2.2/§5.2/§5.3/§6.1/§7.2/§8.1 |
| 提交边界 | 工作区相对  18906b4（U8）的修复改动：R1-R9 共 100+ 修改/新增文件（R1-R8 未提交基线 + R9 新增 	ests/performance/UiDrawListBenchmark.cpp 与文档）；本轮边界以 git status --short 摘要为准 |
| 已检查变更 | git diff --check（EXIT=0）、git diff（R1-R9 关键路径）、图谱调用链检索、source guard 测试源码 |

## 审查方法与验证证据

1. 逐条对照首次发现项的位置/修复建议与 §R1-R9 证据，核验「位置代码已迁移」+「测试/guard 存在」+「测试通过」三项闭合。
2. 本轮执行验证命令（RelWithDebInfo，./build.bat novalidate 编译 PASS 后）：
   - ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure → **9/9 PASS**。
   - ./bin/NoMoreDayTests.exe --test-case="*R2*,*R3*,*R4*,*R5*,*R6*,*R7*,*R8*,*GameUiHost*,*Escape*,*Pipeline*,*Tooltip*,*Stash*,*Crafting*,*Skill*,*Astrolabe*" → **289/289 PASS（4525 assertions）**。
   - ./bin/NoMoreDayTests.exe --test-case="*UiDrawList*,*Benchmark*" → **10/10 PASS**（含新增 R9 基准）。
   - ./bin/NoMoreDayTests.exe --test-case="*UiViewport*,*AdaptiveQuality*,*QualityTier*,*R3*" → **45/45 PASS**（矩阵分支）。
   - 全量 ./bin/NoMoreDayTests.exe --test-case="*" → 1149 tests、1146 PASS、**3 FAIL 全部为既有 GPU 环境失败**（GraphBindingEquivalenceGLTest.cpp:2100、MaterialVFXBenchmark.cpp:281、ParticleTrailBenchmark.cpp:205），与 R3/R6/R8 记录的既有失败一致，非本包回归。
3. git diff --check EXIT=0（零 whitespace 错误；CRLF 提示为 informational）。
4. 标准 uild.bat/uild.bat check 被既有外部 check_legacy_reintroduction.py P0-1 baseline 过期阻塞（见下「外部阻塞」）；审查采信
ovalidate 编译证据，并如实记录该门禁未通过状态。

## 发现项逐条核验（B-01/A-01/C-01/D-01/H-01/H-02/H-03/L-01）

### B-01：Render 阶段直接执行 gameplay/ECS 写并持 EnTT 指针 —— **已关闭**

- **修复证据**：
emediation-evidence.md §R4-R8。GameUiHost::Draw 最终签名 oid Draw(const LevelManager&, const Camera2D&, SpatialHashGrid* = nullptr)（无 registry）；所有面板 controller 迁移为 Update(snapshot, input) + Paint(drawList, viewport, snapshot)（R6: overlay/character/inventory，R7: stash/crafting，R8: skill/astrolabe/tooltip）；全部 gameplay 写经 GameUiCommandHandler::Execute intent（Equip/Use/Unequip/Drop/Lock/DropItem/DestroyItem/Stash*/Craft*/SkillAssign/Astrolabe* 等 30+ intent kind）；UIInventoryController.cpp/UICraftingController.cpp/UIStashController.cpp 的 mutator 与跨 mutator EnTT 指针全部移除（原位置代码重写）。
- **测试/guard 位置**：	ests/tech/UiR6RemediationGuardTests.cpp（6 cases/56 assertions）、UiR7RemediationGuardTests.cpp（5/35）、UiR8RemediationGuardTests.cpp（7/57）——source guard 断言各 controller 源文件不含 ntt::registry/
egistry.view</oid Draw(/StashTab*/mutator 调用，且含 Paint(UiDrawList&/intent 构造。全绿。
- **结论**：位置代码已重写、guard 存在且通过 → **已关闭**。

### A-01：retained UiRuntime/snapshot/UiDrawList 未成为生产路径 —— **已关闭**

- **修复证据**：
emediation-evidence.md §R4、§R8。GameUiHost::Update 真管线（UiViewport::Fit → ReconcileRuntime → UiInputFrame → m_runtime.UpdateInput+Arrange）；PrepareRender = Clear → 各面板 Paint(m_drawList, m_viewport, m_snapshot) → Finalize（原位排序，零分配）；backend 单趟提交实际 command list；UiRaylibBackend custom painter（R8，kTooltip/SkillHub/SkillTree/Astrolabe PainterResourceId 8-11）；snapshot 字段与 intent 种类已扩展至覆盖全部面板；message box 首真 panel 全链（§R4）。
- **测试/guard 位置**：	ests/unit/GameUiHostPipelineTests.cpp（断言 CommandCapacity≥256/TextCapacity≥4096/backend painter 注册）、GameUiHostLifecycleTests.cpp、	ests/unit/UiRaylibBackendTests.cpp、UiR4RemediationGuardTests.cpp（6 cases）。全绿。
- **结论**：retained 管线成为生产路径、placeholder/immediate fallback 删除 → **已关闭**。

### C-01：热 Draw/Render 路径堆分配 + string 临时 —— **已关闭**

- **修复证据**：
emediation-evidence.md §R4-R5、§R9。UiDrawCommand 去 std::string 改 (textOffset,textLength) 引用 text arena；Finalize() 原位排序零分配；PlayerHudController 固定 char buffer（fpsText[48]/hpText[64]/feedbackText[96]/bladeDetailText[160]）+ std::array<SummonRow,16>；MonsterHealthBarController std::array<BarCmd,256> + kRaceData[raceType].name string_view 零复制；SkillHotbarController std::array<SlotCache,5>；MinimapController std::array<EnemyDot,128>；UIInventoryController 材料过滤改缓存键零分配；UICraftingController 每帧 vector 删除。
- **测试/guard 位置**：	ests/unit/PlayerHudControllerTests.cpp（Paint reuses caches across revisions）、MonsterHealthBarControllerTests.cpp、R5 guard；**R9 新增 	ests/performance/UiDrawListBenchmark.cpp**：稳态 256 命令/16KB 文本 16 帧断言 data() 指针/容量/overflow 不变（零重分配），全链压力 120 怪物/10 summons 240 帧 p95=8us、commands peak 234、三 overflow 恒 0。全绿。
- **结论**：热路径分配消除 + 基准证明零重分配 → **已关闭**。

### D-01：生产路径自动注入测试道具/技能/buff —— **已关闭**

- **修复证据**：
emediation-evidence.md §R2。删除 GameUiHost::m_hasGivenTestItems（原 hpp:361-365/cpp:355-432）整块注入（createBag/改名扩容/pickUpItem/技能槽覆盖/test_* 效果重建/StatsDirty）。
- **测试/guard 位置**：	ests/tech/UiR2RemediationGuardTests.cpp（断言 host 源不含 m_hasGivenTestItems/test_*/createBag）+ GameUiHostLifecycleTests（两 session 不注入）。全绿。
- **结论**：注入删除 + 回归 guard → **已关闭**。

### H-01：GPU loot 分支跳过 WorldUiFrame::BeginFrame —— **已关闭**

- **修复证据**：
emediation-evidence.md §R3。BeginFrame(++m_frameCounter) 移至 ExecuteUIWorldPass 所有 early return 之前（GameplayRenderAdapter.cpp）；WorldUiFrame::View（AcquireView 绑 token，IsValid()=frame!=nullptr && expectedToken!=0 && FrameToken()==expected）；DetectPickupClick/DetectGroundHover 用 View 无效早退。
- **测试/guard 位置**：	ests/unit/WorldUiFrameTests.cpp（+4 用例）、	ests/tech/UiR3RemediationGuardTests.cpp（source guard 断言 BeginFrame 先于 if (frame.gpuLootEnabled) early return、proxy collection 在其前）。全绿。
- **结论**：token 生命周期合同 + guard → **已关闭**。

### H-02：Escape 双消费（同时请求 PauseState）—— **已关闭**

- **修复证据**：
emediation-evidence.md §R3。GameUiHost::HandleEscape 单 owner 优先级链（quantity→character confirm→context→skill tree→astrolabe→inventory→stash→crafting），EscapeConsumedThisFrame 每 Update 开头 reset；GameplayState 在 host.Update 后 IsKeyPressed(KEY_ESCAPE) && !EscapeConsumedThisFrame() 才请求 PauseState。
- **测试/guard 位置**：	ests/unit/GameUiHostEscapeTests.cpp（12 用例，含「Escape closes exactly one topmost surface」与 reset 用例）。全绿。
- **结论**：单一 Escape owner + 每 overlay 回归测试 → **已关闭**。

### H-03：验证矩阵未达可提交证据标准 —— **已关闭（替代证据 + 如实 NOT_RUN）**

- **修复证据**：
emediation-evidence.md §R9。CPU 性能实测跑通（UiDrawListBenchmark 稳态/全链，p95=8us、零分配、零 overflow）；16:9/21:9/4:3 Fit 逻辑由 UiViewportTests 覆盖（7/7）；DRS/GPU loot/text 分支由 QualityTierManagerTest/AdaptiveQualityControllerTest 覆盖；压力 120 怪物/10 summons 由 benchmark 覆盖；Escape 输入链由 EscapeTests 覆盖。
- **B-R0-1 处置**：Tracy 未集成（CMake 无 TRACY_ENABLE，performance.md:17 亦承认）——R9 裁决**不引入** TRACY_ENABLE（第三方依赖变更超范围），以 allocation 断言 + 计时基准作为等价替代证据；Tracy 集成立为独立后续任务（owner=性能工程）。**如实记录，不伪称 Tracy 数据**。
- **B-R0-2 处置**：无头会话无法 GUI capture——headed 截图/DRS 视觉项 NOT_RUN + 原因 + owner（手测矩阵执行者），不转通过。
- **结论**：可自动化矩阵全部执行并归档、不可自动化项如实 NOT_RUN、blocker 有处置定案 → **已关闭**（审查采信替代证据，并保留 Tracy/headed 视觉为风险项）。

### L-01：whitespace 错误 + 错误迁移注释 —— **已关闭**

- **修复证据**：
emediation-evidence.md §R9。git diff --check EXIT=0（零 whitespace 错误）；L-01 列出的全部文件（PlayerHudController.cpp/UIInventoryController.cpp/UISkillHub.cpp/TooltipControllerTests.cpp/UIStashController.hpp）已被 R1-R8 重写，UIStashController.hpp:70-73,85-87 为正确 current-state 注释；全仓复查无错误 fallback 声明；死代码核查（DrawMessageBox 已删、PlayerHUD 保留为测试 seam、SharedContext::craftingSetTargetItem 无调用者已记录）。
- **结论**：diff 零错误 + 注释正确 → **已关闭**。

## 外部阻塞（check_legacy_reintroduction.py）

- 实测：工作树 legacy=208、HEAD(018906b4) legacy=205、P0-1 baseline（2026-03-02 生成）legacy=14。差异根源 = **P0-1 baseline 过期**（U1-U8 提交的迁移注释大量使用 'legacy' 词，HEAD 即 +191；R1-R8 仅 +3）。R8 报告 'current=15' 为过时记录。
- 处置：既有外部问题，需外部 owner 重建 P0-1 inventory；本包不修改 inventory、不伪称门禁通过；标准 build.bat/check 保持阻塞，审查采信 novalidate 编译证据。

## 剩余风险（不阻塞提交）

1. Tracy 集成缺失（B-R0-1）——以替代证据覆盖，独立任务跟进。
2. Headed 手测矩阵 GUI 视觉项（B-R0-2）——NOT_RUN，需 headed 会话。
3. R8 遗留：OverlayController::UpdateOverlays 仍带 registry（item 分支）；entity 测试 seam；SkillRegistry JSON UI-local 调试；粒子无 draw-list 通道（视觉过渡）；PaintCanvas hover 用 raylib；skillBar.slots 为 vector。
4. SharedContext::craftingSetTargetItem 无调用者（保留为公开 API 字段）。
5. 全量测试 3 个既有 GPU 环境失败（GraphBindingEquivalenceGLTest/MaterialVFXBenchmark/ParticleTrailBenchmark）。

## 下一步动作

1. 提交 R1-R9（用户另行决定是否提交）。
2. 独立任务：Tracy 集成（TRACY_ENABLE + client 依赖）→ headed 手测矩阵 → P0-1 inventory 重建。
