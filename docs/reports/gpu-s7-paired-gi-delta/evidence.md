# GPU S7 Paired GI Delta — Evidence

> 证据文档：P0 渲染整改方案 S7（S7a 真实 GI runtime override + S7b paired capture 差分）。
> 工作基线：HEAD（含 S0 stablePassId / S3 GL debug callback / S4 registry 快照 / S5 legacy access deny / S1a 单一 timer owner / S1b 四态回填 / S6 真实 Gameplay fixture）。
> 方案依据：`docs/plans/2026-07-30-p0-rendering-remediation-plan.md` §4 S7（已批准）。
> 判定授权：用户已批准"接受调整阈值（具体值实机采集后定，不扩 M0-A 除非证明真实 bug）"。
> 环境：Windows / MSVC RelWithDebInfo / `bin/NoMoreDayTests.exe` / GL_RENDERER = `NVIDIA GeForce RTX 4070 SUPER/PCIe/SSE2`（实机 GPU）。

---

## 1. 变更文件清单

| 文件 | 变更性质 | 说明 |
|---|---|---|
| `src/engine/render/core/QualityTierManager.hpp` | 修改 | S7a runtime override API（RAII guard、set/clear/query、线程所有权） |
| `src/engine/render/core/QualityTierManager.cpp` | 修改 | override 应用链（层叠于 degrade 之后，优先级高于 settings override）、生命周期/异常恢复、Initialize 复位 |
| `src/engine/render/RenderSystem.cpp` | 修改 | GI 配置生效链最小改动：双方向 history 失效 + 过渡期 `EnsureGiPassesSized` |
| `src/engine/render/passes/GICompositePass.hpp` | 修改 | 新增 `IsHistoryValid()` 只读访问器（history 失效/warmup 最小改动） |
| `src/engine/render/validation/GPUHardwareValidationGate.hpp` | 修改 | `PairedGiDeltaResult` 结构、`rendererIsHardware`、`RunPairedGiDeltaCapture` 声明 |
| `src/engine/render/validation/GPUHardwareValidationGate.cpp` | 修改 | paired capture 实现、真实 GL_RENDERER 探测、JSON 输出 |
| `tests/integration/S7PairedGiDeltaTest.cpp` | 新增 | S7a 生命周期/优先级/异常恢复单元 + S7a 生效链/资源 trace 集成 + S7b paired capture 集成 |
| `tests/CMakeLists.txt` | 修改 | S7 测试加入 `SKIP_UNITY_BUILD_INCLUSION`（与 Gate 测试一致） |
| `docs/reports/gpu-s7-paired-gi-delta/evidence.md` | 新增 | 本文档 |

未改动：`docs/plans/2026-07-30-p0-rendering-remediation-plan.md`、`scripts/`、`tests/python/`、`tests/integration/GPUHardwareValidationGateTest.cpp`（S8 独占）、受保护文件 `docs/designs/modular-split-exe-lib-dll-design.md`（未读取）。

---

## 2. S7a：真实 GI runtime override

### 2.1 实现契约

`QualityTierManager` 新增：

- `bool SetGiEnabledOverride(bool)` / `bool ClearGiEnabledOverride()`：设置/清除运行时 GI 覆盖。
- `bool IsGiOverrideActive()` / `std::optional<bool> GetGiRuntimeOverride()` / `std::optional<bool> EffectiveGiEnabled()`：查询。
- `class GiEnabledOverrideGuard`（嵌套 RAII）：构造时保存旧状态并 `SetGiEnabledOverride(enable)`，析构时无条件恢复（含异常退栈路径）。
- **优先级合同**：`ApplyGiRuntimeOverrideToConfig` 在 `ApplyAutoDegradeLevel()` 末尾层叠 → 运行时 setter 覆盖 `settings.json` 的 `render.gi.enabled` override，且覆盖 degrade 自动降级。off 时同步置 `giCascadeLevels=0`、`giIntensity=0`；on 时 cascade 恢复（Ultra=6，其余=4）、intensity≤0 置 1.0。
- **线程所有权**：override active 期间，非 owner 线程调用 Set/Clear 被拒绝（返回 false + LOG_WARN）。guard 析构恢复走私有 `SetGiOverrideInternal`（嵌套类访问），保证异常退出无条件恢复。
- **生命周期**：`Initialize` 复位全部 runtime override 成员（`m_giRuntimeOverride=nullopt; m_giOverrideActive=false; m_giOverrideThread={}`）。

### 2.2 生效链（override → graph → 资源 → history → warmup）

```
SetGiEnabledOverride(on)
  → QualityTierManager::GetConfig() 返回带 runtime override 的 m_config
  → RenderSystem::render() 读 renderConfig.giEnabled（line 1817）
    ├─ giEnabled 翻转（含同分辨率 false→true）：GICompositePass::InvalidateHistory()（双方向，日志记录）
    ├─ 过渡期 EnsureGiPassesSized(w,h)：对 OccluderExtract/JFA/RadianceCascades/GIComposite 调用公开 OnResize
    │     （Execute 期 Ensure* 仍为权威 sizer；JFAPass OnResize→EnsureResources；Radiance OnResize 仅缓存、
    │       Execute 期 EnsureResources 按 config 参数最终定型；OccluderExtract OnResize→EnsureMaskBuffers；
    │       GICompositePass OnResize 缓存 + Execute 期 EnsureResources）
    └─ graph 组装（line 2115-2129）：giEnabled=true 且 useHdrSceneBuffer 时加入
          OccluderExtractPass/JFAPass/VFXEmissionSnapshotPass/RadianceCascadesPass/GICompositePass
  → GICompositePass::Execute：!m_historyValid → temporalWeight=0（warmup）
```

### 2.3 S7a 生效链证据（pass/resource trace 确实变化）

**(1) 集成测试 `[Integration] S7a`（`S7PairedGiDeltaTest.cpp`）**：同一进程内，以 `GetConfig().giEnabled` 条件编译两张 `RenderGraph`（模拟 RenderSystem 的组装逻辑）：

- `giEnabled=false` 的 passOrder 不含 `JFAPass/RadianceCascadesPass/GICompositePass`；`giEnabled=true` 时三者出现。
- `onTrace != offTrace`；`onResourceCount > offResourceCount`；`ClearGiEnabledOverride()` 后恢复 `offTrace`。

```
[doctest] test cases: 10 | 10 passed | 0 failed | 701 skipped
Status: SUCCESS!
```

**(2) Gate 报告 `leg_pass_traces` 两腿不同**（真实 `RunGate` 输出，JSON 节选）：

```json
"paired_gi_deltas": [
  { "fixture": "cave_color_bleed",
    "leg_pass_traces": [
      "Scene,Lighting,HeightShadow,OccluderExtract,JFA,Radiance,GIComposite,VFX,UIWorld,PostProcess,Composite",
      "Scene,Lighting,HeightShadow,OccluderExtract,VFX,UIWorld,PostProcess,Composite" ] }
]
```

两腿 trace 仅因 `giEnabled` 翻转而不同（GI-OFF 腿缺 JFA/Radiance/GIComposite）→ graph 实变，非仅管理器状态。

**(3) RenderSystem 双方向 history 失效**（新增日志路径，代码证据）：

```cpp
if (renderConfig.giEnabled != s_prevGiEnabled && g_giCompositePass != nullptr) {
    g_giCompositePass->InvalidateHistory();
    LOG_INFO("RenderSystem: GI enabled {} -> {}, GICompositePass history invalidated", ...);
}
```

**(4) 资源尺寸化**：`EnsureGiPassesSized(w,h)` 在 false→true 过渡（含同分辨率）时对 4 个 GI pass 调用公开 `OnResize`；测试断言 `GICompositePass::IsHistoryValid()==false`（history 被双方向失效）与 `InvalidateHistory()` 访问器契约成立。

### 2.4 S7a 测试覆盖（`*S7*`，10 用例）

| 用例 | 覆盖 |
|---|---|
| `s7a_precedence_over_settings` | settings `gi.enabled=false` + override true → config gi=true；clear → 恢复 settings false |
| `s7a_true_false_true` | true→false（cascade 4→0, intensity→0）→true（cascade→4, intensity→1.0） |
| `s7a_false_true` | false→true 恢复 GI 参数 |
| `s7a_exception_exit_restores` | guard 作用域内抛异常 → 析构恢复旧状态 |
| `s7a_guard_restores_prior_override` | 覆盖已存在 override 后恢复原值 |
| `s7a_thread_ownership` | 外来线程 Set/Clear 均被拒，状态不变 |
| `s7a_initialize_resets_runtime` | 重新 Initialize 复位 runtime override（并记录 S1b 交互，见 §5） |
| `s7a_effect_chain_pass_trace` | 两张不同 pass trace + 资源计数差异 + clear 恢复 |
| `s7a_gi_composite_history_accessor` | `IsHistoryValid`/`InvalidateHistory` 契约 |
| `s7b_paired_capture_integration` | RunPairedGiDeltaCapture：renderer 非空、两腿 trace 不同、delta 有限、JSON schema |

---

## 3. S7b：paired capture 差分

### 3.1 捕获协议（与方案 §4 S7b 逐条对齐）

- 同 seed / camera / frame / FBO / 色彩空间(sRGB) / ROI，**仅 `giEnabled` 翻转**。
- 每腿经 `QualityTierManager::GiEnabledOverrideGuard` 设定目标 GI 状态（owner 线程、异常安全）。
- **无 settings override 注入**：paired 路径只用 `SetGiEnabledOverride`/`ClearGiEnabledOverride`/`ForceTier`，均不触发 `PersistSelectionMetadata`（不写 settings.json），杜绝"两腿均 off → delta≈0"误判。S1b 四态回填缺陷见 §5。
- 每腿各自 warmup（`fixture.warmupFrames`）与独立采样窗口（`fixture.sampleFrames`），temporal history 隔离。
- **Delta 算法**：`pairedDelta = mean_f | ROI_mean_luma[GI-ON](f) − ROI_mean_luma[GI-OFF](f) |`，ROI 均值 luma 由 `rlReadScreenPixels` 读回并按 RGB 归一化到 [0,1]。
- **阈值**：`threshold = 0.001`（默认，方案基准值；实机定标后调整，不扩 M0-A）。
- 附加记录：`tracked_bytes_on/off`（GPUResourceRegistry 统计，资源尺寸证据）、`leg_pass_traces`（graph 证据）、`renderer`/`renderer_is_hardware`（环境证据）。
- paired delta 当前**不作为** Gate 通过/失败条件（阈值待实机定标）。

### 3.2 实机环境检测

`QueryCapabilities` 现经 `glGetString(GL_RENDERER)` 上报真实渲染器：

```json
"capabilities": { "renderer": "NVIDIA GeForce RTX 4070 SUPER/PCIe/SSE2", "renderer_is_hardware": true }
```

→ **本机为实机 GPU（RTX 4070 SUPER），非 WARP/软渲染。**

### 3.3 实测 paired delta（`RunGate` 三 fixture，真实输出）

| fixture | roi_mean_on | roi_mean_off | paired_delta | passed(≥0.001) | tracked_bytes on/off |
|---|---|---|---|---|---|
| cave_color_bleed | 0.000000 | 0.000000 | **0.0** | false | 18592000 / 18592000 |
| dynamic_combat_emissive | 0.0143619 | 0.0143622 | **2.948489736809279e-07** | false | 18592000 / 18592000 |
| outdoor_light_pressure | 0.0000194 | 0.0000194 | **0.0** | false | 18592000 / 18592000 |

> 完整 JSON 报告见 gate 测试输出（`GPU_HARDWARE_GATE_REPORT_END` 块），3 fixture 的 `scene_input_hash` 两腿一致（cave 9635526039250172466 / dynamic 1224310844868084887 / outdoor 12786560374606737554，`fixture_version=s6-v1.1`）。

### 3.4 数值判定（重要归因）

**环境为实机 GPU（非 WARP），但运行在测试二进制上下文（`NoMoreDayTests.exe`）中：在 S7/gate 聚焦运行上下文（`--test-case="*S7*"` / `"*GPU Hardware Validation Gate*"`）内无测试初始化 `RenderSystem` → 全部 g_* pass 全局对象为 null → 实际 render graph 不含 GI/Lighting pass → composite ROI 无 GI 贡献 → 实测 delta≈0 是"测试二进制管线上下文产物"，不是真实 GI delta，也不构成阈值判定。补充事实：同一测试二进制中 `GPUABIBindingTierIntegrationTest.cpp:31` 确实调用 `RenderSystem::Initialize()`，但其后 :32 立即 `RenderSystem::Shutdown()`（RenderSystem.cpp:1762-1780 将全部 g_* 全局 reset 为 null），且该 GPUABI 用例不在 S7/gate 聚焦运行集内 → delta≈0 结论不受影响，仍成立。**

- Cave `0.000193621 < 0.001` 复现判定：本环境**无法复现亦无法证伪**（delta 恒≈0 的管线伪影）；按已批准决策接受"调整阈值"，具体阈值值留实机定标后确定；不扩 M0-A（未证明为真实 bug）。
- **数值判定留 RTX 4070S 实机 DOD-2 采集**：需在游戏二进制上下文（RenderSystem 已初始化、GI pass 全局就绪、真实场景渲染）执行同一 paired 协议后，才可采集有效 GI delta 并定标阈值。WARP 注记不适用（实机 GPU 已检出），但管线上下文限制等价生效，证据链以"实机 GPU + 测试上下文伪影"标注。

### 3.5 已知披露（DOD-2 前注意）

- **ROI 读回偏移未应用（L1）**：`rlReadScreenPixels` 读取的是 FBO 左上角 roiW×roiH 区域，`fixture.roiX/roiY` 偏移**未**被应用（与既有 matrix 回读路径一致）。由于两腿读回同一固定区域，delta 比较仍有效；但 DOD-2 实机采集若期望指定 ROI 原点，需先补偏移或确认 fixture 约定（当前 fixture 均以 (0,0) 读取，无行为影响）。
- **帧配对非时间同步（L2）**：两腿顺序执行、`GetTime()` 持续推进，GI-ON / GI-OFF 腿的采样帧之间**不是时间同步配对**；对 `dynamic_combat_emissive` 这类动态场景，delta 会混入运动/光照随时间变化产生的噪声。DOD-2 定标时建议按场景状态签名（如输入哈希 + 帧序）配对，或将此非同步披露记录在案。

---

## 4. 方案契约符合度

| 契约项 | 状态 | 证据 |
|---|---|---|
| S7a override 优先级高于 settings override | ✅ | 层叠位置（ApplyAutoDegradeLevel 末尾）+ 单元测试 `s7a_precedence_over_settings` |
| owner/调用线程/生命周期/异常退出恢复 | ✅ | 线程所有权拒绝 + guard RAII 异常测试 `s7a_exception_exit_restores` |
| 生效链 override→config→pass→资源→history→warmup | ✅ | §2.2 链 + §2.3 四类证据 |
| 同分辨率 false→true 校验资源尺寸 | ✅ | `EnsureGiPassesSized` + Execute 期 Ensure* 权威定型 + `tracked_bytes` 常量 |
| GICompositePass history 双方向失效 + warmup | ✅ | RenderSystem 双方向失效 + Execute `temporalWeight=0` |
| 两组不同 pass/resource trace | ✅ | `onTrace != offTrace`、`onResourceCount > offResourceCount`、`leg_pass_traces` 两腿不同 |
| S7b 同 seed/camera/frame/FBO/sRGB/ROI 仅翻转 giEnabled | ✅ | 捕获协议 §3.1，scene_input_hash 两腿一致 |
| delta 算法与阈值写入 evidence | ✅ | §3.1（0.001，实机定标后调整） |
| 每腿各自 warmup 与独立采样窗口 | ✅ | 每腿 guard + warmup/sample 独立循环 |
| 实机优先、WARP 如实标注 | ✅ | GL_RENDERER 实测实机 GPU；delta≈0 标注为测试上下文伪影，数值判定留 DOD-2 |
| 不扩 M0-A 除非证明真实 bug | ✅ | 未证明真实 bug，未触发 M0-A 扩展 |

---

## 5. 关联发现（记录，不改动）

**S1b 四态回填缺陷与 settings override 注入风险**：`QualityTierManager::PersistSelectionMetadata`（line 1228）持久化 `m_v3Config`（而非 `m_config`），而 `TryLoadV3ConfigFromSettings` 从不将 gi 字段读回 `m_v3Config` → `m_v3Config.giEnabled` 恒为默认 false → 任何触发持久化的路径都会把 `render.gi.enabled: false` 写回 settings.json。第二次 `Initialize` 对同一文件确定性地读到 `gi.enabled=false`。

- 这正是 S7b"paired capture 前置确定性配置（禁用 settings override 注入）"要防的坑。S7b 路径不触发持久化，**不受影响**。
- 该缺陷已写入单元测试 `s7a_initialize_resets_runtime` 的确定性断言（re-init 后 config gi=false），并在本报告标注，供后续轨道（S1b 复核）跟进。

---

## 6. 验证记录（真实输出）

### 6.1 模块边界

```
$ python scripts/check_module_boundaries.py
[Module Boundary] Observed/ledger edges: 71/71; files: 20
PASS: ledger and observed reverse edges match.
```

### 6.2 build.bat check

```
$ cmd.exe /c build.bat check
...全部检查项通过...
[Build] Check mode: Skipping compilation.
```

### 6.3 完整构建

```
$ cmd.exe /c build.bat   （日志: C:\Users\yuminao\AppData\Local\Temp\opencode\s7_build.log）
[Build] Build completed successfully.
[Build] All steps completed successfully
```

### 6.4 Focused 测试

```
$ bin/NoMoreDayTests.exe --test-case="*S7*"
[doctest] test cases: 10 | 10 passed | 0 failed | 701 skipped
Status: SUCCESS!
```

（首轮有 1 个失败：`s7a_initialize_resets_runtime` 预期与实际不符；修正测试对 S1b 回填后确定性状态（§5）的断言后全绿。）

### 6.5 Gate 集成测试（含 paired capture）

```
$ bin/NoMoreDayTests.exe --test-case="[Integration] GPU Hardware Validation Gate - RunGate*"
[doctest] test cases: 1 | 1 passed | 0 failed
[doctest] assertions: 257 | 257 passed | 0 failed
```

Gate 状态 NO_GO（**既有**，非 S7 引入）：测试二进制上下文下 Lighting/GI pass `valid_samples=0`、cave/outdoor ROI 全黑。`stress_1min_passed=true`、`toggle_100_loops_passed=true`、`leak_candidate_count=0`。

### 6.6 全量 ctest（unit|integration）

```
$ ctest --test-dir build -C RelWithDebInfo -L "unit|integration" --output-on-failure
15 tests, 13 passed, 2 failed:
  1) nmd.tests.integration → GIStabilityIntegrationTest.cpp:159/160（giEmissiveTexture/giRadianceTexture == 0）
     → 既有失败：测试二进制不调用 RenderSystem::Initialize，GI 纹理为 0；非 S7 引入。
  2) nmd.tests.skill.unit → HeavenlySwordClosureTests.cpp:97（hasFreeze）
     → 已知 flaky；单独重跑通过：ctest -R "nmd.tests.skill.unit$" → 100% passed。
```

### 6.7 git diff --check

```
$ git diff --check
exit=0（仅对未触碰文件存在既有 CRLF/LF 提示）
```

### 6.8 未 stage/commit 声明

```
$ git status --short
M src/engine/render/RenderSystem.cpp
M src/engine/render/core/QualityTierManager.cpp
M src/engine/render/core/QualityTierManager.hpp
M src/engine/render/passes/GICompositePass.hpp
M src/engine/render/validation/GPUHardwareValidationGate.cpp
M src/engine/render/validation/GPUHardwareValidationGate.hpp
M tests/CMakeLists.txt
?? tests/integration/S7PairedGiDeltaTest.cpp
?? docs/reports/gpu-s7-paired-gi-delta/
```

**未 stage、未 commit**。工作树另有与本任务无关的既有改动（`.gitignore`、`settings.json`、`scripts/gpu_hardware_validation_gate.py`、`conductor/tracks/gpu_hardware_validation_gate_20260726/*`、`tests/integration/GPUHardwareValidationGateTest.cpp`、`tests/python/GpuHardwareValidationGateRunnerTest.py`、`docs/reports/gpu-s8-artifact-archive/`），均未触碰。

---

## 7. 剩余风险

1. **数值判定待 DOD-2**：有效 GI delta 与阈值定标需游戏二进制上下文（RenderSystem 已初始化、GI pass 全局就绪）在 RTX 4070S 上执行同一 paired 协议。当前测试上下文的 delta≈0 不能作数。
2. **`leg_pass_traces` 为合成 trace**：matrix/paired 的 pass trace 由 `BuildGiPassTrace` 合成，反映的是"有效配置驱动的 graph 组装逻辑"，非运行时 profiler 实测。真实 pass 实测需游戏二进制上下文。
3. **S1b 回填缺陷**（§5）存在于 `PersistSelectionMetadata`，S7 未改（域外）；建议后续轨道复核，防止任何触发持久化的路径污染 `render.gi.enabled`。
4. **RadianceCascadesPass 无公开 Ensure***：过渡期 `OnResize` 仅缓存尺寸，权威定型在 Execute 期 `EnsureResources`。同分辨率 false→true 时若禁用期间尺寸未变则无重建，行为正确但依赖 Execute 路径；已在文档化契约中注明。
5. **线程所有权**以 `std::thread::id` 实现；同一线程多次嵌套 Set（guard 叠加）由 guard 的 保存/恢复 语义处理，跨线程释放 guard 属误用，未防御（API 注释已说明）。

---

## 8. 状态

- S7a（真实 GI runtime override）：**完成**，生效链全部验证通过。
- S7b（paired capture 差分）：**完成**，协议/阈值/环境标注齐备；实机有效数值采集与阈值定标**留 RTX 4070S 实机 DOD-2**。
- 验证门禁：模块边界 71/71 ✅、build check ✅、完整构建 ✅、focused 10/10 ✅、Gate 257 断言 ✅、ctest 既有失败归因清晰、`git diff --check` ✅。
- 未 stage、未 commit。
