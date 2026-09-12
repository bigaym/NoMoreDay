# 伤害管线 P0 行为变化与数值影响归档（P0-5）

- 基线 commit：`a803623f`
- 对比对象：Phase P0 删桩 + 去武器双算 + origin 门控前后
- 数值平衡裁决：**按计划后置到重构完成后**；本文件仅记录机制性行为变化与基准数据，不作为平衡调整依据。

## 1. 机制性变化总览

1. **删除 CandidateOnly 桩**：`DamagePipeline.cpp` 中 `!is_simulation && has_candidate_runtime_base` 时短路到 `CombatV2RuntimeFacade` 的分支（旧 511-538）彻底移除。真实施法不再返回 `base_pool × 1.05` 的近似值，而是走完整管线（命中判定/暴击/格挡/护甲/减伤/元素转化/事件派发）。
2. **新增 `DamageOrigin`**：`DamageRequest` 携带来源分类，默认 `DirectSkillCast` 作为兼容过渡。
3. **武器注入门控**：仅 `DirectSkillCast`（或调用方 payload 显式提供基础值）允许注入 `weapon_avg * weapon_damage_mult + skill_base_damage`；其余 5 档一律不注入。
4. **去除 base_pool 预写武器**：`CombatSystem::BuildLegacyAttackBasePool` 在持有武器时不再写 `min_weapon_damage`；怪物近战不再预写随机物理；`ProjectileSystem` 直击删除 else 分支武器写入。武器点伤统一由管线注入一次。
5. **怪物节点 More 单位修正**：`mastery_skill_trees.json` 的 `value` 为百分数（1.0=1%），管线按 `pow(1 + value*0.01, pts)` 计算，修复此前 `pow(1+value,pts)` 造成的指数爆炸（例如 7 星测试首击 1.58e10）。
6. **`skill_id=0` 回退保留 `weapon_damage_mult=1.0`**：真实普攻依赖其注入武器；合成伤害由 origin 门控隔离。

## 2. 分来源类别行为变化

| 来源类别 | 删桩前（旧行为） | 删桩后（新行为） | 武器注入变化 | 代表测试（旧值→新值） |
| --- | --- | --- | --- | --- |
| 普攻 / 怪物近战（`DirectSkillCast`, skill_id=0） | 桩返回 `base_pool × 1.05`；base_pool 预含武器 | 完整管线；base_pool 不含武器，武器只注入一次；受命暴/格挡/护甲/减伤 | 双算 → 单次 | `CombatDamageRegressionTests`：玩家 128→64，AI 90→45 |
| DoT tick（`AilmentTick`） | skill_id=0 回退 `mult=1.0` → 会附加武器点伤；再 ×1.05 | 不注入武器；完整减伤链 | 移除误注入 | `AilmentEngine` 相关 tick 伤害下降约 `weapon_avg` |
| 荆棘 / 反击（`ThornsReflect`） | skill_id=0/4 回退 → 附加武器点伤 | 不注入武器，仅反伤基础池 | 移除误注入 | 反击日志量级下降（不再含武器） |
| 环境危险区（`HazardEnvironment`） | skill_id=0 → 附加武器点伤 + ×1.05 | 不注入武器 | 移除误注入 | `HazardSystem`/血海 tick 下降 |
| 物品/怪物词缀（`ItemAffixProc`） | skill_id=0 → 附加武器点伤 + ×1.05 | 不注入武器 | 移除误注入 | `MonsterAffixTests`：虚空命中 105→95.2 |
| 次级投射物/分裂/爆炸/弹射/连锁/回响（`SecondaryProc`） | 桩返回 `base_pool × 1.05`；部分路径 base_pool 含武器 | 不注入武器与技能配置点伤；使用调用方 base_pool/payload | 双算 → 无注入 | `SkillSpecializationBakerTests` 碎裂：9685→9700 |
| 直接技能命中（`DirectSkillCast`，具名技能） | 桩短路；节点 More 指数爆炸（真实施法首次暴露） | 完整管线；注入一次武器+技能基础；节点 More 按百分数修正 | 修正 | `BladeMasteryGameTests`：回响 14.7→254，追击 88.2→354；7 星首击由 1.58e10 恢复正常量级 |

> 说明：旧桩期的 `105`（=100×1.05）出现在大量测试锚点中；P0-4 已按完整管线行为重导（见 §4）。

## 3. 基准对比（同配置，RelWithDebInfo）

### 3.1 DamagePipeline 基准

| 指标 | 基线 | 改动后 | 目标 | 结论 |
| --- | --- | --- | --- | --- |
| Single Calculate Mean / P99 | 0.001 / 0.001 ms | 0.001 / 0.001 ms | < 0.01 ms | 持平 |
| Batch 200 Mean / P99 | 0.056 / 0.258 ms | 0.045 / 0.108 ms | < 1.0 ms | Mean/P99 改善 |
| Batch Scaling 50 Mean/P99 | 0.025 / 0.059 ms | 0.029 / 0.156 ms | — | P99 波动，仍在目标内 |
| Batch Scaling 100 Mean/P99 | 0.032 / 0.043 ms | 0.037 / 0.051 ms | — | 基本持平 |
| Batch Scaling 200 Mean/P99 | 0.047 / 0.063 ms | 0.068 / 0.689 ms | — | P99 波动，仍在目标内 |
| Batch Scaling 500 Mean/P99 | 0.102 / 0.295 ms | 0.085 / 0.145 ms | — | Mean/P99 改善 |

### 3.2 Combat Release Gate

| 指标 | 基线 | 改动后 |
| --- | --- | --- |
| `combat_frame_p95_ms` | 0.0185 | 0.019 |
| `combat_frame_p99_ms` | 0.0257 | 0.0201 |

### 3.3 Combat Core Perf Baseline

| 指标 | 基线 | 改动后 |
| --- | --- | --- |
| doctest 断言 | 59 passed / 0 failed | 59 passed / 0 failed |

## 4. P0-4 测试锚点旧值 → 新值推导摘要

| 测试 | 旧期望 | 新期望 | 推导依据 |
| --- | --- | --- | --- |
| `CombatV2CutoverTests`（真实施法，base_pool 100，skip_mitigation） | 105 | 100 | 桩 ×1.05 移除；skill 42 无技能数据，无注入 |
| `CombatV2CutoverTests`（空实体攻击者，base_pool 100） | 105 | 0 | 无 `CombatStats` 时 `GetStatWithTags` 返回 0，倍增为 0 |
| `DefenseMitigationChainTests` 闪避 | 未闪避/126 | 已闪避/0 | 完整管线执行命中判定 |
| `DefenseMitigationChainTests` 格挡 | 未格挡/84 | 已格挡/60 | `80 × 0.75` |
| `DefenseMitigationChainTests` 护甲 | 105 | 40 | `100 × 0.5 × 0.8`（护甲 50% + 减伤 20%） |
| `MonsterAffixTests` 虚空命中 | 100−bonus×1.05 | 100−bonus（95.2） | 桩因子移除 |
| `MonsterAffixTests` 抑制 | 105 / 10 | 10 / 100 | 完整管线：远距 90% 抑制→10，近距→100 |
| `SkillSpecializationBakerTests` 碎裂 | 9685 | 9700 | `10000 − 300`，不再 ×1.05 |
| `CombatDamageRegressionTests` | 128 / 90 | 64 / 45 | 参考请求不再手动预写武器，与生产一致 |
| `SkillSystemTests` 天剑节点 | 目标 2000 HP 下被冲击秒杀 | 目标 2,000,000 HP 存活 | 完整管线伤害提升；提高夹具血量以真正执行场域 tick 与节点效果路径 |
| `BladeMasteryGameTests` 回响/追击 | 14.7 / 88.2 | 254 / 354 | 包含一次武器+技能基础注入 |
| `SkillBehaviorGuardTests` 减抗 debuff | 假设目标 1000 HP 存活 | 只断言 debuff 挂载 | 完整管线下目标可能已受伤，移除脆弱血量假设 |

## 5. `skill_id=0` 处置结论

保留 `SkillRegistry` 回退 `kDefaultBasicAttack{weapon_damage_mult=1.0f, base_damage=0.0f}`：

- 真实普攻（玩家/怪物近战）以 `skill_id=0` 调用且 `DirectSkillCast`，base_pool 已去武器化，必须依赖该回退注入武器点伤。
- 合成伤害（DoT/荆棘/环境/词缀/次级派生）已显式标注 origin，门控隔离，不受回退值影响。
- 影响面：普攻武器伤害保持单次计入；合成伤害不再被误注入武器（见 §2）。

## 6. 结论与遗留

- P0 达到“止血与分类”目标：删除近似桩、消除武器双算、修正节点 More 单位、完成调用点分类、全测试转绿。
- 各技能/来源的最终 DPS 变化属于预期的机制性差异，**数值平衡统一后置到重构完成后**，本阶段不做数值回调。
- 遗留（P1-P4）：两项 More 数组合并（P1-5）、`calculateBatch` 空返回与单/批事件语义分歧（B2/B3）、`PhantomTrance`/持续激光等 base_pool 预写武器复核、combat_v2 模块整体退出。
