# UI 系统重构遗留修复实施计划（UI System Rearchitecture Remediation Plan）

> **Status:** pending approval
>
> **Date:** 2026-08-12
>
> **Design:** [2026-08-12-ui-system-rearchitecture-remediation-design.md](../designs/2026-08-12-ui-system-rearchitecture-remediation-design.md)
>
> **Review input:** [2026-08-12-ui-system-rearchitecture-review.md](../reviews/2026-08-12-ui-system-rearchitecture-review.md)

## 1. 实施原则与边界

本计划只关闭首次审查中的 B-01、A-01、D-01、C-01、H-01、H-02、H-03、L-01，并复验原设计的 post-composite、tooltip、pickup 和生命周期合同。它不修改 RenderGraph、scene RT、DRS、shader、GPU resource ownership、Engine/Game 依赖方向、存档格式或 UI 视觉产品设计。

每个面板切片必须在同一变更中完成四件事：

1. 扩充其 immutable snapshot view model。
2. 将所有 gameplay 动作改为 intent，并在 handler 中执行。
3. 接入 retained node/runtime input/layout 和 `UiDrawList` paint。
4. 删除该切片的 legacy immediate production path，补齐自动化和性能证据。

禁止以 feature flag、legacy fallback、生产测试数据或“以后接入 runtime”的注释通过任一切片。若发现某动作没有权威 gameplay system API，暂停该切片，先更新设计；不得在 controller 或 handler 内复制组件写规则。

## 2. 实施原理与目标调用链

### 2.1 Update-only gameplay 写入

`GameplayState::OnUpdate` 将从“先 snapshot、UI 内部直接 registry 写、随后局部 handler”收敛为以下骨架：

```text
host.BeginUpdate(rawInput, viewport)
for intent in host.DrainQueuedIntentsFromPreviousPhase():
  result = commandHandler.Execute(registry, intent)
  host.Publish(result)

snapshot = snapshotBuilder.Build(registry, publishedResults)
host.Update(snapshot)  # UiRuntime reconcile/input/layout; only queues future intents

if Escape pressed and not host.EscapeConsumedThisFrame():
  stateManager.PushState(PauseState)
InputSystem.update(registry, camera, host.InputCapture())
```

`GameUiCommandHandler` 按 intent kind 做 validate -> copy required POD -> call authoritative system -> invalidate/re-fetch if needed -> result。它只在 Update 访问 registry。controller、runtime、backend 和 Render 阶段不接收 registry。

### 2.2 Retained render 路径

```text
panel snapshot + panel-local session state
  -> retained nodes / UiRuntime::UpdateInput / Arrange
  -> panel Paint(UiDrawList, immutable render data)
  -> UiDrawList::Finalize()  # fixed-capacity, deterministic, no allocation
  -> UiRaylibBackend::Render(native UiViewport, draw list)
```

`UiDrawList` 的排序 key 为 `(layer, stableNodeId, appendSequence)`；实现使用预分配、可测的无分配排序缓冲或经 allocation test 证明的等价方案。文本只引用 host-owned bounded text arena/缓存，不让 `UiDrawCommand` 在每帧拥有 `std::string`。backend 因命令已排序而单趟提交，保持 clip balance 和 framebuffer/scissor 恢复。

### 2.3 World UI 与 Escape

```text
ExecuteUIWorldPass:
  worldFrame.BeginFrame(nextToken)       # all CPU/GPU branches
  FillVisibleProxiesReadOnly(worldFrame) # all CPU/GPU branches
  if CPU loot: build CPU labels/glyphs/beams
  if GPU loot: skip only CPU label output

post-scene-composite:
  host.PrepareRender(worldFrame.AcquireView(currentToken))
  host paints draw list; a world click queues next-update pickup intent
```

`WorldUiFrameView` 失效时 reader 返回空目标。Escape 的关闭顺序由 runtime modal/z-order/last-opened state 决定，host 每帧只报告一个 `EscapeConsumedThisFrame`；Pause 仅在该值为 false 时发生。

## 3. 原子任务与顺序

### [x] R0：冻结修复基线与 profiling 能力

**依赖：** 无。必须在任何生产代码变化前完成。

**文件/产物：**

- Modify: `docs/reports/ui-system-rearchitecture/baseline.md`，追加明确标为 R0 remediation baseline 的环境、revision、场景、输入、样本数、CPU p95、allocation、draw/clip/overflow 和截图证据。
- Create: `docs/reports/ui-system-rearchitecture/remediation-evidence.md`，作为后续逐项证据索引。
- Inspect/possibly modify: 项目 profiling CMake 配置和相关 game target，仅使用 `docs/workflows/performance.md` 已说明的 `TRACY_ENABLE`/`TRACY_ENABLE_ALLOCATORS` 支持路径；不得下载或添加未批准依赖。

**实施：**

1. 固定被审查 revision、RelWithDebInfo、硬件/驱动、质量设置、GPU loot/text、DRS、固定场景与输入脚本/人工步骤。
2. 验证现有 Tracy client/profiling build 是否可构建；若可用，为 host Update、runtime layout/paint、backend Render 和 UIWorld proxy 采集可定位 zone 与 allocation 数据。
3. 同时记录现有 RenderProfiler 或 VS/WPR 兜底数据，但不得把没有 Tracy 的兜底数据伪称为 Tracy 通过。
4. 记录最大 command/clip/text/scratch 使用量，为固定容量选择提供来源；R0 不是缺失的 U0，报告必须保留此差异。

**完成信号：** R0 artifact 可复现；若 profiling build 或目标硬件不可用，写明 blocker、owner、范围，后续 H-03 不得关闭。

### [x] R1：冻结 snapshot、intent、result 与 handler 域合同

**依赖：** R0。

**主要文件：**

- Modify: `src/game/application/ui/GameUiSnapshot.hpp`
- Modify: `src/game/application/ui/GameUiSnapshotBuilder.hpp`
- Modify: `src/game/application/ui/GameUiSnapshotBuilder.cpp`
- Modify: `src/game/application/ui/GameUiIntent.hpp`
- Modify: `src/game/application/ui/GameUiCommandHandler.hpp`
- Modify: `src/game/application/ui/GameUiCommandHandler.cpp`
- Modify: `src/game/application/states/GameplayState.cpp`
- Modify/Add: `tests/unit/GameUiCommandHandlerTests.cpp`、`tests/unit/UiPickupFlowTests.cpp` 及按域拆分的 intent/snapshot tests。

**实施：**

1. 给 snapshot 加 revision 和所有 panel 所需的只读 view model；逐项列出旧 controller 读取的 component 数据，转换为 value、domain ID、resource/text ID 或有界格式缓存。
2. 将 intent 扩展为单一 tagged POD payload，覆盖 pickup、inventory/equipment、stash、crafting/salvage、attribute 的全部 gameplay 动作；加入源/目标 slot、数量、tab、affix 和 flag 等必要参数。
3. 保持 `GameUiCommandHandler` 单一入口，按 domain 私有分派；对没有权威 API 的 lock/attribute 写，先补 system-owned operation，再由 handler 调用。
4. 先执行上一 phase 的 intents，再 build snapshot，最后 host Update；失败和成功均通过 `GameUiResult` 回到下一 snapshot/notification。
5. 将 drag/crafting session 改为只保存 domain ID 和位置 metadata；破坏性成功后由 result 清空相应 session。

**测试：** 每个 intent kind 至少覆盖成功、无 player、失效 target、无组件、非法 slot/tab/index、容量/距离/权限失败，以及“mutator 后重新取得数据”的 EnTT 安全回归。

**完成信号：** `GameUiIntent.hpp`/snapshot core 仍无 EnTT/raylib/gameplay header；所有新动作可经 handler 被验证和执行，尚未迁移的 controller 不能直接使用新增 contract 声称完成。

### [x] R2：移除生产测试注入并建立 fixture 数据

**依赖：** R1。

**主要文件：**

- Modify: `src/game/application/ui/GameUiHost.hpp`
- Modify: `src/game/application/ui/GameUiHost.cpp`
- Modify/Add: 使用 test items/skills/buffs 的 `tests/unit/*Tests.cpp`、`tests/tech/GameUiHostLifecycleTests.cpp`。

**实施：**

1. 删除 `m_hasGivenTestItems` 和完整的 bag/skill/buff 注入 block，不保留 debug-only production fallback。
2. 将每个测试所需 item、bag、active skill、effect 和 `StatsDirty` 明确建立在 test fixture/harness 中。
3. 清理 host 生命周期 reset 中仅服务该注入的状态，确认进入/离开 Gameplay 不改变真实 player inventory、skill slot 或 active effects。

**测试：** source guard 禁止 `m_hasGivenTestItems`、`test_power`、`test_speed`、`test_stun`、`test_poison` 和 production host 内 `ItemFactory::createBag` 测试注入；生命周期测试断言 host 初始化和进入 Gameplay 不生成上述数据。

**完成信号：** D-01 关闭；不新增存档迁移或按名称删除历史数据。

### [x] R3：修复 WorldUiFrame 生命周期与 Escape 单一所有者

**依赖：** R1。

**主要文件：**

- Modify: `src/game/application/ui/WorldUiFrame.hpp`
- Modify: `src/game/application/ui/WorldUiFrame.cpp`
- Modify: `src/game/application/render/GameplayRenderAdapter.cpp`
- Modify: `src/game/application/ui/GameUiHost.hpp`
- Modify: `src/game/application/ui/GameUiHost.cpp`
- Modify: `src/game/application/ui/TooltipController.cpp`
- Modify: `src/game/application/states/GameplayState.cpp`
- Modify/Add: `tests/unit/WorldUiFrameTests.cpp`、Escape/host input tests、UIWorld integration/tech tests。

**实施：**

1. 将 `BeginFrame(nextToken)` 移到 `ExecuteUIWorldPass` 的所有 early return 前，定义 acquire-view/token match API，并让 host/tooltip/pickup reader 显式拒绝 invalid view。
2. 从 CPU label collection 抽取只读 visible proxy producer；CPU 与 GPU loot 都调用它，GPU 分支只跳过 CPU label/glyph/beam 输出。不得在抽取代码新增 component `get_or_emplace` 或 RenderGraph 改动。
3. 让 `GameUiHost` 在 Update 内处理完整 Escape close policy，含 quantity、character confirmation、context、skill tree、astrolabe、inventory、stash、crafting；只关闭 topmost surface，并在处理后公布 consumed 标记。
4. 将 `PauseState` 判断移动到 host Update 后，删除以 inventory 可见性代理所有 UI 消费的逻辑。

**测试：**

- CPU loot -> GPU loot -> CPU loot 切换 token 递增、proxy 清空/重建、tooltip/click 不读 stale entity。
- valid/invalid `WorldUiFrameView` 行为和无 compute/未绑定 empty-view 降级。
- 对每个可关闭 surface 的 Escape 真值表：仅最上层关闭、不会暂停；无 UI 消费时仅暂停一次。

**完成信号：** H-01 和 H-02 的回归链通过，且 screen UI 仍在 post-composite native framebuffer 绘制。

### [x] R4：使 runtime/draw-list 成为真实 host 管线

**依赖：** R1、R3。

**主要文件：**

- Modify: `src/game/application/ui/UiRuntime.hpp`
- Modify: `src/game/application/ui/UiRuntime.cpp`
- Modify: `src/game/application/ui/UiDrawList.hpp`
- Modify: `src/game/application/ui/UiDrawList.cpp`
- Modify: `src/game/application/ui/UiRaylibBackend.hpp`
- Modify: `src/game/application/ui/UiRaylibBackend.cpp`
- Modify: `src/game/application/ui/GameUiHost.hpp`
- Modify: `src/game/application/ui/GameUiHost.cpp`
- Modify/Add: `tests/unit/UiRuntimeInputTests.cpp`、`tests/unit/UiDrawListTests.cpp`、`tests/unit/UiRaylibBackendTests.cpp`、host pipeline tests。

**实施：**

1. 删除 `PrepareRender` 的空 `Clear/Reserve(64)` 占位行为；在 Update 真实调用 viewport fit、runtime reconcile/input/layout，Render 真实 paint/finalize/submit。
2. 将 command、clip、text arena 和排序 scratch 设为 host-owned、初始化期 reserve、容量可测的缓冲；命令溢出记录 telemetry 并使测试失败，不触发热路径扩容。
3. 以 total ordering 取代 `vector::insert` 有序插入，保持 layer/node/append determinism；backend 改为按已排序命令单趟提交。
4. 注册实际 font/texture/custom painter resource ID，确保 raylib 类型只在 backend；删除 hidden placeholder root 和 `GameUiHost::Draw` 向 controller 传 registry 的公共生产接口。

**测试：** non-empty command list、clip balance、稳定排序、text 生命周期、零分配 append/finalize、16:9/21:9/4:3 logical-to-native conversion、modal/pointer/text capture。

**完成信号：** 新 host 管线可渲染至少一个真实 panel，且没有 immediate fallback；这只是 A-01 基础，未迁移 panel 仍不能标记完成。

### [x] R5：迁移无 gameplay 写的显示 panel

**依赖：** R4。

**主要文件：**

- Modify: `src/game/application/ui/PlayerHudController.hpp`
- Modify: `src/game/application/ui/PlayerHudController.cpp`
- Modify: `src/game/application/ui/MonsterHealthBarController.hpp`
- Modify: `src/game/application/ui/MonsterHealthBarController.cpp`
- Modify: minimap、hotbar、buff、SwordIntentWidget 对应 controller 文件
- Modify/Add: 对应 snapshot builder、unit tests、UI draw-list determinism tests、performance benchmarks。

**实施：**

1. HUD/minimap/hotbar/buff/monster health 改为 snapshot-only paint；将数值和召唤组/affix 文本在 Update 或数据变化时写入复用缓存。
2. 用 controller-owned capped batch/scratch 替换每帧 `std::map`、`std::vector`、`std::string` 和临时 label 拼接。
3. 以 retained root/node 管理 hit/capture（无交互 panel 也必须归属正确 layer/clip），并删除旧 raylib immediate Draw。

**测试：** 固定 snapshot 的 command 序列、数值变更才重新格式化、稳态 draw 零 allocation、HUD/monster health 行为回归。

**完成信号：** C-01 中 HUD/monster health 发现关闭，真实命令在 `Hud` layer 输出。

### [x] R6：迁移 overlay、character 和 inventory

**依赖：** R1、R4、R5。

**主要文件：**

- Modify: `src/game/application/ui/OverlayController.hpp`
- Modify: `src/game/application/ui/OverlayController.cpp`
- Modify: `src/game/application/ui/UIRenderer.cpp`
- Modify: `src/game/application/ui/UICharacterController.hpp`
- Modify: `src/game/application/ui/UICharacterController.cpp`
- Modify: `src/game/application/ui/UIInventoryController.hpp`
- Modify: `src/game/application/ui/UIInventoryController.cpp`
- Modify/Add: overlay, inventory, character, intent handler and integration tests。

**实施：**

1. quantity/context/message/tooltip 迁为 runtime modal nodes；quantity input、open/close 和 local validation 属 UI state，confirm 只 enqueue drop/destroy/use/etc. intent。
2. character 的 temporary point 编辑保持 controller-local；确认分配改 `AssignAttribute` intent，移除 Draw 内 `get_or_emplace`、PrimaryStats/PlayerStats/StatsDirty 写。
3. inventory 的 drag/session 只保存 ID 与 source/target metadata；equip/unequip/move/swap/socket/drop/destroy/lock/organize/bag 操作全部 enqueue intent，handler 重新验证并调用 system。
4. 将 inventory materials filter、scroll、slot painter、drag preview、context open 和 tooltip request 接入 snapshot/runtime/draw-list；filter 和小写缓存只在 query/category/data revision 改变时重建。
5. 删除 `UIRenderer::DrawContextMenu`、`OverlayController::DrawQuantityPopup`、`UICharacterController::Draw`、`UIInventoryController::Draw` 中 registry/gameplay 写和旧 immediate path。

**测试：** 每项 inventory/character/overlay intent 的成功/拒绝；quantity 不在 Render 执行；drag drop 后销毁 target/session 清理；attribute confirm；modal Escape；固定 snapshot draw list；source guard 禁止 Draw 内 mutator。

**完成信号：** 审查报告列出的 Overlay、UIRenderer、Character、Inventory B-01 位置归零，并通过 retained input/capture 与行为测试。

### [x] R7：迁移 stash 与 crafting/salvage

**依赖：** R1、R4、R6。

**主要文件：**

- Modify: `src/game/application/ui/UIStashController.hpp`
- Modify: `src/game/application/ui/UIStashController.cpp`
- Modify: `src/game/application/ui/UICraftingController.hpp`
- Modify: `src/game/application/ui/UICraftingController.cpp`
- Modify/Add: stash/crafting snapshot models、handler domain tests、panel integration/performance tests。

**实施：**

1. stash tab、slot、lock/sort/auto-deposit state 由 snapshot/view model 提供；unlock、quick withdraw、transfer、deposit、sort、auto-deposit 都转 intent。
2. 删除跨操作保存的 `StashTab*`；handler 每次按 tab/slot 重取并验证，系统操作后不复用旧 pointer。
3. crafting/merge/salvage UI-local selection 使用 domain ID；affix 操作、fuse、salvage/batch salvage 全部转 intent。
4. handler 只在单次 system 调用前短暂获取 `ItemComponent`；mutator 后立即丢弃引用，成功后清理 forge/merge/salvage UI session。
5. 以 capped reusable vectors 取代 crafting Draw 临时 vector；删除两个 controller 的 immediate registry Draw。

**测试：** stash 所有 transfer 方向、非法 tab/slot、失效 item；craft affix/fuse/salvage 失败和成功、destructive action 后 session 清除、EnTT reference safety、稳态 allocation、Escape topmost policy。

**完成信号：** `UIStashController.cpp:312-313` 指针生命周期问题和 `UICraftingController.cpp:500` 引用生命周期问题消失，所有动作只在 handler Update 运行。

### [x] R8：迁移 skill/astrolabe、收口所有 immediate surface

**依赖：** R4、R6、R7。

**主要文件：**

- Modify: `src/game/application/ui/SkillHotbarController.*`
- Modify: `src/game/application/ui/SkillTreeController.*`
- Modify: `src/game/application/ui/UISkillHub.*`
- Modify: `src/game/application/ui/AstrolabeController.*`
- Modify: tooltip/drag 相关 controller 和 `GameUiHost.*`
- Modify/Add: skill/astrolabe snapshot, intent, runtime and visual-regression tests。

**实施：**

1. 将 skill/hotbar/tree/astrolabe 的数据读取收敛到 snapshot；将可改变 gameplay 的选择、激活或解锁操作经 intent/handler 处理。
2. 用 custom painter 承载密集画布的内部图形，但 clip、focus、modal、hit-test 和 layer 仍由 retained tree 负责。
3. 将 tooltip 作为 runtime overlay，维持 `0.12s/0.05s/0.08s` 与 `dt*10`/`dt*8` 合同；drag preview 位于 `DragPreview` layer。
4. 删除最后的 controller direct raylib/immediate routes、`GameUiHost` legacy Draw compatibility comments/API 和无消费的 snapshot 副本。

**测试：** skill/astrolabe 交互、tooltip transition/session reset、custom painter clip/focus、draw order、source guard 和 production non-empty draw list。

**完成信号：** 所有 screen panel 都是 snapshot/intent/runtime/draw-list 路径，A-01 和 B-01 的全局完成条件成立。

### [x] R9：性能、卫生、验证矩阵与跟进审查

**依赖：** R2-R8。

**主要文件/产物：**

- Modify: `docs/reports/ui-system-rearchitecture/baseline.md`
- Modify: `docs/reports/ui-system-rearchitecture/remediation-evidence.md`
- Modify: [2026-08-12-ui-system-rearchitecture-review.md](../reviews/2026-08-12-ui-system-rearchitecture-review.md)，按 review workflow 追加跟进审查，不删除首次结论。
- Modify: 受本修复改动影响的 whitespace/迁移注释文件，尤其 `src/game/application/ui/UIStashController.hpp`。
- Add/Modify: `tests/performance/UiDrawListBenchmark.cpp`、panel steady-allocation benchmarks 或现有等价性能测试。

**实施：**

1. 用 allocation counter、benchmark、RenderProfiler/Tracy 对每个 migrated panel 的 Update/layout/paint、backend submit、draw/clip/text arena/overflow 采样；与 R0 同场景比较，记录 p95 和 110% 判断。
2. 执行宽高比、DRS、GPU loot/text、drag、modal/text input、inventory/stash/crafting/skill/astrolabe、ground pickup 的手测矩阵并归档截图/trace 路径。
3. 清理本任务 diff 中的 trailing whitespace/blank EOF，校正当前态错误的 `UISystem::State` fallback 注释；不把历史迁移说明写成活动接口文档。
4. 对遗留 static symbol、Render mutation、direct gameplay mutator、test injection 和 UI core dependency 运行 source/module guards。
5. 只有所有 blocker/high 都关闭且证据完整时，发起跟进审查；任何 `NOT_RUN`、硬件不可用或外部构建门禁都必须写为风险/阻塞，不能转换成通过。

**完成信号：** H-03、L-01 关闭，审查报告有新的复查轮次和可核验证据。

## 4. 测试方法

### 4.1 自动化层级

| 层级 | 覆盖 |
| --- | --- |
| Unit | snapshot builder、intent payload、handler 全部失败/成功矩阵、WorldUiFrame token、Escape consumed、runtime capture/layout、draw-list ordering/text/clip/capacity、controller cached formatting。 |
| Integration/tech | snapshot -> intent -> handler -> next snapshot、CPU/GPU loot switch、host lifecycle、post-composite command submit、modal/drag/tooltip/session reset。 |
| Source/dependency guard | UI core 不含 EnTT/raylib/gameplay；screen Draw/paint 不含 registry mutator、gameplay system 写、`registry.destroy`、`get_or_emplace` 或 direct component assignment；production host 无 test injection。 |
| Performance | allocation-counter steady-frame assertions、draw-list/panel benchmark、Tracy/RenderProfiler CPU p95、draw/clip/text/overflow telemetry。 |
| Manual | 分辨率、DRS、GPU modes、panel/overlay/input、pickup、视觉顺序、scissor/FBO/viewport 恢复。 |

### 4.2 命令与证据纪律

实施时按窄到宽运行，所有构建使用 `RelWithDebInfo`：

```powershell
./build.bat
ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure
ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure
./bin/NoMoreDayTests.exe --test-case="*UI*,*WorldUiFrame*,*GameUiCommandHandler*"
./build.bat check
git diff --check
```

已知 `check_legacy_reintroduction.py` inventory mismatch（`legacy` baseline `14`、current `15`）可能在 `build.bat`/`build.bat check` 编译前阻塞。先如实记录标准门禁结果；仅为取得编译证据才运行经批准的 `./build.bat novalidate` 和受影响目标测试，且绝不将其称为完整门禁通过或修改无关 inventory。

Tracy capture 使用 `docs/workflows/performance.md` 的受支持命令，artifact 记录 build revision、场景、输入、样本帧数和输出路径：

```text
%NMD_TRACY%\tracy-capture.exe -o <artifact>.tracy -a 127.0.0.1 -s <seconds>
%NMD_TRACY%\tracy-csvexport.exe <artifact>.tracy > <artifact>.csv
```

### 4.3 手测矩阵

至少覆盖下列固定场景组合，并在 evidence 报告中记录结果、截图/trace 路径与未运行原因：

| 维度 | 必测值 |
| --- | --- |
| 分辨率 | 1920x1080 (16:9)、3440x1440 (21:9)、1280x960 (4:3) |
| 渲染设置 | DRS on/off；GPU loot on/off；GPU text on/off |
| UI surface | HUD、drag、quantity/context/message、inventory materials、character、stash、crafting merge/salvage、skill tree、astrolabe、tooltip |
| 世界交互 | CPU -> GPU -> CPU loot 切换、hover、click pickup、满包/超距/失效 item |
| 压力 | monster density 20/60/120；summons 0/5/10；60 FPS/16.67 ms 预算观察 |
| 输入 | Escape topmost close、text input、pointer capture、gameplay input 未被误拦截 |

## 5. 完成定义与退出门禁

本计划只有同时满足以下条件才算完成：

1. R0-R9 全部标记 `[x]`，每项都有对应 artifact 或测试结果。
2. 所有 screen panel 只经 snapshot/intent/runtime/draw-list 生产路径运行；没有 immediate fallback，也没有 Render/ECS 写。
3. 所有 review Blocker 和 High 有明确的回归测试，Low 的 whitespace/注释问题清零。
4. `WorldUiFrame` 的 CPU/GPU 生命周期、Escape ownership、test-data removal、tooltip/pickup/post-composite 合同均可观察验证。
5. R0 与 candidate 的 CPU p95、draw/clip 和 allocation evidence 完整；candidate 不超过 R0 的 110%，已迁移 hot path 稳态零 allocation，overflow 为零。
6. 标准 build/check/CTest 结果如实记录。外部 legacy baseline 阻塞和硬件/Tracy 不可用项不得被描述为通过。
7. 手测矩阵、性能 artifact、source guards 和 `git diff --check` 都有结果，并已在首次审查报告追加跟进审查。

## 6. 当前执行状态

- [x] 读取首次审查、原设计/计划、测试/性能工作流、代码标准和渲染约束。
- [x] 用代码图谱核对 `GameUiHost`、snapshot/intent/handler、runtime/draw-list/backend、WorldUiFrame、UIWorldPass、Escape 与受影响 controller 的调用边界。
- [x] 产出本设计与实施计划。
- [ ] R0-R9 生产代码、测试、性能采样和跟进审查。

本轮按用户要求只修改文档，不实施本计划中的生产代码或测试代码。
