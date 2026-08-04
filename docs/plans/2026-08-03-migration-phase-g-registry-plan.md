# Phase G: 资源注册表补齐（registry completeness）

> **关联设计:** `docs/designs/2026-08-03-render-engine-interface-migration-design.md` §5.5
> **关闭债务:** RG-3（registry 覆盖不完整）
> **依赖:** 无（可最先执行，机械、低风险，建立资源台账并暴露 MS-8 泄漏候选）
> **状态:** [~] 进行中（G1-G3 实现完成，验证中；ResourceManager reload 生命周期仍由 F 阶段单独定义）
>
> **注（2026-08-04, B11）:** design §5.5/B9 曾把"FramebufferManager owner 硬编码 Scene / ComputeBuffer owner Unknown 与 graph 合同不一致"标注为本组待办。该债务已由 Phase B11（`GPUResourceRegistry::ReclassifyResourceOwner`，Shadow→Shadow、cluster→LightCulling、LightBuffer→Lighting）单独关闭，不属于 G1-G5 的 VAO/VBO/shader/query 配对范围；本组无需重复处理。

## 1. Authority And Boundaries

- **授权来源**: 渲染全量接口迁移 design §5.5；M0-B spec §3 registry 记账契约。
- **范围**: 将引擎创建但未登记的 8 处专用 VAO/VBO、`ResourceManager` 的 compute shader 与 VS/FS program、`GPUTimerQueryRing` 的 GL query 纳入 `GPUResourceRegistry` 观察。
- **边界**: 不做任何 GL 所有权变更（observer-only）；不新建 wrapper 类型；不修改渲染行为；不改 `RenderGraph` 路径。

## 2. Verified Baseline

- `GPUResourceRegistry`（src/engine/render/resources/GPUResourceRegistry.hpp）：`RegisterResource(handle, kind, ...)`(:57)、`UnregisterResource`(:60)、`UpdateResourceSize`(:61)、`AdvanceFrame`(:62)、`TakeSnapshot`(:70)；key=`(kind<<32)|handle`(:84)。三防护已实现（重复注册拒绝、尺寸防下溢、注销先于 GL 释放）。
- 已登记：ComputeBuffer、GPUEntitySystem shader/VAO/VBO、PersistentBuffer、FramebufferManager FBO+texture、FullscreenQuad。
- 未登记（本次目标）：
  - VAO/VBO（`rlLoadVertexArray()`+`rlLoadVertexBuffer()` 模式）— PopupRenderer.cpp:224、MDIRenderer.cpp:82、GPUParticleSystem.cpp:446、trail/GPUTrailRenderer.cpp:52（仅 dummy VAO）、GPULootSystem.cpp:384、GPUSkillEffectSystem.cpp:405、GPUTextSystem.cpp:74、passes/FluidSimulationPass.cpp:89。
  - ShaderProgram（`ResourceManager::loadShader` :266-310，LoadShaderFromMemory VS/FS）与 compute shader（`loadComputeShader` :319-368）。
  - QueryRing（render/debug/GPUTimerQueryRing.cpp，glGenQueries :150-156 / glDeleteQueries :39-40 动态解析）。

## 3. Implementation Rationale

registry 是纯观察者：注册/注销必须与 GL 生命周期精确配对（创建成功后 register，GL 释放前 unregister）。VAO/VBO 均在系统构造/重建时创建、析构或重建时释放，注入点清晰。shader program 的注销点须找到对应 `UnloadShader` 调用（wrapper 析构链）。query ring 是固定大小的池，创建在构造、释放只在析构，记账简单。

## 4. Pseudocode Guidance

```text
// VAO/VBO 模式（每处同构）
CreateGlResources():
    vao = rlLoadVertexArray(); vbo = rlLoadVertexBuffer(...)
    registry.RegisterResource(vao, VertexArray, name=<ClassId>, ...)
    registry.RegisterResource(vbo, VertexBuffer, name=<ClassId>, ...)
ReleaseGlResources():
    registry.UnregisterResource(vbo); registry.UnregisterResource(vao)   // 先注销再 GL 释放
    rlUnloadVertexBuffer(vbo); rlUnloadVertexArray(vao)

// ResourceManager
loadShader(vs, fs):                                      // VS/FS program
    program = LoadShaderFromMemory(...)
    registry.RegisterResource(program, ShaderProgram, name, estimatedSize)
    ShaderReloadGovernance.RecordReloadAttempt(hash)     // 补齐 F 组契约
loadComputeShader(...):                                  // compute 已有 hash 记录，仅补注册
    registry.RegisterResource(shader, ShaderProgram, ...)
UnloadXxx():
    registry.UnregisterResource(program); UnloadShader(program)

// GPUTimerQueryRing
ctor: ids = glGenQueries(n); registry.RegisterResource(ids[i], QueryRing, "GPUTimerQueryRing")
dtor: registry.UnregisterResource(ids[i]); glDeleteQueries(...)
```

## 5. Atomic Tasks

| # | 任务 | 依赖 | 状态 |
| --- | --- | --- | --- |
| G1 | 8 处 VAO/VBO 注册/注销配对 | - | [~] |
| G2 | `ResourceManager` shader program（VS/FS + compute）注册 + VS/FS 补 ReloadGovernance 记录 | - | [~] |
| G3 | `GPUTimerQueryRing` query 注册/注销 | - | [~] |
| G4 | 台账验证：S4 五秒快照覆盖全部目标资源，无 duplicate/missing | G1-G3 | [~] |
| G5 | 边界脚本 + `legacy` 扫描 + 全量回归 | G1-G3 | [~] |

## 6. Test Method

- **unit**: `GPUResourceRegistryTest`（如存在）补充 Register/Unregister 配对断言；无该测试则新增最小注册/注销用例。
- **integration**: 复用 S4 五秒快照机制断言 8 处 VAO/VBO + shader program + query 出现在快照。
- **manual**: 运行 build，确认 snapshot 日志目标资源齐备。
- **命令**:
  ```powershell
  ./build.bat
  ctest --test-dir build -C RelWithDebInfo -L "unit|integration" --output-on-failure
  python scripts/check_module_boundaries.py
  python scripts/check_legacy_reintroduction.py
  ```

## 7. Verification Of Task Completion

- G1: 8 处 VAO/VBO 在创建后立即 Register、GL 释放前 Unregister；重复 register 不产生异常（防护诊断）。
- G2: VS/FS program 与 compute shader 均登记；`ShaderReloadGovernance` 对 VS/FS 也开始记录。
- G3: query 池全量登记，析构前全部注销。
- G4: 连续两帧 snapshot 中目标资源均出现且计数一致；无 registry 诊断告警。
- G5: build 双成功标记、ctest 无新增失败（既有已知失败除外）、边界 71/71、`legacy` 扫描通过。
- **当前风险:** G2 仅记录现有 reload ledger，不实现 watch/poll/last-good swap；F 阶段必须作为唯一 reload 生命周期 owner，避免重复改造 `ResourceManager.cpp`。query 注册在 `BeginPass` 惰性发生，必须以成功生成的非零句柄为准。
- **当前验证证据:** `cmake --build build --config RelWithDebInfo --target NoMoreDayTests -- /m:2` 成功；Phase G registry unit test 通过（28 assertions），RenderGraph/registry 合计 26 个 unit test、215 个 assertion 通过。尚未完成真实 GL 生命周期快照验证。
- 提交经用户授权，并按 `docs/plans/` 中 Handoff 模板如实报告。

## 8. Handoff Template

```text
package: phase-g-registry
source baseline: <commit>
files changed: ...
contract changed: 无（observer-only）
focused tests + exact result: ...
broader build-test + known unrelated failures: ...
artifact path: ...
Track docs updated: ...
remaining risk or blocker: ...
```
