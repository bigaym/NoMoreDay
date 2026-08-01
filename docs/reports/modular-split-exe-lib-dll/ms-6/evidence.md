# MS-6 GPU Entity Rendering Boundary Adapter — Evidence

Date: 2026-08-01. Batch scope: GPU entity rendering core (GPUEntitySystem + GPUEntitySync -> Game adapter). 20 ledger edges removed (71 -> 51).

## Changes

- Moved `src/engine/render/GPUEntitySync.hpp` / `.cpp` to `src/game/render/` (filesystem move, no `git mv` staging; shown as `D` + untracked). Only change in the `.cpp` is the line-1 self-include `engine/render/GPUEntitySync.hpp` -> `game/render/GPUEntitySync.hpp`; the `.hpp` is byte-identical. The three `render::` sync classes (GPUSlotManager/GPUPhysicsSync/GPUVisualSync) and their `std::vector&` Execute signatures are unchanged so unit/performance tests pass vectors directly.
- New Game-layer adapter `src/game/render/GPUEntityAdapter.hpp` (namespace `NoMoreDay`):
  - `Init(int maxEntities, entt::registry*, systems::GPUEntitySystem&)` — initializes GPUSlotManager with the recycle callback moved verbatim from the old `GPUEntitySystem::Init` (`m_shadowBuffer[slot].radius=0; position={0,0}; m_visualStatsShadowBuffer[slot]={}`), then GPUPhysicsSync + GPUVisualSync; calls `engine.BeginShadowWrite()`.
  - `SetLevelManager(LevelManager*)` — stores the level manager for the fog pass (Game injects `m_context.levelManager`).
  - `Update(entt::registry&, systems::GPUEntitySystem&, float dt, float currentTime)` — old `UpdateLogic` projection: `BeginShadowWrite()` sizing, `m_slotManager.Process`, `m_physicsSync.Execute -> SetHighWaterMark`, `m_visualSync.Execute -> SetUpdatedStatsIndices`, then fog block when `m_levelManager` set.
  - `ApplyFogVision(...)` — the old fog `LimitedVision` block verbatim: `getCurrentBiomeID` + `BiomeRegistry::Get().GetBiome`, `hasFeature(LimitedVision) && visionRadius>0`, `<EnemyTag,Position,GPUIndex>` view, `GRID_TILE_SIZE` from `NoMoreDay::Constants::World`, `levelManager->getFogSystem().isVisible(gx,gy)`, writes through `engine.ApplyShadowFlags(slot, NO_RENDER|0)`.
  - Includes only Game/engine-DTO headers it needs (GPUData.hpp, GPUEntitySystem.hpp, AIComponent.hpp, Common.hpp, BiomeRegistry.hpp, game/render/GPUEntitySync.hpp, LevelManager.hpp).
- `src/engine/render/GPUEntitySystem.hpp`: removed `engine/render/GPUEntitySync.hpp` include (engine must not include the Game-moved file) and `game/components/Common.hpp` (ledger L8); removed `struct SharedContext` fwd decl; added `NoMoreDay::render::EntityRenderFrame{ResourceManager* resources; MDIRenderer* mdi; float renderAlpha;}`; signatures now `Init(ResourceManager&, int=200000)` / `UploadGPU(const render::EntityRenderFrame&)` / `Render(const render::EntityRenderFrame&, const Camera2D&)` / `RenderLegacy(float)`; removed `Get()`, `Update`, `UpdateLogic`, `SyncBack`, `GetSlotManager()`, `s_instance`, `m_frameCounter`, and m_slotManager/m_physicsSync/m_visualSync members; kept `GetEntityBuffer()`, `GetMaxEntities()`, `Shutdown()`; added write contract `BeginShadowWrite()` (returns `m_shadowBuffer.data()`, sizes buffers), `SetHighWaterMark(int)`, `ApplyShadowFlags(int, uint32_t)` (merges only `GPU_ENTITY_FLAG_NO_RENDER` bit, preserving physics/type flags), plus minimal plumbing `SetUpdatedStatsIndices`, `ShadowBuffer()`, `VisualStatsBuffer()` so the retained vector-based GPUPhysicsSync/GPUVisualSync jobs project zero-copy into the Engine-owned buffers.
- `src/engine/render/GPUEntitySystem.cpp`: removed 11 game/app includes + `GPUFlowFieldSystem.hpp` + `GPUUtils.hpp` + `RenderContext.hpp`; `Get()`/`UpdateLogic`/`Update`/`SyncBack` bodies removed (projection now in adapter); `Init` keeps all RG-3 resource creation verbatim except the removed slotManager block and `m_mapBoundary = 5000.0f` (literal equal to `NoMoreDay::Constants::World::MAP_BOUNDARY`); `InitRender`/`RenderLegacy`/`Shutdown` bodies unchanged; `Render`/`UploadGPU` now read `frame.*` with `frame.mdi==nullptr` -> `MDIRenderer::Get()` fallback (preserves the old null-renderContext singleton path). MDI sequence preserved: BindPreviousNoSync(SSBO_ENTITY_DATA) -> ResetCommand -> Cull -> Render, and BindPrevious(SSBO_VISIBLE_ID) + instanced draw in legacy path.
- `src/engine/render/RenderSystem.cpp:714` — single atomic call-site change: `if (frame.context.renderContext != nullptr) { frame.context.renderContext->GPU().Render({frame.context.resources, &frame.context.renderContext->MDI(), frame.context.renderAlpha}, frame.camera); }` (RenderFrameData holds `const SharedContext& context`, hence dot access). The null guard preserves the old null-`renderContext` early-return behavior (gate harness paths keep `renderContext==nullptr`); production path injects `m_context.renderContext` before render, so the guard never triggers there. Graph construction (line 1816+) untouched.
- `src/app/Game.hpp`: added `#include "game/render/GPUEntityAdapter.hpp"` and member `NoMoreDay::GPUEntityAdapter m_gpuEntityAdapter;`.
- `src/app/Game.cpp`: `m_gpuEntitySystem.Init(m_resourceManager, 30000); m_gpuEntityAdapter.Init(30000, &m_registry, m_gpuEntitySystem); m_gpuEntityAdapter.SetLevelManager(m_context.levelManager);` before `m_mdiRenderer.Init(...)`; loop body `m_gpuEntityAdapter.Update(m_registry, m_gpuEntitySystem, fixedDt, (float)GetTime());`; upload `m_gpuEntitySystem.UploadGPU({m_context.resources, &m_context.renderContext->MDI(), m_context.renderAlpha});`.
- Tests: include-only retarget `engine/render/GPUEntitySync.hpp` -> `game/render/GPUEntitySync.hpp` in `tests/unit/GPUVisualSyncTest.cpp`, `tests/unit/GPUSlotManagerTest.cpp`, `tests/performance/GPUSyncBenchmark.cpp`. `tests/integration/MDIRenderTest.cpp`, `tests/performance/MDIRenderBenchmark.cpp`, `tests/performance/RenderingBenchmark.cpp` switched `Init` to new signature, wired a local `GPUEntityAdapter` (adapter namespace: unqualified `GPUEntityAdapter` in `namespace NoMoreDay`, `NoMoreDay::GPUEntityAdapter` in `NoMoreDay::tests`) for projection, call `UploadGPU({...})` / `Render({...})` with explicit frames. No CMake edits needed (GLOB).
- Ledger `docs/reports/modular-split-exe-lib-dll/ms-0/reverse-dependency-ledger.json`: removed exactly 20 entries — 8 for `src/engine/render/GPUEntitySync.cpp`, 11 for `src/engine/render/GPUEntitySystem.cpp`, 1 for `src/engine/render/GPUEntitySystem.hpp`. 71 -> 51 entries.
- `scripts/check_module_boundaries.py`: removed `src/engine/render/GPUEntitySync.cpp` from `REQUIRED_P0_SOURCES` (file is no longer an Engine candidate).

## Boundary Ledger

- `python scripts/check_module_boundaries.py`: PASS `51/51` observed/ledger direct quoted edges; 17 files. Breakdown: LegacyLowerPch -> Game (MS-7) 5; NoMoreDayEngine -> App (MS-6) 8; NoMoreDayEngine -> Game (MS-6) 38.

## Verification (all executed; logs under `F:\NoMoreDay\logs\`)

1. `python scripts/check_module_boundaries.py` -> `[Module Boundary] Observed/ledger edges: 51/51; files: 17` and `[Module Boundary] PASS: ledger and observed reverse edges match.` (exit 0).
2. `python -m unittest tests.python.ModuleBoundaryCheckerTest` -> `Ran 6 tests in 0.569s`, `OK` (exit 0).
3. `cmd.exe /c build.bat check` -> exit 0, all prechecks OK including `OK: Checking candidate module boundaries.` and `Check mode: Skipping compilation.` (log: `logs/ms6_build_check.log`).
4. Full build `cmd.exe /c build.bat > logs/ms6_full_build2.log 2>&1` -> exit 0; log contains both `[Build] Build completed successfully.` and `[Build] All steps completed successfully` (only printed when cmake --build exits 0). First attempt (`logs/ms6_full_build.log`) failed with `error C2039: \"GPUEntityAdapter\": 不是 \"NoMoreDay::render\" 的成员` in `tests/integration/MDIRenderTest.cpp(54)` and `tests/performance/MDIRenderBenchmark.cpp(123)` — fixed by unqualifying/qualifying the adapter name to namespace `NoMoreDay`; second build clean.
5. Targeted tests `bin\NoMoreDayTests.exe --test-case=\"*GPUSlotManager*,*GPUVisualSync*,*GPUSyncBenchmark*\"` -> `test cases: 2 | 2 passed | 0 failed | 709 skipped`, `assertions: 19 | 19 passed`, `Status: SUCCESS!`, exit 0 (log: `logs/ms6_targeted_tests.log`). Note: `*GPUSyncBenchmark*` matches no case name (the benchmark TEST_CASE is named `[Performance] GPUEntitySync - Sync Performance Benchmark`), so it was re-run explicitly: `--test-case=\"*GPUEntitySync*\"` -> 1 passed, PhysicsSync Mean=0.253ms / VisualSync Mean=0.201ms (within targets), exit 0 (log: `logs/ms6_benchmark.log`).
6. `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` -> `100% tests passed, 0 tests failed out of 9` (unit label incl. progression/ui/item/combat/parity/skill/world/ai unit suites), exit 0 (log: `logs/ms6_ctest_unit.log`).
7. `git grep -n \"engine/render/GPUEntitySync\" -- src tests` -> 0 matches (exit 1). `git grep -n \"game/components/Common.hpp\" -- src/engine/render` -> matches ONLY the 9 residual ledgered files (RenderSystem.cpp, GPULootSystem.cpp, GPUParticleSystem.cpp, lighting/GlobalHeightField.cpp, lighting/LightManager.cpp, passes/HeightShadowPass.cpp, passes/OccluderExtractPass.cpp, passes/RadianceCascadesPass.cpp, passes/ShadowBuildPass.cpp) which are part of the 51 remaining ledger edges; `GPUEntitySystem.hpp`/`GPUEntitySystem.cpp` and the moved sync files contribute 0 matches.
8. `git diff --check` -> exit 0, CRLF warnings only, no whitespace errors.
9. `git status --short` -> only the MS-6 file set below; nothing staged or committed.

## Deferred Scope and Risks

- **RG-3 resource lifecycle untouched**: GPUEntitySystem's m_quadVAO/m_quadVBO/m_renderShader/5x ComputeBuffer/PersistentBuffer creation/destruction/registration are byte-identical to HEAD; `Shutdown()` retains its known leaks (blockSumBuffer/physicsOutputBuffer not released; VAO/VBO/shader not unloaded) — intentionally NOT fixed per the hard constraint; documented here as evidence.
- **RenderSystem.cpp:714 atomicity**: only the single GPU entity render call-site changed (plus the null guard added post-review); RenderGraph build, the 7 render passes, and all other 20 ledgered RenderSystem.cpp edges are untouched. Production path injects `m_context.renderContext` before `RenderSystem::render`, so the guard is inactive there; gate/harness paths with `renderContext==nullptr` skip GPU entity rendering, equivalent to the old RenderLegacy early-return.
- **Zero-copy hot path**: adapter writes directly into the Engine-owned `m_shadowBuffer`/`m_visualStatsShadowBuffer` via `BeginShadowWrite()` + mutable-ref accessors; no intermediate vectors/copies. 64B GPUEntity layout and highWaterMark memcpy range in UploadGPU unchanged.
- **Behavior preservation**: MDI sequence (BindPreviousNoSync(SSBO_ENTITY_DATA) -> ResetCommand -> Cull -> Render) and RenderLegacy (BindPrevious(SSBO_VISIBLE_ID) + instanced draw) preserved; `frame.mdi==nullptr` -> `MDIRenderer::Get()` singleton fallback mirrors the old `renderContext==nullptr` path. Fog flag merge (`ApplyShadowFlags`) only touches the NO_RENDER bit, identical to the old fog block.
- **Extra engine accessors**: `ShadowBuffer()`/`VisualStatsBuffer()` mutable-ref accessors + `SetUpdatedStatsIndices` were added beyond the prescribed three-method write contract so the retained vector-based GPUPhysicsSync/GPUVisualSync Execute jobs can write into Engine-owned buffers; GPUEntitySync classes are themselves unchanged (kept Game-layer, tested directly). The Engine header now includes `engine/render/GPUData.hpp` (pure DTO, zero Game deps) instead of retargeting the sync include, which would have created a new forbidden engine->game reverse edge.
- **Remaining ledger work**: 51 edges stay (RenderSystem.cpp 20 + RenderSystem.hpp 1 + GPULoot 2 + GPUParticle 1 + GPUSkillEffect 1 + lighting 6 + passes 13 + VFX 2 = 46 MS-6 edges; pch.hpp 5 = MS-7). RenderSystem is the next-priority sub-batch.
- **Guard**: `GPUEntitySystem::Get()` removed; no remaining call sites (build confirms). GameplayState.cpp:522 accessors intact.
- Nothing was staged or committed (per project rule; main agent commits).

---

## Batch 2 — GameplayRenderAdapter (RenderSystem.cpp Game draw migration)

Date: 2026-08-01. Scope: move Game-specific rendering out of `RenderSystem.cpp` into a Game-layer adapter. 17 more ledger edges removed (48 -> 31). Committed as `363b196` (Batch 1) then this batch.

### Changes

- New Engine-side pure-DTO interface `src/engine/render/GameplayRenderHooks.hpp` (namespace `render`): virtual `onFrameData/onScene/onVFX/onUIWorld` + `render::GameplayRenderFrame` DTO (registry/camera refs; Engine-owned labelBuffer/glyphBuffer/beamBuffer instance buffers that the adapter fills; font out-field; gpuTextEnabled/gpuLootEnabled/gpuLootGlowEnabled flags; GPUBeamInstance struct). Zero game/app deps (only entt/raylib/GPUData.hpp/vector).
- New Game-layer `src/game/render/GameplayRenderAdapter.{hpp,cpp}` (namespace `NoMoreDay`) implementing the hooks: onFrameData (biome/fog segment), onScene (stash + sprite loop + enemy fog cull + pixel dot + BloodSea + MoltenTrail + Trail/SwordIntent/HoloBlade calls), onVFX (AttackEffect/VisualEffect switch + Projectile->GPUSkillEffect submit + ResistOverlay), onUIWorld (DamagePopup + loot label collect/sort/overlap + itemGrid query + beam buffer + label/glyph buffer fill).
- `RenderSystem::render` gained `const render::GameplayRenderHooks* gameplayHooks = nullptr`; null hooks -> gameplay segments skipped (gate/empty-harness safe). Graph construction (AddPass order/owner/composite selection) untouched; only 3 lambdas forward the hooks param.
- Public statics `RenderSystem::s_itemGrid`/`s_itemGridDirty`/`VisibleItemCache::visibleItems` migrated to `GameplayRenderAdapter`; 7 Game files retargeted (InventorySystem x6, DropSystem x7, FragmentDropSystem x1, LootGridSystem x3, UISystem.cpp:587).
- `src/app/SharedContext.hpp` gained a forward-declared `render::GameplayRenderHooks* gameplayRenderHooks` field (no include, no edge).
- RenderSystem.cpp includes: 17 Game edges removed (Common/EffectComponent/EnemyComponent/ItemComponent/Projectile/SkillDefs/StashComponent/HoloBladeComponent/BiomeRegistry/MonsterAffixSystem/LootFilter/UISystem/HoloBladeRenderSystem/SwordIntentVisualSystem/TrailSystem/FogOfWarSystem/LevelManager). `MonsterAffixSystem.hpp` is RESTORED (see L2 clarification) — `MoltenTrailTag` is defined at `game/systems/combat/MonsterAffixSystem.hpp:37`, used by adapter.cpp:243.
- Ledger: 48 -> 31 (removed exactly 17 RenderSystem.cpp edges). `REQUIRED_P0_SOURCES`: RenderSystem.cpp removed (0 residual edges); RenderSystem.hpp retained (1 App edge, Batch 3).
- `tests/tech/UITests.cpp:836-839` Blood Sea content lock retargeted to `GameplayRenderAdapter.cpp` (assertion patterns match adapter L204-242).

### Accepted layering-order deviations (review M1/M2)

- **M1 Scene order**: `GPU().Render()` (Engine primitive) now runs first, before all adapter game draws (old order: trail->sword->stash->sprite->GPU().Render->pixel->blood->molten->holo). Comment in code declares this intentional (player above enemies is the correction direction); non-GPU sprites/trails/stash moved from below to above GPU entities. Needs visual confirmation on real hardware (accepted risk).
- **M2 VFX resist-overlay order**: resist overlay now draws inside onVFX before the Engine `GPUSkillEffectSystem::Render` (old order: Submit->Render->distortion->resist overlay), moving the debuff ring from above to below the skill mesh. Documented, accepted risk; needs visual confirmation.

### Fixes after review

- **M3 fixed**: `GPU().Render()` (Engine primitive, independent of hooks) moved BEFORE the `if (gameplayHooks==nullptr) return;` early-out in ExecuteScenePass (RenderSystem.cpp:569-585) with a comment; null renderContext guard retained. Prevents hooks-null + renderContext-nonnull future harness from silently skipping GPU entity rendering.
- **L2 clarified**: reviewer claimed `MonsterAffixSystem.hpp` was unused; actually `MoltenTrailTag` is defined there (adapter.cpp:243). Include restored after a build C2039 (`MoltenTrailTag is not a member of NoMoreDay`). The include is valid.

### Verification (real output)

1. `python scripts/check_module_boundaries.py` -> PASS `31/31; files: 16` (5 pch + 8 App [RenderSystem.hpp 1 + passes 7] + 18 Game).
2. Full build `cmd.exe /c build.bat > ms6-b2-fix2-build.log 2>&1` -> both success markers, 0 error lines.
3. Targeted `--test-case="*GPULootSystem*,*S1a*,[Tech] Blood Sea*,*GPUSkillEffect*,*InventoryUI*"` -> 6/6, 26 assertions, SUCCESS.
4. `git grep -n "game/" -- src/engine/render/RenderSystem.cpp` -> 0 matches.
5. `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` -> intermittent failures with DIFFERENT binaries each run (unit/ai.unit/skill.unit), all pass in isolation — confirmed as pre-existing `HeavenlySwordClosureTests.cpp:97` hasFreeze flaky, not a Batch 2 regression.
6. `git diff --check` -> exit 0 (CRLF warnings only).

### Deferred

- RenderSystem.hpp App edge (1) -> Batch 3 (render() DTO + graphContext.shared evaluation).
- Remaining MS-6 edges: GPULoot 2 + GPUParticle 1 + GPUSkillEffect 1 + lighting 6 + passes 13 + VFX 2 = 25; plus 5 pch (MS-7).
- RG-3 resource lifecycle untouched; graph construction zero-modification verified.
- Nothing was staged or committed (per project rule; main agent commits).

---

## Batch 3 — render() DTO (delete RenderSystem.hpp App edge)

Date: 2026-08-01. Scope: DTO-ize `RenderSystem::render` with an Engine-side `render::RenderFrameInput` and delete the last App edge on `RenderSystem.hpp`. 1 ledger edge removed (31 -> 30). Uncommitted working tree at HEAD `c3f97e7`.

### Changes

- New Engine-side pure-DTO header `src/engine/render/RenderFrameInput.hpp` (namespace `NoMoreDay::render`, mirrors the `EntityRenderFrame` precedent): `struct RenderFrameInput { ResourceManager* resources = nullptr; float renderAlpha = 0.0f; NoMoreDay::RenderContext* renderContext = nullptr; float cameraZoom = 1.0f; };` with global `class ResourceManager;` and `namespace NoMoreDay { struct RenderContext; }` forward declarations. Pointer-only fields, zero app deps. `levelManager` deliberately excluded (its consumption already lives in the Game-layer `GameplayRenderAdapter` since Batch 2).
- `src/engine/render/RenderSystem.hpp`: deleted `#include "app/SharedContext.hpp"` (ledger edge `RenderSystem.hpp:4:app/SharedContext.hpp`); added `#include "engine/render/RenderFrameInput.hpp"`; `render()` signature changed to `static void render(entt::registry&, const NoMoreDay::render::RenderFrameInput&, const Camera2D&, NoMoreDay::render::GameplayRenderHooks* gameplayHooks = nullptr);`.
- `src/engine/render/RenderSystem.cpp`: `RenderFrameData::context` member re-typed `const NoMoreDay::render::RenderFrameInput&`; `render()` parameter re-typed identically (`RenderFrameData frame{registry, context, camera};` unchanged); internal member access `frame.context.resources / renderAlpha / renderContext` unchanged (only the type changed). No `settings->cameraZoom` / fontScale code existed in the cpp (fontScale already moved to Game-side in Batch 2), so **no fontScale change was needed** — the only fontScale input, `frame.context.cameraZoom`, now comes from the DTO field, keeping the computation equivalent by construction. Graph-build line `graphContext.shared = &context;` -> `graphContext.resources = frame.context.resources;`.
- `src/engine/render/graph/RenderContext.hpp`: added `class ResourceManager;` forward decl + `ResourceManager *resources = nullptr;` member. The `shared` field and its forward decl are **kept this batch** (7 passes still read `context.shared->resources`; pass rewiring is Batch 4).
- `src/engine/render/validation/FixtureRenderDriver.hpp`: added `#include "engine/render/RenderFrameInput.hpp"` and a new pure virtual `virtual NoMoreDay::render::RenderFrameInput RenderInput() const = 0;` (after `Context()`). `Context()` kept intact for interface compatibility. This lets the gate build a DTO without an engine->app include.
- `src/engine/render/validation/GPUHardwareValidationGate.cpp`: both `SharedContext` locals removed (former L447 `driver.Context()`, L628 `driver->Context()`) in favor of `const NoMoreDay::render::RenderFrameInput renderInput = driver.RenderInput();`; all 5 render call sites (L491/498/745/760/1025/1115) now pass the DTO. No `SharedContext` references remain.
- `src/game/states/GameplayState.cpp:985`: `RenderSystem::render(*m_context->registry, render::RenderFrameInput{m_context->resources, m_context->renderAlpha, m_context->renderContext, (m_context->settings != nullptr) ? m_context->settings->cameraZoom : 1.5f}, m_camera, m_context->gameplayRenderHooks);` — cameraZoom is **null-safe** with the same ternary + `1.5f` default as the existing `m_camera.zoom` fallback at GameplayState.cpp:172.
- `tests/integration/GameplayRuntimeHarness.hpp`: added `RenderInput() const override` returning `{resources=nullptr, renderAlpha=m_context->renderAlpha (=1.0f), renderContext=nullptr, cameraZoom=(m_context->settings != nullptr) ? settings->cameraZoom : 1.0f}`.
- `tests/integration/SingleGpuTimerOwnerRegressionTest.cpp:199-213`: `SharedContext shared;` -> `render::RenderFrameInput input;` (all-null defaults) and `RenderSystem::render(registry, input, camera);`.
- Ledger: deleted exactly 1 entry (id `src/engine/render/RenderSystem.hpp:4:app/SharedContext.hpp`) -> **30 entries**. Checker: removed `"src/engine/render/RenderSystem.hpp"` from `REQUIRED_P0_SOURCES`.

### graphContext.shared replacement evaluation (record only; pass files untouched this batch)

- After this batch `graphContext.shared` is **never assigned** (always nullptr) in RenderSystem.cpp; `graphContext.resources` carries `frame.context.resources`. The full removal of `shared` is **NOT feasible in this batch** because the 7 passes still dereference `context.shared->resources`.
- 7 passes and their `shared->resources` use points (all guarded by `context.shared == nullptr || context.shared->resources == nullptr -> ReportFailure/return`): `FluidSimulationPass.cpp:681,697-718,795`; `GICompositePass.cpp:178,187`; `JFAPass.cpp:600,620`; `LightCullingPass.cpp:123,141`; `OccluderExtractPass.cpp:360,376`; `RadianceCascadesPass.cpp:235,245,755-756,774`; `ShadowBuildPass.cpp:317,337`.
- **Replacement plan (Batch 4)**: rewire those 7 passes from `context.shared->resources` to `context.resources`; drop the `shared` field + its `NoMoreDay { struct SharedContext; }` forward decl from `graph/RenderContext.hpp`; adjust `IsValid()` accordingly. `graph::RenderContext::IsValid()` has no callers, so no call-site impact.
- **Graph contract impact**: with `shared==nullptr` (current state), the 7 passes early-return, so in the real game the HDR/GI/lighting/fluid/shadow passes are temporarily degraded (skipped) until Batch 4 rewires them to `context.resources`. Gate/harness tests are unaffected (harness `resources` is already nullptr, so those passes already early-returned before this batch).
- Files touched in Batch 4 (deferred): the 7 pass `.cpp` files + `graph/RenderContext.hpp`. NOT touched this batch (hard constraint honored): RenderGraph class, 7 pass classes, graph build structure (AddPass order/conditions/owner tags), ResourceManager/GPUResourceRegistry, `src/pch.hpp`, CMake targets, build.bat, RG-3.
  - *Note: this Batch-4 plan was subsequently executed in-place as Batch 3b below (7-pass rewire + `shared` field removal + ledger 30->23); the interim HDR/GI/lighting/fluid/shadow degradation no longer exists.*

### Verification (real output)

1. `python scripts/check_module_boundaries.py` -> `[Module Boundary] Observed/ledger edges: 30/30; files: 15` and `PASS: ledger and observed reverse edges match.` (exit 0). Breakdown: LegacyLowerPch -> Game (MS-7) 5; NoMoreDayEngine -> App (MS-6) 7; NoMoreDayEngine -> Game (MS-6) 18.
2. `python -m unittest tests/python/ModuleBoundaryCheckerTest.py` -> `Ran 6 tests ... OK` (exit 0).
3. `cmd.exe /c build.bat check` -> `OK: Checking candidate module boundaries.` (exit 0).
4. Full build redirected to `C:\Users\yuminao\AppData\Local\Temp\opencode\ms6-b3-build.log` -> both `[Build] Build completed successfully.` and `[Build] All steps completed successfully` markers, **0 `error C`** occurrences.
5. `git grep -n "SharedContext" -- src/engine/render/RenderSystem.hpp src/engine/render/RenderSystem.cpp` -> 0 hits (exit 1).
6. Targeted `bin\NoMoreDayTests.exe --test-case="*S1a*,*GPUABI*,*RenderGraphV5*,*GPU Hardware Validation Gate*"` -> `test cases: 5 | 5 passed | 0 failed | 706 skipped`, `assertions: 307 | 307 passed`, `Status: SUCCESS!` — the gate's 5 DTO-ized call sites still pass 4/4 with no crash (gate report: stress_1min_passed:true, toggle_100_loops_passed:true).
7. `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` -> intermittent failures in **non-render** tests, proven pre-existing by stash/rebase testing (see below).
8. `git diff --check` -> exit 0 (CRLF warnings only).

### Pre-existing test failures (NOT Batch 3 regressions; proven via stash)

Both were verified by stashing Batch 3 (`git stash push -u -m ms6-b3-wip`), rebuilding the base commit, re-running the tests (still failing/flaky), then `git stash pop` (clean restore of all 11 files):

- **`tests/unit/HeavenlySwordClosureTests.cpp:97 CHECK(hasFreeze)`**: chance-based; `FrozenDominion` rolls `ThreadSafeRandom::GetFloat01() < 0.15f` (15%) in `HeavenlySwordDescent.cpp:247` over 30 ticks — inherently flaky (~1/6-1/2 failure rate depending on RNG state), passes 10/10 in isolation, last touched in commit 3ddec2b (predates Batch 3).
- **`tests/integration/GIStabilityIntegrationTest.cpp:159-160`** (`CHECK(context.giEmissiveTexture != 0u)` / `CHECK(context.giRadianceTexture != 0u)`): fails 3/3 at base commit and 4/4 with Batch 3. The test bypasses RenderSystem entirely — it builds its own `graph::RenderContext` (`context.shared = &shared`, L87-89) and calls `pass.Execute(context)` directly (L155); `RadianceCascadesPass` only writes those fields when all 6 pipeline steps succeed, so an earlier GPU-env step (emissive/material/particle/trace) fails here. A purely additive graph change cannot affect it.

### Deferred

- Remaining MS-6 edges (Batch 4): GPULoot 2 + GPUParticle 1 + GPUSkillEffect 1 + lighting 6 + passes Game edges 6 (OccluderExtract 2 / ShadowBuild 2 / RadianceCascades 1 / HeightShadow 1) + VFX 2 = 18; plus 5 pch (MS-7).
- RG-3 resource lifecycle untouched; graph construction (AddPass order/conditions/owner tags) zero-modification verified.
- Nothing was staged or committed (per project rule; main agent commits).

---

## Batch 3b — pass resource rewire (App edges cleared)

Date: 2026-08-01. Scope: rewire the 7 render passes from `context.shared->resources` to `context.resources` (removing the interim `graphContext.shared == nullptr` pass degradation introduced by Batch 3) and delete all 7 pass App edges on `app/SharedContext.hpp`. 7 ledger edges removed (30 -> 23); App edges now **zero**. Uncommitted working tree at HEAD `c3f97e7`.

### Changes

- 7 passes (`FluidSimulationPass`, `GICompositePass`, `JFAPass`, `LightCullingPass`, `OccluderExtractPass`, `RadianceCascadesPass`, `ShadowBuildPass`): `context.shared->resources` -> `context.resources`; null guards `context.shared == nullptr || context.shared->resources == nullptr` -> `context.resources == nullptr`; deleted `#include "app/SharedContext.hpp"`.
- `src/engine/render/graph/RenderContext.hpp`: removed the `shared` member + `NoMoreDay { struct SharedContext; }` forward declaration; `IsValid()` now checks `resources` only.
- 4 direct-`pass.Execute` test/benchmark files synchronized (`ClusteredLightingIntegrationTest.cpp`, `GIStabilityIntegrationTest.cpp`, `ClusteredLightingBenchmark.cpp`, `RadianceCascadesBenchmark.cpp`): `context.shared = &shared` -> `context.resources = &resources`; dropped `SharedContext` construction + `app/SharedContext.hpp` include.
- Guard hygiene fixes (main agent): two passes (`FluidSimulationPass.cpp:680-684`, `GICompositePass.cpp:177-180`) had an unclosed `if` introduced during rewire (missing `context.camera == nullptr` clause + `{`), fixed to `if (context.qualityManager == nullptr || context.resources == nullptr || context.camera == nullptr) { ... }`; all rewire guards re-indented to 2-space project style.
- Guard consistency (post-review): restored `context.qualityManager == nullptr` in `OccluderExtractPass.cpp:359` guard and `!context.hdrSceneBuffer.IsValid()` in `RadianceCascadesPass.cpp:234` guard (review L2/L3), keeping all pass guards uniform; `FluidSimulationPass.cpp:582-583` stale "through SharedContext" comment updated to `graph::RenderContext::resources` (review I4).
- Ledger: deleted exactly 7 entries (the pass `app/SharedContext.hpp` edges) -> **23 entries** = LegacyLowerPch -> Game (MS-7) 5 + NoMoreDayEngine -> Game (MS-6) 18. Checker: removed the 4 fully-resolved passes from `REQUIRED_P0_SOURCES` (RenderSystem.hpp removed in Batch 3); the 3 passes with remaining Game edges stay listed.

### Verification (real output)

1. `python scripts/check_module_boundaries.py` -> `[Module Boundary] Observed/ledger edges: 23/23; files: 11` and `PASS` (exit 0).
2. `python -m unittest tests/python/ModuleBoundaryCheckerTest.py` -> 6 tests OK.
3. Full build redirected to `C:\Users\yuminao\AppData\Local\Temp\opencode\ms6-b3b-build2.log` -> EXIT=0, both success markers, 0 error C/LNK/FAILED (also rebuilt after the guard-restoration edits).
4. `git grep -n "context.shared\|shared->resources"` -> 0 hits; `git grep -n "app/SharedContext" -- src/engine/render/passes` -> 0 hits.
5. Targeted `bin\NoMoreDayTests.exe --test-case="*S1a*,*S1b*,*GPU ABI*,*RenderGraph V5*"` -> 13 passed / 178 assertions / SUCCESS.
6. `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` -> the only failures are the pre-existing `HeavenlySwordClosureTests.cpp:97` `hasFreeze` chance-based flaky (~15%), confirmed non-regression by isolation reruns (unit/ai.unit/skill.unit each pass alone).
7. `git diff --check` -> exit 0 (CRLF warnings only).

---

## Batch 4 — shared occluder projection (Engine->Game edges cleared)

Date: 2026-08-02. Scope: deduplicate the byte-identical occluder projection between `OccluderExtractPass` and `ShadowBuildPass` by moving it to a single Game-side projector (`src/game/render/OccluderProjector`), and delete all 4 Engine->Game ledger edges on the two passes (`OccluderExtractPass.cpp:12,13` + `ShadowBuildPass.cpp:13,14`). 4 ledger edges removed (21 -> 17); both passes now have **zero** Game edges and were removed from `REQUIRED_P0_SOURCES`. Uncommitted working tree at HEAD `1874266`.

### Changes

- **NEW `src/game/render/OccluderProjector.hpp` + `.cpp`** (namespace `NoMoreDay`, modeled on the `GPUEntityAdapter` split-adapter precedent): `OccluderProjector::Project(entt::registry&) -> OccluderProjection {casters, staticCount, dynamicCount, staticSignature, dynamicSignature}`. Projects `Position + ShadowCasterComponent` (+ optional `VisionComponent` radius override, default 24.0f) into the pure DTO array `std::vector<NoMoreDay::components::GPUShadowCaster>` and the FNV signatures. Projection + FNV logic moved **verbatim** from `OccluderExtractPass.cpp` `UploadOccluders` (incl. `kFnvOffset`/`kFnvPrime`/`HashAppend`/`BuildOccluderWord` quantize *16 + pack + `word ^= shapeIndex<<8; word ^= dynamicFlag<<1`/`FinalizeSignature` sort + FNV + size append). Single shared copy; both passes consume the same projection.
- `src/engine/render/GameplayRenderHooks.hpp`: `GameplayRenderFrame` gains engine-owned `std::vector<NoMoreDay::components::GPUShadowCaster>* occluderBuffer` (after `beamBuffer`) + out-fields `occluderStaticCount`/`occluderDynamicCount`/`occluderStaticSignature`/`occluderDynamicSignature`; new pure-virtual `onOccluders(GameplayRenderFrame&)` (only implementer is `GameplayRenderAdapter`).
- `src/game/render/GameplayRenderAdapter.hpp/.cpp`: implements `onOccluders` — calls `OccluderProjector::Project(frame.registry)`, moves casters into `*frame.occluderBuffer`, copies the 4 stat out-fields.
- `src/engine/render/graph/RenderContext.hpp`: forward-declared `NoMoreDay::components::GPUShadowCaster`; added fields `const components::GPUShadowCaster *occluders = nullptr; uint32_t occluderCount/occluderStaticCount/occluderDynamicCount; uint64_t occluderStaticSignature/occluderDynamicSignature;` (pointer+count+scalar DTO style, matching existing raw-pointer fields). `registry` field **kept** (still used by `HeightShadowPass`/`RadianceCascadesPass`).
- `src/engine/render/passes/OccluderExtractPass.hpp/.cpp`: `UploadOccluders` re-signatured to `bool UploadOccluders(const NoMoreDay::components::GPUShadowCaster*, uint32_t)` — now GPU upload only (buffer Create/Update sizing for the injected span, stays in pass); deleted `#include "game/components/Common.hpp"` + `ShadowCasterComponent.hpp`, the anon-namespace FNV helpers, and `m_occluderStaging`; `Execute` reads stats from `context.occluder*` fields instead of `registry` projection, dropped the `context.registry == nullptr` guard clause, and the screen-bounds loop iterates the injected `context.occluders` span (same dynamic-flag filter + `GetWorldToScreen2D` math). FNV signature algorithm + usage unchanged (bitwise-equivalent signature verification).
- `src/engine/render/passes/ShadowBuildPass.hpp/.cpp`: `UploadOccluders` re-signatured to the same `(const GPUShadowCaster*, uint32_t)` span input, capped at `kMaxShadowCasters` (`min(occluderCount, 8192)`), GPU buffer Create/Update stays in pass; deleted both game includes + `using namespace entt::literals;` + `m_occluderStaging`; `Execute` consumes `context.occluders`/`context.occluderCount`, dropped `context.registry == nullptr` guard clause. Projection is uncapped game-side; ShadowBuild truncates at consumption -> first `min(count,8192)` casters in identical view order, bitwise equivalent to the old capped loop.
- `src/engine/render/RenderSystem.cpp`: added static `s_occluderBuffer` (engine-owned, mirrors `s_beamBuffer`); `RenderFrameData` gains the 4 occluder stat fields; `ToHooksFrame()` passes `&s_occluderBuffer`; the hooks block calls `gameplayHooks->onOccluders(hooksFrame)` after `onFrameData` and stashes the stats on `frame`; graphContext assembly (between `hdrSceneBuffer` and GI texture fields) fills `occluders`/`occluderCount`/stats from `s_occluderBuffer` + `frame`. **Graph build structure (AddPass order/conditions/owner tags/composite input selection) zero modification.**
- Ledger: deleted exactly 4 entries -> **17 entries** = LegacyLowerPch -> Game (MS-7) 5 + NoMoreDayEngine -> Game (MS-6) 12. Checker: removed both fully-resolved passes from `REQUIRED_P0_SOURCES` (0 residual edges each, per precedent).
- No CMake edits needed (`GLOB_RECURSE CONFIGURE_DEPENDS` auto-picks the new `.cpp`).

### Verification (real output)

1. `python scripts/check_module_boundaries.py` -> `[Module Boundary] Observed/ledger edges: 17/17; files: 8` and `PASS` (exit 0). Breakdown: LegacyLowerPch -> Game (MS-7) 5, NoMoreDayEngine -> Game (MS-6) 12.
2. `python -m unittest tests/python/ModuleBoundaryCheckerTest.py` -> 6 tests OK (test_repository_baseline_passes runs the real-repo checker).
3. Full build redirected to `C:\Users\yuminao\AppData\Local\Temp\opencode\ms6_batchb_build.log` -> EXIT=0, both success markers (`[Build] Build completed successfully.` / `[Build] All steps completed successfully`), 0 error C/LNK; `NoMoreDayCore.lib` rebuilt, `bin\NoMoreDay.exe` + `bin\NoMoreDayTests.exe` refreshed (0:48).
4. Targeted `bin\NoMoreDayTests.exe --test-case="*RenderGraphV3*,*RenderGraphTier*,*ShadowPipeline*,*Occluder*,*S7*"` -> 17 passed / 180 assertions / SUCCESS (0 failed). Broader `--test-case="*RenderGraph*,*ShadowPipeline*,*Occluder*,*S7*"` -> 48 passed / 397 assertions / SUCCESS.
5. `git grep "game/" -- src/engine/render/passes/OccluderExtractPass.cpp src/engine/render/passes/ShadowBuildPass.cpp` -> 0 hits (exit 1).
6. `ctest --test-dir build -C RelWithDebInfo -L "unit|integration" --output-on-failure` -> 73% pass; the only failures are the **pre-existing flaky** `HeavenlySwordClosureTests.cpp:97` (`hasFreeze`, chance-based, exempted) and `GIStabilityIntegrationTest.cpp:156-157` (`giEmissiveTexture/giRadianceTexture != 0`, GPU-env shader-load dependent, exempted) — neither touches the occluder passes (GIStability bypasses RenderSystem and drives `RadianceCascadesPass` directly; `LightCullingPass failed to load compute shader` log confirms env cause).
7. `git diff --check` -> exit 0 (CRLF warnings only).

### Deferred

- Remaining MS-6 edges (Batch 5): GPULoot 2 + GPUSkillEffect 1 + GlobalHeightField 3 + LightManager 2 + HeightShadowPass 1 + RadianceCascadesPass 1 + VFX 2 = 12; plus 5 pch (MS-7). OccluderExtractPass/ShadowBuildPass are fully Game-edge-free.
- RG-3 resource lifecycle untouched; graph construction (AddPass order/conditions/owner tags) zero-modification verified.
- Nothing was staged or committed (per project rule; main agent commits).

---

## Batch 5 — LightAdapter (LightManager Game edges cleared)

Date: 2026-08-02. Scope: split the ECS -> GPULight projection out of `src/engine/render/lighting/LightManager.cpp` into a Game-side adapter, and delete both Engine->Game ledger edges (`LightManager.cpp:5:game/components/Common.hpp` + `LightManager.cpp:6:game/components/LightComponent.hpp`). 2 ledger edges removed (17 -> 15). Uncommitted working tree at HEAD `a5cc909`.

### Changes

- **NEW `src/game/render/LightAdapter.hpp` + `.cpp`** (namespace `NoMoreDay`, modeled on the GPUEntityAdapter/OccluderProjector split-adapter precedents): `LightAdapter::BuildLightCandidates(entt::registry&, float gameTime) -> LightProjection {std::vector<components::GPULight> lights; int ecsLights;}`. Projects `Position + LightComponent` into the pure Engine DTO array. Projection logic moved **verbatim** from `LightManager.cpp` `Update`'s ECS loop: `BuildGpuLight` (position/radius/flicker intensity via `ComputeFlickerIntensity` incl. entity-phase hash/color/`ToRawLightType`/spot dir + `spotCosHalfAngle`/priority), the `enabled` filter, and the `radius<=0 || intensity<=0` filter; `ecsLights` mirrors the old `view.size_hint()`. Uncapped (Engine truncates to budget).
- `src/engine/render/lighting/LightManager.hpp`: `Update(entt::registry&, const Camera2D&, int, float)` -> `UpdateCandidates(std::span<const components::GPULight>, const Camera2D&, int maxLights, int ecsLights)`; dropped `#include <entt/entt.hpp>` (no longer needs the registry).
- `src/engine/render/lighting/LightManager.cpp`: deleted `game/components/Common.hpp` + `LightComponent.hpp` includes and the moved projection helpers; `Update` rewritten as `UpdateCandidates` — keeps view culling (`IntersectsView`), transient handling (`SanitizeRuntimeLight` + priority 255 + clear), sort (priority desc / distance asc), budget truncation, `m_stagingBuffer`/`m_activeLightRecords` fill and `OrphanAndUpload` GPU upload. ECS candidate sort priority now read from the DTO field `gpuLight.priority` (the adapter sets it == `light.priority`, so the `static_cast<uint8_t>` is lossless); transient stays hard-coded 255 — behavior identical.
- `src/engine/render/GameplayRenderHooks.hpp`: `GameplayRenderFrame` gains Engine-owned `std::vector<components::GPULight>* lightBuffer` (after `occluderBuffer`) + out-field `int ecsLights`; new pure-virtual `onLights(GameplayRenderFrame&)` (only implementer is `GameplayRenderAdapter`).
- `src/game/render/GameplayRenderAdapter.hpp/.cpp`: implements `onLights` — calls `LightAdapter::BuildLightCandidates(frame.registry, (float)GetTime())`, moves lights into `*frame.lightBuffer`, copies `ecsLights`.
- `src/engine/render/RenderSystem.cpp`: static `s_lightCandidateBuffer`; `RenderFrameData.ecsLights`; `ToHooksFrame()` passes `&s_lightCandidateBuffer`; the lighting block (`useHdrSceneBuffer && !offscreenV3SafeMode && dynamicLightingEnabled && g_lightingPass`) now calls `gameplayHooks->onLights(hooksFrame)` (or clears `s_lightCandidateBuffer` + ecsLights=0 for gate/harness null-hooks) then `LightManager::Get().UpdateCandidates(s_lightCandidateBuffer, camera, renderConfig.maxLights, frame.ecsLights)`. **Graph build structure (AddPass order/conditions/owner tags/composite selection) zero modification.**
- Ledger: deleted exactly 2 entries -> **15 entries** = LegacyLowerPch -> Game (MS-7) 5 + NoMoreDayEngine -> Game (MS-6) 10. Checker: removed `src/engine/render/lighting/LightManager.cpp` from `REQUIRED_P0_SOURCES` (0 residual edges).
- Tests synchronized to the new engine signature via the shared adapter: `tests/unit/LightingTest.cpp` (5 call sites), `tests/integration/ClusteredLightingIntegrationTest.cpp` (7 call sites; the "Boundary conditions" case uses distinct `singleProjection`/`overfullProjection` locals in one scope), `tests/integration/LightingStabilityTest.cpp` (1 call in the 2400-frame loop, `timeSeconds` forwarded), `tests/performance/ClusteredLightingBenchmark.cpp` (1 call in `MeasureLightingPath` loop), `tests/performance/LightingBenchmark.cpp` (1 call in `MeasureLightingGpuMs` loop — an extra build-breaking caller beyond the audited list). Each builds `LightAdapter::BuildLightCandidates(registry, time)` then `UpdateCandidates(projection.lights, camera, N, projection.ecsLights)` — no per-test projection rewrite, per the ledger's reuse-the-adapter requirement.
- No CMake edits needed (`GLOB_RECURSE CONFIGURE_DEPENDS` auto-picks the new `.cpp`).

### Verification (real output)

1. `python scripts/check_module_boundaries.py` -> `[Module Boundary] Observed/ledger edges: 15/15; files: 7` and `PASS` (exit 0). Breakdown: LegacyLowerPch -> Game (MS-7) 5, NoMoreDayEngine -> Game (MS-6) 10.
2. `python -m unittest tests/python/ModuleBoundaryCheckerTest.py` -> 6 tests OK (exit 0).
3. Full build redirected to `C:\Users\yuminao\AppData\Local\Temp\opencode\ms6-b4c-build.log` -> EXIT=0, both success markers (`[Build] Build completed successfully.` / `[Build] All steps completed successfully`), 0 error C/LNK/FAILED. First attempt failed with `error C2374/C2086/C2371: "lightProjection" redefinition` in `tests/integration/ClusteredLightingIntegrationTest.cpp` (Boundary-conditions case declares 3 same-scope projections) -> fixed by renaming the 2nd/3rd to `singleProjection`/`overfullProjection`; second build clean.
4. Targeted `bin\NoMoreDayTests.exe --test-case="*[Unit] Lighting*,*Clustered Lighting*,*Lighting - Stability*"` -> `test cases: 19 | 19 passed | 0 failed | 692 skipped`, `assertions: 28762 | 28762 passed | 0 failed`, `Status: SUCCESS!` (unit lighting + 7 clustered integration + stability + 2 clustered benchmarks).
5. `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` -> 78% pass; the only failures are the **pre-existing flaky** `HeavenlySwordClosureTests.cpp:97` `hasFreeze` (chance-based ~15%; `nmd.tests.skill.unit`/`nmd.tests.ai.unit` fail on different runs and each pass in isolation — confirmed non-regression).
6. `git grep -n "game/" -- src/engine/render/lighting/LightManager.cpp` -> 0 hits (exit 1).
7. `git diff --check` -> exit 0 (CRLF warnings only).

### Deferred

- Remaining MS-6 edges (Batch 6): GPULoot 2 + GPUSkillEffect 1 + GlobalHeightField 3 + HeightShadowPass 1 + RadianceCascadesPass 1 + VFX 2 = 10; plus 5 pch (MS-7). LightManager is fully Game-edge-free.
- RG-3 resource lifecycle untouched; graph construction (AddPass order/conditions/owner tags) zero-modification verified.
- Nothing was staged or committed (per project rule; main agent commits).
