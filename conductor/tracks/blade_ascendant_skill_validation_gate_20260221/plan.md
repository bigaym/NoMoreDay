# Blade Ascendant Skill Validation Gate - 执行计划

> **Track ID**: `blade_ascendant_skill_validation_gate_20260221`

---

## 阶段总览

| 阶段 | 名称 | 核心产出 | 状态 |
|---|---|---|---|
| Phase 1 | 验证资产准备 | 测试清单、基线、脚本参数 | [ ] |
| Phase 2 | 功能与合同门禁 | unit/integration + 合同检查 | [ ] |
| Phase 3 | 性能与稳定性 | perf + 压力回归 | [ ] |
| Phase 4 | 发布结论 | validation 报告与 release posture | [ ] |

---

## Phase 1: 验证资产准备

- [ ] Task 1.1: 汇总 9 技能关键行为断言
- [ ] Task 1.2: 汇总合同检查规则与脚本入口
- [ ] Task 1.3: 明确性能基线来源与阈值
- [ ] Task 1.4: 准备 validation.md 模板

## Phase 2: 功能与合同门禁

- [ ] Task 2.1: 执行 unit 测试集
- [ ] Task 2.2: 执行 integration 测试集
- [ ] Task 2.3: 执行合同检查并记录差异
- [ ] Task 2.4: 修复 blocker 并复跑

## Phase 3: 性能与稳定性

- [ ] Task 3.1: 执行性能测试集
- [ ] Task 3.2: 高频施法压力测试
- [ ] Task 3.3: tier 切换与回退路径测试
- [ ] Task 3.4: 记录非阻塞问题并挂接 bug_registry

## Phase 4: 发布结论

- [ ] Task 4.1: 输出 validation 证据
- [ ] Task 4.2: 更新 tracks.md / roadmap 状态
- [ ] Task 4.3: 形成 GO/CONDITIONAL-GO/NO-GO 结论
- [ ] Task 4.4: 运行 `build.bat`

