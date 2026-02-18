# V3 Validation and Release Gate Validation

## 1. Gate Execution Validation

1. Gate runner executes end-to-end from clean workspace.
2. Each gate emits expected artifact outputs.
3. Failures include actionable reason and failing category.

## 2. Performance Gate Validation

1. Profiles:
   - `baseline_270`
   - `combat_180`
   - `stress_144`
2. Performance comparator correctly detects:
   - pass budget breach,
   - frame regression > 10%.

## 3. Stability Gate Validation

1. 30-minute stress test reports no sustained VRAM growth trend.
2. Resize and context restore scenarios complete without black screen.

## 4. Contract Gate Validation

1. ABI version/layout mismatch is detected and blocks merge.
2. Binding collision is detected and blocks merge.
3. RenderGraph contract violation is detected and blocks merge.
4. Schema mismatch is detected and blocks merge.

## 5. Rollout Validation

1. `render.v3.enabled=true` path works when gates pass.
2. Failure path triggers V2 fallback and blocks merge.
3. Rollout log includes gate summaries and fallback reason.

## 6. Evidence Checklist

- [x] Functional matrix report attached.
- [x] Performance report attached.
- [x] Stability report attached.
- [x] Contract checks attached.
- [x] Fallback drill report attached.

## 7. Latest Execution Snapshot (2026-02-18)

1. `build.bat` -> `PASS`.
2. `build.bat analyze` -> `PASS`（仅剩既有静态分析告警，无新增 blocker）。
3. `build.bat perf` -> `PASS`.
4. `python scripts/v3_release_gate.py --build-dir build --config Release --allow-missing-screenshots --update-baseline --final-verification` -> `PASS with warnings`:
   - Summary: `checks=43 pass=41 warning=2 fail=0`.
   - `waiverHits`:
     - `WVR-20260218-F4.6-001` -> `F4.6 perf_clustered_uplift`（`clustered_128_improvement_pct=0.502 < 5.0`，降级为 warning）。
   - Other warning:
     - `F6.2 screenshot_compare`（截图基线/候选文件尚未填充，当前启用 `--allow-missing-screenshots`）。
5. `build.bat gate` integration path validated:
   - Gate runner is invoked by build script.
   - Current integrated result is non-blocking with warning summary (`checks=43 pass=40 warning=3 fail=0`).
6. `fallbackTriggered=false`（本次无 critical fail，无 V2 fallback 触发）。
7. 截图门禁说明：当前阶段无法完成严格截图回归（上游系统未全部接入），已登记为 V4 实施前依赖检查项，避免误判为“视觉全绿”。

Artifacts:
- `bin/release_gate/v3_gate_report.json`
- `bin/release_gate/v3_gate_report.csv`
- `bin/release_gate/v3_gate_baseline_snapshot.json`
- `conductor/validation/v3_gate_waivers.json`
