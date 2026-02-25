# Combat DoT Closure — Specification

> Track ID: `combat_dot_closure_20260225`  
> Series: CS-M1-02  
> Priority: P0  
> Milestone: M1  
> Scope: DoT tick 闭环扣血 + 强制 Tag::DamageOverTime 标签语义

---

## 1. Overview

当前 DoT tick 路径（`EffectSystem.cpp` L95-117）仅调用 `DamagePipeline::Calculate` 和 `EmitDamagePopup`，**未调用 `CombatSystem::ApplyDamage`**（review §2.2）。同时 DoT tick 调用 Pipeline 时使用 `Tag::None`（review §2.3），导致 DoT 可能被当作 Hit 处理。

影响：
- 玩家看到伤害数字但目标生命值不变 — **功能正确性错误**。
- DoT 有机会触发不该触发的击中收益（如 life on hit）。

---

## 2. Evidence (Code)

### 2.1 当前 DoT tick 路径 — `EffectSystem.cpp` L102-117

```cpp
if (effect.type == BuffType::DamageOverTime && effect.remaining > 0.0f) {
    effect.tick_timer += dt;
    if (effect.tick_timer >= effect.tick_interval) {
        effect.tick_timer = 0.0f;
        DamagePool base_pool;
        base_pool.Add(Tag::Poison, effect.tick_damage);
        auto result = DamagePipeline::Calculate(registry, effect.source, entity, 0, base_pool, Tag::None);
        //                                                                              ^^^^^^^^^ BUG: should be Tag::DamageOverTime
        EmitDamagePopup(registry, {pos.x, pos.y - 20.0f}, result.total_damage, result.is_crit, Tag::Poison);
        // MISSING: CombatSystem::ApplyDamage(registry, entity, result.total_damage, effect.source);
    }
}
```

### 2.2 Pipeline 中 Tag::DamageOverTime 守卫点

- `DamagePipeline.cpp` L415: `!HasTag(inst.tags, Tag::DamageOverTime)` — 控制是否走 Hit 分支
- `DamagePipeline.cpp` L584: 类似的 Hit/DoT 分流点
- `DamagePipeline.cpp` L900: 事件派发中用 `Tag::DamageOverTime` 控制 `OnSkillHit` 语义

---

## 3. Constraints

- DoT tick 频率已由 `effect.tick_interval` 控制，不改变节奏。
- DoT 元素类型（Poison/Fire/Cold 等）应从 `effect` 或 DamagePool 中继承，不硬编码。
- 修复后 DoT 不得触发任何 `OnSkillHit` 语义的收益（life on hit, mana on hit 等）。

---

## 4. Behavioral Contract

### 4.1 DoT Tick Contract

- DoT tick **必须** 调用 `DamagePipeline::Calculate` **且** 随后调用 `CombatSystem::ApplyDamage`。
- DoT `Calculate` 调用 **必须** 传入 `Tag::DamageOverTime`（不得为 `Tag::None`）。
- DoT 伤害飘字数值 **必须** 与实际 HP 扣除值一致。

### 4.2 Hit/DoT 语义互斥 Contract

- 含 `Tag::DamageOverTime` 的伤害 **禁止** 进入 `OnSkillHit` 事件分支。
- 含 `Tag::DamageOverTime` 的伤害 **禁止** 触发 life_on_hit、mana_on_hit 等 Hit-only 收益。
- DoT 可触发自身专属收益（如 `OnDoTTick` 事件，若未来实现）。

---

## 5. Implementation Targets

### Files to Modify

- `src/game/systems/combat/EffectSystem.cpp` — L111: 修改 tag 参数 + 添加 ApplyDamage 调用
- `src/game/systems/combat/DamagePipeline.cpp` — 验证 Tag::DamageOverTime 守卫逻辑正确性

### Tests to Create/Update

- `tests/unit/DoTDamageClosureTests.cpp` — 新建
  - DoT tick 扣血验证
  - DoT 不触发 OnSkillHit 验证
  - 多 DoT 类型（Poison/Fire/Cold）覆盖

---

## 6. Acceptance Criteria

- [ ] DoT tick 后目标 `HealthComponent.current` 减少量 == `result.total_damage`。
- [ ] DoT tick 期间不触发 `OnSkillHit` 事件（可通过事件计数器验证）。
- [ ] 燃烧/中毒/流血至少 3 种 DoT 类型各有 1 个测试通过。
- [ ] `build.bat` PASS。
- [ ] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` PASS。
- [ ] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS。

---

## 7. Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| DoT 扣血后可能导致意外双重死亡事件 | 中 | ApplyDamage 内已有死亡判定，验证无重入 |
| DoT 类型未来扩展（bleed、ignite）可能需要不同 tag 组合 | 低 | 使用 `Tag::DamageOverTime \| specific_element` 组合，保持扩展性 |
| 修复后 DoT 流派实际伤害大幅变化（之前为零） | 高 | 这是预期行为（修正 bug），但需在 balance review 中关注 |
