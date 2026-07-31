# S6 真实 Gameplay fixture 实施证据

> 轨道：`gpu_hardware_validation_gate_20260726`（M0-C R1.2）
> 计划：`docs/plans/2026-07-30-p0-rendering-remediation-plan.md` §4 S6（状态 `[x]`）
> 实施日期：2026-08-01
> 基线：HEAD `beace9f`（本变更未提交，`git status --short` 见文末）

## 1. 只读审计结论（复用/新建决策）

### 审计范围
- `tests/` 全部目录（fixtures/functional/game/integration/perf/performance/python/tech/unit）中的 ECS 场景构造模式。
- `src/app/Game.cpp`（真实初始化参考）、`src/app/SharedContext.hpp`、`src/game/components/`、`src/engine/render/RenderSystem.cpp` 对 registry 的消费组件。

### 审计结果

| 候选复用源 | 内容 | 复用结论 |
|---|---|---|
| `tests/fixtures/LightingStabilityTest.cpp` | `PopulateStabilityLights` 批量 `Position+LightComponent` 实体（256 个） | **复用其模式**（非代码复制）：三个配方均以 `Position+LightComponent` 构造真实光源实体 |
| `tests/.../GameplaySystems.cpp` | 真实 `emplace<PlayerTag/Position/Velocity>` 演示 | **复用其模式**：dynamic_combat 配方照此构造 PlayerTag/EnemyTag 实体 |
| `tests/` 现存关卡快照 | 无完整"关卡快照"（只有散点组件测试场景） | **无可用** |
| `src/app/Game.cpp` | 完整 LevelManager/SceneManager/StateManager 初始化 | **不复用**：涉及 raylib 窗口/音频初始化，gate 环境需最小化；仅参考 SharedContext 接线 |

### 决策（依据 §8 决策 3：优先复用现有测试 fixture，无法实现则新建关卡快照）

1. **复用面不足**：现有测试 fixture 均为散点组件场景，无完整关卡快照可直接驱动生产 `RenderSystem::render`。因此**新建三个最小真实场景配方**——使用**真实 Game 组件类型**（PlayerTag/EnemyTag/Position/PrevPosition/Velocity/Radius/ColliderComponent/VisionComponent/ShadowCasterComponent/LightComponent/MapTileComponent/ColorComponent/VisualEffect/AttackEffect），但不要求完整关卡（不引入 LevelManager/SceneManager）。
2. **复用既有批量构造模式**：光源构造沿用 `PopulateStabilityLights` 的 `Position+LightComponent` 模式；角色实体沿用 GameplaySystems 的 `PlayerTag+Position+Velocity` emplace 模式。
3. **架构分界**：抽象接口 `FixtureRenderDriver` 放 `src/engine/render/validation/`（仅依赖引擎类型，无 game/app 头），真实实现 `GameplayRuntimeHarness` 放 `tests/integration/`（测试侧可自由引用 game/app 头）——避免 engine→game 反向依赖破坏模块边界 71/71。

## 2. 变更文件清单

| 文件 | 变更类型 | 要点 |
|---|---|---|
| `src/engine/render/validation/FixtureRenderDriver.hpp` | 新增 | S6 抽象接口：`PrepareFixture/Registry/Context/CompositeFramebuffer/CompositeWidth/CompositeHeight/SceneInputHash/FixtureVersion/SceneSource`；仅引擎类型 + `entt/entt.hpp`；前向声明 `NoMoreDay::SharedContext` |
| `src/engine/render/validation/GPUHardwareValidationGate.hpp` | 修改 | `RunGate` 增加 `FixtureRenderDriver* driver=nullptr` 参数；`FixtureExecutionResult` 增加 `sceneInputHash/fixtureVersion/sceneSource` 字段 |
| `src/engine/render/validation/GPUHardwareValidationGate.cpp` | 修改 | driver==nullptr → fail-closed `NOT_RUN`（不再构造空 registry/SharedContext，符合 validation.md L121）；用 driver 的 registry/context 驱动矩阵/采样/压力/toggle 循环；`PrepareFixture` 于 fixture 级调用；fixture target 改为 `driver->CompositeFramebuffer()` 引用（T6.4）；JSON 输出 `scene_input_hash/fixture_version/scene_source`（T6.5） |
| `tests/integration/GameplayRuntimeHarness.hpp` | 新增 | 真实 harness：三个配方构造、owned RGBA16F FBO、FNV-1a 64 输入哈希、RAII 生命周期 |
| `tests/integration/GPUHardwareValidationGateTest.cpp` | 修改 | RunGate 测试传 harness；新增 "Missing driver fails closed NOT_RUN" 用例；断言 matrix 单元格含 fixture 信息 |
| `tests/unit/GameplayRuntimeHarnessTest.cpp` | 新增 | 5 个 harness 单元测试（配方组件计数、哈希确定性/差异性、未知 recipe 拒绝） |
| `docs/plans/2026-07-30-p0-rendering-remediation-plan.md` | 修改 | S6 状态 `[x]`，T6.1-T6.5 各标记完成，附实施记录 |
| `docs/reports/gpu-s6-gameplay-fixture/evidence.md` | 新增 | 本文档 |

未改动：`docs/designs/modular-split-exe-lib-dll-design.md`（受保护）、CMake/build.bat/PCH、ledger、任何其他引擎文件。

## 3. T6.1-T6.5 契约符合度

### T6.1 `GameplayRuntimeHarness`（真实 ECS/SharedContext/资源构造，owner 与生命周期合同）— 符合
- 构造真实 `entt::registry`（unique_ptr 持有），emplace 真实 Game 组件（见 §1）。
- 最小 `SharedContext`：`registry/settings/renderAlpha` 接线，其余字段默认 nullptr（RenderSystem 消费路径已审计全部 nullptr 安全）；`GameSettings` 默认构造（cameraZoom=1.5f），无文件 IO。
- RAII：构造创建 registry/settings/context，析构顺序 = ReleaseCompositeTarget → 释放 context/settings/registry；copy 删除（独有所有权）。

### T6.2 `FixtureRenderDriver`（固定 fixture 驱动 `RenderSystem::render`）— 符合
- 抽象接口定义于引擎侧，gate 通过它消费 driver 的 registry/context/composite target；`RunGate` 主流程与 matrix/采样逻辑保持原样，仅替换空 registry/SharedContext 构造点与 fixture target 来源。
- driver==nullptr 时 gate 返回 `NOT_RUN`（fail-closed），不再走空 registry 合成路径。
- gate 的 pass 预算采样循环、ROI 读回、压力循环全部在 driver 的真实场景上运行。

### T6.3 三个 fixture 配方 — 符合
- **cave_color_bleed**（seed 0xCA000001）：16 箱体 shadow occluders（r=340 环）、40 个 emissive 色块晶体（红/蓝/紫）、中心暖光 + 12 个 accent 光源、地板 `MapTileComponent`。
- **dynamic_combat_emissive**（seed 0xC0CB0002）：player（PlayerTag+Position+Velocity+Radius+Collider）、10 enemies（EnemyTag+Vision+ShadowCaster dynamic）、8 移动遮挡柱、24 VisualEffect（SwordIntentBurst/Pickup）、6 AttackEffect 弧、10 combat lights（含 flicker）。
- **outdoor_light_pressure**（seed 0x00000003）：按循环放置的 treeline Circle occluders（中心 r<160 留空过滤，实际数量视布局而定，seed 0x00000003 下实际放置 45 个）、宽地板、60 地面色块、**220 光源压力网格**（9×9 扩大为 15×15 以达 220 上限）。
- 确定性：**本平台确定性**（不承诺跨编译器位级复现）——全部用 xorshift32（`DeterministicRng`），**不用 `std::srand/rand`**（实现定义序列不可复现）；cave 环形遮挡坐标经 `std::cos/sin`，其末位 ULP 因 libm 而异，故 FNV 输入哈希仅承诺同平台/同 libm 下可复现；同 seed 两次构造哈希一致（单元测试断言）。
- 版本与 provenance：`FixtureVersion()=="s6-v1.1"`（v1.0 冻结后经 9×9→15×15 网格修正，版本提升 v1.0 → v1.1，见 §4.5；版本字符串不参与 FNV 输入哈希，哈希值不变），`SceneSource()=="tests/integration/GameplayRuntimeHarness.hpp (deterministic recipe, real game components, no level snapshot)"`。

### T6.4 render target 所有权 — 符合
- 离屏 RGBA16F composite FBO **由 harness 独占**：`PrepareFixture` 时按 fixture 尺寸 `FramebufferManager::Create`（若已有先 `ReleaseCompositeTarget`），gate 结束时由 harness 析构/`ReleaseCompositeTarget` 销毁。
- gate 不再创建/销毁 fixture target（原 `Create/Destroy(offscreenHandle)` 移除），仅通过 `driver->CompositeFramebuffer()` 读 fbo id。
- gate 的 stress/toggle 循环自建/自毁的**临时压力 FBO** 仍归 gate 自有，与 harness 的 fixture target 是不同对象，无重复创建、无 UAF。
- 泄漏验证：gate 报告 `leak_candidate_count: 0`，`stress_1min_passed: true`（S4 净增长判定通过）。

### T6.5 artifact/version 合同（fixture 输入哈希、输出与证据对应）— 符合
- `FixtureExecutionResult.sceneInputHash/fixtureVersion/sceneSource` 写入 GateReport JSON：
  - `scene_input_hash`：FNV-1a 64，对 recipe 名 + seed + 每实体标识数据（tag/坐标）确定性累计；复现性由单元测试断言（同 seed 相等、异 seed 不等）。
  - 三 fixture 实测哈希：cave `9635526039250172466`、dynamic `1224310844868084887`、outdoor `12786560374606737554`（互不相同且非零）。
  - `fixture_version`：`s6-v1.1`（v1.0 → v1.1 版本提升，见 §3 T6.3 与 §4.5；版本字符串不参与 FNV 输入哈希）。
  - `scene_source`：harness 文件路径与说明。
- 输出/证据对应：每 matrix 单元格含该 fixture 的哈希/版本/来源，直接对应本 evidence §5 的 JSON 摘录。

## 4. 验证记录（真实输出）

### 4.1 模块边界
```
python scripts/check_module_boundaries.py
[Module Boundary] Observed/ledger edges: 71/71; files: 20
[Module Boundary] PASS: ledger and observed reverse edges match.
```

### 4.2 构建检查
```
cmd.exe /c build.bat check
[Build] OK: ... （全部检查项通过）
[Build] Check mode: Skipping compilation.
```

### 4.3 完整构建（日志 `%TEMP%\opencode\s6-build.log`）
```
exit=0
[Build] Build completed successfully.
[Build] All steps completed successfully
```
两条成功标记均已读取确认。

### 4.4 gate 集成测试（含新增 driver / NOT_RUN 用例）
```
bin\NoMoreDayTests.exe --test-case="*GPU Hardware Validation Gate*"
[doctest] test cases:   4 |   4 passed | 0 failed | 690 skipped
[doctest] assertions: 299 | 299 passed | 0 failed |
[doctest] Status: SUCCESS!
```
- 用例：QueryCapabilities/Fixtures（3 fixture 名称与字段）、RunGate（harness 驱动，JSON 含 fixture 信息、S4 快照 schema 14 字段）、GL diagnostics schema、Missing driver fails closed NOT_RUN。
- WARP/无头环境 gate 结论为 `NO_GO`（无独立 GPU，GI/Lighting 等 pass 无有效 GPU 采样）——属预期，但 harness 真实驱动已确认（matrix 单元格含非空 `scene_input_hash`，三 fixture 互异；无崩溃、无泄漏）。
- fail-closed 用例：driver==nullptr 时 `gate_status=="NOT_RUN"`、`matrix_results` 空、`global_failures` 非空。

### 4.5 harness 单元测试
```
bin\NoMoreDayTests.exe --test-case="*S6 GameplayRuntimeHarness*"
[doctest] test cases:  5 |  5 passed | 0 failed | 689 skipped
[doctest] assertions: 24 | 24 passed | 0 failed |
[doctest] Status: SUCCESS!
```
- 断言覆盖：cave 组件计数、combat 组件计数（PlayerTag==1/EnemyTag>=8/VisualEffect>0/AttackEffect>0/VisionComponent>0）、outdoor 光源>=200/地板/遮挡、输入哈希确定性（同 seed 相等、异 seed 不等、非零）、未知 recipe 拒绝。
- 实施中修正：outdoor 光源网格由 9×9 扩为 15×15（220 上限），修复测试期暴露的 `>=200` 断言。该修正发生于配方冻结之后，故版本由 **s6-v1.0 提升至 s6-v1.1**（GameplayRuntimeHarness.hpp 内 `FixtureVersion()`、本 evidence、plan 三处一致）；版本字符串不参与 FNV 输入哈希，最终哈希即 15×15 网格实测值。

### 4.6 全量 ctest unit|integration
```
ctest --test-dir build -C RelWithDebInfo -L "unit|integration" --output-on-failure
87% tests passed, 2 tests failed out of 15
```
失败归因（均为既有失败，非 S6 引入）：
1. `nmd.tests.integration` → `GIStabilityIntegrationTest.cpp:159/160` `giEmissiveTexture/giRadianceTexture != 0`；日志 `RadianceCascadesPass fallback ... reason=particle emissive pass failed` —— 无头/WARP 硬件读回问题，计划 S4/S5 已记录为既有失败。
2. `nmd.tests.skill.unit` → `HeavenlySwordClosureTests.cpp:97` `hasFreeze` 概率断言 —— 既有随机性断言失败。

其余 13/15 通过，含全部 S6 相关用例（gate 集成 + harness 单元）。

### 4.7 git diff --check
```
git diff --check
exit=0
```

### 4.8 git status（未 stage/commit）
```
 M src/engine/render/validation/GPUHardwareValidationGate.cpp
 M src/engine/render/validation/GPUHardwareValidationGate.hpp
 M tests/integration/GPUHardwareValidationGateTest.cpp
?? src/engine/render/validation/FixtureRenderDriver.hpp
?? tests/integration/GameplayRuntimeHarness.hpp
?? tests/unit/GameplayRuntimeHarnessTest.cpp
```
**未 stage、未 commit。**

## 5. JSON 报告摘录（fixture 信息对应）

```
"gate_status": "NO_GO",          // WARP/无头环境属预期；非 S6 缺陷
"matrix_results": [
  { "fixture": "cave_color_bleed",        "tier": "High", "gi_enabled": true,
    "fixture_version": "s6-v1.1",
    "scene_input_hash": "9635526039250172466",
    "scene_source": "tests/integration/GameplayRuntimeHarness.hpp (deterministic recipe, real game components, no level snapshot)" },
  { "fixture": "dynamic_combat_emissive", "tier": "High", "gi_enabled": true,
    "fixture_version": "s6-v1.1",
    "scene_input_hash": "1224310844868084887", ... },
  { "fixture": "outdoor_light_pressure",  "tier": "High", "gi_enabled": true,
    "fixture_version": "s6-v1.1",
    "scene_input_hash": "12786560374606737554", ... }
]
"resources": { "leak_candidate_count": 0, ... }
"stress_test": { "stress_1min_passed": true, "toggle_100_loops_passed": true,
                 "leak_candidate_count": 0, "resource_snapshots": [ ... 14 字段/项 ... ] }
```
`dynamic_combat_emissive` 三 mode 的 `roi_mean_brightness=0.0144`（非黑），证明真实场景被实际渲染；cave/outdoor 在 WARP 下 0.0（无 GI/Lighting 采样），实机 GO 判定待 RTX 4070 采集（DOD-2）。

## 6. 剩余风险

1. **无实机硬件 GO 采集**：WARP/无头环境仅能验证 harness 真实驱动、无崩溃/泄漏、JSON 含 fixture 信息；Cave/outdoor 的 ROI 非黑与 pass 预算需 RTX 4070 实机复跑（S7 与 DOD-2 依赖）。
2. **s6-v1.1 配方版本冻结**：v1.0 冻结后发生过一次 9×9→15×15 修正并提升为 v1.1；后续配方调整须再 bump 版本并重记录哈希。FNV-1a 哈希不具密码学强度，且 `std::cos/sin` 末位 ULP 因 libm 而异，仅承诺本平台复现性/一致性，不作安全用途。
3. **Stress/toggle 循环使用 driver 的 registry/context**：压力循环渲染同一 harness registry 多帧，未做场景演化（无 game systems 更新）；对资源泄漏/计时压力判定无影响，但对"动态场景压力"覆盖面有限——属已知范围取舍。
4. **module-gate 依赖**：gate 集成测试的 RunGate 用例现必须持有 harness；若未来 gate 主流程变化（如 S7 paired delta），需同步维护 harness/driver 接口。

## 7. 状态

- 未 stage、未 commit（待独立审查后按 §6 提交策略执行）。
- 与 S6 契约（T6.1-T6.5）符合度：全部符合（详见 §3）。
- 阻塞项：无（实机 GO 采集属 DOD-2，另列）。
