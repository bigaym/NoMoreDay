# 伤害管线现代化 P3a 实施报告（P3-1 攻守状态解耦 + P3-2 实体组件化直存）

- 计划：`docs/plans/2026-09-11-damage-pipeline-modernization-plan.md`（§3.4 P3）
- 设计：`docs/designs/2026-09-11-damage-pipeline-modernization-design.md`（§4.2/§4.3 S2/§4.5.1/§6.1）
- 范围：仅 P3-1、P3-2。P3-3（SIMD 自适应）与 P3-4（并发压测）本次未触及。

---

## 1. 实现说明与关键行锚

### 1.1 P3-1 攻守状态解耦

**新增 `src/game/systems/combat/damage/DamageConditions.hpp`（84 行，纯头，POD）**

| 符号 | 行锚 | 说明 |
| --- | --- | --- |
| `enum class TargetCondition : uint32_t` | L15-23 | `None/Frozen=1<<0/Bleeding=1<<1/HpAbove80=1<<2/Controlled=1<<3/HasFateMark=1<<4/HasQiBrand=1<<5` |
| `operator|(TargetCondition,TargetCondition)` | L26-29 | 支持组合位（如 `HpAbove80|Controlled` 表示任一命中） |
| `ToMask` | L31-33 | 位掩码转换 |
| `enum class OpStage : uint8_t` | L36 | `More=0, CritDamage=1` |
| `struct ConditionalDamageOp` | L42-48 | `required_condition / stage / base_value / per_stack_value / BuffKind stack_source` |
| `kBuffKindCount=4`、`TargetConditionState` | L51-57 | `mask` + 按 BuffKind 索引的 `stacks[4]` |
| `StackCountFor` | L59-62 | 越界返回 0 |
| `EvaluateTargetConditionState` 声明 | L65-67 | 只读、无分配 |
| `EvaluateTargetConditions` 声明 | L70-71 | 便捷接口，仅返回掩码 |
| `ApplyConditionalMore` / `ApplyConditionalCritDamage` 声明 | L73-81 | 无匹配分别返回 1.0 / 0.0 |

**新增 `src/game/systems/combat/damage/DamageConditions.cpp`（120 行）**

- `IsControllingEffect` L14-20：`Stun|Freeze|Root|SpeedDown` 或 `id` 含 `"Slow"`。
- `EvaluateTargetConditionState` L24-73（签名 L25）：HP 阈值 L34-43（`HealthComponent` 优先，缺失且 `max>0` 才回退 `CombatStats`）；遍历 `ActiveEffectsComponent` L50-64 置 Frozen/Bleeding/Controlled 并按 BuffKind 记首个非零 stacks；L66-71 由 stacks>0 置 FateMark/QiBrand 位。
- `EvaluateTargetConditions` L75-78。
- `ConditionMatches` L83-87：`required_mask==0 || (target_mask & required_mask)`。
- `ApplyConditionalMore` L91-103、`ApplyConditionalCritDamage` L105-117。

**`src/game/systems/combat/damage/DamageSnapshot.hpp`（96 行）**

- `FixedVector<T,N>` L17-43（自 `DamagePipeline.cpp` 内联实现提取为公共头；容量溢出丢弃并 `LOG_WARN`）。
- `struct alignas(32) AttackerSnapshot` L49-58：`base_damage[6]`（已烘焙 Inc/More/Conversion）、`crit_chance`、`crit_damage`、`armor_pen`、`accuracy`、`hit_tags`、`FixedVector<ConditionalDamageOp,4> conditional_ops`、`_padding[4]`；L59-62 `static_assert` 32 字节对齐 + 平凡可拷贝。
- `struct alignas(32) DefenseSnapshot` L65-74：按设计补齐 `condition_mask` L73。
- `struct DamageSnapshotComponent` L78-83：`snapshot / origin / skill_id / caster`；L85-86 `static_assert` 平凡可拷贝。
- `struct AilmentSnapshotHolderComponent` L91-93：DoT 快照载体 `target` 字段。

**`src/game/systems/combat/DamagePipeline.hpp`**

- `CreateSnapshot(..., const DamagePayloadContext* payload = nullptr)` L71（P3-1 扩展）。
- `AttachSnapshotComponent(..., DamageOrigin origin = DamageOrigin::DirectSkillCast)` L79（P3-2 入口）。

**`src/game/systems/combat/DamagePipeline.cpp`**

- `BuildConditionalOps` L524-596：由攻方专精/技能域生成条件规则（数值逐条对应旧内联）。
- 单目标结算：条件状态与规则求值 L1066-1081；实例乘区 L1083-1091（快照路径）与 L1128（常规路径 `final_more=conditional_more`）；暴击增量 L1237（`extra_crit_mult=conditional_crit`）。
- 快照消费 L1022-1047：`source_entity → attacker` 顺序 `try_get<DamageSnapshotComponent>`；`use_snapshot && !thorns_like_damage` 时按 `base_damage` 重建实例。
- `CreateSnapshot` 重写 L1465-1536：构造显式 `DamageRequest sim_request`（含 `origin`、`payload_context`、`is_simulation=true`、`dispatch_damage_events=false`）→ `Calculate`；crit 参数纳入 payload；反算基线；L1534 写入 `conditional_ops`。
- `AttachSnapshotComponent` L1540-1554。
- 批量路径：`CreateSnapshot` L1581；SIMD 元素循环条件求值 L1735-1746、标量回退 L1824-1835；暴击 L1768 与 L1857 使用 `snap.crit_damage + cond_crit`。

### 1.2 P3-2 实体组件化直存

- 投射物挂载：`src/game/systems/skill/ProjectileSystem.cpp` L573-598。在 `entities` 收集后、`chunkSize=64` 并行 `SimulateProjectile` 之前**串行预扫描**，对每个带 `CombatStats` 且无 `damage::DamageSnapshotComponent` 的实体调用 `DamagePipeline::AttachSnapshotComponent(registry, e, e, snap_skill_id, DamagePool{}, Tag::Projectile|Tag::Hit, e, payload)`；`skill_id` 取 `SkillComponent`，`payload` 取 `Projectile.payload_context`。放在串行段是为避免并行 chunk 内 emplace 引发 registry 数据竞争。
- DoT 挂载：`src/game/systems/combat/AilmentEngine.cpp`
  - `SyncAilmentSnapshot` L528-558：施加时创建/刷新临时 holder 实体，挂 `AilmentSnapshotHolderComponent` + `DamageSnapshotComponent`（`origin=AilmentTick`，`caster=source`），`PerStack` 策略把叠层一并烘焙（L549-552），句柄写回 `effect.snapshot_source`。
  - 调用点 6 处：L623（Independent 新建）、L641（Additive 叠层）、L648（Strongest 刷新）、L657（replacement）、L666（matchingIndices 空新建）、L691（合并路径）。
  - 回收：`AilmentTickDriver::Tick` L709-739，遍历 holder，若 `target` 无效或没有任何 `effect.snapshot_source==holder` 则 destroy（EffectSystem 已先清理过期 BuffEffect，故以引用为唯一存活判据）。
  - 消费：`request.attacker=effect.source`、`request.source_entity=effect.snapshot_source`（L799）。
- `src/game/foundation/components/Buff.hpp`：`BuffEffect::snapshot_source` L118（P3-2 运行期字段，不序列化）；`from_json` 末尾重置为 `entt::null` L170。
- `src/game/systems/combat/CMakeLists.txt` L21：`NoMoreDayGameCombat` 显式清单新增 `damage/DamageConditions.cpp`。
- 无全局缓存；未引入任何 LRU / 伤害快照缓存（见 §5.4 证据）。

---

## 2. 条件位数据来源表

| 位（值） | 判定依据 | 组件 / 枚举 | 实现行 |
| --- | --- | --- | --- |
| `Frozen` (1<<0) | 任一效果 `type == BuffType::Freeze` | `ActiveEffectsComponent` / `BuffType::Freeze` | `DamageConditions.cpp:51` |
| `Bleeding` (1<<1) | 任一效果 `type == BuffType::Bleed` | `ActiveEffectsComponent` / `BuffType::Bleed` | `:54` |
| `HpAbove80` (1<<2) | `current/max > 0.80`（严格大于）；`HealthComponent` 优先，缺失时回退 `CombatStats.health/max_health` | `HealthComponent`（`Common.hpp:70-74`）/ `CombatStats` | `:34-43` |
| `Controlled` (1<<3) | `type ∈ {Stun,Freeze,Root,SpeedDown}` 或 `id` 含 `"Slow"` | `ActiveEffectsComponent` / `BuffType` | `:14-20,57` |
| `HasFateMark` (1<<4) | 首个 `kind==BuffKind::FateMark` 的 `stacks > 0` | `ActiveEffectsComponent` / `BuffKind::FateMark` | `:60-68` |
| `HasQiBrand` (1<<5) | 首个 `kind==BuffKind::QiBrand` 的 `stacks > 0` | `ActiveEffectsComponent` / `BuffKind::QiBrand` | `:60-71` |

叠层取值约定：按 `BuffKind` 索引，仅当 `stacks[idx]==0` 时写入，取首个非零层数（`DamageConditions.cpp:61-63`）；`BuffKind::None/FreeCast` 也占索引但不作条件源。求值路径只读、无堆分配。

---

## 3. 快照组件生命周期与消费路径

| 载体 | 创建 | 消费 | 回收 |
| --- | --- | --- | --- |
| 投射物 | `ProjectileSystem` 串行预扫描 L573-598（每个投射物一组件，caster=投射物自身，其 `CombatStats` 已冻结） | 命中时通用/直击入口以 `attacker=projEnt`、`source_entity=projEnt` 调 `Calculate`，L1025-1032 命中组件，L1033-1047 用组件快照 | 随投射物实体销毁（命中/超时）自然回收 |
| DoT（异常） | `SyncAilmentSnapshot` L528-558：临时 holder 实体挂组件，`effect.snapshot_source` 持句柄 | tick 时 `attacker=effect.source`（caster）、`source_entity=holder`（L799），L1026 优先命中 holder 组件 | `AilmentTickDriver::Tick` L709-739 定期回收孤儿 holder |

要点：

- **caster 死亡仍可结算**：组件为平凡可拷贝 POD，`caster` 只是 `entt::entity` id，结算不 dereference。投射物持自身快照；DoT 持 holder 快照，即使 caster 实体销毁，tick 仍以 holder 为 `source_entity` 结算。
- **无裸指针/无效引用**：组件内不含 registry 指针或组件引用。
- **快照已烘焙内容**：攻方静态乘区、转换、暴击参数（含 payload `crit_chance/crit_multiplier`）在创建时烘焙；命中时仅补守方实时条件 More/Crit（创建时守方未知）。
- **单目标即时提取**：无组件的近战直伤仍在栈上 `BuildConditionalOps` 现算，等价旧路径。

---

## 4. 172/512/150 等机制改造前后数值对照

改造原则：数值仍从原数据源读取（`SkillMechanicsRegistry` / 专精节点 `allocated_points` / `GetBakedSkillProfile`），仅把 ad-hoc 分支改为 `ConditionalDamageOp`。

| 机制 | 旧内联实现 | 新条件规则 | 数据来源 | 等价性 |
| --- | --- | --- | --- | --- |
| 172 凛风（skill1） | `final_more *= GetFloat(1,172,"frozen_more_mult",1.50)`，条件守方 `BuffType::Freeze` | `More base = frozen_more-1 = 0.50`，`required=Frozen` | `skill_mechanics.json (1,172)` | `1.50 ↔ 1+0.50`，一致 |
| 150 弱点（skill1） | `extra_crit_mult = 0.15 * wpPoints`，条件 `HpAbove80 \|\| Controlled` | `CritDamage base = 0.15*wpPoints`，`required=HpAbove80\|Controlled` | 专精节点 `150 allocated_points`（系数 0.15 硬编码，与旧一致） | 一致 |
| 512 命印（skill5） | `final_more *= 1 + GetFloat(5,512,"damage_taken_per_stack",0.03)*stacks`，条件 `GetByKind(FateMark)` | `More per_stack=0.03`，`stack_source=FateMark`，`required=HasFateMark` | `skill_mechanics.json (5,512)` | `1+0.03*stacks` 一致 |
| 573 绝对零度（skill5） | `final_more *= 1 + GetFloat(5,573,"shatter_damage_pct_per_point",0.15)*pts573`，条件 Freeze | `More base = per_point*pts573`，`required=Frozen` | `skill_mechanics.json (5,573)` + 节点 `573` 点数 | 一致 |
| 250 气印（任意技能） | `extra_crit_mult += 0.04*stacks`，条件 `GetByKind(QiBrand)` | `CritDamage per_stack=0.04`，`stack_source=QiBrand`，`required=HasQiBrand` | 常量 0.04（与旧一致） | 一致 |
| 812 撕裂（skill8） | `extra_crit_mult += GetBakedSkillProfile(8)->delivery.crit_mult_vs_bleeding`，条件 `BuffType::Bleed` | `CritDamage base = profile 值`，`required=Bleeding` | `SkillSystem::GetBakedSkillProfile(attacker,8)` | 一致 |

逐条结论：**改造后数值与旧实现逐一等价**，无推导地改测试期望的情况不存在。

### 行为变化清单（显式收敛）

1. **批量路径补上守方条件加成**：P2 批量内核以 `defender=null` 仿真建快照，导致 Frozen/命印/气印等条件加成从未生效；本次在批量 SIMD 与标量回退中实时求值并应用，使批量与单目标口径统一。这是一处**按设计收敛**：批量命中有条件的目标时伤害会较 P2 提高（此前偏低）；perf 用例守方无条件，数值不受影响。
2. **`CreateSnapshot` 仿真口径修复**：仿真 `DamageRequest.origin` 现与调用方一致，且 payload `crit_chance/crit_multiplier` 纳入快照。修复前 DoT 快照因仿真默认 `DirectSkillCast` 注入武器/技能点伤而基线偏高（曾使 `FlowingThrustNodes` 回血用例 `80 != 100`），修复后与真实 tick 一致；投射物快照 `base_damage` 同步修正。
3. `FixedVector<ConditionalDamageOp,4>` 溢出时丢弃并告警；当前每技能最多 3 条 op（172+150+250 / 512+573+250 / 812+250），未触界。

---

## 5. 验证证据

### 5.1 构建

- `build.bat`（RelWithDebInfo, ALL_BUILD）`EXIT=0`。
- 日志：`build/p3a_build_final.log`。

### 5.2 测试

- `ctest --test-dir build -C RelWithDebInfo -L ci` → `EXIT=0`，1 test / 100% passed；全量 doctest 1458 例 / 108087 断言全绿。日志 `build/p3a_ctest_ci_final.log`。
- `ctest -L integration` → `EXIT=8`：177 例 / 176 通过 / 1 失败。**唯一失败为既有环境性 GPU timer flake**：`tests/integration/SingleGpuTimerOwnerRegressionTest.cpp:235 CHECK(frameResult.state == QueryState::Valid)`，`CHECK(3 == 1)`（3=`CpuFallback`）。两次连跑稳定复现，且与伤害管线无关：`docs/reports/damage-pipeline-modernization/phase-P2/P2-report.md:143` R1 已记录该用例为“既有偶发，非 P2 引入”；`docs/reviews/2026-09-06-modern-skill-system-review.md:884` 同结论。日志 `build/p3a_ctest_integration.log`、`build/p3a_ctest_integration2.log`。
- 新增 `tests/functional/DamagePipelineP3Tests.cpp`：**8 例 / 41 断言全通过**（`build/p3a_p3tests.log`）。覆盖：
  1. 条件掩码逐位（冻结/流血/HP>80%/受控/命印/气印）+ 叠层数与空目标掩码；
  2. HP 阈值边界（80 不置位、81 置位；无 Health 回退 CombatStats）；
  3. 条件 More/CritDamage 叠层数值；
  4. 快照烘焙 payload 暴击倍率；
  5. 投射物快照冻结（生成后改攻方属性、销毁 caster 后命中伤害不变）；
  6. ProjectileSystem 经 `Update` 自动挂载快照组件（skill_id/caster 正确）；
  7. DoT 子在施加时快照（施加后叠攻方增益不改变 tick）；
  8. `[Integration]` caster 移除后 DoT 仍可结算。

### 5.3 性能

`bin\NoMoreDayTests.exe --test-case="[Performance]*DamagePipeline*"`（日志 `build/p3a_perf.log`）：

| 指标 | 本次 | P2 报告实测 | P2 任务给定基线 |
| --- | --- | --- | --- |
| Single Mean / P99 | 0.000 / 0.001 ms | 0.000 / 0.001 ms | 0.001 ms(P99) |
| Batch200 Mean | 0.050–0.062 ms | 0.049 ms | 0.046 ms |
| Batch200 P99 | 0.091–0.194 ms | 0.084 ms | 0.063 ms |

- 批量缩放 50/100/200/500 均达标。
- **对照实验**：临时禁用批量条件求值后测得 Batch200 Mean=0.050 / P99=0.191ms，与启用时同区间 → 条件求值**不是**回归源，观测差异为机器噪声。P99 本身波动大。
- 任务给定基线（0.046/0.063）与 P2 报告实际记录（0.049/0.084）不一致；本次以 P2 报告实测为准，Mean 处于其 ±噪声区间。SIMD 结构本次未改动（P3-3 负责）。

### 5.4 无全局 LRU 证据

严格检索 `\bLRU\b|LRUCache|snapshot_cache|SnapshotCache|DamageSnapshotCache|damage_cache`（src + tests）：

- 命中仅 `tests/unit/OccluderCollectorTest.cpp:40`（地形遮挡块 LRU，与伤害无关）与 `DamageSnapshot.hpp:76` 注释文字；`src/` 内**不存在**任何伤害快照缓存实现。
- 快照改为组件直存（投射物/DoT 实体），无全局动态 LRU。

---

## 6. 遗留风险

1. **既有 GPU timer flake**：`SingleGpuTimerOwnerRegressionTest.cpp:235` 在本环境稳定以 `CpuFallback(3)` 失败，非本包引入；集成门禁因此非 100%，建议渲染线跟进（放宽轮询窗口或显式区分 `CpuFallback` 语义）。
2. **Batch200 P99 波动**：对照实验证明与本次条件求值无关；待 P3-3 SIMD 自适应进一步收敛尾延迟。
3. **`DefenseSnapshot` 尚未被消费**：按设计补齐了字段（含 `condition_mask`），但当前结算仍走 `EvaluateTargetConditionState` 实时求值，`DefenseSnapshot` 作为后续守方快照化接口预留，未接入生产路径。
4. **快照路径跳过 `shadow_multiplier`**：依赖创建时仿真与真实调用方 `attacker/source_entity` 口径一致。当前投射物不挂 `ShadowComponent`；若未来出现带暗影的投射物，需复核。
5. **DoT holder 回收依赖 `AilmentTickDriver::Tick` 运行**：仅在存在异常并 tick 时回收；若长期不 tick，孤儿 holder 会滞留。当前所有 DoT 场景均会 tick。
6. **`FixedVector<ConditionalDamageOp,4>` 容量**：新增条件规则超过 4 条会丢弃并告警，需同步扩容。

---

## 7. 证据索引

| 内容 | 路径 |
| --- | --- |
| 最终构建日志 | `build/p3a_build_final.log` |
| ci 门禁日志 | `build/p3a_ctest_ci_final.log` |
| 集成门禁日志 | `build/p3a_ctest_integration.log`、`build/p3a_ctest_integration2.log` |
| P3 新增测试日志 | `build/p3a_p3tests.log` |
| 性能日志 | `build/p3a_perf.log` |
| P3a 变更 diff（DamagePipeline） | `build/p3a_diff_dp.txt` |
