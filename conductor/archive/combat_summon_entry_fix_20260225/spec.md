# Combat Summon Entry Fix — Specification

> Track ID: `combat_summon_entry_fix_20260225`  
> Series: CS-M1-06  
> Priority: P0  
> Milestone: M1  
> Scope: 基于当前代码完成召唤路径收敛，消除残留硬编码伤害语义并闭环验证

---

## 1. Overview

该 track 初版基于早期代码，当前代码已演进，需按现状重基线：

1. 近战环绕路径已从 `SummonSystem` 迁移到 `SummonAISystem -> SummonCombatBridge`。
2. 近战环绕与灵剑 execute 均已先经过 `DamagePipeline::Calculate`，不再是“直接写血”旁路。
3. 仍存在残留硬编码伤害语义：
   - `SummonAISystem` 调用 `ApplyMeleeOrbitContact(..., 30.0f, 25.0f)`。
   - `SwordArray` execute 使用 `hp->max * 0.1f` 内联常量构造 `DamagePool`。

---

## 2. Current Evidence (Code)

### 2.1 近战环绕已接入 Pipeline

```cpp
SummonCombatBridge::ApplyMeleeOrbitContact(..., 30.0f, 25.0f);
...
const auto result = DamagePipeline::Calculate(registry, request);
CombatSystem::ApplyDamage(registry, target, result.total_damage, summon, result.is_crit);
```

### 2.2 灵剑 execute 已接入 Pipeline，但保留 `%HP` 常量

```cpp
executePool.Add(Tag::Physical, hp->max * 0.1f);
auto executeResult = DamagePipeline::Calculate(registry, executeReq);
CombatSystem::ApplyDamage(registry, target_ent, executeResult.total_damage, ...);
```

---

## 3. Behavioral Contract

### 3.1 Summon Melee Orbit (Talent 352)

- 伤害必须经由 `DamagePipeline::Calculate` 计算（现状已满足）。
- 基础伤害来源：由 owner/summon 配置派生，不保留 `25.0f` 常量。
- `source_skill` 应标记为 Talent 352 的技能 ID。
- 攻击频率保持 5 hits/sec（`ai.attack_timer = 0.2f` 不变）。

### 3.2 SwordArray

- 伤害必须经由 `DamagePipeline::Calculate` 计算（现状已满足）。
- execute 可保留 `%HP` 设计，但必须合同化（配置字段或明确标签），禁止内联 `0.1f`。
- `%HP` 语义需可审计（配置来源可追踪，测试可覆盖）。

### 3.3 不变量

- 收敛后视觉表现不变（粒子效果/弹字保持一致）。
- 收敛后总 DPS 在合理范围内（允许偏移，但不应超过 ±20%）。

---

## 4. Implementation Targets

### Files to Modify

- `src/game/systems/skill/SummonAISystem.cpp` — 去除近战环绕 `25.0f` 常量输入。
- `src/game/systems/skill/SummonCombatBridge.cpp` — 接收/计算配置化近战基值并保持归因/预算逻辑。
- `src/game/systems/skill/behaviors/SwordArray.cpp` — execute `%HP` 数值来源配置化或标签化，去除 `0.1f` 内联。

### Tests to Create

- `tests/unit/SummonDamageEntryTests.cpp`（新增或并入现有 `SummonStrategyTests.cpp`）
  - Test: melee orbit damage no longer depends on hardcoded `25.0f`。
  - Test: sword execute ratio source is config/contract (not inline constant)。
  - Test: summon paths still route through Pipeline before settlement。

---

## 5. Acceptance Criteria

- [x] 召唤近战环绕与灵剑 execute 在落地前均通过 `DamagePipeline::Calculate`。
- [x] `SummonAISystem.cpp` / `SummonCombatBridge.cpp` 不再存在近战环绕 `25.0f` 内联伤害常量。
- [x] `SwordArray.cpp` 不再存在 execute `hp->max * 0.1f` 内联常量，改为合同化来源。
- [x] 召唤伤害保持 owner/summon/source_skill 三元归因。
- [x] 对应单测/集测覆盖残留语义收敛点并 PASS。
- [x] `build.bat` PASS。
- [x] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` PASS。
- [x] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS。

---

## 6. Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| 近战常量移除后 DPS 漂移 | 中 | 以 owner/summon 配置驱动并记录前后 DPS 对照 |
| `%HP` execute 语义不清 | 中 | 固化为合同字段或标签语义，并加测试锁定 |
| 与已完成 CS-M2-03 产生交叉改动 | 低 | 限定为“语义收敛补丁”，不重构子系统边界 |
