# P4 独立审查阻塞项修复报告

- 范围：伤害管线重构 P0–P4 结项后的独立审查修复（未 commit）。
- 基线：`main` 脏工作区（P0–P4 全部未提交变更）。
- 构建：`build.bat`（RelWithDebInfo）exit=0。
- 门禁：`ctest --test-dir build -C RelWithDebInfo -L ci`、`-L integration` 全绿。
- 约束：未 commit；仅使用 `build.bat`；ctest 均带 `-C RelWithDebInfo`；中文注释、英文标识符/日志；未改动无关内容。

行号说明：本次修复会移动 `DamagePipeline.cpp` 行号，故下文以「函数名 + 锚点代码」为主，行号仅作当前工作区定位参考。

---

## 1. 阻塞项

### B1 转换 API 类型语义不安全（Poison/Shadow）

**Finding**：`damage/DamageConversion.hpp` 的 `TransformTagsOnConversion` 用 `1ULL << static_cast<uint8_t>(target_type)` 位移；`ApplyConversion` 把池下标当作 `DamageType` 枚举序比较/写入（`static_cast<size_t>(rule.dst_type)`）；调用方 `DamagePipeline.cpp` 的 `AddConversionRule` 又用「池索引（tag 位序，4=Shadow/5=Poison）」强转 `DamageType`（枚举序 4=Poison/5=Shadow）。管线内往返自洽，但外部按签名传真实 `DamageType::Shadow(5)` 会得到 `1<<5 == Tag::Poison`。

**修复**：
1. `src/game/systems/combat/damage/DamageTypes.hpp`
   - 新增公共 `kElementCount = 6`（`DamageTypes.hpp:12`）。
   - 新增公共 `ElementTagOf(DamageType)`（`DamageTypes.hpp:100`），内部经 `DamageTypeToPoolIndex` 映射池序：`Shadow→1<<4=Tag::Shadow`、`Poison→1<<5=Tag::Poison`；`True/Count` 越界返回 `Tag::None`。
2. `src/game/systems/combat/DamageMitigationService.cpp`
   - 删除匿名命名空间的内部 `ElementTagOf` 副本，改调 `damage::ElementTagOf`（`:48`、`:186`、`:218`）。
3. `src/game/systems/combat/damage/DamageConversion.hpp`（整体重写契约）
   - 文件头注释明确：**数组按池索引（tag 位序）对齐，`ConversionRule` 字段为真实 `DamageType`**（`:18`、`:31`、`:44`）。
   - `TransformTagsOnConversion` 改用 `ElementTagOf`（`:45-49`）。
   - `ApplyConversion` 中 source 匹配用 `PoolIndexToDamageType(src)`，落点用 `DamageTypeToPoolIndex(rule.dst_type)`，返回 `kElementCount` 视为非法跳过；不再出现任何 `static_cast<uint8_t>` 位移（`:73-140`）。
   - 单遍守恒算法（retained / scale / `GainExtra`）保持不变。
4. 调用方 `DamagePipeline.cpp` 的 `AddConversionRule`（`DamagePipeline.cpp:983`）
   - 由池索引经 `damage::PoolIndexToDamageType(source_pool/target_pool)` 构造真实 `DamageType` 规则；`source_pool == target_pool` 自转静默跳过；越界跳过（`:995-996`）。
5. 契约注释同步更新：`kElementCount` 与 `ConversionOutput` 注释与池序契约一致。

**测试**：`tests/unit/DamageConversionTypesTests.cpp`（新增）
- `[Unit] DamageConversionTypes - ElementTagOf respects pool order`：`Shadow→Tag::Shadow`、`Poison→Tag::Poison`、`True/Count→Tag::None`。
- `[Unit] DamageConversionTypes - TransformTagsOnConversion Shadow/Poison`：Fire|Cold → `DamageType::Shadow` 后含 `Tag::Shadow` 且不含 `Tag::Poison`；Fire|Cold → Poison 反向同理；非元素 `Hit/Projectile` 标签保留。
- `[Unit] DamageConversionTypes - ApplyConversion uses pool-index arrays`：池序数组 + 真实 `DamageType` 规则，`Shadow(池4)→Poison` 后值落池 5、标签为 Poison；`Poison→Shadow` 逆序覆盖；守恒成立。
- `[Unit] DamageConversionTypes - reverse direction conversion allowed`：Cold→Fire 25%、Fire→Cold 100% 均实际发生且守恒。

**命令结果**：`bin\NoMoreDayTests.exe --test-case=*DamageConversion*` 纳入专项套件，68 cases / 6292 assertions 全绿（`review-fix-posttest.log`）。

---

### B2 ProjectileSystem 快照 attacker 传了投射物自身

**Finding**：`src/game/systems/skill/ProjectileSystem.cpp` 预扫描 `AttachSnapshotComponent(registry, e, e, ...)` 第二个 `e`（attacker）错传投射物实体自身。`CreateSnapshot` / `BuildConditionalOps` 随即在投射物实体上查 `ActiveSkillsComponent`，专精点恒 0，172/150/512 等动态条件与专精 More 全部丢失。

**修复**：`src/game/systems/skill/ProjectileSystem.cpp`
- 取施法者并回退：`const entt::entity attacker = (p && registry.valid(p->owner)) ? p->owner : e;`（`:590-593`）。
- 守卫按所选 attacker 判断：`if (!registry.all_of<CombatStats>(attacker)) continue;`（`:594`）。投射物本身已由生成时复制 `owner` 的 `CombatStats`，owner 仍存活时正常；owner 已销毁则 `valid(p->owner)` 为假、回退到投射物自身（保留原有降级行为，避免快照整段丢失）。
- `AttachSnapshotComponent` 第二参改传 `attacker`，`source_entity = e` 保持不变（`:604`）。
- 参数语义与 `AilmentEngine::SyncAilmentSnapshot`（holder 与 attacker=source 分离）保持一致。

**测试**：`tests/functional/DamagePipelineP3Tests.cpp`
- `[Unit] DamagePipeline P3 - projectile system attaches snapshot`：owner 挂 `ActiveSkillsComponent`（`skill_id=1`，`allocated_points[150]=3`），断言 `component.caster == player`（owner）、`component.skill_id == 1u`，并断言存在 `stage==CritDamage && base_value≈0.45f` 的条件节点——证明专精点确实从 owner 读取。
- 修复前该断言为 `component.caster == projectile`（失败），修复后通过。

---

### B3 单目标快照路径忽略快照暴击/穿透（施法者死亡即失效）

**Finding**：快照分支已用 `snapshot.base_damage`（After Inc/More/Conversion，已烘焙），但暴击判定在施法者销毁后因 `attacker_stats==nullptr` 且无 payload 被整段跳过；存活时又用实时 `GetStatWithTags` / `attacker_stats->crit_damage` 与快照口径漂移。`DamageMitigationService::Apply` 的物理穿透同样实时查询，未消费 `snapshot.armor_pen`。

**修复**：
1. 暴击口径统一（`DamagePipeline.cpp` `ResolveSingleTarget` 暴击段，锚点 `const bool crit_from_snapshot = use_snapshot;` `:1296`）
   - 暴击率优先级：`snapshot.crit_chance` → `payload.crit_chance` → 实时。
   - 暴伤倍率：新增 `auto resolve_crit_damage = [&]()`（`:1309`），优先级 `snapshot.crit_damage` → `payload.crit_multiplier(>0)` → 实时 `crit_damage` → `DEFAULT_CRIT_MULT`。
   - `crit_available = crit_from_snapshot || payload || attacker_stats`（`:1319`）。
   - `is_simulation` 期望暴击分支同样使用：`crit_mult = 1 + chance*(resolve_crit_damage()+extra-1)`；实际暴击 `crit_mult = resolve_crit_damage()+extra_crit_mult`。`crit_damage` 与 `extra_crit_mult` 叠加语义不变。
2. 攻方 accuracy：`ResolveDefenseResolution`（`DamagePipeline.cpp:122`）新增可选 `float attacker_accuracy_override = -1.0f`（`:143-144`）；调用处（`:831`）传 `use_snapshot ? snapshot_component->snapshot.accuracy : -1.0f`。
3. 穿透覆盖：`DamageMitigationService::Apply`（`DamageMitigationService.hpp:20`）新增 `float armor_pen_override = -1.0f`；实现（`DamageMitigationService.cpp:246-249`）`>=0` 用覆盖值，否则回退实时查询。管线单目标快照路径经 `snapshot_armor_pen`（`DamagePipeline.cpp:1357-1364`）传入。`:258` 注释更新为「已由单目标快照路径正常消费，不再承载布尔语义」。
   - 其余 `Apply(` 调用方（`tests/unit/DamageMitigationServiceTests.cpp`、`MindBladeNodes.cpp`、`InfiniteBladesNodes.cpp`、`BladeFormationNodes.cpp`、`BladeBoomerangElementPathTests.cpp`）依赖默认参 `-1.0f`，语义不变。

**测试**：`[Unit] DamagePipeline P3 - snapshot crit and armor pen survive caster death`
- owner（attacker 语义为 owner，source_entity 为投射物）挂 `DamageSnapshotComponent`（`crit_chance=1.0`、`crit_damage=2.0`、`armor_pen=50`），**先销毁 owner**，再经 `DamagePipeline::Calculate` 结算。
- 断言必暴且 `crit_pen.total_damage == Approx(2.0 * no_crit_pen.total_damage)`；防御方 `armor=100` 时 `no_crit_pen.total_damage > no_crit_no_pen.total_damage`（穿透生效）。
- 非快照路径行为不变（现有测试全绿；`CalculateBatch` 本就使用 `snap.armor_pen` / `snap.crit_*`）。

---

### B4 无攻击者实体的 base_pool 伤害被乘区清零

**Finding**：`DamagePipeline.cpp` 无条件 `GetStatWithTags(registry, attacker, dmg_stat, ...)`；`attacker==entt::null`（环境/陷阱/落石/亡魂，如 `HazardSystem::DealAreaDamage`）返回 0 → `inst.amount *= 0/100` → 全部归零。`CombatDamagePipelineCutoverTests.cpp` 还把 0 锁成断言。

**修复**：`DamagePipeline.cpp:1169` 条件查询
```cpp
float multiplier_pct = 100.0f;
if (registry.valid(attacker) && registry.all_of<CombatStats>(attacker)) {
  multiplier_pct = StatsSystem::GetStatWithTags(registry, attacker, dmg_stat, instance_tags, request.skill_id, source_entity);
}
```
payload `increased_damage` 叠加逻辑不变（`inst.amount *= pct/100` 之前先叠加）。审计同类无攻方分支（`is_simulation`、thorns、shadow）：`is_simulation` 走同一乘区代码；thorns/shadow 均要求 attacker 有效，不受影响，未引入新的 0 基线。

**测试**：`tests/integration/CombatDamagePipelineCutoverTests.cpp`
- 两处断言由 0 改为 `doctest::Approx(100.0f)`，注释「无攻方 = 100% 基础乘区，不再归零」。
- NaN 用例（base_pool NaN）保持 `Approx(0.0f)`（NaN>0 为假，不入实例）。
- 新增 `[Integration] CombatDamagePipelineCutover - HazardEnvironment keeps base pool without attacker`：`origin=HazardEnvironment`、`attacker=entt::null`、`base_pool=100` → `total_damage == Approx(100.0f)`。
- 排查其它受影响测试：无（全量 1446 cases 绿）。

---

## 2. 建议项

### S2 废除 IsValidConversion 单向限制

**Finding**：`CombatConstants.hpp` 的 `CONVERSION_ORDER={0,3,2,1,4,5}` / `IsValidConversion` 与设计 §D2「无级联、顺序无关、互转确定性」矛盾，还在 `AddConversionRule` 丢弃合法逆向转换（火转冰、电转物等）。

**修复**：
- 删除 `IsValidConversion` 与 `CONVERSION_ORDER`；保留 `MAX_CONVERSION_DEPTH` 并注释说明单向限制已废除（`CombatConstants.hpp:50`）。
- `AddConversionRule` 移除非法方向告警/丢弃，仅保留 `source_pool == target_pool` 自转静默跳过。
- rg 确认无其它引用（仅 `DamagePipelineConversionTest.cpp` 一处陈旧注释，已一并更新）。

**测试**：
- `tests/unit/DamageConversionTypesTests.cpp` 新增逆序用例。
- `tests/functional/DamagePipelineConversionTest.cpp` 的对应 SUBCASE 重命名为 `Bidirectional Conversion (S2)`，断言 Fire→Cold 实际发生（`fire=0, cold=100`）。

### S3 快照命中跳过无效预计算

**Finding**：快照检测原本在 base_pool 实例、武器点伤、专精树遍历、转换计算之后，`instances.clear()` 全部丢弃，浪费高频路径。

**修复**：`DamagePipeline.cpp` 将 `use_snapshot` 检测前移到 `attacker_stats/defender_stats` 获取之后、`ResolveDefenseResolution` 之前（锚点 `const bool use_snapshot = ...` `:826`）。随后：
- 快照填充前移（`:866-882`），沿用原有标签合并语义（base 池位标签 | `snapshot.hit_tags` | `combined_hit_tags`）。
- Step 1 base_pool、Step 2 武器/技能点伤与专精、Step 3 效能、Step 4 转换收集全部以 `!use_snapshot` 短路（`:883`、`:906`、`:963`、`:1010`、`:1063`）。
- `combined_hit_tags`、`skip_mitigation`、`is_simulation` 行为不变。
- 快照路径不再有「先算后清」的浪费。

**性能**（`review-fix-perf.log`，`[Performance]*DamagePipeline*`，5 cases / 4023 assertions 全绿）：
- Single Calculate：Mean=0.000ms，P99=0.001ms（Target < 0.01ms）。
- Batch 200：Mean=0.052ms，P99=0.077ms（Target < 1.0ms）。
- Batch Scaling 50/100/200/500：Mean 0.029 / 0.035 / 0.049 / 0.085ms。
- Adaptive Dispatch：小规模 0.001–0.002ms；50/200/500 = 0.029 / 0.049 / 0.097ms。

与修复前基线 `phase-P4/P4-gate-perf-damage.log` 对比（Mean/P99，修复前 → 修复后）：

| 项目 | 修复前 | 修复后 |
|---|---|---|
| Single Calculate | 0.000 / 0.001ms | 0.000 / 0.001ms（持平） |
| Batch 200 | 0.059 / 0.078ms | 0.052 / 0.077ms（略优） |
| Batch Scaling 50 | 0.028 / 0.037ms | 0.029 / 0.038ms（持平） |
| Batch Scaling 100 | 0.040 / 0.077ms | 0.035 / 0.043ms（优） |
| Batch Scaling 200 | 0.056 / 0.154ms | 0.049 / 0.062ms（优） |
| Batch Scaling 500 | 0.104 / 0.277ms | 0.085 / 0.112ms（优） |
| Adaptive 500 | 0.109 / 0.285ms | 0.097 / 0.160ms（优） |

结论：无劣化，快照短路径的无效预计算已消除。

### S4 次级打击不得注入技能固有伤害

**Finding**：`payload_supplies_base` 让 `SecondaryProc` 携带 payload 时也进入 `skill_data->base_damage + payload_base*weapon_damage_mult` 分支，违反设计 D6「唯有 DirectSkillCast 允许注入武器与技能配置点伤；次级打击一律仅用调用方提供的基础点伤」。

**修复**：`DamagePipeline.cpp` 技能固有伤害与武器乘区仅在 `request.origin == DamageOrigin::DirectSkillCast` 时附加（`:906-907`）；`SecondaryProc` 带 payload base 时走纯点伤实例分支（`:935-961`），元素类型取 payload / 现有主元素推导，不再乘 `weapon_damage_mult`、不再加 `skill.base_damage`。payload 覆盖 min/max 的逻辑保留。

**SecondaryProc + payload 调用点审计**（`assets`/`src`）：`SummonCombatBridge.cpp`、`SkillSystem.cpp`、`ProjectileSystem.cpp`、`OrbitingSentinelDeliverySystem.cpp`、`ElementPathSystem.cpp`、`RendingWave.cpp`、`InfiniteBlades.cpp`、`FlowingThrust.cpp`、`HeavenlySwordDescent.cpp`、`BladeFormation.cpp`、`BeamChannelDeliverySystem.cpp`。审计结论：这些调用点原本依赖「payload 覆盖 min/max + 技能/武器叠加」，按 D6 现改为纯调用方点伤；无测试回归（全量绿），数值变化见第 4 节。

**测试**：`tests/functional/DamagePipelineOriginTests.cpp`（新增）
- `[Functional] DamagePipeline - SecondaryProc payload skips skill base and weapon mult`：技能 5（`base_damage=40`、`weapon_damage_mult=1.0`）、payload base 100。
  - `DirectSkillCast` → `40 + 100*1.0 = 140`。
  - `SecondaryProc` → `100`（不含技能 base / 武器乘区）。
  - 断言 `secondary < direct`。

---

## 3. Nits

### N3 ElementTagOf 统一（并入 B1）
公共 `damage::ElementTagOf` 已提升至 `DamageTypes.hpp:100`，`DamageMitigationService.cpp` 内部副本删除并改调公共版（`:48`、`:186`、`:218`），无重复实现。

### N2 延迟动作队列 LIFO → FIFO
**Finding**：`DamagePipeline.cpp` 原用 `back()+pop_back()`（LIFO）。
**修复**：改为 `thread_local std::deque<DeferredCombatAction>`（`:472`），`Flush` 取 `front()+pop_front()`（`:495-496`），入队 `push_back`（`:528`）；注释说明因果链按入队顺序结算。
**测试**：全量套件无任何测试/业务依赖 LIFO 顺序（全量 1446 cases 绿），未触发「停下报告」条件。

### N1（延期，不修）
`DamageConditions.cpp:19` 的 `id.find("Slow")` 字符串热路径匹配。理由：依赖历史数据 id，规范化到 `BuffType::SpeedDown` 属数据治理范畴，且已有 typed 检查在前，收益低、风险高，列入延期跟踪。

---

## 4. 行为变化清单

| 编号 | 变化 | 影响面 | 说明 |
|---|---|---|---|
| B1 | Shadow/Poison 转换按池序契约修正 | 所有元素转换 | `ElementTagOf` 保证 `Tag::Shadow=1<<4`、`Tag::Poison=1<<5`；外部按真实 `DamageType` 调用不再错位 |
| B2 | 投射物快照 attacker = owner | 投射物专精条件 | 172 More / 150 / 512 等专精节点恢复生效 |
| B3 | 快照暴击 / 暴伤 / 穿透在施法者销毁后仍生效 | 投射物/持续伤害结算 | 快照路径口径与 batch 一致；`snapshot.accuracy` 亦已消费 |
| B4 | 无攻方环境/陷阱伤害恢复全额 | HazardEnvironment 等 | 由 0 恢复为 `base_pool × 100%` |
| S2 | 逆向元素转换放行 | 火↔冰、电→物等 | 单遍、顺序无关、互转确定 |
| S4 | 次级打击不再叠加技能 base / 武器乘区 | SecondaryProc + payload | 例：技能 5 base=40、payload=100、wmult=1.0 时，由 140 变为 100 |
| N2 | 延迟队列 FIFO | 延迟动作结算顺序 | 因果链按入队顺序 |

---

## 5. 延期项

| 编号 | 内容 | 理由 | 建议落点 |
|---|---|---|---|
| S1 | 生产 AoE 接入 SIMD 批量接口（`ResolveDamageBatch` → `AreaFieldDeliverySystem` 等） | 依赖先补齐 `CalculateBatch` 与单路语义等价（invuln / FrostAmp / `source_entity` 快照，即 M4 未决项），属新阶段工作 | P5 候选，前置 M4 语义对齐 |
| N1 | `id.find("Slow")` 字符串匹配规范化到 `BuffType::SpeedDown` | 依赖历史数据 id，属数据治理，收益低风险高 | 数据治理阶段随条件数据迁移 |

---

## 6. 残留风险

1. **`[Integration] SkillSystem - BladeWard 470 counter on Melee and Block`（`tests/integration/SkillSystemTests.cpp:1263`）存在顺序/状态相关偶发失败**：单独运行通过（2 assertions），全量直跑第二次 1446/1446 全绿；首次直跑出现一次失败。与本次修复无因果关系（全局 `SkillSystem` hook 状态），列为既有 flaky 项，建议后续隔离 hook 生命周期。
2. **S4 数值变化未见测试回归，但属真实平衡改动**：`SecondaryProc + payload` 的伤害下调为纯调用方点伤，需在策划侧确认符合 D6 预期。
3. **`CalculateBatch` 的 `CreateSnapshot` 不传 payload**：既有行为，属 M4 未决项，本次未改。
4. **S3 性能结论的基线为跨时间日志**：采用既有 `P4-gate-perf-damage.log`（修复前 gate 运行）与本次 `review-fix-perf.log` 对比，非同一进程交替双跑；结论为「无劣化且多数场景更优」。
5. **B3 的 accuracy**：仅在 `ResolveDefenseResolution` 层接入快照覆盖；若后续有其它攻方命中判定入口仍需逐一核对。

---

## 7. 验证命令与结果汇总

| 命令 | 结果 |
|---|---|
| `build.bat` | exit=0（`review-fix-build.log` / `review-fix-build2.log` / `review-fix-build3.log`） |
| `ctest --test-dir build -C RelWithDebInfo -L ci` | 1/1 passed（`review-fix-ctest-ci.log`） |
| `ctest --test-dir build -C RelWithDebInfo -L integration` | 6/6 passed：integration / progression / ui / combat / skill / ai（`review-fix-ctest-integration.log`） |
| `bin\NoMoreDayTests.exe --test-case=*DamagePipeline*,*DamageConversion*,*CombatDamage*,*Projectile*,*DamageElementIndex*,*DamageMitigation*` | 68 cases / 6292 assertions，0 failed（`review-fix-posttest.log`） |
| 全量 `--test-case-exclude=*Performance*,*GPU-Diagnostic*` | 1446 cases / 109699 assertions，0 failed（`review-fix-ci-direct2.log`） |
| `bin\NoMoreDayTests.exe --test-case=[Performance]*DamagePipeline*` | 5 cases / 4023 assertions 全绿（`review-fix-perf.log`） |

新增文件：
- `tests/unit/DamageConversionTypesTests.cpp`（B1/N3/S2）
- `tests/functional/DamagePipelineOriginTests.cpp`（S4）

修改测试：
- `tests/functional/DamagePipelineP3Tests.cpp`（B2/B3）
- `tests/functional/DamagePipelineConversionTest.cpp`（S2）
- `tests/integration/CombatDamagePipelineCutoverTests.cpp`（B4）
