# UI 系统重构实施计划（UI System Rearchitecture Plan）

> **Status:** in progress; U0-U7 implemented (2026-08-12), U8 pending
>
> **Progress note (2026-08-12):** U3-U7 已全部实施并通过验证（focused + 全量 UI 回归 211 cases / 2803 assertions 全绿；全量 976 cases 中仅 3 个既有无关失败：GraphBindingEquivalenceGLTest 渲染图集成、RadianceCascadesBenchmark 性能基准）。U7 六个组全部完成：组1 HUD/minimap/hotbar/buffs/MonsterHealthBar、组2 character/inventory、组3 stash/crafting、组4 skill hub/talent tree、组5 astrolabe（UIAstrolabe 删除）、组6 context menu/quantity popup/message box/tooltip。所有面板/overlay 均为 GameUiHost 持有的实例控制器（src/game/application/ui/*Controller.{hpp,cpp}），无静态可变 UI 状态，UISystem::State 保留为兼容镜像（U8 断开）。U8（WorldUiFrame bridge + UiShared 静态槽替换 + UIContext/UISystem facade 删除）未实施，工作量较大（涉及 render 层写侧、GameplayState/Game/UIRenderer/InventorySystem/InventoryState 等全部 State 读方迁移），建议独立一轮。
>
> **Design:** [`2026-08-11-ui-system-rearchitecture-design.md`](../designs/2026-08-11-ui-system-rearchitecture-design.md)
>
> **Execution rule:** 每个任务都是原子切片。任务通过 focused tests、`build.bat`、`build.bat check` 后才进入下一项；不在本计划中修改 RenderGraph、GPU resource ownership、shader 或 scene RT。若仓库已有独立门禁阻塞，必须记录根因和替代编译证据，不得静默绕过或更新基线。

## 1. 实施顺序与依赖

| ID | 切片 | 前置 | 主要风险 | 完成信号 |
| --- | --- | --- | --- | --- |
| U0 | 冻结基线与兼容合同 | 无 | 迁移时无可比较证据 | 行为/性能/依赖基线入库 |
| U1 | Value types 与 viewport | U0 | 坐标语义不一致 | 不依赖 raylib/EnTT 的单测通过 |
| U2 | Retained node、layout、输入状态 | U1 | 过度实现 CSS 式布局 | 约束布局、hit-test、modal 单测通过 |
| U3 | Draw list 与 raylib backend | U2 | 改变 native UI 渲染顺序 | command/clip/backend contract 测试通过 |
| U4 | `GameUiHost` 与旧 facade | U3 | 生命周期和资源释放次序 | 老面板无行为变化地通过 host 绘制 |
| U5 | 输入捕获解耦 | U4 | 玩法输入漏拦截或误拦截 | `InputSystem` 不再读 UI 全局状态 |
| U6 | Snapshot/intent 与拾取迁移 | U5 | Render 阶段写 ECS、交互延迟 | gameplay command handler 验证通过 |
| U7 | Overlay 与逐面板迁移 | U6 | 40 文件一次性改动 | 每个 panel 独立可验证/回退 |
| U8 | World frame bridge 与 legacy 删除 | U7 | render/ui 桥重建循环依赖 | `UiShared` 静态状态和 `UISystem` 删除 |

本轮已完成 U0/U1/U2 的无行为变化基础；U3 及后续切片必须按表中顺序执行。U2 尚未接入 legacy panel、raylib backend 或 gameplay input 路径。

## 2. 全局不变量

1. 新 runtime 核心不得包含 `entt`、raylib、`UiShared` 或 gameplay system headers。
2. 只有 Update 阶段的 `GameUiCommandHandler` 可执行玩法操作；Render 阶段只读帧数据和生成绘制/intent。
3. screen UI 始终在 `GameplayState::OnRender` 的 scene RT 合成之后绘制；DRS 不得缩放文字、HUD 或 modal。
4. `UiRuntime`、panel 和 overlay 都是实例；不新增静态可变 UI 状态。
5. 任何 gameplay model 都以 snapshot 形式进入 UI；没有 `entt::registry&` 或 `entt::entity` 泄漏到 core。
6. 一个任务只迁移一类所有权。禁止在同一变更中混合 RenderGraph、资源生命周期和 panel 重写。

## 3. U0：冻结基线与兼容合同

### 文件与证据

- Create: `docs/reports/ui-system-rearchitecture/baseline.md`
- Modify: `tests/tech/UITests.cpp`（仅补缺失的行为断言；不改已有场景）
- Inspect: `src/game/application/states/GameplayState.cpp`
- Inspect: `src/game/application/ui/UISystem.cpp`
- Inspect: `src/game/application/input/InputSystem.cpp`

### 实施

1. 记录当前调用顺序：`OnUpdate -> UISystem::Update`、world render/UIWorldPass、scene composite、`UISystem::Draw`。
2. 记录 viewport 基线：`2560x1440`、16:9、超宽和窄高窗口的 logical-to-pixel 结果、letterbox 行为和 panel drag clamp。
3. 用 Tracy 记录代表场景的 UI update/draw CPU 时间、draw command/clip 计数和稳态分配；结果写入 baseline 报告，未跑的硬件项标为 `NOT_RUN`。
4. 锁定行为合同：C/Z/N/S/E/F/Escape 快捷键、modal/text input、tooltip 的 `0.12s/0.05s/0.08s` 延迟和 `dt*10/dt*8` 淡入淡出、world-item `180` 距离门槛、背包满提示。
5. 将无法从现有 UI tech tests 观测的合同补成小型 source/contract test，避免先重写再发现行为漂移。

### 验证和完成标准

- `build.bat`
- `bin/NoMoreDayTests.exe --test-case="*[Tech]*UI*"`
- `build.bat check`
- 报告明确区分已测量、未运行和待后续硬件验证项目；不把进程退出码当作性能/视觉证据。

## 4. U1：基础值对象与 `UiViewport`

### 文件

- Create: `src/game/application/ui/UiRuntimeTypes.hpp`
- Create: `src/game/application/ui/UiViewport.hpp`
- Create: `src/game/application/ui/UiViewport.cpp`
- Modify: `src/game/application/ui/CMakeLists.txt`
- Create: `tests/unit/UiViewportTests.cpp`
- No test CMake modification: `tests/CMakeLists.txt` already uses `GLOB_RECURSE` for `*.cpp`.

### 设计和伪代码

`UiRuntimeTypes.hpp` 只使用标准库，定义小型值对象：

```text
UiVec2 { x, y }
UiRect { origin, size; Contains; Intersection }
UiInsets { left, top, right, bottom }
UiId (opaque stable integer)
UiResourceId (opaque stable integer)
UiInputCapture { pointer, keyboard, text, modal }
```

`UiViewport` 固定 logical reference size（初始 `2560x1440`），计算 native content rect：

```text
Fit(pixelSize, logicalSize, safeInsets):
  usable = pixelSize - safeInsets
  scale = min(usable.width / logical.width, usable.height / logical.height)
  contentRect = centered(logical * scale, usable) + safeInsets

ToLogical(pixel):
  return (pixel - contentRect.origin) / scale

ToPixel(logical):
  return contentRect.origin + logical * scale
```

`UiViewport` 不读取 `GetScreenWidth`/`GetScreenHeight`；平台/host 提供像素尺寸。圆整策略在一个位置定义，避免 panel 各自使用浮点缩放。

### 测试

1. 16:9 viewport 的 scale/content rect 与现有 `min(scaleX, scaleY)` 一致。
2. 21:9 和 4:3 viewport 正确居中 letterbox/pillarbox。
3. logical -> pixel -> logical 的 round-trip 误差小于 `0.01f`。
4. contentRect 外的 pointer 不命中；safe inset 改变 origin，不改变 logical 尺寸。
5. `UiRuntimeTypes.hpp` source guard 断言不含 `raylib.h`、`entt/`、`UiShared` 和 `InventorySystem`。

### 完成标准

- U1 的生产代码可独立被单测包含，不接入老 `UISystem`，因此零玩法/视觉行为改动。
- focused viewport 测试、全量 build、module check 通过。

## 5. U2：Retained node、布局、事件与动画状态

### 文件

- Create: `src/game/application/ui/UiLayout.hpp`
- Create: `src/game/application/ui/UiLayout.cpp`
- Create: `src/game/application/ui/UiRuntime.hpp`
- Create: `src/game/application/ui/UiRuntime.cpp`
- Create: `src/game/application/ui/UiTooltipController.hpp`
- Create: `src/game/application/ui/UiTooltipController.cpp`
- Create: `tests/unit/UiLayoutTests.cpp`
- Create: `tests/unit/UiRuntimeInputTests.cpp`
- Create: `tests/unit/UiTooltipControllerTests.cpp`
- Modify: `src/game/application/ui/CMakeLists.txt`
- No test CMake modification: `tests/CMakeLists.txt` already uses `GLOB_RECURSE`.

### 实施

1. 使用 `UiId` 建立 retained node arena；node 只保存 parent/children、layout style、visibility、z-index、hit-test/focus/modal flags 和 custom-painter token。
2. 第一版 layout 只实现 `Overlay`、`Row`、`Column`、`Anchor`，以及 `Auto|Pixels|Fraction` 尺寸、min/max、margin/padding、gap 和 alignment。
3. 第一版执行 `measure -> arrange -> hit-test`，并保存 deterministic 的 `measuredSize`、arranged `UiRect` 和继承 `clipRect`；reconcile/paint 在 U3/U4 接入实际 panel 时落地。
4. 实现 topmost-first hit-test、pointer capture、keyboard focus 和 modal root 截断。modal 打开时取消背景 capture/focus；关闭后恢复正常命中。capture 的四个字段准确反映当前 owner。keyboard/text 的实际事件 payload 在 U5 连同 `InputSystem` 解耦实现。
5. `TooltipController` 作为 runtime-owned overlay service；先复刻现有时序常量，不改变视觉内容。
6. `UIPanelDragService` 不重写：用 adapter 将它逐步接入 pointer capture，保持现有 clamp 语义。

### 测试

- Row/Column 的 fixed/fraction/auto、padding/gap、min/max 和 anchor 结果。
- 嵌套 clip/overlay 的稳定 z-order 与 hit-test。
- pointer press/drag/release 只路由给 captured node。
- modal 打开时阻断背景 pointer/keyboard/text；关闭后恢复。
- tooltip 首次 hover、目标切换、hover exit 的时间状态与 U0 合同相同。
- 同一 node tree 两次 layout 输出字节等价或逐 rect 等价。

### 完成标准

- U2 core 没有 backend API 和 gameplay API。
- 稳态节点 tree/layout 不进行无界增长；容器容量经 `Reserve` 或 arena 复用控制。
- `UiRuntime` 不向调用方暴露会被 arena/vector 扩容失效的 node 指针或引用；以 stable `UiId` 写入、snapshot 读取。

### 本轮已验证证据

- `build.bat novalidate`（`RelWithDebInfo`）通过；它仅跳过 pre-check scripts，生产代码和测试均经过编译。
- `bin/NoMoreDayTests.exe --test-case="*UI Viewport*,*UI Runtime Types*,*UI retained*,*UI runtime*,*UI tooltip*" --reporters=console` 通过 26 cases / 172 assertions。
- `ctest --test-dir build -C RelWithDebInfo -R "^nmd\\.tests\\.ui\\.unit$" --output-on-failure` 通过 1/1。
- worktree mapping、module boundary、JSON validation、render ABI governance 均通过；`git diff --check` 无空白错误。
- `build.bat` 和 `build.bat check` 仍在编译前被既有 `check_legacy_reintroduction.py` inventory baseline 阻塞：baseline `legacy=14`、当前 `legacy=15`。该差异不来自 U0-U2（新 UI core 不含 `legacy` token）；不得以更新 baseline 或修改无关 source 作为本任务的修复。

## 6. U3：`UiDrawList` 与 raylib backend

### 文件

- Create: `src/game/application/ui/UiDrawList.hpp`
- Create: `src/game/application/ui/UiDrawList.cpp`
- Create: `src/game/application/ui/UiRaylibBackend.hpp`
- Create: `src/game/application/ui/UiRaylibBackend.cpp`
- Create: `tests/unit/UiDrawListTests.cpp`
- Modify: `src/game/application/ui/CMakeLists.txt`

### 实施

1. `UiDrawList` 支持 fill/stroke rect、line、text、image/icon、clip push/pop 和 custom painter；每条命令携带 logical coordinates、`UiResourceId`、node ID 和 layer。
2. `UiRaylibBackend` 是唯一将 `UiResourceId` 映射到 raylib `Font`/`Texture2D` 并调用 `Draw*`/scissor API 的实现。
3. backend 接受已经计算好的 `UiViewport`，在提交时统一转换 logical -> native pixel；不得在 panel 内再算 screen scale。
4. push/pop clip 采用 RAII 或显式 balance guard；命令流不平衡时在 debug/test 失败，不默默修复。
5. 在 `GameplayState::OnRender` 的 scene composite 之后添加一个空 draw-list submit，验证 framebuffer/viewport/scissor 恢复，不迁移面板视觉。

### 测试和手测

- draw command append/clear/layer ordering/text ownership/clip balance。
- logical rect 在 16:9、21:9、4:3 的 backend conversion。
- 现有 `UITests.cpp` scissor balance test 保持通过。
- 手测：空 backend submit 不改变 world/scene UI 的颜色、尺寸、位置或 DRS 行为。

## 7. U4：`GameUiHost`、资源生命周期与旧 facade

### 文件

- Create: `src/game/application/ui/GameUiHost.hpp`
- Create: `src/game/application/ui/GameUiHost.cpp`
- Modify: `src/game/application/ui/UISystem.hpp`
- Modify: `src/game/application/ui/UISystem.cpp`
- Modify: `src/app/Game.cpp`
- Modify: `src/game/application/states/GameplayState.hpp`
- Modify: `src/game/application/states/GameplayState.cpp`
- Create: `tests/tech/GameUiHostLifecycleTests.cpp`

### 实施

1. `Game`（现有 composition root）拥有一个 `GameUiHost` 实例，负责资源句柄、theme、`UiRuntime`、backend 和 panel instances 的生命周期。
2. `GameplayState` 仅借用 host；进入/离开 Gameplay 调用 session reset，不允许状态通过静态变量泄漏到下一局。
3. `UISystem` 保留原 public entry points，但只转发到 host。迁移期可同步少数兼容查询；禁止新调用写 `UISystem::State`。
4. 保持 `GameUiHost::Shutdown` 在 resource unload/window close 前执行，复用 `Game::cleanup` 的既有顺序。
5. 先通过 host 调用既有 panel renderer，确保路径转换本身不改变面板输出；不要在本任务改 panel 内部。

### 验证

- Game 初始化、进入 Gameplay、离开、再次进入、cleanup 的 lifecycle tech test。
- 在 scene composite 后执行 host backend，确认 UI 仍为 native resolution。
- 现有 UI tech tests 与 `build.bat check` 全部通过。

## 8. U5：输入捕获解耦

### 文件

- Modify: `src/game/application/input/InputSystem.hpp`
- Modify: `src/game/application/input/InputSystem.cpp`
- Modify: `src/game/application/states/GameplayState.cpp`
- Modify: `tests/unit/InputSystemTests.cpp`
- Create: `tests/unit/UiInputCaptureTests.cpp`

### 实施

1. 将 `InputSystem::update` 改为接收 value-only `const UiInputCapture&`，或由同等 action router 在调用前过滤输入。
2. 在 `GameplayState::OnUpdate` 中先 sample UI raw input 和调用 `GameUiHost::Update`，再把 capture 交给玩法输入；核对既有 Update 调用顺序，不通过猜测重排。
3. 删除 `InputSystem` 对 `UISystem::IsSkillTreeVisible`、`UISystem::State.isTyping`、`UISystem::IsModalInputCaptured` 和 `UISystem::State.isMouseOverUI` 的直接读取。
4. 保持 Escape/modal/text editing、鼠标拖拽和按键技能的优先级与 U0 基线一致。

### 验证

- text focus、modal、panel hover、drag capture、无 UI focus 五种表驱动单测。
- source-level dependency guard：`InputSystem.cpp` 不包含 `UISystem.hpp`，不读取 `UISystem::State`。
- 游戏手测移动/技能/快捷键在 UI 开闭和 modal 场景正确。

## 9. U6：Snapshot、intent、命令处理和物品拾取迁移

### 文件

- Create: `src/game/application/ui/GameUiSnapshot.hpp`
- Create: `src/game/application/ui/GameUiSnapshotBuilder.hpp`
- Create: `src/game/application/ui/GameUiSnapshotBuilder.cpp`
- Create: `src/game/application/ui/GameUiIntent.hpp`
- Create: `src/game/application/ui/GameUiCommandHandler.hpp`
- Create: `src/game/application/ui/GameUiCommandHandler.cpp`
- Modify: `src/game/application/ui/UISystem.cpp`
- Modify: `src/game/application/states/GameplayState.cpp`
- Create: `tests/unit/GameUiCommandHandlerTests.cpp`

### 伪代码

```text
Update:
  snapshot = BuildSnapshot(registry, session state)
  host.Update(input, snapshot)
  for intent in host.DrainUpdateIntents():
    result = commandHandler.Execute(registry, intent)
    host.Publish(result)

Render:
  worldFrame = render adapter output
  host.PrepareRender(worldFrame)
  backend.Render(host.DrawList())
  # render-time world intents stay queued for next Update
```

`GameUiIntent::PickupItem` 只携带域 ID/来源 UI node，不承担距离或库存判断。`GameUiCommandHandler` 在 Update 中重新取得 player/item，验证距离 `180.0f`、实体有效性和容量，再调用 `InventorySystem::pickUpItem`。

### 验证

- invalid item/player、超距离、满包、堆叠成功、材料银行成功的 command-handler tests。
- test/trace guard：`UISystem::Draw` 和 backend 不调用 `InventorySystem::pickUpItem`。
- render-time点击生成 intent，下一次 Update 才产生 ECS 变化；tooltip/highlight 仍可同帧显示。

## 10. U7：Overlay 与逐面板迁移

### 固定迁移顺序

1. HUD、buffs、hotbar、minimap：验证最简单的 native layer 和 viewport。
2. character、inventory：迁移 item slot、drag preview、tooltip request 和 context action intent。
3. stash、crafting：迁移复杂选择/confirmation，并通过 snapshot/command handler 访问玩法数据。
4. skill hub、talent tree：保留 custom painter，统一 node hit-test/focus、tooltip request 和 panel instance state。
5. astrolabe：最后迁移高密度特殊画布。
6. context menu、quantity popup、message box、tooltip：统一 overlay/modal 层并删除各处手写状态机。

### 每个 panel 的原子任务模板

1. 为该 panel 写/扩展 view-model、layout、event/intent 和 tech test。
2. 用 host route 替换一个旧 `Draw/Update` 调用点，保留旧实现作为仅该 panel 的回退。
3. 对比 U0 行为、截图和 Tracy 指标；修正后删除该 panel 的静态状态。
4. 运行 focused doctest、UI tech suite、`build.bat` 和 `build.bat check`。

禁止把整个 `UIRenderer.cpp` 机械拆文件而不先拆职责。通用 draw primitive 进入 backend；item/skill/buff 语义进入 panel/presenter；tooltip/layout 进入 runtime overlay。

## 11. U8：World UI frame bridge 与 legacy 删除

### 文件

- Modify: `src/game/foundation/ui_shared/UiShared.hpp`
- Modify: `src/game/foundation/ui_shared/UiShared.cpp`
- Modify: `src/game/application/render/GameplayRenderAdapter.hpp`
- Modify: `src/game/application/render/GameplayRenderAdapter.cpp`
- Modify: `src/game/application/states/GameplayState.cpp`
- Delete after all consumers migrate: `src/game/application/ui/UIContext.hpp`
- Delete after all consumers migrate: legacy static panel state and `UISystem` facade

### 实施

1. 以 object-owned、frame-scoped `WorldUiFrame` 替换 `VisibleItemCache` 和 `HoveredItem` 静态可变槽；其中只含稳定 domain ID、screen/world hit proxy、排序/depth 和 frame token。
2. `GameplayRenderAdapter::ExecuteUIWorldPass` 写入该 frame object；`GameUiHost::PrepareRender` 只读它。不得让 Engine 持有 Game UI 类型。
3. `UiShared` 最终只保留确有 foundation 所有权的无状态合同；若没有剩余职责则删除目标并更新模块边界检查/设计。
4. 删除 `UIContext`、`UISystem::State`、旧静态 panel 成员、Draw 中的 tooltip/ground-pickup 逻辑和临时兼容 API。

### 完成标准

- grep/source guard 找不到 `UISystem::State`、`UiShared::HoveredItem`、`VisibleItemCache` 的旧静态读写。
- 新 world bridge 通过 frame token/lifetime test，不读取过期上一帧 vector。
- UI 核心、render adapter、gameplay command handler 的依赖图仍为单向，`build.bat check` 无违规。

## 12. 最终验证矩阵

| 维度 | 证据 |
| --- | --- |
| 编译 | `build.bat`（RelWithDebInfo） |
| 模块边界 | `build.bat check` |
| 单元 | `ctest --test-dir build -C RelWithDebInfo -L unit` 和 UI core focused doctest |
| UI 回归 | `bin/NoMoreDayTests.exe --test-case="*[Tech]*UI*"` |
| 集成 | `ctest --test-dir build -C RelWithDebInfo -L ci` |
| 手测 | 16:9/21:9/4:3、HUD、drag、modal/text、inventory/stash/crafting/skill/astrolabe、地面拾取 |
| 渲染 | scene composite 后 native UI、DRS 开关前后 UI 像素尺寸不变、scissor/FBO/viewport 无泄漏 |
| 性能 | Tracy baseline 对比；每个 panel migration 记录 update/layout/paint CPU、分配、draw/clip 数；超过 U0 baseline 110% 必须分析并记录 |

## 13. 完成定义

本计划完成时：

1. UI 有实例化 `GameUiHost` 和 retained `UiRuntime`，不是静态全局系统。
2. 运行时包含 viewport/layout/input/focus/modal/tooltip/draw-list 合同，且核心不依赖 gameplay/raylib。
3. 所有 screen panels 通过 snapshot/intent 与玩法交互；没有 Draw 阶段 ECS 写操作。
4. world UI 与 screen UI 通过帧内桥接，而非 `UiShared` 全局可变状态耦合。
5. `UISystem`、`UIContext`、静态 panel state 和遗留 tooltip 状态机已经删除。
6. 所有验证矩阵项均有通过证据；无法在当前硬件运行的项目明确标为 `NOT_RUN`，不隐含成功。
