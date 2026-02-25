# Combat Effectiveness Integration — Specification

> Track ID: `combat_effectiveness_integration_20260225`  
> Series: CS-M1-03  
> Priority: P0  
> Milestone: M1  
> Scope: 接入 added_damage_effectiveness 与 trigger.effectiveness 到统一公式

---

## 1. Overview

两个已解析但未消费的核心系数（review §2.4-2.5）：

1. **`added_damage_effectiveness`** — 已存在于 `SkillData`（`SkillRegistry.hpp` L20），已解析（`SkillRegistry.cpp` L470-471），但 `DamagePipeline.cpp` 未引用。
2. **`trigger.effectiveness`** — 已存在于 `NodeContractData`（`SkillContract.hpp`），已解析（`SkillRegistry.cpp` L206-207），但 `SkillSystem.cpp` 触发派发不读取。

缺失影响：高频技能与低频技能无法用 effectiveness 系数精准平衡；触发流派缺少线性调参手段。

---

## 2. Evidence (Code)

### 2.1 added_damage_effectiveness — 已声明未消费

```
SkillRegistry.hpp:20   float added_damage_effectiveness;
SkillRegistry.cpp:470  data.added_damage_effectiveness =
SkillRegistry.cpp:471      item.value("added_damage_effectiveness", 1.0f);
DamagePipeline.cpp     — NO REFERENCE to added_damage_effectiveness
```

### 2.2 trigger.effectiveness — 已声明未消费

```
SkillRegistry.cpp:206  node.trigger.effectiveness =
SkillRegistry.cpp:207      trigger.value("effectiveness", node.trigger.effectiveness);
SkillSystem.cpp        — trigger dispatch reads trigger_skill_id, ICD, mana cost, but NOT effectiveness
```

---

## 3. Target Formula

```
FinalDamage = ((Base + Added * AddedEff) * (1 + Σ Increased) * Π More * TriggerEff) * Mitigation
```

Where:
- `AddedEff` = `SkillData.added_damage_effectiveness` (default 1.0, range [0.0, 2.0+])
- `TriggerEff` = `NodeContractData.trigger.effectiveness` (default 1.0, range [0.0, 1.0])
- `TriggerEff` applies **only** when damage originates from a trigger dispatch

---

## 4. Behavioral Contract

### 4.1 AddedEff Contract

- `DamagePipeline::Calculate` must accept `added_damage_effectiveness` via `DamageRequest` or skill context.
- Added flat damage (from gear/buffs/aura) is scaled by `AddedEff` **before** Increased/More multipliers.
- If `AddedEff` is absent or zero, added damage contributes nothing (not the base damage).

### 4.2 TriggerEff Contract

- When `SkillSystem` dispatches a trigger, `trigger.effectiveness` must be passed into the execution context.
- `DamagePipeline::Calculate` applies `TriggerEff` as a final multiplicative factor **before** Mitigation.
- Direct casts (non-triggered) have implicit `TriggerEff = 1.0`.

---

## 5. Implementation Targets

### Files to Modify

- `src/game/systems/combat/DamagePipeline.hpp` — Extend `DamageRequest` with `added_effectiveness` and `trigger_effectiveness` fields
- `src/game/systems/combat/DamagePipeline.cpp` — Integrate both coefficients into formula
- `src/game/systems/skill/SkillSystem.cpp` — Pass `trigger.effectiveness` when dispatching triggered skills

### Tests to Create

- `tests/unit/EffectivenessIntegrationTests.cpp`
  - AddedEff=0.5 → added damage halved
  - TriggerEff=0.5 → triggered damage halved
  - Direct cast → TriggerEff=1.0 (no change)
  - Combined: both coefficients applied correctly

---

## 6. Acceptance Criteria

- [ ] 高频技能（AddedEff=0.4）与低频技能（AddedEff=1.2）的 added damage 比例符合系数。
- [ ] `trigger.effectiveness=0.5` 时触发伤害确认为直接施法的 50%。
- [ ] 直接施法不受 TriggerEff 影响（隐式 1.0）。
- [ ] 单元测试覆盖所有公式路径。
- [ ] `build.bat` PASS。
- [ ] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` PASS。
- [ ] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS。

---

## 7. Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| AddedEff 全面生效后可能改变平衡 | 高 | 逐步启用，先验证公式正确性，平衡调参在独立 pass 中进行 |
| TriggerEff 传递链路复杂 | 中 | 从 SkillSystem 触发派发点单一传入，不做多层传递 |
| 现有 skills.json 中部分技能缺少 effectiveness 字段 | 低 | 默认值 1.0 保证向后兼容 |
