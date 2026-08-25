# 工作包 01 — engine-coord（引擎坐标单一来源）

**Role**: Engine/Render 坐标专家
**Depends**: Phase 0（本设计/track 已就绪）
**Files Owned**: `src/engine/render/CoordSystem.hpp`（新增）、`src/engine/render/GPUParticleSystem.cpp`（仅 BuildMVP）、`src/game/application/states/GameplayState.cpp`（仅 FRAGCOORD 翻转行 + include）、`tests/unit/CoordSystemTests.cpp`（新增）

## Mission
建立并验证坐标转换的唯一入口，且**不改变任何视觉行为**。

## 必做条款（对照设计 §3）
1. 新增 header-only `NoMoreDay::render::coord`：
   - `enum class Space`
   - `struct Camera2DTransform`（`From(const Camera2D&)` / `ToRaylib()`）
   - `WorldToScenePixel`、`ScenePixelToWorld`（zoom 护栏）
   - `Build2DMvp(camera, fbW, fbH)`——Y-down ortho，公式与旧 `GPUParticleSystem::BuildMVP` **逐位等价**
   - `NativeYToGl(y, height) = height - y`（唯一 FRAGCOORD 翻转点，禁止其他地方再写）
2. `GPUParticleSystem::BuildMVP` 改为委托 `coord::Build2DMvp`（保留原注释与 framebuffer 尺寸读取）。
3. `GameplayState.cpp` 的 `screenPlayer.y = GetScreenHeight() - screenPlayer.y` 改为 `coord::NativeYToGl`。
4. 新增 `tests/unit/CoordSystemTests.cpp`：
   - world→screen→world round-trip（含 zoom≠1、target/offset 非零）
   - `NativeYToGl` 边界（0→h、h→0）
   - `Build2DMvp` 与旧公式对同一 camera 等价
5. 不提交、不建 worktree；只改工作树。

## Acceptance
- Task 1.1-1.4 代码落地并更新 `plan.md` 状态为 `[~]`。
- `build.bat` 与 focused CTest 由 qa-coord 阶段统一验证；本包如无法构建可先行标记 `build-pending`。
