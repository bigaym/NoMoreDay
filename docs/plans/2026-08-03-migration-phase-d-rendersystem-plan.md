# Phase D: RenderSystem 手工条件链收敛

> **关联设计:** `docs/designs/2026-08-03-render-engine-interface-migration-design.md` §5.4
> **关闭债务:** RG-1/RG-2（compiled plan 未保存跨 pass transition / descriptor 名称脱离）
> **依赖:** Phase B、C（graph 覆盖 shadow/cluster/lighting 后，RenderSystem 才能整体依赖 graph）
> **状态:** [x] 已完成（2026-08-05，含 D1-D7）

## 1. Authority And Boundaries

- **授权来源**: 渲染全量接口迁移 design §5.4；M0-B spec §3。
- **范围**: `RenderSystem::render` 内手工 owner 追踪、逐 pass OnResize fan-out、HDR/GI 状态机、composite input 手工选择、Distortion 手工接线、lambda 内手工 BindFramebuffer/Viewport/Clear。
- **边界**: 不改变渲染顺序、HDR/GI 策略与质量行为；接通 `RenderGraph::OnResize`（现从未被调用，RenderGraph.cpp:381-387）。

## 2. Verified Baseline

- 每帧重建 graph（:1466）+ 手工 owner 追踪 sceneHdrOwner/ldrOwner（:1463-1572）。
- 手工 OnResize fan-out 两处重复（:1319-1344 创建分支、:1357-1381 resize 分支）+ `EnsureGiPassesSized`（:297-308）。
- HDR 切换状态机（:1246-1273）、GI re-enable 状态机（:1388-1399）、offscreen seed blit（:1404-1418）。
- Distortion `SetInputBuffer(g_postProcessPass->GetOutputBuffer())`（:1580）；composite input tag 手工选择（:1585-1593）+ 变更日志（:1595-1613）。
- lambda 手工 BindFramebuffer/Viewport/Clear（:1470-1480 ScenePass、:1615-1633 Composite）。
- 正确锚点保留：`AdvanceFrame` 恰一次在 graph.Execute 后（:1700-1704）；FlushRing 先于 DRS/HUD（:1705-1713）。

## 3. Implementation Rationale

D 组的目标是让 graph 成为唯一接线源：owner 由 `Write(SceneHdrColor)` 的 pass owner 推断；resize 由 descriptor `extentPolicy`（MatchScreen）驱动 `RenderGraph::OnResize` 统一分发；HDR/GI 状态机改为基于 registry snapshot 与 config 的组合状态判定；Distortion 输入经 typed access 声明；ScenePass/Composite 的 FBO bind/viewport/clear 由 graph attachment 声明接管（渲染动作保留在 pass 内，但 target 来源 graph 化）。

## 4. Pseudocode Guidance

```text
// resize 统一入口
OnResize(w, h):
    for each registered descriptor with extentPolicy==MatchScreen:
        resize backing resource                          // 取代逐 pass OnResize fan-out
    graph.OnResize(w, h)                                 // 接通从未被调用的入口

// render 主链
sceneHdrOwner = graph.ProducerOf(SceneHdrColor)          // 取代手工 owner 追踪
...
graph.Build(); graph.Execute();
// 移除 compositeInput 手工选择 -> 由 graph 推断"最后一个 Write(SceneHdrColor) 的 owner"
// Distortion: 移除 SetInputBuffer；改 DeclareResource + Read(g_postProcessPass output tag)

// HDR 切换状态机
if config.useHdrSceneBuffer != s_prevUseHdrSceneBuffer:
    graph.MarkResizeDirty()                              // descriptor 重建走统一 resize
```

## 5. Atomic Tasks

| # | 任务 | 依赖 | 状态 |
| --- | --- | --- | --- |
| D1 | 接通 `RenderGraph::OnResize`，统一 resize 分发，移除两处重复 fan-out 与 `EnsureGiPassesSized` | B、C | [x] |
| D2 | owner 追踪改 graph 推断（`FindLastWriterOwner` 取代设计伪代码的 `ProducerOf`） | B、C | [x] |
| D3 | composite input 选择 + 变更日志改 graph 推断 | B、C | [x] |
| D4 | Distortion 输入改 typed access（移除 SetInputBuffer 生产调用，保留方法供测试） | B、C | [x] |
| D5 | HDR/GI 状态机改 registry snapshot/config 驱动 | D1-D4 | [x] |
| D6 | ScenePass/Composite lambda 手工 bind 收敛为 graph attachment 声明（target 来源 context 化） | D1-D4 | [x] |
| D7 | resize/toggle 回归测试 + 边界脚本 + `legacy` 扫描 | D1-D6 | [x] |

## 6. Test Method

- **unit**: `RenderGraphTest`：断言 `RenderGraph::OnResize` 被调用、resize 后 descriptor 尺寸正确。
- **integration**: resize 循环 + HDR on/off + GI re-enable 切换 smoke（既有 fixture 扩展或新增）。
- **manual**: 实机调整窗口尺寸、切 HDR、开关 GI，确认无 GL 错误、无泄漏增长（对照 S4 快照）。
- **命令**:
  ```powershell
  ./build.bat
  ctest --test-dir build -C RelWithDebInfo -L "unit|integration" --output-on-failure
  python scripts/check_module_boundaries.py
  python scripts/check_legacy_reintroduction.py
  ```

## 7. Verification Of Task Completion

- D1: `RenderGraph::OnResize` 出现生产调用；逐 pass fan-out 移除；resize 后资源尺寸正确。
- D2/D3: 手工 owner 追踪、composite input 手工选择与变更日志 static 移除。
- D4: `SetInputBuffer` 生产调用移除；Distortion 经 typed access。
- D5: HDR/GI 状态机 static 状态移除，由 snapshot/config 驱动；切换行为无回归。
- D6: ScenePass/Composite lambda 内 BindFramebuffer/Viewport/Clear 移除手工 target 源。
- D7: build 双成功；ctest 无新增失败；resize/toggle 无 GL 错误；边界 71/71；`legacy` 通过。
- 提交经用户授权；handoff 如实报告。

## 8. Handoff Template

```text
package: phase-d-rendersystem
source baseline: <commit>
files changed: ...
contract changed: RenderGraph::OnResize 接入；owner/composite 推断化
focused tests + exact result: ...
broader build-test + known unrelated failures: ...
artifact path: ...
Track docs updated: M0-B spec §3 / debt RG-1/RG-2
remaining risk or blocker: ...
```
