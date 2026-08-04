# Phase F: reload/capability 单一路径

> **关联设计:** `docs/designs/2026-08-03-render-engine-interface-migration-design.md` §5.6
> **关闭债务:** RG-4（reload/capability 平行路径）
> **依赖:** 建议 Phase B 后启动（可并行）
> **状态:** [ ] 未开始

## 1. Authority And Boundaries

- **授权来源**: 渲染全量接口迁移 design §5.6；M0-B spec §3（ABI/binding manifest、include 递归 hash、失败重试、capability gate、GL debug callback）。
- **范围**: 移除生产路径 `ShaderHotReloadManager` 的平行职责，统一到 `ShaderReloadGovernance`；VS/FS 纳入记录；`DeviceCapabilityMatrix` 接入生产路径 fail-closed；GL debug callback 安装（P0 S3 未做项）。
- **边界**: 不删除 dev 工具能力（open decision F3：整体删除或移出生产路径需用户确认）；保持 Debug-only 语义为配置而非硬编码。

## 2. Verified Baseline

- `ShaderHotReloadManager` 生产路径：RenderSystem.cpp:16 include、:189 实例、:927-987 注册 12 watch、:1115-1117 Clear、:1223-1231 每帧 SetEnabled+PollAndReload（kDevHotReloadAllowed=Debug）、:1234 MaterialManager::TryHotReload。
- `ShaderReloadGovernance`：ResourceManager.cpp:365-368 仅 compute 分支记录（ComputeIncludeHash+RecordReloadAttempt）；VS/FS `loadShader`（:266-310）未记录。
- `DeviceCapabilityMatrix` 仅 GPUHardwareValidationGate.cpp:880 使用，未 gating 生产路径。
- capability 检查项（spec §3）：GL 4.3/SSBO/compute/image/barrier/format/timer/debug callback。

## 3. Implementation Rationale

reload 双轨导致 hash/失败状态分裂。收敛方案：`ShaderReloadGovernance` 成为唯一 reload 状态与失败重试 owner；`ShaderHotReloadManager` 降级为 dev-only 驱动（或移除，按 open decision）。capability 在引擎初始化时建矩阵，生产路径在依赖缺失时 fail-closed（例如 clustered 依赖 compute/image/SSBO）。GL debug callback 在初始化安装，错误经 callback 上报而非静默。

## 4. Pseudocode Guidance

```text
// ResourceManager
loadShader(vs, fs): program = LoadShaderFromMemory(...)
    ShaderReloadGovernance.Record(program, {vsHash, fsHash, includesRecursive})  // 补全 VS/FS
loadComputeShader(...): existing compute record (keep)

// 生产路径（RenderSystem）
init: capability = DeviceCapabilityMatrix::Query()
      for each required feature: if !capability.feat: failClosed(ReportCapabilityMissing(feat))
render loop:
    #if DEBUG
    if config.devHotReload: governance.PollAndReload()      // 单一 owner，替代 ShaderHotReloadManager
    #endif

// GL debug callback（P0 S3）
installDebugCallback(severity->log);  // GL_DEBUG_OUTPUT + callback，severity 过滤
```

## 5. Atomic Tasks

| # | 任务 | 依赖 | 状态 |
| --- | --- | --- | --- |
| F1 | `ShaderReloadGovernance` 覆盖 VS/FS + 递归 include hash + 失败重试 | - | [ ] |
| F2 | 生产路径 reload 统一走 governance；`ShaderHotReloadManager` 决策（删除或 dev-only 移出） | F1、用户确认 F3 | [ ] |
| F3 | `DeviceCapabilityMatrix` 接入生产路径 fail-closed（clustered 等依赖项） | - | [ ] |
| F4 | GL debug callback 安装（P0 S3） | - | [ ] |
| F5 | reload/capability fallback 自动化测试 + 回归 | F1-F4 | [ ] |

## 6. Test Method

- **unit**: `ShaderReloadGovernanceTest`（如存在）或新增：VS/FS hash 记录、include 修改触发重载、失败保留上次成功 fingerprint。
- **integration**: capability 缺失模拟（强制禁用某 feature）断言 fail-closed 报告；reload 触发集成 smoke。
- **manual**: Debug 构建改 shader 文件触发重载；Release 构建确认 reload 代码不编译。
- **命令**:
  ```powershell
  ./build.bat
  ctest --test-dir build -C RelWithDebInfo -L "unit|integration" --output-on-failure
  python scripts/check_module_boundaries.py
  python scripts/check_legacy_reintroduction.py
  ```

## 7. Verification Of Task Completion

- F1: VS/FS 与 compute 均记录；include 递归 hash；失败重试保留上次成功 fingerprint。
- F2: 生产路径无第二 reload owner；`ShaderHotReloadManager` 决策书面化。
- F3: 能力缺失触发 fail-closed（报告而非静默）；clustered 依赖项纳入。
- F4: GL debug callback 安装；错误经 callback 上报；无影响正常渲染。
- F5: build 双成功；ctest 无新增失败；边界 71/71；`legacy` 通过。
- 提交经用户授权；handoff 如实报告。

## 8. Handoff Template

```text
package: phase-f-reload-capability
source baseline: <commit>
files changed: ...
contract changed: reload 单一 owner；capability fail-closed；GL debug callback 安装
focused tests + exact result: ...
broader build-test + known unrelated failures: ...
artifact path: ...
Track docs updated: M0-B spec §3 / debt RG-4 / P0 S3
remaining risk or blocker: ...
```
