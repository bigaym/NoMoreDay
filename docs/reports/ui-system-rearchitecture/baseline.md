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
