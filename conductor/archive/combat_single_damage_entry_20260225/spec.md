# Combat Single Damage Entry — Specification

> Track ID: `combat_single_damage_entry_20260225`  
> Series: CS-M1-01  
> Priority: P0-Critical  
> Milestone: M1  
> Scope: Unify all damage calculation to `DamagePipeline`, deprecate legacy `CombatSystem::CalculateDamage`

---

## 1. Overview

当前系统存在伤害结算双轨：新链 `DamagePipeline::Calculate` / `CalculateBatch` 与旧链 `CombatSystem::CalculateDamage` 并存（review §2.1）。这导致：

- 同一词缀/标签在不同技能路径表现不一致。
- 平衡调参出现"同改一处、结果多口径"的不可控状态。
- 后续 Track（DoT/Effectiveness/Event/Summon）无法在统一基础上展开。

本 Track 的目标是：**`DamagePipeline` 成为唯一数值计算入口**，`CombatSystem` 退化为生命结算与战斗动作层。

---

## 2. Constraints

- ECS: EnTT; 组件无所有权/生命周期逻辑。
- Build target: Windows + MSVC (RelWithDebInfo / Release)。
- `CombatSystem::ApplyDamage` 保留为唯一 HP 结算入口（不迁移）。
- 保持现有帧序：`Input → Player Movement → AI → Combat → Spatial Grid Rebuild → Physics`。
- 迁移期间必须提供兼容开关（feature flag），支持紧急回退。

---

## 3. Current Call Sites (Evidence)

### 3.1 Legacy Chain — `CombatSystem::CalculateDamage` callers

| # | File | Line | Context |
|---|---|---|---|
| 1 | `CombatSystem.cpp` | L260 | 近战循环 — 逐伤害类型累加 |
| 2 | `CombatSystem.cpp` | L272 | 近战循环 — 附加伤害类型累加 |
| 3 | `CombatSystem.cpp` | L428 | AI 敌人攻击 |

### 3.2 Non-Pipeline `ApplyDamage` direct callers（绕过 Pipeline）

| # | File | Line | Context |
|---|---|---|---|
| 1 | `CombatSystem.cpp` | L318 | 近战最终结算（使用旧链计算值） |
| 2 | `CombatSystem.cpp` | L330 | 荆棘反噬 |
| 3 | `CombatSystem.cpp` | L457 | AI 敌人攻击最终结算 |
| 4 | `SummonSystem.cpp` | L83 | 召唤近战环绕 — 硬编码 `25.0f` |
| 5 | `SkillSystem.cpp` | L878 | 自伤/消耗型技能 |
| 6 | `ProjectileSystem.cpp` | L601 | 投射物自伤/爆炸 |
| 7 | `ProjectileSystem.cpp` | L639 | 投射物命中伤害 |
| 8 | `SwordArray.cpp` | L215 | 灵剑阵列 — 硬编码 `max * 0.1f` |

### 3.3 Pipeline Compliant（已合规）

| # | File | Line | Context |
|---|---|---|---|
| 1 | `DamagePipeline.cpp` | L896 | Pipeline 正式结算链 → `ApplyDamage` |
| 2 | `HazardSystem.cpp` | L423 | 环境危害 — 已通过 Pipeline 计算 |

---

## 4. Target Architecture

```
┌──────────────────────┐
│   Skill / AI / Env   │  (调用方)
└──────────┬───────────┘
           │ DamageRequest
           ▼
┌──────────────────────┐
│   DamagePipeline     │  唯一数值计算入口
│  Calculate / Batch   │  (base, tags, convert, incr, more, crit, mitigation)
└──────────┬───────────┘
           │ FinalDamage
           ▼
┌──────────────────────┐
│ CombatSystem         │  唯一 HP 结算入口
│ ::ApplyDamage(...)   │  (扣血, 死亡, 事件派发)
└──────────────────────┘
```

`CombatSystem::CalculateDamage` 标记 `[[deprecated("Use DamagePipeline::Calculate")]]`，禁止新调用。

---

## 5. Behavioral Contract

### 5.1 Unified Entry Contract

- 任何导致 HP 变化的伤害路径，必须经由 `DamagePipeline::Calculate` 或 `DamagePipeline::CalculateBatch` 计算数值。
- `CombatSystem::ApplyDamage` 仅负责 HP 扣除与死亡流程，不含数值公式。
- 不符合的路径在 CI 中触发编译警告（`[[deprecated]]`），Release 构建中 warning-as-error 升级可选。

### 5.2 兼容层 Contract

- `CombatSystem::CalculateDamage` 保留函数体，但标记 `[[deprecated]]`。
- 新增编译期宏 `COMBAT_LEGACY_CALC_ENABLED`（默认 OFF），ON 时恢复旧路径用于紧急回退。
- 兼容层在 M1 结束后移除。

### 5.3 迁移不变量

- 迁移前后，相同输入（攻击属性/防御属性/基础伤害/伤害类型）产出相同 `FinalDamage`（精度 ±0.01%）。
- 荆棘反噬按现有逻辑通过 Pipeline 计算（可使用特殊标签 `Tag::Thorns`）。
- 自伤/消耗类走 Pipeline 但跳过防御阶段（`DamageRequest.skip_mitigation = true`）。

---

## 6. Implementation Targets

### Source Files to Modify

- `src/game/systems/combat/CombatSystem.hpp` — `[[deprecated]]` 标记
- `src/game/systems/combat/CombatSystem.cpp` — 迁移 3 处 `CalculateDamage` 调用 + 3 处直接 `ApplyDamage` 调用
- `src/game/systems/skill/ProjectileSystem.cpp` — 迁移 2 处 `ApplyDamage` 调用
- `src/game/systems/skill/SkillSystem.cpp` — 迁移 1 处自伤 `ApplyDamage`
- `src/game/systems/skill/behaviors/SwordArray.cpp` — 迁移 1 处硬编码 `ApplyDamage`
- `src/game/systems/combat/DamagePipeline.hpp/cpp` — 可能需扩展 `DamageRequest` 以支持 thorns / self-damage 标签

### Source Files NOT Modified

- `SummonSystem.cpp` L83 — 留给 CS-M1-06 (combat_summon_entry_fix) 处理

### Tests to Create/Update

- `tests/unit/DamagePipelineUnifiedEntryTests.cpp` — 新建
- `tests/integration/CombatDamageRegressionTests.cpp` — 新建/扩展
- 现有 combat 测试 — 确保不回归

---

## 7. Acceptance Criteria

- [ ] `Select-String -Path "src\**\*" -Pattern "CalculateDamage" -Recurse` 仅返回 `CombatSystem.hpp` 声明（带 `[[deprecated]]`）和 `CombatSystem.cpp` 函数体。
- [ ] 所有新代码中的伤害路径通过 `DamagePipeline::Calculate` 或 `CalculateBatch`。
- [ ] 单体 / AoE / DoT / Trigger 四场景回归矩阵通过。
- [ ] 荆棘反噬 / 自伤 / AI 敌人攻击 / 投射物命中 — 数值与迁移前一致（±0.01%）。
- [ ] `build.bat` 编译无新增 error。
- [ ] `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` PASS。
- [ ] `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` PASS。
- [ ] `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS。

---

## 8. Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| 迁移过程中技能行为回归 | 高 | 保留旧链兼容开关 `COMBAT_LEGACY_CALC_ENABLED`；TDD 先写回归测试再迁移 |
| Pipeline 公式与旧链公式有微小数值差异 | 中 | 使用精确浮点比对测试（epsilon = 0.01%），迁移前建立数值快照 |
| 迁移影响帧序或性能 | 低 | Pipeline 已在热路径优化；迁移仅改调用方，不改 Pipeline 内部 |
| SwordArray / ProjectileSystem 特殊路径遗漏 | 中 | 全量 grep 审计 + 编译期 `[[deprecated]]` 捕获漏网 |
