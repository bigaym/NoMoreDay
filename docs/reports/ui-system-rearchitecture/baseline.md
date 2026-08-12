# UI System Rearchitecture Baseline

> **Date:** 2026-08-11
>
> **Status:** static contract baseline complete; runtime performance evidence `NOT_RUN`.
>
> **Scope:** records the behavior and frame-boundary facts that UI migration must preserve. This is not a claim that retained-runtime performance has been measured.

## Current Frame Contract

```text
Game::run
  -> StateManager::Update
  -> BeginDrawing / ClearBackground
  -> StateManager::Render
  -> EndDrawing

GameplayState::OnRender
  -> RenderSystem::render into m_sceneRT
     -> GameplayRenderAdapter::ExecuteUIWorldPass
  -> DrawTexturePro m_sceneRT to default framebuffer
  -> UISystem::Draw and native screen overlays
```

Evidence:

- `src/app/Game.cpp:346-416`
- `src/game/application/states/GameplayState.cpp:983-1157`
- `src/engine/render/RenderSystem.cpp:1069-1697`
- `src/game/application/render/GameplayRenderAdapter.cpp:572-819`

Screen UI is native framebuffer output after the scene composite. DRS may affect
world/HDR targets, but must not scale HUD, menu, text, tooltip, or modal UI.

## Existing UI Behavior Contract

| Area | Current behavior | Evidence |
| --- | --- | --- |
| Logical reference area | `2560x1440`; scale is `min(screenWidth/2560, screenHeight/1440)` | `src/game/application/ui/UICommon.hpp:11-12`, `src/game/application/ui/UISystem.cpp:531-535` |
| Pointer conversion | inverse scale in UI helpers; current code does not retain a centralized letterbox content rect | `src/game/application/ui/UISystem.cpp:185-191` |
| UI input gate | gameplay input reads `UISystem::State.isTyping`, UI hover/modal queries and skill-tree visibility | `src/game/application/input/InputSystem.cpp:13-152` |
| Tooltip first hover | delay `0.12f`, then alpha increases by `dt * 10.0f` | `src/game/application/ui/UISystem.cpp:649-665` |
| Tooltip target switch | delay `0.05f` while already visible | `src/game/application/ui/UISystem.cpp:649-655` |
| Tooltip exit | delay `0.08f`, then alpha decreases by `dt * 8.0f` | `src/game/application/ui/UISystem.cpp:667-686` |
| World item hover | render-produced `UiShared::VisibleItemCache::visibleItems`, mouse converted with camera | `src/game/application/ui/UISystem.cpp:567-607` |
| World item pickup | left click calls `InventorySystem::pickUpItem` only if squared distance is at most `180.0f * 180.0f`; full bag opens a two-second message | `src/game/application/ui/UISystem.cpp:608-628` |
| Gameplay ownership | `InventorySystem::pickUpItem` validates entities/components, handles stacks/materials/effects and marks spatial grid dirty | `src/game/systems/item/InventorySystem.cpp:73-235` |
| Panel drag | one active header capture, reference-space clamp retaining 50 visible pixels | `src/game/application/ui/UIPanelDragService.cpp:9-54` |

## State And Dependency Baseline

- `UIContext` at `src/game/application/ui/UIContext.hpp:26-123` owns unrelated resource, session, panel, drag, hover, tooltip and modal fields.
- `UISystem::Update` at `src/game/application/ui/UISystem.cpp:220-525` writes gameplay/UI state and performs nearby pickup logic; `UISystem::Draw` at `:527-694` also writes state and invokes gameplay pickup.
- Static panel state exists in `UIInventory`, `UICrafting`, `UIStash`, skill UI and renderer helpers.
- `UiShared` at `src/game/foundation/ui_shared/UiShared.hpp:25-58` bridges item, renderer and UI domains. It currently carries `VisibleItemCache`, hovered item, font and rarity state.
- `NoMoreDayGameUi` is the existing application-band static target; no new target is required for the first foundation slice.

## Automated Regression Surface

| Suite | Current relevance |
| --- | --- |
| `tests/tech/UITests.cpp` | drag/drop, context menu, scissor balance, tooltip, inventory fallback/swap, mastery UI, health/HUD rendering |
| `tests/unit/UIPanelDragServiceTests.cpp` | panel capture, clamp and release |
| `tests/unit/InputSystemTests.cpp` | typing/input gate behavior |
| `tests/CMakeLists.txt` | registers `[Unit]*UI*`, `[Integration]*UI*`, full CI and module-gate labels |

## Performance And Visual Evidence

| Item | Status | Reason / required follow-up |
| --- | --- | --- |
| Tracy UI update/draw CPU baseline | `NOT_RUN` | Requires a representative local gameplay scene and interactive runtime capture. Record update/layout/paint time before U4/U7 integration. |
| Steady-state allocations | `NOT_RUN` | Capture after U2 and compare against the current scene. |
| 16:9/21:9/4:3 visual screenshots | `NOT_RUN` | Capture before first migrated panel and compare after each panel cutover. |
| DRS-on screen UI verification | `NOT_RUN` | Requires active render path/hardware evidence; screen UI must remain native-sized. |

`NOT_RUN` is deliberately not treated as pass. The implementation plan requires these
artifacts before a panel migration is accepted.

## First Foundation-Slice Exit Criteria

1. `UiRuntimeTypes` and `UiViewport` compile without raylib, EnTT, `UiShared` or gameplay-system headers.
2. Unit tests fix the initial logical/pixel and letterbox contract.
3. No existing UI draw/update call site changes in this slice.
4. `build.bat`, focused viewport tests and `build.bat check` provide build/module evidence.

## U2 Foundation Evidence

The retained layout/input/tooltip core has been added without changing legacy UI call
sites, rendering order, `UiShared`, or gameplay interaction ownership.

| Check | Result |
| --- | --- |
| `build.bat novalidate` in `RelWithDebInfo` | `PASS` (production/test compilation; validation scripts intentionally skipped) |
| Focused U1/U2 doctest selection | `PASS`: 26 cases, 172 assertions |
| `nmd.tests.ui.unit` CTest group | `PASS`: 1/1 |
| Mapping, module-boundary, JSON and render ABI checks | `PASS` |
| `build.bat` / `build.bat check` | `BLOCKED` before compile by pre-existing legacy-marker inventory mismatch (`legacy`: baseline 14, current 15) |

The blocked full gate is not treated as a successful verification. It must be resolved
by an owner-approved review of the repository-wide P0-1 inventory, not by this UI
foundation change.

## R0 Remediation Baseline (2026-08-12)

> R0 freezes the fix baseline for the remediation plan
> (`docs/plans/2026-08-12-ui-system-rearchitecture-remediation-plan.md`). It is a NEW
> baseline taken at the reviewed revision — **not** the historical U0 baseline that
> the first review found missing. U0 was never captured as a reproducible artifact;
> R0 is the reproducible freeze point for R1-R9. The distinction is deliberate: R0
> must not be renamed to U0, and it makes no U0-equivalent claim.

### R0.1 Frozen Environment

| Item | Frozen value |
| --- | --- |
| Reviewed revision | `018906b42f8e495fbdf595db64aa53640c6c2789` (`018906b4`, from `git rev-parse HEAD`; R0 makes no production-code change) |
| Build configuration | `RelWithDebInfo` (`build.bat` default; `docs/workflows/performance.md`) |
| Motherboard | MAXSUN MS-Terminator B650M |
| CPU | AMD Ryzen 7 7700 8-Core Processor |
| Primary GPU | NVIDIA GeForce RTX 4070 SUPER, driver `32.0.15.9186` |
| Other GPU devices | AMD Radeon(TM) Graphics (integrated, driver `32.0.21030.2001`); GameViewer Virtual Display Adapter (driver `15.6.5.199`) |
| GPU loot | `render.gpuLoot.enabled` = `false` (`settings.json`, nested key under `render.gpuLoot` and flat key `"render.gpuLoot.enabled"` both present) |
| GPU text | `render.gpuText.enabled` = `false` (`settings.json`, nested and flat keys) |
| DRS | `render.adaptiveQuality.dynamicResolutionEnabled` = `false`; `renderScale` = `1.0`; `renderScaleLocked` = `true` (`settings.json`) |
| Auto-detect quality | `renderQualityAutoDetect.selectedTier` = `Ultra`, `capabilityTier` = `Ultra`, `benchmarkTier` = `Ultra`, `renderer` = `"NVIDIA GeForce RTX 4070 SUPER/PCIe/SSE2"`, `benchmarkScore` = `124.29959869384766` (`settings.json`) |
| Fixed scenes and input | Plan §4.3 manual-test matrix scenarios: resolutions 1920x1080 (16:9) / 3440x1440 (21:9) / 1280x960 (4:3); render toggles DRS / GPU-loot / GPU-text; UI surfaces (HUD, drag, quantity/context/message, inventory materials, character, stash, crafting merge/salvage, skill tree, astrolabe, tooltip); world interactions (CPU->GPU->CPU loot switch, hover, click pickup, full-bag / out-of-range / invalid item); stress (monster density 20/60/120, summons 0/5/10, 60 FPS / 16.67 ms); input (Escape topmost close, text input, pointer capture) |
| Sample count | `NOT_RUN` — reason, owner and scope recorded under R0.4 blockers B-R0-1/B-R0-2 |

### R0.2 Profiling Capability Verification

| Check | Result |
| --- | --- |
| Tracy toolchain at `%NMD_TRACY%` (`F:\devtools\tracy`) | `AVAILABLE`: `tracy-capture.exe`, `tracy-csvexport.exe`, `tracy-profiler.exe` (Tracy v0.13.1) |
| Repo CMake `TRACY_ENABLE` / `TRACY_ENABLE_ALLOCATORS` support path | `ABSENT`: no `TRACY` reference in the top-level `CMakeLists.txt`, any `src/**/CMakeLists.txt`, or the vendored `third_party/raylib` CMake and sources |
| Profiling build can be built | `NO` — blocker B-R0-1 |
| Fallback profiler available | `AVAILABLE`: `src/engine/render/debug/RenderProfiler.hpp` (`RenderProfiler`, 120-frame window, four-state GPU query model, single `FlushRingToProfiler` poll point) — fallback data only, never reported as a Tracy pass |

Blockers:

- **B-R0-1 — profiling build not buildable.** The repo has no Tracy integration
  (no `TRACY_ENABLE` in CMake; vendored `third_party/raylib/src` contains no Tracy
  code) and no Tracy client headers are on any include path. A Tracy build requires
  a CMake `TRACY_ENABLE` compile definition plus making Tracy client sources/headers
  available to the build — a build/dependency change that is out of scope for R0's
  freeze. Owner: implementation owner of R1-R9 (no performance evidence can be
  captured until this is resolved). Scope: R0 freezes the baseline only; the Tracy
  build integration is a required follow-up before any performance evidence item can
  be marked `PASS`. Impact: finding **H-03 stays open**; it must not be closed by any
  non-Tracy fallback data.
- **B-R0-2 — no interactive capture session.** A representative gameplay capture
  requires launching the GUI game under a Tracy-enabled build with manual input; the
  current environment has no headed session and no Tracy build to run. Owner:
  implementation owner (manual-test matrix executor). Scope: capture happens during
  R1-R9 manual matrix runs once B-R0-1 is resolved.

### R0.3 Fallback Profiling Data

- `RenderProfiler` exists at `src/engine/render/debug/RenderProfiler.hpp:53`
  (`kWindowSize = 120`; per-pass `PassTimingStats`; GPU query four-state model
  Pending / Valid / Unavailable / CpuFallback; `FlushRingToProfiler` single poll
  point at the end of `RenderSystem::render`). Not executed this round: requires a
  full build and a running game session.
- Per plan §R0 step 3: fallback data is recorded as fallback only. Non-Tracy data
  must not be described as a Tracy pass, and `NOT_RUN` items are not treated as pass.

### R0.4 Capacity Observation Sources

Current fixed capacities and peak-usage observation points (sources for the R4
host-owned fixed-capacity buffers):

| Purpose | Location | Current capacity | Peak observation |
| --- | --- | --- | --- |
| Runtime node arena | `src/game/application/ui/GameUiHost.cpp:72` `m_runtime.Reserve(64)` (in `Initialize`) | 64 nodes | after layout reconcile, observe node count (`UiRuntime` owns `std::vector<Node> m_nodes`, no initial capacity) |
| Draw-list commands + clips | `src/game/application/ui/GameUiHost.cpp:538` `m_drawList.Reserve(64)` (in `PrepareRender`, after per-frame `Clear`) | 64 commands + 64 clips | after host paint/finalize read `UiDrawList::Commands()` / `Clips()` sizes (`UiDrawList.cpp:145-151`); `PrepareRender` is invoked every frame from `GameplayState.cpp:1125` |
| `UiDrawList::Reserve` implementation | `src/game/application/ui/UiDrawList.cpp:26-29` | reserves `m_commands` and `m_clips` with the same capacity | — |
| `UiRuntime::Reserve` implementation | `src/game/application/ui/UiRuntime.cpp:27` | `m_nodes.reserve(nodeCapacity)` | — |
| Monster health bar batch | `src/game/application/ui/MonsterHealthBarController.cpp:93` and `MonsterHealthBarSystem.cpp:48` `batch.reserve(200)` | 200 | — |
| Inventory filtered material list | `src/game/application/ui/UIInventoryController.cpp:782` `filteredList.reserve(bank->materials.size())` | sized from materials | — |
| Renderer display tags | `src/game/application/ui/UIRenderer.cpp:1321` `displayTags.reserve(tags.size())` | sized from tags | — |
| Text arena / scratch | **does not exist** — `UiDrawCommand` owns a per-command `std::string text` (`src/game/application/ui/UiDrawList.hpp:31-41`); this is the R4 host-owned bounded text-arena migration source | n/a | per-frame text bytes accumulated across commands; R4 replaces it with a host-owned arena |

Peak-usage observation method: record `Commands().size()` / `Clips().size()` after the
paint pass (before backend submit); overflow telemetry = `UiDrawList::ClipBalanced`
depth/underflow flags plus a post-append size-vs-capacity comparison. The per-frame
`PrepareRender` contract (`Clear` + `Reserve(64)`) is the placeholder behavior that
R4 step 1 deletes.

### R0.5 Evidence Status

| Item | Status | Reason / owner / scope |
| --- | --- | --- |
| CPU p95 (host update / runtime layout/paint / backend render) | `NOT_RUN` | needs Tracy build (B-R0-1); owner: R1-R9 implementer; scope: capture on first Tracy-enabled build |
| Steady-state allocation profile | `NOT_RUN` | same blocker (B-R0-1) |
| Draw/clip/overflow telemetry | `NOT_RUN` | same blocker (B-R0-1) |
| 16:9 / 21:9 / 4:3 screenshots | `NOT_RUN` | needs headed game session (B-R0-2) |
| DRS-on screen UI verification | `NOT_RUN` | needs headed game session (B-R0-2) |

R0 artifact: environment, revision, scene definition, capacity sources and profiling
capability verification are fully recorded and reproducible (the frozen values can be
re-derived from `settings.json`, `git rev-parse HEAD` and a CMake grep at any time).
Performance numbers themselves are not captured because of B-R0-1/B-R0-2;
consequently H-03 stays open.

## R9 性能证据更新 (2026-08-13)

> R0 冻结时上表四项为 NOT_RUN（B-R0-1/B-R0-2）。R9 修复任务按计划 §R9 处置：可自动化的性能/分配证据已实测并归档至
emediation-evidence.md §R9；仍受硬件/无头会话限制的项保留 NOT_RUN 但附原因与 owner，**不转通过**。

| Item | Status（R9 更新） | Evidence |
| --- | --- | --- |
| CPU update/draw 性能基线（替代 Tracy） | PASS (替代证据) | 	ests/performance/UiDrawListBenchmark.cpp：稳态 256 命令/16KB 文本 avg 6.6–8.4 us/frame；全链 120 怪物/10 summons 240 帧 p50/p95=8 us、p99=9–11 us、commands peak 234、text peak 101，三 overflow 恒 0。见 remediation-evidence.md §R9。 |
| Steady-state allocations | PASS | benchmark 1 断言 16 帧 Commands() 指针/命令/裁剪/文本容量全部不变 + 三 overflow==0 + ClipBalanced（零重分配/零溢出）。 |
| 16:9/21:9/4:3 视觉截图 | NOT_RUN | 无头会话无法 headed 截图（B-R0-2）；owner=手测矩阵执行者。Fit 逻辑已由 UiViewportTests（3440x1440/2560x1440/1920x1440）7/7 覆盖。 |
| DRS-on screen UI 验证 | NOT_RUN | 需 headed 会话；分支逻辑已由 QualityTierManagerTest/AdaptiveQualityControllerTest 覆盖（settings 解析 + 分支）。 |

Tracy 集成（B-R0-1）：仓库 CMake 无 TRACY_ENABLE 路径（R0 确认 + docs/workflows/performance.md:17 承认），R9 裁决不引入第三方 Tracy 依赖，以 allocation 断言 + 计时基准作为等价替代证据；Tracy 集成立为独立后续任务（owner=性能工程）。**不伪称 Tracy 数据存在。**
