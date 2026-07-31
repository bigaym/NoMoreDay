# S8（M0-C R6）Artifact 归档 + runner 接线 Evidence

> 关联计划：`docs/plans/2026-07-30-p0-rendering-remediation-plan.md` §4 S8
> 关联轨道：`conductor/tracks/gpu_hardware_validation_gate_20260726/`（plan.md R6 / validation.md）
> 实施日期：2026-08-01
> 基线：HEAD `0fdce87`（本变更未提交，`git status --short` 见文末）

## 1. 目标与本步内容

S8（M0-C R6）把 gate 产物从临时 `bin/gpu_hardware_gate/` 迁移到版本化归档路径
`artifacts/gpu-gate/<revision>/`，把 runner 的死参数接线到 C++ 侧，并引入 waiver
元数据机制与 schema validator 的 CI 用法。**waiver 是归档元数据，永不改变 GO 判定**
（`gate_succeeded` 保持 `return_code==0 AND status=="GO"`）。

## 2. 变更文件清单

| 文件 | 变更类型 | 要点 |
|---|---|---|
| `.gitignore` | 修改 | 新增 `artifacts/`（生成产物不入库；`bin/` 已有同模式） |
| `scripts/gpu_hardware_validation_gate.py` | 修改 | 归档路径默认 `artifacts/gpu-gate/<revision>/`；`--samples/--toggle-loops/--stress-test-1min` 经 `NMD_GATE_*` 环境变量注入 C++；超时与 stress 时长联动；waiver CLI 参数写入归档元数据；artifact 新增 `gate_succeeded` 字段 |
| `tests/integration/GPUHardwareValidationGateTest.cpp` | 修改 | gate 测试读取 `NMD_GATE_SAMPLES/NMD_GATE_TOGGLE_LOOPS/NMD_GATE_STRESS`（默认 120/100/true）传入 `RunGate`；报告输出在 END 标记后 `std::flush` 防止后续用例日志交错损坏 JSON |
| `tests/python/GpuHardwareValidationGateRunnerTest.py` | 修改 | 新增 8 个用例：环境变量注入、超时联动、waiver 元数据写入、waiver 不改变 GO 判定负例、归档路径创建 |
| `docs/reports/gpu-s8-artifact-archive/evidence.md` | 新增 | 本文档 |

未改动：`src/engine/` 下除 `tests/integration/GPUHardwareValidationGateTest.cpp` 外的任何
C++ 文件（S7 并行域）；`docs/plans/2026-07-30-p0-rendering-remediation-plan.md`（主代理
统一更新）；`docs/designs/modular-split-exe-lib-dll-design.md`（受保护）。

## 3. CLI 参数与环境变量契约

### CLI 参数

| CLI 参数 | 默认 | 转发 | 说明 |
|---|---|---|---|
| `--revision` | `HEAD` | 归档路径 + artifact | 决定归档子目录 `artifacts/gpu-gate/<revision>/` |
| `--samples` | `120` | `NMD_GATE_SAMPLES` | 每 fixture 采样帧数 |
| `--toggle-loops` | `100` | `NMD_GATE_TOGGLE_LOOPS` | GI/tier/resize 切换循环次数 |
| `--stress-test-1min` / `--no-stress-test-1min` | on | `NMD_GATE_STRESS` | 60s 压力循环开关（BooleanOptionalAction） |
| `--output-dir` | `None` | 归档路径 | 显式覆盖默认归档目录 |
| `--waiver-authorizer/--waiver-reason/--waiver-scope/--waiver-expiry` | 空 | artifact `waiver` 字段 | waiver 元数据 |

### 环境变量契约（runner → C++）

- `NMD_GATE_SAMPLES`：int，缺省 `120`，经 `RunGate(sampleFramesPerFixture)` 生效。
- `NMD_GATE_TOGGLE_LOOPS`：int，缺省 `100`，经 `RunGate(toggleLoops)` 生效。
- `NMD_GATE_STRESS`：`"1"`/`"true"`/`"TRUE"` 为真，缺省 `true`，经 `RunGate(stressTest1Min)` 生效。
- `RunGate` 声明的 `stressTest1Min` 默认参数已对齐为 `true`（原为 `false`，与 runner/env 缺省不一致；
  测试调用方均显式传值，无行为影响；`build.bat check` 通过）。

实测：`NMD_GATE_STRESS=0` → `stress_test.duration_seconds==5.0`（对照 stress=1 为 60.0）；
`NMD_GATE_SAMPLES=125` → `valid_samples==125`。

### 超时联动

`gate_timeout_seconds = GATE_BASE_TIMEOUT_SECONDS(120) + (stress ? GATE_STRESS_ADDED_SECONDS(60) : 0)`。
即 stress 1min 时子进程预算 180s，禁 stress 时 120s，保证 60s 压力循环不会撞超时；
超时异常明确提示增大 samples 或关闭 stress。

## 4. Waiver 语义

- 四条 CLI 参数任意非空即生成 `waiver` 对象写入 artifact（authorizer/reason/scope/expiry）。
- **禁止** `NOT_RUN`/`waived`/`NO_GO` 被当作 GO：`gate_succeeded` 恒为
  `return_code == 0 AND status == "GO"`；waiver 不参与该判定。
- artifact 新增 `gate_succeeded`（原始判定）与 `meets_preflight`（含 schema 校验），
  均不受 waiver 影响；waiver 仅作为可追溯元数据记录。

## 5. 验证记录（真实输出）

### 5.1 Python 单测（本文件）
```
python -m unittest tests/python/GpuHardwareValidationGateRunnerTest.py
Ran 24 tests in 0.009s
OK
```
共 24 用例 = 13 既有 + 11 新增。新增 11 中含 8 个 S8 用例（`build_gate_env` 三变量、超时联动、
`build_waiver` None/字段、waiver 不改变 GO 判定负例、waiver 写入 artifact 后 verdict 不变、默认归档路径
`artifacts/gpu-gate/<revision>/` 创建）与 **3 个后补 validate-schema 用例**
（`test_validate_archived_artifact_*`：接受合法归档、拒绝缺失 report、拒绝损坏 report），后者在 S8 主
实施后为补足 §6 schema validator CI 契约而追加。

### 5.2 全量 Python 测试
```
python -m unittest discover -s tests/python -p "*Test.py"
Ran 63 tests in 7.019s
OK
```
63 用例 = 既有 60 + **3 个后补 validate-schema 用例**（同上，见 5.1）。

### 5.3 模块边界
```
python scripts/check_module_boundaries.py
[Module Boundary] Observed/ledger edges: 71/71; files: 20
[Module Boundary] PASS: ledger and observed reverse edges match.
```

### 5.4 C++ 集成测试（env 注入 + flush 修复后）
```
bin\NoMoreDayTests.exe --test-case="*GPU Hardware Validation Gate*"   # NMD_GATE_* 注入
[doctest] test cases:   4 |   4 passed | 0 failed | 697 skipped
[doctest] assertions: 133 | 133 passed | 0 failed |
[doctest] Status: SUCCESS!
```
payload 经 `json.loads` 验证为合法 JSON（gate_status=NO_GO，snapshots=1，256 条 GL 诊断）。

### 5.5 runner 端到端（真实执行）
```
python scripts/gpu_hardware_validation_gate.py --revision s8-e2e2-20260801-035706 \
  --samples 125 --toggle-loops 110 --no-stress-test-1min \
  --waiver-authorizer render-lead --waiver-reason "WARP-only CI runner" \
  --waiver-scope "nmd.tests.integration gate" --waiver-expiry 2026-09-01
exit=1   # NO_GO + waiver 正确不通过
```
- 归档文件：`artifacts/gpu-gate/s8-e2e2-20260801-035706/gpu_hardware_validation_artifact.json` 落盘。
- `gate_report_schema_errors == []`（schema validator 通过）。
- waiver 字段写入：`{'authorizer': 'render-lead', 'reason': 'WARP-only CI runner',
  'scope': 'nmd.tests.integration gate', 'expiry': '2026-09-01'}`。
- `gate_succeeded == False`、`meets_preflight == False`、exit 1 —— 失败路径不通过为 GO。
- 报告含 9 matrix_results、256 条 GL 诊断、快照序列（完整 GateReport，非仅 status）。

### 5.6 git diff --check
```
git diff --check
exit=0   # 仅 CRLF 警告（Windows 既有）
```

## 6. schema validator 的 CI 接入方式

- **校验命令**：`python scripts/gpu_hardware_validation_gate.py --validate-schema artifacts/gpu-gate/<revision>/gpu_hardware_validation_artifact.json`
  （exit 0/1）；对既有归档执行与 runner 相同的 `validate_gate_report_schema` 校验。
- **归档时自动校验**：runner 每次归档都会运行 schema validator 并把错误写入
  artifact 的 `gate_report_schema_errors`（空列表 = 通过），CI 无需单独解析 JSON。
- **接入位置**：CI 在实机 runner 归档后检查该归档的 `gate_report_schema_errors == []`
  且 `gate_succeeded == true`；无 GPU 环境显式 `--waiver-*` 记录，绝不改写判定。
- 单测回归：`python -m unittest tests/python/GpuHardwareValidationGateRunnerTest.py`。
- 见轨道 `plan.md` R6 与 `validation.md` §15。

## 7. 归档产物清单

`artifacts/gpu-gate/<revision>/gpu_hardware_validation_artifact.json`：
revision / timestamp / runner / gate_status / gate_succeeded / meets_preflight /
waiver / return_code / gate_report（完整 C++ GateReport：capabilities、matrix_results、
resources、stress_test + resource_snapshots、gl_diagnostics） / gate_report_schema_errors /
stdout_summary / stderr_summary。

保留策略：最近 20 次或 90 天（见轨道文档 R6）。

## 8. 剩余风险

1. 归档目录以 `--revision` 直接作路径段；非安全字符（如含 `/` 的分支名）需自行保证合法。
2. WARP/无头环境 gate 仍为 `NO_GO`（无独立 GPU 采样），DOD-2 需 RTX 4070 实机复跑。
3. `std::flush` 修复依赖 C++ 测试用例在打印报告后立即冲刷；后续用例若在 report 打印
   与 flush 之间插入输出，仍可能交错（当前顺序已验证无此路径）。

## 9. 状态

- 未 stage、未 commit。
- 与 S8 契约符合度：归档路径 ✅、runner 接线 ✅、waiver ✅、schema validator CI 说明 ✅、
  runner 测试扩展 ✅、端到端归档 ✅。
