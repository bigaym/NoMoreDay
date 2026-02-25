# Combat Event Consistency — Specification

> Track ID: `combat_event_consistency_20260225`  
> Series: CS-M1-04  
> Priority: P0  
> Milestone: M1  
> Scope: 统一 CalculateBatch 事件载荷使用 final_damage

---

## 1. Overview

`DamagePipeline::CalculateBatch` 中先计算 `final_damage`（经反制/拦截后），再调用 `ApplyDamage`。但部分事件仍使用 `res.damage` 构造载荷（review §2.6）。

### 证据 — `DamagePipeline.cpp`

```
L856:  float final_damage = res.damage;            // 初始赋值
L896:  CombatSystem::ApplyDamage(..., final_damage, ...);  // 使用 final_damage ✓
L931:  ...res.damage, res.is_crit));               // 事件用 res.damage ✗
L937:  ...res.damage, res.is_crit, source_entity)); // 事件用 res.damage ✗
L943:  ...res.damage, res.is_crit));               // 事件用 res.damage ✗
L950:  ...res.damage, res.is_crit, source_entity)); // 事件用 res.damage ✗
L955:  ...res.damage, res.is_crit));               // 事件用 res.damage ✗
L961:  ...combined_tags, res.damage));              // 事件用 res.damage ✗
```

影响：战斗日志、回放、统计和实际血量变化不一致。

---

## 2. Behavioral Contract

- 所有事件载荷中的 damage 字段必须使用 `final_damage`（ApplyDamage 之后的实际值）。
- 事件一致率目标：>= 99.9%（允许浮点精度误差）。
- 事件载荷统一包含：`final_applied_damage`, `damage_tags`, `is_crit`。

---

## 3. Implementation Targets

### Files to Modify

- `src/game/systems/combat/DamagePipeline.cpp` — L931-L961: 将 `res.damage` 替换为 `final_damage`

### Tests to Create

- `tests/unit/EventConsistencyTests.cpp`
  - Test: event damage value == ApplyDamage actual value
  - Test: batch with mitigation reduction → event reflects post-mitigation value

---

## 4. Acceptance Criteria

- [ ] 事件 `reported_damage` 与 `ApplyDamage` 实际扣除值一致率 >= 99.9%。
- [ ] 所有 6 处事件构造使用 `final_damage`。
- [ ] `build.bat` PASS。
- [ ] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` PASS。
- [ ] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS。

---

## 5. Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| 下游系统依赖 res.damage 的旧值 | 低 | 搜索所有事件消费方确认无假设 |
| final_damage 在某些路径被二次修改 | 低 | 审计 final_damage 赋值点确保唯一 |
