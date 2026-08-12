# UI 系统重构遗留修复设计（UI System Rearchitecture Remediation）

> **Status:** proposed
>
> **Date:** 2026-08-12
>
> **Purpose:** 在不改变既有产品交互、RenderGraph、场景 RT、GPU 资源所有权或 DRS 合同的前提下，关闭 UI System Rearchitecture 首次最终实施审查中的 Blocker、High 和 Low 发现，使 retained UI、snapshot/intent、WorldUiFrame 与性能验证成为实际生产路径而非未接入的基础设施。

## 1. 输入、归属与结论

### 1.1 输入

- 首次实施审查：[2026-08-12-ui-system-rearchitecture-review.md](../reviews/2026-08-12-ui-system-rearchitecture-review.md)。结论为 `修改`，本修复任务逐项关闭 B-01、A-01、D-01、C-01、H-01、H-02、H-03 和 L-01。
- 原始设计：[2026-08-11-ui-system-rearchitecture-design.md](2026-08-11-ui-system-rearchitecture-design.md)。本文件是其遗留修复设计，不改写已发生的 U1-U8 历史实施声明。
- 原始计划：[2026-08-11-ui-system-rearchitecture-plan.md](../plans/2026-08-11-ui-system-rearchitecture-plan.md)。其“完成”叙述不能替代本修复任务的完成证据。
- 基线报告：[ui-system-rearchitecture/baseline.md](../reports/ui-system-rearchitecture/baseline.md)。其中运行期性能、稳态分配、宽高比和 DRS 项均为 `NOT_RUN`，不得作为通过证据。
- 约束：`docs/workflows/design.md`、`docs/workflows/planning.md`、`docs/workflows/testing.md`、`docs/workflows/performance.md`、`conductor/code_standard.md`、`conductor/tech-stack.md`、`conductor/specs/rendering_engine_v5_master_spec.md` 和 `conductor/rendering_system_progress.md`。

### 1.2 归属与核心结论

本任务属于既有 UI System Rearchitecture，不新建、迁移或重命名 Track。修复以现有 `NoMoreDayGameUi`、`GameUiHost`、`UiRuntime`、`UiDrawList`、`UiRaylibBackend`、`GameUiSnapshotBuilder`、`GameUiIntent`、`GameUiCommandHandler` 和 `WorldUiFrame` 为基础，不重新选择 UI 框架。

最终架构结论如下：

1. 所有 screen panel 的生产渲染必须经过 retained node/layout/input、immutable snapshot 和 `UiDrawList`；不保留 immediate renderer 作为常驻兼容回退。
2. 所有会改变 gameplay/ECS 的动作必须由 `GameUiIntent` 表达，并且只在 `GameplayState::OnUpdate` 的 `GameUiCommandHandler` 中执行。
3. `WorldUiFrame` 每个 UIWorldPass 都创建一个新的、可验证的帧视图；CPU/GPU loot 路径均提供同一语义的可拾取/可悬停 proxy。
4. UI Escape 由 host 单一消费；`GameplayState` 只在 host 未消费该按键时进入 `PauseState`。
5. Update/Render 热路径以固定容量、复用缓冲和可观测 overflow 为合同，不得以 heap allocation、临时 `std::string`、每帧容器创建或有序插入掩盖性能问题。
6. 没有自动化、性能和手测证据时，本任务不得标记为完成，也不得在后续审查中声称 `提交`。

## 2. 目标、体验与边界

### 2.1 目标

1. 保持 HUD、inventory、stash、crafting、character、skill、astrolabe、tooltip、context menu、quantity popup、drag 和地面拾取的既有用户可见语义；允许由显式队列引入最多一个 Update 帧的玩法动作延迟。
2. 将所有 screen panel 的数据输入收敛到只读 `GameUiSnapshot`，将 gameplay 请求收敛到纯 POD `GameUiIntent`。
3. 使 `UiRuntime::UpdateInput`、布局、focus/modal/capture 和 `UiDrawList` 成为生产调用链，并让 raylib 调用只留在 `UiRaylibBackend` 或其注册的 backend custom painter 中。
4. 删除生产 Gameplay session 的测试背包、技能和 buff 注入，避免污染真实会话和存档状态。
5. 修复 GPU loot 切换后的 stale WorldUiFrame，修复 Escape 双消费，并补齐可复现的性能与行为证据。

### 2.2 非目标

- 不重做 UI 视觉风格、快捷键产品设计、资产格式、本地化方案、存档格式或 panel 布局持久化。
- 不修改 `RenderGraph`、scene RT、DRS 算法、shader、GPU resource ownership、Engine 到 Game UI 的依赖方向，或当前 P0 GPU 生产整改 Track 的资源合同。
- 不引入第三方 UI 框架，也不把 Game UI 类型暴露给 Engine。
- 不将审查提到的、早于本 UI 包存在的 `GameplayRenderAdapter` 中 `LabelCacheComponent` Render 写入纳入本任务；新增加的 WorldUiFrame proxy 提取必须只读。
- 不自动清理已经保存的疑似测试物品、技能或 buff。按名称删除会误删真实数据；如需存档迁移，须另立设计。
- 不以长期双渲染路径、静态全局状态、生产 dummy data 或测试专用生产分支作为过渡方案。每个已迁移 panel 切片完成时即删除该 panel 的旧生产路径。

## 3. 修复后的运行时合同

### 3.1 Update、Render 与数据所有权

| 阶段 | 所有者 | 允许读取 | 允许写入 | 禁止项 |
| --- | --- | --- | --- | --- |
| 平台输入采样 | `GameplayState`/`GameUiHost` | 平台一次性输入、viewport | `UiFrameInput` | 多个系统独立轮询同一按键作为业务决定 |
| UI Update | `GameUiHost`、`UiRuntime`、panel controller | `GameUiSnapshot`、UI session state | retained nodes、layout、focus/capture、UI-local state、下一帧 intent queue | `entt::registry`、gameplay system、raylib draw |
| Intent 执行 | `GameUiCommandHandler` | intent、registry、domain system | gameplay/ECS、`GameUiResult` | Render 调用、跨 mutator 保留 component pointer/reference |
| World UI bridge | `GameplayRenderAdapter` | render frame、只读 world/item data | 当帧 `WorldUiFrame` proxy/buffer | Engine 持有 Game UI 类型、GPU 分支跳过帧重置 |
| UI Render | `GameUiHost`、`UiRaylibBackend` | immutable snapshot、valid `WorldUiFrameView`、`UiDrawList` | draw-list frame buffers、backend graphics state | registry/gameplay 写、controller 直接 raylib draw |

稳定的帧顺序为：

```text
OnUpdate:
  1. 执行上一个已完成 UI phase 入队的 intents，并发布 result。
  2. GameUiSnapshotBuilder 在所有这些写入后构建 snapshot revision N。
  3. GameUiHost 接收一次 UiFrameInput + snapshot N：reconcile、UpdateInput、Arrange，
     panel 只更新 UI-local state，并为下一次 Update 入队 intent。
  4. Host 给出 UiInputCapture；GameplayState 在 UI Escape 未消费时才处理 Pause，
     InputSystem 只处理未被 capture 的玩法输入。

OnRender:
  1. UIWorldPass 无条件打开并填充当前 WorldUiFrame。
  2. scene RT 合成到默认 framebuffer。
  3. GameUiHost::PrepareRender(valid WorldUiFrameView) 只 paint draw list；
     world hit 产生的 intent 入队到下一 Update。
  4. UiRaylibBackend 在 native framebuffer 提交 draw list。
```

该顺序把 UI 事件明确为最多一个 Update 帧延迟，避免在 Render 中写 ECS，也避免为呈现刚执行的 gameplay 写而在一个 Update 内重复构建 snapshot。成功/失败结果在下一 snapshot revision 中以通知或已更新 view model 反映。

### 3.2 `GameUiSnapshot` 合同

`GameUiSnapshot` 是 Game/application 边界的 immutable、frame-scoped view model。它含 revision，并以 panel 所需数据而非 registry 形态表达：

- player/HUD：数值、hotbar、buff、召唤和怪物血条所需的已解析显示数据；
- inventory/character：背包、equipment、bag、item display、属性和可分配点；
- stash/crafting：tab、slot、材料、recipe、affix、salvage 与可用操作状态；
- skill hub/tree/astrolabe/minimap：显示、选择、资源和画布所需的稳定 domain ID 与已解析数据；
- overlay/tooltip：当前 UI-local target 对应的只读展示数据和上一 Update 的 `GameUiResult` 通知；
- pickup：仅 Update 所需的 gameplay 可验证数据。Render 产生的可见/命中数据仍由 `WorldUiFrameView` 提供，不复制入 snapshot。

snapshot 不含 `entt::registry`、`entt::entity`、component pointer/reference、raylib 类型或可写 gameplay 对象。实体标识在 application 边界用稳定整数 domain ID 表示。动态文本优先传数值、文本/资源 ID 或预格式化的有界缓存；panel 不在 Draw 中从 component 读取或拼接字符串。

panel 的 open/tab/search/drag/scroll/hover/modal 等纯 UI 会话状态由对应 controller 和 `UiRuntime` 实例拥有。它们不进入存档，也不以 snapshot 反向写回 gameplay。

### 3.3 `GameUiIntent`、命令处理与 EnTT 安全

`GameUiIntent` 保持无 EnTT/raylib/gameplay header 的纯 POD 边界。单一 enum + tagged POD payload 足以覆盖操作，避免为每个按钮建立类型层级。payload 只包含稳定 domain ID、数量、源/目标 slot、equipment slot、stash tab/slot、affix index、枚举和布尔标志，不保存 registry 地址、component 指针/reference 或跨帧 `entt::entity`。

| 域 | 必须可表达的 gameplay intent |
| --- | --- |
| ground item | pickup |
| inventory/equipment | equip、unequip、use、drop、destroy、lock/unlock、organize、move/swap、bag 操作、socket/unsocket |
| stash | transfer、deposit、withdraw、unlock tab、sort、auto-deposit |
| crafting/salvage | affix upgrade/chaos/refine/add、fuse、salvage/batch salvage |
| character | confirm attribute allocation |

选择 crafting target、开关 panel、编辑 quantity 文本、搜索、拖拽预览和关闭 overlay 是 UI-local 行为，不创建 gameplay intent。

`GameUiCommandHandler` 是唯一公共执行入口，可按 inventory、stash、crafting、attribute 域拆私有执行器。其规则为：

1. 每次执行重新解析 player、目标实体和源/目标位置，校验有效性、权限、距离、容量、slot/tab/index 和 domain 前置条件。
2. 委托现有 gameplay system。若现有系统没有 lock 或属性确认等权威操作，新增最小的 system-owned operation；handler 不复制 gameplay 规则到 UI。
3. 在可能创建/销毁实体或增删组件的 mutator 前拷贝所需小 POD；调用后立即丢弃 component pointer/reference，需要后续数据时重新获取。
4. 对 drop/destroy/socket/salvage/fuse 等破坏性成功结果，清除 UI drag/crafting session 中的相应 ID，并以 `GameUiResult` 发布成功/失败通知。
5. FIFO 执行 intents。一个 intent 失败不得阻断后续独立 intent，但必须提供可观察的失败结果。

### 3.4 Retained panel、layout 与 draw list

每个可见 panel 必须完成同一转换：`snapshot view model -> retained node tree/layout -> runtime hit/focus/capture -> draw-list paint -> backend submit`。controller 的 render-facing API 只接收 immutable view model、`UiRuntime`/node ID、`UiDrawList` 和 UI-local state，不能接收 registry、LevelManager、SpatialHashGrid 或 gameplay system。

- `UiRuntime::UpdateInput`、`Arrange`、modal/focus/pointer capture 必须在每个 UI Update 调用；placeholder node 必须删除或成为实际可见、可命中的 panel root。
- `UiDrawList` 按 `Hud < Panels < DragPreview < Modal < Tooltip < Debug` 输出。排序 key 为 layer、stable node ID、append sequence，保证确定性；排序和提交使用已分配缓冲，不允许每 command 有序 `vector::insert` 或每层重复扫描所有 command。
- 文本 command 不拥有每帧 `std::string`。host-owned、容量受限且复用的 text arena 或等价的稳定文本引用负责生命周期；超容量必须产生测试可见的 overflow telemetry，不能静默重新分配。
- raylib `Font`、`Texture2D`、scissor 和 `Draw*` 只存在于 `UiRaylibBackend` 及其已注册 custom painter。skill tree/astrolabe 等特殊画布可用 custom painter，但 painter 只读 panel render data，仍服从 runtime 的 clip/focus/layer 合同。
- HUD/minimap/hotbar/buffs/monster health、character/inventory、stash/crafting、skill hub/tree、astrolabe、context menu/quantity/message/tooltip 均必须迁移。单个切片完成后删除该切片的 immediate `Draw` 路径，不保留生产 feature flag 回退。

### 3.5 `WorldUiFrame` 帧内桥

`GameplayRenderAdapter::ExecuteUIWorldPass` 在所有分支、所有质量设置下首先打开新 frame token、清除 visible proxy 和 hover。随后：

1. 通过只读 query/cull 抽取一份轻量 item proxy，CPU 和 GPU loot 路径均写入 `WorldUiFrame`；proxy 至少含 domain ID、world/screen hit rect、depth 和有效 token。
2. GPU loot 路径可以跳过 CPU label/glyph/beam 输出，但不能跳过 proxy 生成。不得通过改动 Engine `GPULootPass` 或 RenderGraph 达成该目标。
3. `WorldUiFrameView` 在获取时绑定 expected token；`IsValid()` 只有 token 匹配且来自当前 UIWorldPass 时为真。host、tooltip 和 pickup reader 无有效 view 时必须视为没有 world target，不能读取上一帧 vector。
4. `PrepareRender` 只消费此 view，并将 world click 转成下一 Update 的 pickup intent。无 compute/未绑定路径明确输出 invalid/empty view，而非依赖旧容器残留。

### 3.6 Escape 与输入捕获

`GameUiHost` 是 UI Escape 的唯一所有者。每次 Update 开始重置 `EscapeConsumedThisFrame`；runtime 依据 modal、focus、z-order 和 panel 打开次序关闭恰好一个最高优先级 surface，并在关闭、取消 quantity/text input 或处理确认 dialog 时置位。

quantity popup、character confirmation、context menu、skill tree、astrolabe、inventory、stash 和 crafting 均必须拥有明确的 retained z-order/close policy。不能以 `IsInventoryVisible()` 作为 Pause 的代理。`GameplayState` 在 host Update 后仅于 Escape 边沿存在且 `EscapeConsumedThisFrame == false` 时 Push `PauseState`。

### 3.7 热路径与性能证据

Update/Render 的稳态目标是零 heap allocation，包含 panel format、filter、draw-list append/sort、backend submit 和已迁移 controller 的临时集合。实现采用：

- 数值/状态变化驱动的格式缓存、固定 char buffer 或 text arena；
- controller-owned、容量有上界的复用 vector/map/scratch buffer；材料小写索引、affix label 等预解析/按变化重建缓存；
- 初始化期 reserve、运行期容量检查和计数器，而不是每帧新建 `map`/`vector`；
- 一次无分配 finalize 排序和单趟 backend submit。

历史 U0 runtime 数据不可用，因此修复开始前必须从被审查 revision 采集并归档“R0 remediation baseline”，不得改名为 U0。候选版本在相同场景、构建、输入、硬件和样本数下的 UI CPU p95、allocation、draw/clip/overflow 指标不得超过 R0 的 110%，且已迁移 panel 的稳态分配必须为零。原始设计要求的 Tracy 证据仍有效：本任务使用 `docs/workflows/performance.md` 所述现有工具链和 opt-in profiling 配置，不增加新的第三方包；若当前仓库不能构建该集成，性能门禁保持阻塞而非用 `NOT_RUN` 替代通过。

## 4. 生命周期、兼容与影响

### 4.1 生命周期

`Game` 继续拥有 `GameUiHost` 与 `WorldUiFrame`，并维持 host shutdown 早于资源 unload/window close。进入/离开 Gameplay 时，host 清空 intent、notification、retained nodes、capture、drag、tooltip、panel-local temporary state 和 frame view；不会创建任何测试 item/skill/buff。

### 4.2 存档、资产与配置

- 不新增存档字段或资产格式；UI session state 不持久化。
- 删除测试注入仅阻止未来污染。历史数据清理不在范围内，避免根据名称误删真实物品或效果。
- 复用当前 font/texture 资源并以 `UiResourceId` 注册 backend；不迁移资源所有权。
- 如需启用 Tracy，仅使用仓库已文档化的 `TRACY_ENABLE`/`TRACY_ENABLE_ALLOCATORS` 兼容路径和现有 `%NMD_TRACY%` 工具。发现缺失 client/library 时暂停，不擅自添加外部依赖。

### 4.3 回退

不保留 shipped dual renderer。每个 panel 切片以小型、可审查提交完成，回退通过版本控制回退该原子切片；不能在同一运行版本中恢复 legacy immediate path。RenderGraph、scene RT 和 DRS 保持不变，因此 UI 修复失败时可回退 Game UI 切片而不影响 P0 渲染工作。

## 5. 验收标准

| 审查项 | 可观察完成条件 |
| --- | --- |
| B-01 | 所有 screen controller 的 Draw/paint 接口无 registry/gameplay mutator；source/AST guard 不在 panel、overlay、renderer Draw 中发现 `InventorySystem`/`CraftingSystem`/`StashSystem`/`SalvageSystem` 写、`registry.destroy`、`get_or_emplace` 或直接 component 写；每种 intent 经 handler 的成功和拒绝路径有测试。 |
| A-01 | 可见 panel 的真实 draw list 非空且由 runtime layout/input 产出；`GameUiHost` Render 不向 controller 传 registry；placeholder/immediate fallback 删除；modal/text/pointer capture 由 runtime 测试证明。 |
| D-01 | 生产 `GameUiHost` 不再包含 `m_hasGivenTestItems`、test item、`test_power`、`test_speed`、`test_stun` 或 `test_poison` 注入；测试 fixture 明确创建所需数据。 |
| C-01 | allocation counter、benchmark 和 profiler 证明已迁移 hot path 稳态零分配；无 per-frame string/map/vector 建立、ordered insert 或未受控 text ownership；容量 overflow 为零。 |
| H-01 | CPU loot -> GPU loot -> CPU loot 的切换中 token 每 pass 前进，GPU 路径 proxy 为当前帧，tooltip/click 不消费 stale proxy。 |
| H-02 | 各 overlay/panel 的 Escape 仅关闭最顶层 UI；同一按键不 Push Pause；无 UI 消费时才暂停。 |
| H-03 | R0/candidate 性能 artifact、自动化测试、16:9/21:9/4:3、DRS、GPU loot/text、modal/drag/pickup 手测矩阵完整归档。不可运行项目有原因、范围和 owner，且不作为通过依据。 |
| L-01 | `git diff --check` 为零；`UIStashController.hpp` 等迁移注释只描述当前所有者和真实 fallback 状态。 |

最终仍须通过原设计的 core dependency、post-composite native UI、tooltip timing、pickup validation 和 session reset 合同；本文件不削弱任何原验收项。

## 6. 风险、依赖与设计通过条件

| 风险/依赖 | 缓解与停止条件 |
| --- | --- |
| 全量 panel 迁移范围大 | 按 panel 族原子切片实施；一个切片必须同时完成 snapshot、intent、runtime paint、tests 和旧路径删除，禁止半迁移。 |
| GPU loot proxy 与 label 数据耦合 | 先提取只读 proxy producer，再让 CPU/GPU label 路径分别消费；如无法保持同一 pick/hover 集合，暂停该切片并更新设计。 |
| 缺少历史 U0 性能数值 | 在任何代码改动前采集 R0 remediation baseline；报告中明确其来源和局限，不能伪造历史比较。 |
| Tracy 当前未集成 | 先验证现有支持的 profiling build；不能构建或无法取得目标硬件 capture 时，H-03 仍为 blocker。 |
| 既有 `check_legacy_reintroduction.py` baseline mismatch | 记录为外部阻塞，不更新 inventory，也不将被阻塞的 `build.bat check` 表述为通过；以补充编译/测试证据区分。 |
| 已污染的历史存档 | 本任务不做基于名称的自动删除；出现确证的持久化修复需求时另立存档设计。 |

本设计在问题、非目标、所有权、生命周期、性能、回退、验收和风险上满足 `docs/workflows/design.md`。通过后按 [2026-08-12-ui-system-rearchitecture-remediation-plan.md](../plans/2026-08-12-ui-system-rearchitecture-remediation-plan.md) 实施；本轮只产出文档，不修改生产代码。
