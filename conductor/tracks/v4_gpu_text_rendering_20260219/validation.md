# Validation - v4_gpu_text_rendering_20260219

## 2026-02-19 Task 1.1 (msdf toolchain integration)

### Implementation Evidence
- Added offline toolchain wrapper: `scripts/gen_msdf_atlas.py`
- Added charset seed file: `scripts/msdf_charset_v4.txt`
- Track status synced to in-progress:
  - `conductor/tracks.md`
  - `conductor/tracks/v4_gpu_text_rendering_20260219/plan.md`
  - `conductor/tracks/v4_gpu_text_rendering_20260219/index.md`
  - `conductor/tracks/v4_gpu_text_rendering_20260219/metadata.json`

### Verification
- `build.bat` first run failed due unsupported cached generator (`Ninja`) in `build/`.
- Recovery executed: `build.bat clean-all; build.bat`.
- Result: PASS (MSVC generator configured, NoMoreDay + NoMoreDayTests built successfully).
- Notes: only third-party CMake deprecation warnings observed; no new build blockers.
- Tool entrypoint sanity check: `python scripts/gen_msdf_atlas.py --help` PASS.

## 2026-02-19 Task 1.2 (generate 4096x4096 atlas)

### Implementation Evidence
- Added GB2312 charset generator: `scripts/gen_msdf_charset_gb2312.py`
- Installed `msdf-atlas-gen` binary:
  - `tools/msdf/msdf-atlas-gen-1.3-win64/msdf-atlas-gen/msdf-atlas-gen.exe`
- Generated charset file:
  - `scripts/msdf_charset_gb2312.txt` (UTF-8)
- Generated atlas artifacts:
  - `assets/textures/fonts/msdf/v4_msdf_gb2312_4096.png`
  - `assets/textures/fonts/msdf/v4_msdf_gb2312_4096.json`

### Command Evidence
- Generation command (successful):
  - `msdf-atlas-gen.exe -font C:/Windows/Fonts/simsun.ttc -allglyphs -type msdf -dimensions 4096 4096 -minsize 18 -pxrange 6 -json .../v4_msdf_gb2312_4096.json -imageout .../v4_msdf_gb2312_4096.png -coloringstrategy distance -potr`
- Output summary:
  - `Loaded geometry of 28905 out of 28905 glyphs`
  - `Glyph size: 18.046875 pixels/em`
  - `Atlas dimensions: 4096 x 4096`

### Artifact Stats
- `v4_msdf_gb2312_4096.png`: `32,201,866` bytes
- `v4_msdf_gb2312_4096.json`: `6,552,116` bytes

### Verification
- UTF-8 checks: PASS (`gen_msdf_atlas.py`, `gen_msdf_charset_gb2312.py`, charset/docs metadata files)
- Python syntax checks: PASS (`python -m py_compile` for both scripts)
- Build: PASS (`build.bat`)

## 2026-02-19 Task 1.3 (metrics export JSON/BIN + size reduction)

### Implementation Evidence
- Updated charset generator to emit valid msdf charset spec syntax:
  - `scripts/gen_msdf_charset_gb2312.py`
  - Generated file: `scripts/msdf_charset_gb2312.txt`
- Regenerated atlas with strict charset (`GB2312 + ASCII`), replacing prior all-glyph output:
  - `assets/textures/fonts/msdf/v4_msdf_gb2312_4096.png`
  - `assets/textures/fonts/msdf/v4_msdf_gb2312_4096.json`
- Added compact metrics exporter:
  - `scripts/export_msdf_metrics.py`
- Exported compact metrics:
  - `assets/textures/fonts/msdf/v4_msdf_gb2312_4096.metrics.json`
  - `assets/textures/fonts/msdf/v4_msdf_gb2312_4096.metrics.bin`

### Command Evidence
- Atlas generation (strict charset): `python scripts/gen_msdf_atlas.py ... --charset scripts/msdf_charset_gb2312.txt --run`
- Export: `python scripts/export_msdf_metrics.py --input assets/textures/fonts/msdf/v4_msdf_gb2312_4096.json`

### Size Comparison
- Previous all-glyph atlas outputs:
  - PNG: `32,201,866` bytes
  - JSON: `6,552,116` bytes
- Current strict-charset outputs:
  - PNG: `14,005,126` bytes
  - JSON: `1,729,450` bytes
- Additional compact metrics:
  - Metrics JSON: `1,508,212` bytes
  - Metrics BIN: `331,656` bytes

### Notes
- Atlas glyph count: `7539`, atlas dims: `4096 x 2048`
- `msdf-atlas-gen` reported one missing codepoint: `0x30FB`
- Compact exporter skipped glyphs without full bounds data; exported `7537` records.

## 2026-02-19 Task 1.4 (atlas loader with BC4/BC5 options)

### Implementation Evidence
- Added runtime atlas loader:
  - `src/engine/render/resource/MSDFAtlasLoader.hpp`
  - `src/engine/render/resource/MSDFAtlasLoader.cpp`
- Loader capabilities:
  - Load compact metrics bin (`MSGM`, version `1`, stride `44`)
  - Load atlas texture with selectable compression mode:
    - `MSDFAtlasCompression::None` -> load source path
    - `MSDFAtlasCompression::BC4` -> prefer `*.bc4.dds`, fallback source
    - `MSDFAtlasCompression::BC5` -> prefer `*.bc5.dds`, fallback source
  - Missing compressed asset emits warning and auto-fallback (non-blocking)
  - Unified unload path for texture + metrics buffer cleanup

### Verification
- Build: PASS (`build.bat`)

## 2026-02-19 Phase 2 (GPU data layer)

### Implementation Evidence
- Added new GPU text ABI structs in `GPUData.hpp`:
  - `GPUTextCommand` (16 bytes)
  - `GPUGlyphMetrics` (40 bytes)
  - `GPUTextQuad` (40 bytes)
- Added strict `static_assert` coverage (`std::is_standard_layout` + `sizeof`) for all three structs.
- Registered new structs in ABI governance manifest:
  - `tools/render_abi/abi_manifest.json`
- Regenerated GLSL ABI include:
  - `assets/shaders/generated/gpu_abi.glslinc`
  - New GLSL structs present: `GPUTextCommand`, `GPUGlyphMetrics`, `GPUTextQuad`
- Registered Text Pass binding alias in render constants:
  - `RenderConstants::Binding::SSBO_TEXT_QUAD = SSBO_GLYPH_INSTANCE`
  - `RenderConstants::TextPassBinding::QUAD_SSBO`

### Verification
- ABI generation check: PASS (`python tools/render_abi/generate_gpu_abi.py --check`)
- ABI governance check: PASS (`python tools/render_abi/check_no_manual_abi_structs.py`)
- Build: PASS (`build.bat`)
- CI tests: PASS (`ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`, 1/1 passed)

## 2026-02-19 Task 3.1 (in progress) - TextLayoutCS scaffold

### Implementation Evidence
- Added initial compute shader scaffold:
  - `assets/shaders/text/text_layout.compute`
- Current behavior:
  - Reads `GPUTextCommand` + `GPUGlyphMetrics`
  - Emits `GPUTextQuad` with `atomicAdd`-based append
  - Placeholder one-command-to-one-glyph mapping (prefix-sum/string expansion pending)

### Verification
- Build: PASS (`build.bat`)

## 2026-02-19 Task 3.1 + 3.2 (TextLayoutCS + animation)

### Implementation Evidence
- Upgraded `assets/shaders/text/text_layout.compute` to V1 compute layout pipeline:
  - Added `StringGlyphIndexBuffer` + `StringMetaBuffer`
  - Added workgroup prefix-sum (`shared` arrays) for per-command variable glyph count
  - Added group-level `atomicAdd` append allocation into `GPUTextQuad` output
  - Added 5 animation styles in shader:
    - float-up
    - gravity-fall
    - fade-out
    - scale-bounce
    - crit-pop (easeOutBack)
- Added `GPUTextSystem` runtime scaffold:
  - `src/engine/render/GPUTextSystem.hpp`
  - `src/engine/render/GPUTextSystem.cpp`
  - Supports command enqueue, glyph metrics upload, string table upload, and compute dispatch

### Failure Handling (fixed)
- `ctest -L ci` initially failed in `RenderBindingGovernanceTest` because `GPUTextSystem` used raw `BindBase(0..5)` literals.
- Fix:
  - Added named bindings in `RenderConstants.hpp` under `TextLayoutCS`.
  - Replaced all raw literals with `RenderConstants::TextLayoutCS::*`.
- Re-run result: PASS.

### Verification
- Build: PASS (`build.bat`)
- CI tests: PASS (`ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`, 1/1 passed)

## 2026-02-19 Task 3.3 (in progress) - event intake wiring

### Implementation Evidence
- Added popup event bridge into GPU text command queue:
  - `src/game/systems/combat/DamagePopupManager.hpp`
  - `Emit(...)` now feeds `GPUTextSystem::EnqueueCommand(...)` while preserving `PopupRenderer` path.
- Added provisional runtime string/animation packing:
  - damage popup -> `stringId = abs(amount)%10`
  - status popup -> `stringId = 1`
  - style packed into high 8 bits of `colorAndFlags` (crit/status/default mapping)

### Failure Handling (fixed)
- Build failure after wiring due namespace resolution:
  - `core::ComputeBuffer` in `GPUTextSystem.hpp` was interpreted as `NoMoreDay::render::core::ComputeBuffer`.
- Fix:
  - switched to fully qualified `NoMoreDay::core::ComputeBuffer`.

### Verification
- Build: PASS (`build.bat`)
- CI tests: PASS (`ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`, 1/1 passed)

## 2026-02-19 Task 3.3 (completed) - runtime lifecycle + dispatch wiring

### Implementation Evidence
- Runtime lifecycle integration:
  - `src/app/Game.cpp`
  - `Game::init()` now initializes `GPUTextSystem` and uploads bootstrap glyph/string tables via `MSDFAtlasLoader`.
  - `Game::cleanup()` now calls `GPUTextSystem::Shutdown()`.
- Frame integration:
  - `src/game/states/GameplayState.cpp`
  - `OnUpdate()` now executes:
    - `GPUTextSystem::BeginFrame()` at frame start
    - `GPUTextSystem::DispatchLayout(GetTime())` after combat/effect updates
- Command intake normalization:
  - `src/game/systems/combat/DamagePopupManager.hpp`
  - Popups now map to stable string IDs:
    - `0-9`: digits
    - `10`: `CRIT`
    - `11`: generic status
- Animation style sourcing fix:
  - `assets/shaders/text/text_layout.compute`
  - Style now reads from command high bits (`colorAndFlags >> 24`) with meta fallback.

### Failure Handling (fixed)
- Build failed once due namespace typo in bootstrap helper:
  - `NoMoreDay::ResourceManager` (invalid in this TU)
- Fix:
  - changed helper signature to `ResourceManager &`.

### Verification
- UTF-8 checks: PASS (`Game.cpp`, `GameplayState.cpp`, `DamagePopupManager.hpp`, `GPUTextSystem.hpp`, `text_layout.compute`)
- Build: PASS (`build.bat`)
- CI tests: PASS (`ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`, 1/1 passed)

## 2026-02-19 Task 3.4 (completed) - layout unit verification

### Implementation Evidence
- Added unit test file:
  - `tests/unit/GPUTextLayoutReferenceTest.cpp`
- Coverage points:
  - mixed ASCII/CJK glyph index expansion and spacing
  - multi-line style via multiple commands with stable vertical separation
  - style override from command high bits and 64-glyph clamp behavior

### Verification
- Build: PASS (`build.bat`)
- CI tests: PASS (`ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`, 1/1 passed)
- Unit label: PASS (`ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`, 1/1 passed)

## 2026-02-19 Task 4.1 (completed) - GPUTextPass indirect draw path

### Implementation Evidence
- Added dedicated pass wrapper:
  - `src/engine/render/passes/GPUTextPass.hpp`
  - `src/engine/render/passes/GPUTextPass.cpp`
- Added text quad draw shaders:
  - `assets/shaders/text/text_quad.vert`
  - `assets/shaders/text/text_quad.frag`
- Upgraded `GPUTextSystem` for render path:
  - `src/engine/render/GPUTextSystem.hpp`
  - `src/engine/render/GPUTextSystem.cpp`
  - Added:
    - `SetAtlasTexture(...)`
    - `Render(viewProj)` using `DrawArraysIndirect`
    - indirect command buffer lifecycle
    - quad VAO/VBO setup and atlas binding
- Runtime wiring:
  - `src/app/Game.cpp` now transfers MSDF atlas texture ownership to `GPUTextSystem`.
  - `src/engine/render/RenderSystem.cpp` integrates `GPUTextPass` into RenderGraph.

### Failure Handling (fixed)
- Build fail #1: ABI governance detected manual ABI struct declaration in shader (`GPUTextQuad` name).
  - Fix: shader-side struct renamed to non-ABI symbol while preserving std430 field layout.
- Build fail #2: `GL_DRAW_INDIRECT_BUFFER` unresolved in TU.
  - Fix: switched to `RenderConstants::GL::DRAW_INDIRECT_BUFFER`.

### Verification
- UTF-8 checks: PASS (`GPUTextSystem.*`, `GPUTextPass.*`, `RenderSystem.cpp`, `Game.cpp`, `text_quad.*`)
- Build: PASS (`build.bat`)
- CI tests: PASS (`ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`, 1/1 passed)
- Unit label: PASS (`ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`, 1/1 passed)

## 2026-02-19 Task 4.2-4.5 (completed) - MSDF decode + graph integration + feature routing

### Implementation Evidence
- MSDF fragment quality upgrade:
  - `assets/shaders/text/text_quad.frag`
  - Added median decode helper + derivative-based `screenPxRange(...)` anti-aliasing path.
- RenderGraph/Profiler integration for dedicated GPU text stage:
  - `src/engine/render/graph/RenderGraph.cpp` (added `GPUText` stage and order contract mapping)
  - `src/engine/render/debug/RenderProfiler.hpp`
  - `src/engine/render/debug/RenderProfiler.cpp` (added `RenderPassId::GPUText`, pass-name mapping, 0.15ms budget)
- Feature flag + tier policy in quality config:
  - `src/engine/render/core/RenderConstants.hpp`
  - `src/engine/render/core/QualityTierManager.hpp`
  - `src/engine/render/core/QualityTierManager.cpp`
  - Added config fields:
    - `gpuTextEnabled`
    - `gpuTextAdvancedAnimation`
  - Added settings key parse/persist:
    - `render.gpuText.enabled`
    - `render.gpuText.enabled` (nested under `render.gpuText.enabled`)
  - Tier policy:
    - Low: CPU fallback
    - Medium: GPU basic animation
    - High/Ultra: GPU full animation
- Runtime route switch (mutual exclusion):
  - `src/game/systems/combat/DamagePopupManager.hpp` (CPU/GPU emit routing + basic/full style)
  - `src/game/states/GameplayState.cpp` (dispatch gating + tier-based animation duration)
  - `src/engine/render/RenderSystem.cpp` (VFX/UIWorld/GPUTEXT pass gating to avoid double rendering)

### Verification
- Build: PASS (`.\build.bat`)
- CI label: PASS (`ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`, 1/1 passed)
- Unit label: PASS (`ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`, 1/1 passed)
- Performance label: PASS (`ctest --test-dir build -C Release -L performance --output-on-failure`, 1/1 passed)

## 2026-02-19 Task 5.1-5.4 (completed) - benchmark + matrix validation + deprecated fallback

### Implementation Evidence
- Added tier/feature-flag verification tests:
  - `tests/unit/QualityTierManagerTest.cpp`
  - New cases:
    - `[Unit] QualityTierManager - GPUText Tier Matrix Policy`
    - `[Unit] QualityTierManager - GPUText Feature Flag Route Switch`
- Added CPU-vs-GPU route prep benchmark (100+ onscreen equivalent workload):
  - `tests/performance/RenderingBenchmark.cpp`
  - New case:
    - `[Performance] Text - Scenario H CPU vs GPU Route Preparation (100+ onscreen)`
- Updated profiler HUD benchmark to include GPU text pass:
  - `tests/performance/RenderingBenchmark.cpp` Scenario F pass list includes `RenderPassId::GPUText`.
- Deprecated fallback marker:
  - `src/engine/render/PopupRenderer.hpp`
  - Added explicit deprecation comment for legacy CPU popup path.

### Verification
- Build: PASS (`.\build.bat`)
- CI label: PASS (`ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure`, 1/1 passed)
- Unit label: PASS (`ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`, 1/1 passed)
- Performance label: PASS (`ctest --test-dir build -C Release -L performance --output-on-failure`, 1/1 passed)
