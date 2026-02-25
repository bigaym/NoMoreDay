# Combat Summon Strategy V1 — Specification

> Track ID: `combat_summon_strategy_v1_20260225`  
> Series: CS-M2-03 | Priority: P1 | Milestone: M2

---

## 1. Overview

将召唤系统从单 `SummonSystem` 拆分为三个子系统，建立完整的归因、继承和预算治理。

## 2. Target Architecture

```
SummonLifecycleSystem — 生成、续期、销毁、归属变更
SummonAISystem        — 目标选择与行为状态机
SummonCombatBridge    — 伤害归因、继承、预算，统一进入 DamagePipeline
```

## 3. Data Model (From Improvement Plan §4)

```cpp
enum class SummonInheritMode : uint8_t { Snapshot, Dynamic, Mixed };
enum class SummonRole : uint8_t { Melee, Ranged, Support, Orbit };
enum class SummonCommandMode : uint8_t { Passive, Defend, Assist, Aggressive };

struct SummonComponent { /* archetype_id replaces name string */ };
struct SummonCombatProfile { /* damage_scale, inherit_mode, proc_budget */ };
struct SummonAIProfile { /* role, command_mode, retarget_interval, leash_radius */ };
struct SummonRuntimeState { /* current_target, attack_cd, budgets */ };
```

## 4. Summon Contract

- 三元归因必须强制：`owner` + `summon` + `source_skill`。
- 禁止同来源重复乘区。
- 禁止召唤路径绕过 `DamagePipeline`。

## 5. Acceptance Criteria

- [ ] 所有召唤伤害事件含三元归因字段。
- [ ] 灵剑为首个迁移样板通过全回归。
- [ ] 召唤热路径无 `std::string` 依赖。
- [ ] SummonSystem 拆分为 3 子系统。
- [ ] 召唤事件链无漏发、无重复发。
- [ ] `build.bat` + `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` PASS。

## 6. Risks

| Risk | Mitigation |
|---|---|
| 大范围重构导致灵剑行为漂移 | 灵剑先行灰度，对照日志比对 |
| AI 子系统拆分影响帧序 | 保持现有 Update 顺序合同 |
