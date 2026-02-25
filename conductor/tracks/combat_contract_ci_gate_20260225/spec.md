# Combat Contract CI Gate — Specification

> Track ID: `combat_contract_ci_gate_20260225`  
> Series: CS-M1-05  
> Priority: P0  
> Milestone: M1  
> Scope: Fix skill contract drift, harden generation script, integrate into CI as blocking gate

---

## 1. Overview

2026-02-25 审查时执行 `python scripts/gen_skill_contracts.py --check` 结果为 `[FAIL] skill_contract blocks are out of date.`（review §2.9）。

这意味着技能树节点、合同上限、触发约束可能已偏离真实实现，后续内容扩展的回归风险持续放大。

本 Track 的目标：
1. 修复当前合同漂移，使 `--check` 恢复 PASS。
2. 强化脚本的健壮性和确定性。
3. 将 `--check` 纳入 CI pipeline 作为阻断门禁。

---

## 2. Constraints

- 脚本语言：Python 3.x，使用 `ai` conda 环境或 base。
- 输入源：`assets/data/skills.json` + `assets/data/skill_contracts_compact.json`。
- 输出目标：`src/game/systems/skill/SkillRegistry.cpp` 中的 `skill_contract` 代码块。
- 不改变合同的语义内容（只同步漂移）。
- CI 环境：当前为本地 `build.bat` 构建链，未来可扩展到 CI 服务。

---

## 3. Root Cause Analysis

需在实施时确认以下可能的漂移来源：

1. `skills.json` 有新增/修改节点但未重新生成合同。
2. `skill_contracts_compact.json` 与 `skills.json` 间存在不一致。
3. `SkillRegistry.cpp` 中的合同块被手动编辑。
4. 脚本本身的生成逻辑有 corner case（排序、格式化差异）。

---

## 4. Behavioral Contract

### 4.1 Script Contract

- `python scripts/gen_skill_contracts.py` — 重新生成合同块并写入 `SkillRegistry.cpp`。
- `python scripts/gen_skill_contracts.py --check` — 比对生成结果与当前文件，不一致时返回非零退出码。
- 生成必须是幂等的：连续运行两次结果相同。
- 生成必须是确定的：同输入同输出，不依赖时间戳或随机因素。

### 4.2 CI Gate Contract

- 任何改动 `skills.json`、`skill_contracts_compact.json` 或 `SkillRegistry.cpp` 的提交，必须通过 `--check`。
- CI 中 `--check` 失败应阻断合入（或在 nightly 中标红预警）。
- 连续 7 天 nightly 通过作为 M1 验收指标。

---

## 5. Implementation Targets

### Files to Modify

- `scripts/gen_skill_contracts.py` — 修复漂移 + 健壮性增强
- `assets/data/skills.json` — 可能需要微调（如果发现数据侧不一致）
- `assets/data/skill_contracts_compact.json` — 可能需要重新生成
- `src/game/systems/skill/SkillRegistry.cpp` — 重新生成合同块

### Files to Create

- CI 集成配置（如 pre-commit hook 或 build.bat 扩展）

---

## 6. Acceptance Criteria

- [ ] `python scripts/gen_skill_contracts.py --check` 返回 `[OK] skill_contract blocks are up to date.`
- [ ] 连续重新生成两次，`--check` 均通过（幂等性验证）。
- [ ] `build.bat` 编译通过。
- [ ] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` PASS。
- [ ] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS。
- [ ] CI 门禁配置就绪（手动触发验证一次即可）。
- [ ] 连续 7 天 nightly `--check` 通过（M1 验收延伸指标）。

---

## 7. Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| 漂移根因在 skills.json 数据，修复可能影响游戏内技能 | 中 | 只做同步，不改语义；diff review 确认无功能变更 |
| 脚本生成格式依赖 Python 版本 | 低 | 固定 Python 3.10+ 输出格式，添加版本检查 |
| CI 环境与本地环境差异导致误报 | 低 | 使用 `--check` 的纯文本比对模式，不依赖编译 |
