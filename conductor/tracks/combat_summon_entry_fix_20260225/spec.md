# Combat Summon Entry Fix — Specification

> Track ID: `combat_summon_entry_fix_20260225`  
> Series: CS-M1-06  
> Priority: P0  
> Milestone: M1  
> Scope: 消除召唤路径直写 ApplyDamage，路由至 DamagePipeline

---

## 1. Overview

召唤系统存在绕过统一结算的直写路径（review §2.7）：

1. **`SummonSystem.cpp` L83** — 近战环绕（Talent 352）直接 `CombatSystem::ApplyDamage(registry, target, 25.0f, summon.owner)`，硬编码 25.0f 伤害，不经任何公式计算。
2. **`SwordArray.cpp` L215** — 灵剑阵列直接 `CombatSystem::ApplyDamage(registry, target_ent, hp->max * 0.1f, ...)`，按目标最大 HP 10% 硬编码。

这些路径不受词缀、增伤、暴击、防御任何系统影响，属于结算旁路。

---

## 2. Evidence (Code)

### 2.1 SummonSystem.cpp L78-84 — 近战环绕

```cpp
if (distSq <= hitRadius * hitRadius) {
    // Apply Damage — Use owner as source
    CombatSystem::ApplyDamage(registry, target, 25.0f,
                              summon.owner);
    // VFX...
}
```

### 2.2 SwordArray.cpp L215 — 灵剑阵列

```cpp
CombatSystem::ApplyDamage(registry, target_ent, hp->max * 0.1f, ...);
```

---

## 3. Behavioral Contract

### 3.1 Summon Melee Orbit (Talent 352)

- 伤害必须经由 `DamagePipeline::Calculate` 计算。
- 基础伤害来源：从 owner 的 CombatStats 派生（不再硬编码 25.0f）。
- `source_skill` 应标记为 Talent 352 的技能 ID。
- 攻击频率保持 5 hits/sec（`ai.attack_timer = 0.2f` 不变）。

### 3.2 SwordArray

- 伤害必须经由 `DamagePipeline::Calculate` 计算。
- 基础伤害从 owner 属性派生，不再按目标最大 HP 百分比硬编码。
- 若设计意图确实是 % HP 伤害，应作为 Pipeline 特殊标签处理。

### 3.3 不变量

- 迁移后视觉表现不变（粒子效果/弹字保持一致）。
- 迁移后总 DPS 在合理范围内（允许因公式差异有偏移，但不应超过 ±20%）。

---

## 4. Implementation Targets

### Files to Modify

- `src/game/systems/skill/SummonSystem.cpp` — L83: 替换为 Pipeline 调用
- `src/game/systems/skill/behaviors/SwordArray.cpp` — L215: 替换为 Pipeline 调用

### Tests to Create

- `tests/unit/SummonDamageEntryTests.cpp`
  - Test: melee orbit damage goes through Pipeline
  - Test: sword array damage goes through Pipeline
  - Test: no direct ApplyDamage calls remain in summon paths

---

## 5. Acceptance Criteria

- [ ] `SummonSystem.cpp` 无直写 `ApplyDamage(..., 25.0f)` 路径。
- [ ] `SwordArray.cpp` 无直写 `ApplyDamage(..., hp->max * 0.1f)` 路径。
- [ ] 召唤伤害经由 DamagePipeline 结算，受 owner 属性影响。
- [ ] `Select-String -Path "src\game\systems\skill\SummonSystem.cpp" -Pattern "ApplyDamage"` 返回 0 结果。
- [ ] `build.bat` PASS。
- [ ] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` PASS。
- [ ] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS。

---

## 6. Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| 硬编码 25.0f 移除后近战环绕 DPS 变化 | 中 | 记录迁移前后 DPS 对照，调参在后续 balance pass |
| SwordArray % HP 伤害是设计意图 | 中 | 与设计确认；若为意图，Pipeline 新增 `%HP` 标签支持 |
| 灵剑路径已通过 ShadowCast 走了 SkillSystem | 低 | 本 Track 只处理 melee orbit 和 SwordArray，不碰 ShadowCast |
