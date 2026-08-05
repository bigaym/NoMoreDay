# Phase E: 旧光照 fallback 收敛

> **关联设计:** `docs/designs/2026-08-03-render-engine-interface-migration-design.md` §5.3
> **关闭债务:** 验收 §4.1（单一路径）
> **依赖:** Phase B、C（clustered 主路径访问已声明、barrier 已收敛）、Phase F（capability gate fail-closed）
> **状态:** [x] 已完成（2026-08-05）

> **open decision E2 已关闭（2026-08-05，用户确认）**：`clusteredLightingEnabled=false` 语义 = **fail-closed**（不做静默 V2 降级）。配置要求 clustered 而 capability 缺失（compute/image/SSBO/GL4.3 任一）走 Phase F `CheckProductionRequirements`（RenderSystem::Initialize fail-closed 报告，已实现，E4 核实接线一致，不新增重复探测）。LightingPass/VolumetricLightPass 在 clustered 未启用或 cluster 数据不可用时报告并跳过渲染（不产生错误输出），语义与 LightCulling 一致。

## 1. Authority And Boundaries

- **授权来源**: 渲染全量接口迁移 design §5.3；V4 spec §5.4 clustered 主路径。
- **范围**: LightingPass V2 直读 SSBO 降级路径（:310-406）与 VolumetricLightPass 非 clustered 直绑路径（:172-181）。
- **边界**: 明确 open decision E2——`clusteredLightingEnabled=false` 语义：fail-closed 还是显式降级路径（需用户确认后实施）。不引入新光照算法。

## 2. Verified Baseline

- 主路径已收敛：ClusteredLightingState 5 buffer 共享；LightCullingPass 需 `v3Enabled && dynamicLightingEnabled && clusteredLightingEnabled`（LightCullingPass.cpp:129-133）。
- 旧路径：LightingPass.cpp:310-406（clustered 失败降级 `fallback=V2Lighting`，uLightCount 直读）；VolumetricLightPass.cpp:172-181（非 clustered 直绑 SSBO 循环）。

## 3. Implementation Rationale

clustered 为主路径后，旧 V2 直读路径是运行时静默降级——隐藏能力缺失与行为分歧。收敛方式：删掉 fallback 分支，把"是否 clustered"变成显式 feature 状态。若配置要求 clustered 而能力缺失，按 fail-closed 原则报告（走 F 组 capability gate），不做静默 V2。

## 4. Pseudocode Guidance

```text
// LightingPass::Execute
// 移除: fallback=V2Lighting 分支（:310-406）
// 保留: clustered 主路径（SSBO_LIGHT_DATA + cluster buffer 绑定）
assume state.clusteredLightingEnabled == true (由 LightCulling 已执行保证)

// VolumetricLightPass::Execute
// 移除: 非 clustered 直绑 SSBO + 循环（:172-181）
// 仅 clustered 路径绑定 cluster index buffer

// 配置层
if renderConfig.clusteredLightingEnabled && !capability.clustered:
    failClosed(ReportCapabilityMissing("clustered"))
```

## 5. Atomic Tasks

| # | 任务 | 依赖 | 状态 |
| --- | --- | --- | --- |
| E1 | 决策落地：`clusteredLightingEnabled=false` 语义（fail-closed 或显式降级） | 用户确认 | [x] 2026-08-05 关闭为 fail-closed |
| E2 | LightingPass 移除 V2 fallback 分支 | E1 | [x] 2026-08-05 |
| E3 | VolumetricLightPass 移除非 clustered 路径 | E1 | [x] 2026-08-05 |
| E4 | 能力缺失 fail-closed 接入（与 F 组 gate 协作） | E2/E3、F | [x] 2026-08-05 核实已覆盖 |
| E5 | clustered 集成测试 + 回归 | E2/E3 | [x] 2026-08-05 |

## 6. Test Method

- **unit**: lighting 路径契约测试：断言无 `V2Lighting` fallback 符号/分支残留；clustered 主路径绑定正确。
- **integration**: clustered 光照 smoke（场景含多光源、cluster readback）。
- **manual**: 实机确认 clustered 光照视觉与降级删除前一致。
- **命令**:
  ```powershell
  ./build.bat
  ctest --test-dir build -C RelWithDebInfo -L "unit|integration" --output-on-failure
  python scripts/check_module_boundaries.py
  python scripts/check_legacy_reintroduction.py
  ```

## 7. Verification Of Task Completion

- E1: 语义决策有书面记录（design §8 open decision 关闭）。
- E2/E3: `V2Lighting`/非 clustered 分支移除；grep 无 `fallback=V2`、无非 clustered SSBO 直绑循环。
- E4: 能力缺失报告接入生产路径（配合 F）。
- E5: build 双成功；ctest 无新增失败；边界 71/71；`legacy` 通过；实机视觉一致。
- 提交经用户授权；handoff 如实报告。

## 8. Handoff Template

```text
package: phase-e-lighting-fallback
source baseline: <commit>
files changed: ...
contract changed: clustered 成为唯一主路径；降级语义显式化
focused tests + exact result: ...
broader build-test + known unrelated failures: ...
artifact path: ...
Track docs updated: V4 spec §5.4 更新确认
remaining risk or blocker: ...
```
