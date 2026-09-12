# 伤害请求调用点分类审计（P0-2）

- 基线 commit：`a803623f`
- 审计时间：Phase P0（止血与分类）
- 权威依据：`docs/plans/2026-09-11-damage-pipeline-modernization-plan.md` §1.3/§3.1、`docs/designs/2026-09-11-damage-pipeline-modernization-design.md` §4.2/§4.3
- 检索方式：`rg` 全仓扫描 `DamageRequest` 构造点、`ResolveDamage`/`ResolveDamageBatch` 调用点、`DamagePipeline::Calculate/Execute/CalculateBatch` 调用点、`base_pool` 写入点、`skill_id=0` 使用点、武器注入点（`weapon_damage_mult`/`min_weapon_damage`/`weapon_avg`）。
- 原始扫描产物：`_scan_calls.txt`、`_scan_requests.txt`、`_scan_current.txt`（本目录）。行号为改码后当前行号；P0-2 初扫时的旧行号见 `_scan_requests.txt`。

## 1. `DamageOrigin` 分档口径

| 档位 | 语义 | 是否允许注入武器点伤 + 技能配置点伤 |
| --- | --- | --- |
| `DirectSkillCast` | 玩家/怪物主动技能/普攻直接命中（技能自身投射物、近战、技能直击） | 允许（唯一允许） |
| `SecondaryProc` | 由直接命中派生的次级伤害：分裂/爆炸/弹射/连锁/回响/余震/追击 | 不允许；基础值由调用方在 `base_pool` 或 `payload_context` 提供 |
| `AilmentTick` | 持续伤害 tick（燃烧/中毒/流血等） | 不允许 |
| `HazardEnvironment` | 环境/地面危险区（危险池、血海、地火等） | 不允许 |
| `ThornsReflect` | 荆棘/反伤/Blade Ward 反击 | 不允许 |
| `ItemAffixProc` | 物品/怪物词缀触发伤害（虚空命中、抑制等） | 不允许 |

## 2. 调用点全表（按 origin 分档）

### 2.1 `DirectSkillCast`（显式标注，玩家/怪物主动直伤）

| file:line | 调用者 / 场景 | skill_id | base_pool 语义 | 现状问题 | 处置 |
| --- | --- | --- | --- | --- | --- |
| `src/game/systems/combat/CombatSystem.cpp:266` | 玩家近战普攻 `ProcessPlayerMelee` | 0 | `BuildLegacyAttackBasePool`：有武器时不再写武器，无武器时写 `baseDamage` | 旧实现把 `min_weapon_damage` 预写入 base_pool，叠加管线注入 → 武器双算 | 显式保持 `DirectSkillCast`；base_pool 去武器化；由管线统一注入 `weapon_avg*mult + skill_base` |
| `src/game/systems/combat/CombatSystem.cpp:454` | 怪物近战普攻 | 0 | 已移除 base_pool 随机物理 | 旧实现预写随机物理，叠加注入 → 双算 | 显式保持 `DirectSkillCast`；base_pool 置空，由管线注入 |
| `src/game/systems/combat/DamagePipeline.cpp:482` | `Calculate(...)` 便捷重载内部构造 | 透传调用方 | 透传 | 无 | 透传 origin（默认 `DirectSkillCast` 兼容） |
| `src/game/systems/combat/DamagePipeline.cpp:1332` | `Execute(...)` 复制请求 | 透传调用方 | 透传 | 无 | 复制 origin |
| `src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp:306` | 天剑降临 `ApplySingleHit`（冲击/场域 tick/基础直击） | 11 | `BuildHeavenlyDamagePool(attunement, damage)`（技能基础×倍率） | 管线段已加技能配置点伤，base_pool 不再含武器，无双算；场域 tick 是否应算次级见 P1-5 合并节点 More 议题 | 暂保持 `DirectSkillCast`；测试锚点按完整管线重导 |
| `src/game/systems/skill/behaviors/SevenStarSlash.cpp:303` | 七星归一 `ApplySlashDamage` | 10 | `pool.Add(Physical, slashDamage)`（weapon×wmult×(1+flow)） | `slashDamage` 已含武器倍率，管线再注入 `weapon_avg*mult` → 次级乘区叠加（非基础双算），行为层节点已在末斩单独结算 | 暂保持 `DirectSkillCast`；节点 More 重复结算问题归 P1-5 |
| `src/game/systems/skill/behaviors/PhantomTrance.cpp:425` / `:520` | 幻影移形脉冲 / 爆发 | 6 | base_pool 预写入 `weapon_avg*mult` | 预写武器 + 管线注入 → 需在 P0 后复核是否双算 | 暂保持 `DirectSkillCast`，列入 P1-5 死代码清理复核 |
| `src/game/systems/skill/behaviors/SwordArray.cpp:391` / `:433` | 万剑归宗冲刺 / 连锁连接 | 7 | 调用方 `damage` | 节点条件由行为层结算 | 暂保持 `DirectSkillCast` |
| `src/game/systems/skill/behaviors/AreaFieldDeliverySystem.cpp:177` / `:411` / `:450` | 区域场直伤 / 连锁弧 / 天降 | 透传 | 透传 | 无 | 暂保持 `DirectSkillCast` |
| `src/game/systems/skill/BeamChannelDeliverySystem.cpp:385` / `:1266` | 光束通道直伤 / 持续激光 | 透传（`:1266` 为 7） | `:1266` 预写入 `weapon_avg` | 预写武器 + 管线注入 → 需 P0 后复核 | 暂保持 `DirectSkillCast`，列入 P1-5 复核 |
| `src/game/systems/skill/ProjectileSystem.cpp:116` / `:706` | 投射物直击 / 投射物 | 透传（多为 1/2） | payload_context 提供 `base_damage_min/max`（`:116`） | 旧实现 `else` 分支把武器写入 base_pool，与管线注入双算 | 已移除 else 武器写入，保留 payload 路径；保持 `DirectSkillCast`；payload 基础值由门控的 `payload_supplies_base` 放行 |

### 2.2 `SecondaryProc`（次级派生，显式标注）

| file:line | 调用者 / 场景 | skill_id | 注入门控结果 |
| --- | --- | --- | --- |
| `src/game/systems/skill/SummonCombatBridge.cpp:240` | 召唤物战斗桥接 | 召唤技能 | 不注入 |
| `src/game/systems/skill/SkillSystem.cpp:1051` | 技能过载连锁 `chain_request` | 透传 | 不注入 |
| `src/game/systems/skill/ProjectileSystem.cpp:197` | 粘性投射物爆炸 | 透传 | 不注入 |
| `src/game/systems/skill/ElementPathSystem.cpp:97` | 御剑回旋元素路径伤害 | 8 | 不注入；payload 提供基础值（`payload_supplies_base` 放行） |
| `src/game/systems/skill/behaviors/BladeFormation.cpp:462` / `:548` | 剑阵连锁 / 引爆 | 3 | 不注入 |
| `src/game/systems/skill/BeamChannelDeliverySystem.cpp:942` | 巨剑爆炸 | 5 | 不注入 |
| `src/game/systems/skill/behaviors/FlowingThrust.cpp:466` / `:643` | 流血爆发 / 碎裂溅射 | 9 | 不注入 |
| `src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp:894` | 天剑联动回响 `HandleLinkedHit` | 11 | 不注入 |
| `src/game/systems/skill/behaviors/InfiniteBlades.cpp:333` / `:365` | 万刃满层溅射 / 层数溅射 | 5 | 不注入 |
| `src/game/systems/skill/behaviors/RendingWave.cpp:610` / `:680` | 裂空波碎裂 / 连锁 | 12 | 不注入 |
| `src/game/systems/skill/OrbitingSentinelDeliverySystem.cpp:104` | 环绕哨兵交付 | 哨兵技能 | 不注入 |

### 2.3 `AilmentTick`

| file:line | 调用者 / 场景 | skill_id | base_pool 语义 | 处置 |
| --- | --- | --- | --- | --- |
| `src/game/systems/combat/AilmentEngine.cpp:716` | DoT tick `AilmentEngine::ApplyTickDamage` | 0 | `base_pool = tickDamage`，`additional_tags = DamageOverTime` | 显式 `AilmentTick`，严禁注入武器 |

### 2.4 `HazardEnvironment`

| file:line | 调用者 / 场景 | skill_id | base_pool 语义 | 处置 |
| --- | --- | --- | --- | --- |
| `src/game/systems/combat/HazardSystem.cpp:420` | 环境危害区 tick | 0 | 区域伤害 | 显式 `HazardEnvironment` |
| `src/game/systems/skill/behaviors/BloodSea.cpp:293` | 血海持续场（`aftershock = request` 复制携带同 origin） | 4 | 场域伤害 | 显式 `HazardEnvironment` |

### 2.5 `ThornsReflect`

| file:line | 调用者 / 场景 | skill_id | base_pool 语义 | 处置 |
| --- | --- | --- | --- | --- |
| `src/game/systems/combat/CombatSystem.cpp:350` | 玩家荆棘反伤 | 0 | `base_pool = thorns`，`skip_mitigation=true` | 显式 `ThornsReflect`，不注入 |
| `src/game/systems/combat/DamagePipeline.cpp:1299` | Blade Ward 单目标反击 | 4 | `counterPool` | 显式 `ThornsReflect`，不注入 |
| `src/game/systems/combat/DamagePipeline.cpp:1771` | Blade Ward 批处理反击 | 4 | `counterPool` | 显式 `ThornsReflect`，不注入 |
| `src/game/systems/skill/ProjectileSystem.cpp:670` | 投射物 Blade Ward 反击 | 4 | `counterPool` | 显式 `ThornsReflect`，不注入 |

### 2.6 `ItemAffixProc`

| file:line | 调用者 / 场景 | skill_id | base_pool 语义 | 处置 |
| --- | --- | --- | --- | --- |
| `src/game/systems/combat/MonsterAffixSystem.hpp:1034` | 怪物词缀虚空命中 `ApplyVoidOnHit` | 0 | 词缀伤害 | 显式 `ItemAffixProc`，不注入 |

## 3. 已知线索逐点核实

| 线索（初扫旧行号） | 核实结果 |
| --- | --- |
| `CombatSystem.cpp:40-50` | `BuildLegacyAttackBasePool`：旧逻辑 `physBase = (有武器>0.1) ? min_weapon_damage : baseDamage`。**已改为有武器时不写 base_pool**，避免与管线注入双算。 |
| `CombatSystem.cpp:266-273` | 玩家近战普攻，`skill_id=0`，`DirectSkillCast`，base_pool 已去武器化。 |
| `CombatSystem.cpp:349-357` | 荆棘，**已标注 `ThornsReflect`**。 |
| `CombatSystem.cpp:455-462` | 怪物近战，`skill_id=0`，base_pool 置空、改由管线注入。 |
| `AilmentEngine.cpp:711-722` | DoT tick，**已标注 `AilmentTick`**。 |
| `HazardSystem.cpp:419-426` | 环境危害区，**已标注 `HazardEnvironment`**。 |
| `MonsterAffixSystem.hpp:1016-1044` | 怪物词缀虚空命中，**已标注 `ItemAffixProc`**。 |
| `ProjectileSystem.cpp:128-134` | 直击 payload 路径：保留 payload（`base_damage_min/max`），**删除 else 分支武器 base_pool 写入**。 |
| `ProjectileSystem.cpp:199-210` | 粘性爆炸，**已标注 `SecondaryProc`**。 |
| `RendingWave.cpp:605-617` | 裂空波碎裂，**已标注 `SecondaryProc`**。 |

## 4. 统计

| origin 档位 | 显式标注数 |
| --- | --- |
| `DirectSkillCast` | 16 个调用点（含显式保持 + 兼容默认；其中 2 个为管线内部透传/复制） |
| `SecondaryProc` | 15 |
| `AilmentTick` | 1 |
| `HazardEnvironment` | 2 |
| `ThornsReflect` | 4 |
| `ItemAffixProc` | 1 |
| 合计显式标注 | 23 |

> `src/game/foundation/combat_v2/CombatV2RuntimeFacade.hpp:77` 内的 `DamageRequest damageRequest{}` 属 combat_v2 模块（P4 整体处理），本阶段不标注 origin、不删模块，仅移除其在 `DamagePipeline.cpp` 的调用与 include。

## 5. `skill_id=0` 回退裁决

- `SkillRegistry::GetSkill(0)` 无对应技能数据（`skills.json` id 为 1–12，无 0），返回静态 `kDefaultBasicAttack{weapon_damage_mult=1.0f, base_damage=0.0f}`。
- 真实普攻调用点：`CombatSystem.cpp:266`（玩家近战，skill_id=0）与 `:454`（怪物近战，skill_id=0）。两者**依赖管线按 `DirectSkillCast` 注入武器点伤**；由于 base_pool 已去武器化，若把回退 `weapon_damage_mult` 置 0，普攻将完全丢失武器伤害。
- **结论：保留 `weapon_damage_mult=1.0f`**。合成伤害（DoT/荆棘/词缀/环境）通过显式 `origin` 门控隔离，不依赖回退值。若未来将真实普攻改为具名技能 id，可再评估置 0。

## 6. 遗留与后续（明确不属于 P0）

- 行为层节点结算 + 管线节点 More 结算的“两套 More 数组”重复：归 P1-5 合并（本次仅修正百分号单位）。
- `PhantomTrance`、`BeamChannelDeliverySystem` 的持续激光等仍在 base_pool 预写武器倍率，是否存在次级乘区叠加需在 P1-5 复核。
- `calculateBatch` 钩子返回空 vector（B2）与单/批事件语义分歧（B3）：归 P1。
- 数值平衡（各技能最终 DPS）统一后置到重构完成后的 balance 阶段。
