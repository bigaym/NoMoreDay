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
