# Combat Release Gate Suite — Specification

> Track ID: `combat_release_gate_suite_20260225`  
> Series: CS-M3-04 | Priority: P2 | Milestone: M3

---

## 1. Overview

建立战斗系统发布门禁套件，为版本发布提供标准化质量准入。

## 2. Gate Dimensions

| 维度 | 阈值 | 来源 |
|---|---|---|
| 战斗帧时间 p95 | < 目标硬件阈值 | Telemetry |
| 战斗帧时间 p99 | < 目标硬件阈值 | Telemetry |
| 新词缀/节点回归覆盖率 | >= 80% | 自动化测试 |
| 事件一致率 | >= 99.9% | Pipeline 验证 |
| 合同校验通过率 | 100% | CI |
| 重大回归率（vs M1 基线） | 下降 >= 50% | Bug registry |

## 3. Suite Components

- **CI Gate**: 每次提交运行合同校验 + 单元测试。
- **Nightly Gate**: 每晚运行全回归 + 性能基准。
- **Release Gate**: 发布前运行全维度门禁 + 手动审计项。

## 4. Acceptance Criteria

- [ ] CI/Nightly/Release 三级门禁全部就绪。
- [ ] p95/p99 帧时间达标。
- [ ] 回归覆盖率 >= 80%。
- [ ] 重大回归率下降 >= 50%。
- [ ] `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS。
