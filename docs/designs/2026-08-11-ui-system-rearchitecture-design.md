# UI 系统重构设计（UI System Rearchitecture）

> **Status:** proposed
>
> **Date:** 2026-08-11
>
> **Purpose:** 将当前集中在静态类和 Draw 循环中的 UI 实现，收敛为拥有明确生命周期、布局、输入、渲染合同和数据边界的独立 Game UI 模块。第一阶段保持现有视觉与玩法行为，不改变当前 RenderGraph 及场景渲染路径。

## 1. 设计输入与结论

### 1.1 已验证的现状

- `src/game/application/ui/` 当前有 40 个文件。`NoMoreDayGameUi` 已经是独立的静态库目标，但这只是编译边界，不是运行时模块边界。
- `UISystem::Update`（`src/game/application/ui/UISystem.cpp:220-525`）处理热键、面板状态、附近交互、物品拾取和测试数据注入；`UISystem::Draw`（`src/game/application/ui/UISystem.cpp:527-694`）同时处理缩放、面板绘制、世界物品命中/拾取和 tooltip 状态机。
- `UIContext`（`src/game/application/ui/UIContext.hpp:26-123`）同时承载会话状态、面板状态、拖拽状态、输入门控、tooltip、弹窗和资源缓存。面板还各自拥有静态成员，状态实际散落在三处：`UIContext`、静态面板类和 `src/game/foundation/ui_shared/UiShared.hpp`。
- 当前屏幕 UI 的真实渲染边界已经存在：`GameplayState::OnRender`（`src/game/application/states/GameplayState.cpp:983-1157`）先渲染 `m_sceneRT`，再将其合成到默认 framebuffer，最后在 `:1110-1156` 绘制原生屏幕 UI。`RenderSystem` 不负责最终呈现。
- 世界 UI 由 `RenderSystem::UIWorldPass` 调用 `GameplayRenderAdapter::ExecuteUIWorldPass` 产出；`UiShared::VisibleItemCache::visibleItems` 是当前渲染阶段写入、屏幕 UI 阶段读取的临时命中数据。
- `InputSystem::update`（`src/game/application/input/InputSystem.cpp:13-152`）通过读取 `UISystem::State` 和静态查询函数决定是否屏蔽玩法输入，形成反向依赖。

### 1.2 核心决策

采用 **保留式运行时（retained runtime）+ 游戏呈现控制器（presentation controllers）+ 后端无关绘制列表（draw list）** 的混合方案：

1. UI 树、布局结果、焦点、指针捕获和动画状态由一个实例化的 `UiRuntime` 持有。
2. 游戏面板不再直接读写 `entt::registry` 或调用玩法系统；它读取每帧只读的 `GameUiSnapshot`，通过 `GameUiIntent` 请求玩法操作。
3. `UiRuntime` 只产生逻辑坐标绘制命令；首个后端为现有 raylib 路径。后端抽取不要求当前阶段把通用绘制原语迁回 Engine。
4. 屏幕 UI 和世界 UI 保持两个渲染阶段：屏幕 UI 在场景合成之后以原生分辨率绘制；世界 UI 继续走 `UIWorldPass`，只向 UI 提供同帧命中代理。
5. 保留现有 `UISystem` 作为有具体调用者支撑的迁移外观层；新代码禁止增加对其静态可变状态的依赖。迁移完成后删除该外观层。

### 1.3 为什么不直接采用第三方 UI 框架

- 当前技术栈只批准 C++20、EnTT、raylib/OpenGL 4.3，没有已批准的 UI 第三方依赖。
- Dear ImGui 仍是 immediate-mode，不能直接解决持久化状态、焦点、布局和面板生命周期问题。
- RmlUi 等 retained/HTML 方案会引入新的渲染、字体、资源和输入集成面，且需要在 P0 RenderGraph 治理期间改变依赖图。
- 因此先建立小型、可测试且不依赖 raylib 的内部合同；若未来确有外部框架需求，后端和输入合同可作为适配边界。

## 2. 目标、非目标与硬约束

### 2.1 目标

1. 有唯一运行时所有者和明确生命周期，消除 UI 全局可变状态。
2. 提供 retained UI node tree、测量/排列布局、锚点/安全区、层级和命中测试。
3. 将输入优先级、焦点、模态捕获和 tooltip 变成可测试的 UI runtime 行为。
4. 将游戏数据读取和玩法写操作从绘制路径中移出。
5. 让 UI 绘制可以替换 raylib 后端，并保持屏幕 UI 不受 DRS 和场景曝光影响。
6. 迁移过程中保持现有面板行为、快捷键语义、tooltip 延迟和交互结果。
7. 在不改变存档、资产格式和当前渲染生产合同的前提下逐面板迁移。

### 2.2 非目标

- 本设计不做视觉风格重做，不重新设计 inventory、skill tree 或 astrolabe 的产品交互。
- 不在当前 Track 中修改 `RenderGraph`、GPU 资源所有权、shader、场景 RT 或 DRS 算法。
- 不把世界 UI 强行塞进屏幕 UI 树，也不把 `UIWorldPass` 改成 raylib 屏幕绘制。
- 不引入第三方 UI 框架、脚本语言或新的资产格式。
- 不把整个 `entt::registry` 快照复制到 UI；UI 只接收面板所需的只读 view model。
- 不在第一阶段一次性重写 40 个文件；每个迁移切片必须可独立回退和验证。
- 不把 `UiShared` 立即删除；它是当前已经批准的环断开层，先缩减其语义，再在世界 UI 桥接完成后移除静态状态。

### 2.3 硬约束

- 依赖方向保持 `application UI -> systems/foundation/engine`，Engine 不得反向包含 Game UI 类型。
- 屏幕 UI 必须位于 `GameplayState::OnRender` 的场景合成之后、`EndDrawing` 之前，并使用 native framebuffer 尺寸。
- 所有玩法变更只能在 Update 阶段由命令处理器执行；Render 阶段只能读取帧快照、命中测试和提交绘制命令。
- 构建使用 `RelWithDebInfo` 的 `build.bat`；不得使用 `debug` 配置。
- 每个阶段结束都必须通过 `build.bat check` 和相关 CTest 门禁。

## 3. 目标架构

### 3.1 逻辑模块

```text
Game / GameplayState（组合根，拥有 GameUiHost）
        │
        ├── GameUiSnapshotBuilder（registry -> 只读面板模型）
        ├── GameUiCommandHandler（UI intent -> 玩法系统，Update 执行）
        └── GameUiHost
                │
                ├── Game panels/controllers（InventoryPanel、SkillPanel ...）
                └── UiRuntime
                        ├── retained node tree
                        ├── layout / hit-test / focus / animation
                        ├── UiDrawList
                        └── UiInputCapture
                                │
                                └── UiRaylibBackend（首个实现）

RenderSystem::UIWorldPass
        └── WorldUiFrame / hit proxies -> GameUiHost（只读、帧内）
```

`GameUiHost` 是 Game/application 的组合对象，不放进 `UiShared`，不作为静态单例。`GameplayState` 借用它；进入/离开 Gameplay 时分别调用 `EnterGameplay`/`LeaveGameplay` 清理会话状态。

第一阶段不新增 CMake 顶层目标：现有 `NoMoreDayGameUi`（`src/game/application/ui/CMakeLists.txt:14-33`）已经提供编译隔离。先用合同和依赖检查形成逻辑独立性；待 P0 渲染工作完成且 UI core 不再依赖游戏资源后，再评估拆出 `NoMoreDayGameUiRuntime`，避免破坏当前 1:1 目录/目标约束。

### 3.2 Core 与 Game 的边界

`UiRuntime` 允许包含的内容：

- `UiId`、逻辑坐标、矩形、颜色、长度和约束布局类型。
- retained node 的父子关系、可见性、z-order、focusable/modal/disabled 标志。
- 事件路由、鼠标/键盘/文本输入快照、指针捕获和动画时钟。
- `UiDrawList` 和资源句柄（font/texture/icon 的不透明 ID）。

`UiRuntime` 禁止包含或调用：

- `entt::registry`、`entt::entity`、`ItemComponent`、`SkillSystem`、`InventorySystem`、`SaveManager` 等游戏类型/系统。
- raylib `Font`、`Texture2D`、`GetMousePosition`、`DrawRectangle` 等后端 API。
- `UiShared` 静态变量和 `GameplayRenderAdapter`。

游戏呈现层负责：

- 将 ECS 数据转换为 `GameUiSnapshot`；字符串、图标和语义状态在此层解析。
- 将 UI 事件转换为带来源和域 payload 的 `GameUiIntent`。
- 在 Update 中验证并执行 intent，生成成功/失败通知和新的 snapshot 版本。
- 将旧的 raylib 资源映射为 `UiResourceId`，由后端完成真正绘制。

### 3.3 核心合同

以下是稳定合同的接口草图，实际命名和字段在实施计划的第一项中冻结；这里不是可编译实现：

```text
UiViewport
  logicalSize, pixelSize, contentRect, scale
  ToLogical(pixelPoint), ToPixel(logicalRect), Contains(pixelPoint)

UiFrameInput
  pointerPixel, pointer buttons, wheel, key transitions, text/IME, dt

UiInputCapture
  pointer, keyboard, text, modal

UiRuntime
  BeginFrame(UiViewport, UiFrameInput)
  Update(GameUiSnapshotView)
  PrepareRender(WorldUiFrameView)
  DrawList() -> const UiDrawList&
  Capture() -> UiInputCapture
  EndFrame() -> UiFrameResult

GameUiIntent
  source UiId + action kind + domain identifiers/payload

GameUiSnapshot
  immutable, frame-scoped, panel-specific read models and notifications
```

`UiRuntime::PrepareRender` 是必要的第二阶段：当前世界命中代理在 `RenderSystem::UIWorldPass` 中才产生，而该 pass 位于 `GameplayState::OnRender` 内，晚于 `OnUpdate`。因此：

1. `OnUpdate` 处理屏幕树、布局、焦点、键盘和上一帧已提交的 intent。
2. `OnRender` 先完成世界渲染和场景合成；`PrepareRender` 接收本帧 `WorldUiFrame`，只更新世界 hover/tooltip 和 draw list。
3. `PrepareRender` 产生的世界交互 intent 放入下一帧 Update 队列，绝不直接改变 ECS。

这样既保留当前帧世界物品高亮的正确时序，也禁止在 Draw 中调用 `InventorySystem::pickUpItem`。

## 4. 状态、数据与生命周期

### 4.1 状态所有权

| 状态 | 新所有者 | 生命周期 | 说明 |
| --- | --- | --- | --- |
| viewport、raw input、focus、pointer capture | `UiRuntime` | 每个 UI runtime | 不再写 `UISystem::State` |
| panel open/tab/search/drag | 各 panel instance | Gameplay session | 从静态成员拆出，panel 间不共享可变字段 |
| tooltip target/delay/alpha | `TooltipController` | runtime | 延迟沿用 `0.12s` 初次、`0.05s` 目标切换、离开 `0.08s`；淡入 `dt*10`、淡出 `dt*8` |
| context menu/quantity/message modal | overlay layer | runtime | 由 modal/focus 层统一捕获输入 |
| fonts/textures/theme | `GameUiHost` + resource provider | Game UI 生命周期 | runtime 只保存不透明句柄，禁止静态 `Font` |
| inventory/skill/player read data | `GameUiSnapshotBuilder` | 帧内只读 | 不保存 `registry` 引用到 UI tree |
| world item hit proxies | render adapter -> `WorldUiFrame` | 当前渲染帧 | 替代 UI 直接查询 world cache 的语义；过渡期可由 `UiShared` 提供 |
| pickup/equip/stash/crafting 等写操作 | `GameUiCommandHandler` + 对应 gameplay system | Update | UI 只发 intent，系统再次验证实体、距离、容量和权限 |

`UIContext` 最终删除。迁移期间只允许 `UISystem` facade 读取/同步兼容字段，禁止新 panel 直接引用它。

### 4.2 生命周期

```text
Game 初始化资源
  -> GameUiHost::Initialize(resource provider, theme)
Gameplay 进入
  -> GameUiHost::EnterGameplay()
每帧 Update
  -> sample raw input
  -> build snapshot
  -> UiRuntime::BeginFrame / Update
  -> execute queued intents and publish result
  -> InputSystem::update(..., UiInputCapture)
每帧 Render
  -> RenderSystem world + UIWorldPass
  -> scene RT composite to default framebuffer
  -> UiRuntime::PrepareRender(WorldUiFrame)
  -> UiRaylibBackend::Render(native viewport, UiDrawList)
Gameplay 离开
  -> GameUiHost::LeaveGameplay()
  -> GameUiHost::Shutdown()
  -> ResourceManager unload / window close
```

资源释放顺序保持现有 Game 生命周期：UI backend 不在窗口/GL context 关闭之后释放 raylib 资源。

## 5. 布局、坐标与渲染合同

### 5.1 Viewport

`UiViewport` 取代当前 `scaleX/scaleY` 分散计算。它同时保存 logical size（初始仍为 `2560x1440`）、native pixel size、居中后的 `contentRect`、safe-area inset 和双向转换。

- 所有 node/layout/tooltip 坐标使用 logical space。
- 后端在提交 `UiDrawList` 时转换到 native pixel space。
- 鼠标输入先由 `ToLogical` 逆变换；letterbox 区域不命中 UI。
- panel 拖拽位置按 logical space 保存，并在 safe-area 内统一 clamp。
- DRS 只影响 `m_sceneRT` 和世界管线；`UiRaylibBackend` 直接对最终 framebuffer 绘制。

### 5.2 Layout

第一版布局只承诺四种组合原语：`Overlay`、`Row`、`Column`、`Anchor`。每个 node 有 `width/height (Auto|Pixels|Fraction)`、`min/max`、`margin/padding`、`gap`、`align`、`zIndex` 和可选 anchor。面板的特殊可视化区域（skill tree/astrolabe）通过 custom painter node 接入，不绕过 tree 的 hit-test/focus/clip 合同。

布局顺序固定为 `reconcile -> measure -> arrange -> hit-test -> paint`。布局输出是可测试的 `UiRect` 表，不把 raylib draw call 混入 measure/arrange。

### 5.3 Draw list/backend

`UiDrawList` 初版支持 fill/stroke rect、line, text, image/icon、clip push/pop 和 custom painter。命令只携带逻辑坐标、颜色、资源 ID、clip 和 node ID；raylib 类型只出现在 `UiRaylibBackend`。

UI layer 顺序固定为：`Hud` < `Panels` < `DragPreview` < `Modal` < `Tooltip` < `Debug`。同层按稳定 node ID 保证确定性绘制。

## 6. 输入与跨系统数据流

### 6.1 输入优先级

平台输入只采样一次，形成 `UiFrameInput`。runtime 先处理 modal、pointer capture 和 focused text input，再把 `UiInputCapture` 交给 `InputSystem`：

```text
raw platform input
  -> UiRuntime (modal/focus/pointer/key/text)
  -> UiInputCapture
  -> InputSystem (only uncaptured gameplay actions)
```

`InputSystem` 不再读取 `UISystem::State.isTyping`、`isMouseOverUI` 或 `IsModalInputCaptured()`。快捷键冲突通过 UI capture 和 action priority 解决，而不是两个静态系统同时轮询 raylib。

### 6.2 玩法意图

典型物品拾取路径变为：

```text
WorldUiFrame hit proxy
  -> UiRuntime hit-test
  -> GameUiIntent::PickupItem(itemId)
  -> next Update: GameUiCommandHandler
  -> distance/entity/capacity validation
  -> InventorySystem::pickUpItem
  -> UiNotification + new snapshot version
```

`InventorySystem::pickUpItem`（`src/game/systems/item/InventorySystem.cpp:73-235`）仍是实际物品变更的拥有者；UI 不能信任 entity ID，也不能在渲染阶段执行它。

## 7. 迁移策略

### Phase 0：合同冻结与基线

记录现有快捷键、tooltip 时间、场景合成顺序、UI CPU/分配基线；添加依赖边检查和新 runtime 的空实现测试。

### Phase 1：核心 runtime 并行接入

加入 `UiViewport`、输入值对象、retained node/layout、draw list 和 raylib backend。`GameUiHost` 先包装现有静态 panel，`UISystem` facade 转发到 host；视觉输出保持不变。

### Phase 2：状态归属与输入解耦

按 `runtime/session/overlay/panel` 拆 `UIContext`；把 `InputSystem` 改为接收 `UiInputCapture`；tooltip、drag 和 modal 迁到 runtime service。

### Phase 3：数据/命令桥

建立 `GameUiSnapshotBuilder`、`GameUiCommandHandler` 和结果通知。先迁移 inventory/stash 的写操作，再迁移 crafting/skill；移除 `UISystem::Update/Draw` 内的拾取和测试注入。

### Phase 4：逐面板迁移

顺序为 `HUD/minimap -> inventory/character -> stash/crafting -> skill hub/tree -> astrolabe -> tooltip/context/quantity overlays`。每个 panel 变成 host 所有的实例，特殊画布通过 custom painter 保留性能和现有视觉。

### Phase 5：世界 UI 桥接与清理

将 `VisibleItemCache`/`HoveredItem` 从静态 `UiShared` 迁为帧内 `WorldUiFrame`/intent queue；确认 `GameplayRenderAdapter` 与 screen UI 只通过窄合同交互后，删除旧静态桥、`UIContext` 和 `UISystem` facade。

每个 phase 都可独立回退；不得在一个切片中同时迁移所有 panel、RenderGraph 和资源所有权。

## 8. 影响评估与回退

| 领域 | 影响 | 处理 |
| --- | --- | --- |
| 玩法行为 | intent 执行点从 Draw 移到下一次 Update，可能增加最多一帧延迟 | 通过队列明确化；实体/距离/容量由玩法系统再次验证 |
| 输入接口 | `InputSystem::update` 增加 value-only capture 参数 | 只改 Game/application 调用面；过渡 facade 保留旧查询 |
| 渲染 | 新 backend 仍在最终 framebuffer 绘制 | 不修改 RenderGraph、scene RT、shader 或 DRS |
| 资源 | 新 runtime 只持有句柄，host 管资源生命周期 | 过渡期复用现有 `ResourceManager`/raylib 资源 |
| 存档/资产 | 无格式改动 | panel session state 不进入存档，除非另立设计 |
| 性能 | retained layout 有新增 CPU 成本 | 先测基线；稳定帧禁止无界分配，迁移切片以 baseline +10% 为首阶段回归阈值 |
| 回退 | facade 可以切回原 panel 路径 | 每个 panel 以 feature/host route 独立切换，保留旧测试证据 |

## 9. 验收标准

### 架构

1. 新 `UiRuntime` 代码不包含 `entt`、raylib draw API、`UiShared` 或 gameplay system headers。
2. runtime 状态由实例拥有；新 panel 不声明静态可变 UI 状态，也不暴露可写全局 context。
3. screen UI 的最终绘制仍发生在 `GameplayState::OnRender` 的 scene composite 之后，并且不进入 `m_sceneRT`。
4. `UISystem::Draw/Update` 最终只做 host 生命周期/兼容转发，不执行 registry 写操作。

### 行为

1. 现有 UI tech tests、drag/context/tooltip/HUD/skill tests 全部保持通过。
2. 快捷键和 modal/text input 捕获结果与迁移前一致。
3. tooltip 的初次/切换/离开延迟及淡入淡出数值保持不变，除非另立视觉设计。
4. 物品拾取仍经过 `InventorySystem::pickUpItem` 的实体、距离和容量验证，且没有 Render 阶段 ECS 写操作。

### 验证

- `build.bat`（默认 `RelWithDebInfo`）成功。
- `ctest --test-dir build -C RelWithDebInfo -L ci` 成功；每个切片另运行对应 focused doctest。
- `build.bat check` 成功，模块边界无新增违规。
- 单测覆盖 viewport round-trip/letterbox、layout measure/arrange、z-order/hit-test、modal capture、tooltip transition、intent queue 和 draw-list determinism。
- Tracy 基线显示第一阶段 UI wrapper/layout/paint CPU 不超过迁移前同场景基线的 110%；若最终全量 retained tree 超过该阈值，必须先更新性能记录和设计，不以未测定的预算掩盖回归。

## 10. 风险与未决项

| 项 | 风险/取舍 | 默认决策 |
| --- | --- | --- |
| 通用原语归属 Engine 还是 Game | 当前 UIRenderer 已在 Game，P0 不允许重开资源边界 | 第一阶段留在 Game UI backend；P0 后单独评估 Engine native overlay |
| Skill tree/Astrolabe 特殊画布 | 全部强行通用化会损失性能并扩大范围 | retained tree 管 clip/focus，custom painter 管内部图形 |
| 世界命中代理时序 | 当前代理只能在 UIWorldPass 后获得 | `PrepareRender` 两阶段合同；intent 下一帧 Update 执行 |
| CMake 是否拆 `UiRuntime` target | 新 target 可能破坏当前目录/目标闭合检查 | 第一阶段不拆 target；合同稳定且 P0 解除后再立项 |
| 字体/本地化 | 当前面板直接持有 raylib Font 和字面量文本 | runtime 使用 resource/text ID；字体与本地化迁移另列子任务 |
| UI 持久化 | 当前状态主要为 session state | 不改存档；需要保存 panel layout 时另立数据格式设计 |

## 11. 设计通过条件

本设计满足 `docs/workflows/design.md` 的问题、非目标、所有权/生命周期、兼容与性能影响、可观察验收和风险项要求。设计通过后按 `docs/workflows/planning.md` 执行对应实施计划；若 P0 渲染合同改变，先更新本设计和渲染 Track，再继续实现。
