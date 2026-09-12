# P4 第三轮审查修复报告（damage pipeline modernization）

- 范围: 独立审查对 P0–P4 未提交变更提出的 2 个建议修复（1、2）+ 3 个防御项（3、4、5）。
- 环境: Windows + MSVC（VS 2026 generator），`RelWithDebInfo`，`build.bat`。
- 约束: 未 commit；未改动无关内容；注释中文、标识符/日志英文；`ctest` 均带 `-C RelWithDebInfo`。
- 本轮新增源码改动均使用「函数名 + 代码锚点」定位（相对行号可能随后续改动漂移）。
- 证据日志（同目录）:
  - `review3-build.log`（最终 build.bat，exit=0）
  - `review3-ctest-ci.log` / `review3-ctest-ci-rerun.log`（`-L ci` 连跑 2 次）
  - `review3-ctest-integration.log`（`-L integration`）
  - `review3-targeted.log`（P3 归因、P4 事件标签、守恒、payload、EventConsistency、BladeWard）
  - `review3-bladeward-loop15.log`（flaky 用例单跑 x15）
  - `review3-perf-damagepipeline.log`（`[Performance] DamagePipeline*`）

---

## 1.（🟡）ProjectileSystem 命中结算 attacker 回退倒置

### 根因
`src/game/systems/skill/ProjectileSystem.cpp` 处理 deferred `Damage` 动作时，攻击者选择为
`valid(projEnt) && all_of<CombatStats>(projEnt) ? projEnt : act.instigator`。投射物普遍带复制来的
`CombatStats`，故 attacker 恒为投射物自身；一旦快照未挂载/被跳过（`thorns_like_damage`、测试构造等），
管线用投射物查 `ActiveSkillsComponent`/`GlobalModifierComponent`，专精点与条件 More 归零。与 B2 已修的
同文件 attach 分支（`attacker = p->owner`）语义倒置。

### 修复
- 文件: `src/game/systems/skill/ProjectileSystem.cpp`，函数 `ProjectileSystem::Update` 内 deferred
  `Damage` 分支（代码锚点: `Tag hit_tags = Tag::Projectile | Tag::Hit;` 之后）。
- 改为优先施法者、失效时回退投射物:
  `(registry.valid(act.instigator) && registry.all_of<CombatStats>(act.instigator)) ? act.instigator : projEnt`。
- `request.source_entity = projEnt` 保持不变：快照仍经 `source_entity` 查找，attacker 归因 / dodge /
  endgame 使用真实施法者。

### 测试与等效回归说明
- 新增 `tests/functional/DamagePipelineP3Tests.cpp` →
  `[Unit] DamagePipeline P3 - hit attacker attribution prefers owner`（2 断言）:
  owner 有 `CombatStats.flat_damage[Physical]=50`、投射物有 `CombatStats`（flat=0），`request.source_entity`
  指向无快照的投射物；断言 owner 归因造成 50 伤害、投射物归因为 0。
- **直接 Update 级回归不可行的原因（已核实并记录）**: `ProjectileSystem::Update` 的串行预扫描对任何
  `owner` 拥有 `CombatStats` 的投射物都会先 `AttachSnapshotComponent`，且伤害在同一 Update 内处理，
  因此不存在「owner 有 CombatStats 但无快照」的稳定构造路径（即旧代码的回退分支无法经 Update 触发）。
  故采用上述等效 Calculate 回归锁定「owner 归因 vs 投射物归因」语义差异；真实 Update 归因链由既有
  `[Unit] DamagePipeline P3 - projectile system attaches snapshot` 与
  `[Unit] EventConsistency - single execute uses owner attribution for projectile events` 覆盖。

---

## 2.（🟡）元素转换后战斗事件标签未重映

### 根因
`DamagePipeline::Calculate` 的 `combined_hit_tags` 与 `DamagePipeline::Execute` 重新合成的标签用于
`DispatchSingleDamageEvents`；Step 4 转换后实例已按 `TransformTagsOnConversion` 改为目标元素，但事件
标签仍是转换前元素（100% 物转火后事件仍带 `Physical` 无 `Fire`），Proc / 异常判定脱节。

### 修复
1. `src/game/contracts/DamagePipelineTypes.hpp`：`DamageResult` 在 `final_pool` 后新增
   `Tag resolved_element_tags = Tag::None;`（结算时实际参与元素的类型标签）。
2. `src/game/systems/combat/damage/DamageSnapshot.hpp`：`AttackerSnapshot` 新增
   `Tag resolved_element_tags = Tag::None;`，供批量路径复用。
3. `src/game/systems/combat/DamagePipeline.cpp` 匿名命名空间新增
   `ResolveEventElementTags(combined_hit_tags, resolved_element_tags, payload_element_tags)`（代码锚点:
   More 累加器注释之前）:
   - `resolved_element_tags == Tag::None` → 返回原 `combined_hit_tags`；
   - 否则用 `(combined & ~kAllDamageTypeTags) | resolved` 替换元素位，保留 `Melee/Projectile/Hit/
     DamageOverTime` 等非元素位；再 `| (payload_element_tags & kAllDamageTypeTags)` 保留 payload 显式声明
     的元素（见下方「审查中额外发现」）。
4. `Calculate` 实例循环（代码锚点: `std::countr_zero(...)` 取 `type_idx` 之后、`DamageMitigationService::Apply`
   之前）: 对 `inst.amount > 0.0f` 的实例累计 `result.resolved_element_tags |= inst.final_type;`
   （选择在减免前累计，保证实际进入结算的实例都被计入）。
5. 派发处统一重映（`Calculate` 与 `Execute` 均使用 `ResolveEventElementTags`）；`CalculateBatch` 内部
   event 块同样重映，7 个事件工厂（`CreateSkillHit/MeleeHit/ProjectileHit/AreaHit/DealDamage/TakeDamage/
   OnCrit`）全部改用重映后的 `event_tags`；`BladeWard`/counter/防御结算等机制判定仍用原 `combined_tags`。
6. `CreateSnapshot` 将仿真 `Calculate` 的 `res.resolved_element_tags` 写入 `snap.resolved_element_tags`，
   使批量路径事件标签与实际结算元素一致。
7. 审计 `DispatchSingleDamageEvents` 调用点：仅 `Calculate`、`Execute` 两处（`CalculateBatch` 自身实现
   事件派发但不调用该函数），无遗漏。

### 审查中额外发现并修复的真实回归（重要）
首轮全量 `-L ci` 出现 1 例失败：`tests/unit/SkillSpecializationBakerTests.cpp` →
`[Unit] FlowingThrust - 133 Swap Explosion Damage Payload`，证据 `capturedTags=4310761473 hasFire=false`
（4310761473 = `Physical|Melee|bits20-23|Hit`），Ignite/FrostSlow 未生成。
根因: 该技能通过 payload 显式声明 Fire/Cold（专精 170/172），而伤害本体是 Physical；最初的 2 参重映把
**所有**元素位替换为 resolved，抹掉了 payload 元素位。已扩展 `ResolveEventElementTags` 第三参保留 payload
元素位后通过。

### 测试与差异化证据
`tests/functional/DamagePipelineConversionTest.cpp` 新增 3 个用例（共 21 断言）:
1. `[Functional] DamagePipeline P4 - event tags follow resolved element`（12 断言，两 SUBCASE）:
   - 100% Physical→Fire: `resolved_element_tags == Fire`，事件标签含 `Fire`、**不含** `Physical`，且保留
     `Hit`/`Melee`；
   - 50% 部分转换: `resolved == Physical|Fire`，事件标签同时含两元素并保留 `Hit`/`Melee`。
   - 差异化: 修复前 `combined_hit_tags` 无元素位（本用例 skill_id=0），事件无 `Fire` → 该断言必失败。
2. `[Functional] DamagePipeline P4 - explicit payload element tags survive remap`（6 断言）:
   payload `effective_tags=Fire` + 伤害 `Physical`，断言事件同时含 `Physical`（实际伤害）与 `Fire`
   （payload 异常），锁定 Baker 契约。
3. `[Functional] DamagePipeline P4 - final pool conservation under suppressor`（见第 3 项）。

现有监听器期望核对: `tests/unit/EventConsistencyTests.cpp` 仅断言伤害数值、不断言元素标签，无需同步；
全量 `-L ci` 通过。

---

## 3.（💭）终态倍率硬编码 6 而非池容量 16

### 根因
`DamagePipeline::Calculate` 终态倍率（suppressor、frost amp）循环 `i < 6`，而 `final_pool` 允许
`type_idx < 16`（`DAMAGE_POOL_SIZE=16`，`Tag::Void` 位 6）。Void 伤害时 `total_damage` 被乘、
`final_pool.values[6]` 未乘 → `total_damage != sum(final_pool)`。

### 修复
- 两处改为 `for (int i = 0; i < DAMAGE_POOL_SIZE; ++i)`（代码锚点: `total_final_damage *= suppressor_multiplier`
  之后的池缩放循环，及 `ResolveFrostAmpMultiplier` 之后的池缩放循环）。
- 同函数全池缩放审计: 修复后文件内 `i < 6` 不再存在；其余 `final_pool`/`ELEMENTAL_TYPE_COUNT` 循环均为
  「元素数」语义（基池写入、逐元素循环、`CreateSnapshot` 与 `CalculateBatch` 标量/SIMD 内核遍历
  `snap.base_damage` 的 6 元素），非守恒缺口；`BladeWard` 拦截用 `final_pool.Clear()`，守恒不变。

### 测试
`[Functional] DamagePipeline P4 - final pool conservation under suppressor`（3 断言）:
守方 `SuppressorComponent{threshold=10, damageReduction=0.5}`，攻守距离 100（触发），`base.values[6]=100`
（Void），`skip_mitigation=true`；断言 `total_damage == 50`（证明 0.5 倍率确已施加）、
`final_pool.values[6] == total_damage` 且 `sum(final_pool) == total_damage`。
- 差异化: 修复前 `values[6]` 不参与缩放 → `sum=100 != total=50`，断言失败。

---

## 4.（💭）SkillSystemTests flaky 夹具隔离

### 根因
`tests/integration/SkillSystemTests.cpp` 的 `TEST_CASE("[Integration] SkillSystem - BladeWard 470 counter on
Melee and Block")`（其后 `:1267` 用例显式 `SkillSystem::ShutdownHooks(); InitHooks();`）缺少夹具隔离，
受全局 Hook/单例污染，全量下偶发失败。

### 修复
- 在该用例开头补齐与后续用例一致的隔离:
  `TestSetupScope scope;` + `SkillSystem::ShutdownHooks(); SkillSystem::InitHooks();`。
- 断言语义未改动。

### 验证
- 用例单独循环运行 15 次: `review3-bladeward-loop15.log`，`run 1..15 exit=0`（0 失败）。
- `ctest -C RelWithDebInfo -L ci` 连跑 2 次全绿（7.70s / 7.92s）。

---

## 5.（💭）CalculateBatch 防误用标记

### 修复
- `src/game/systems/combat/DamagePipeline.hpp`：`CalculateBatch` 声明前加
  `[[deprecated("Use DamageResolutionHooks::ResolveDamageBatch in production; CalculateBatch is test/benchmark only")]]`
  + 中文注释。
- `CMakeLists.txt:98` 已 `/wd4996`，编译无告警；未改行为、未改测试调用。
- 核查: `rg` 确认 `src/` 内无 `CalculateBatch` 生产调用方（仅测试/基准），无冲突。

---

## 行为变化

1. **投射物命中 attacker 归因**: 有施法者时 attacker 归因为 owner（真实施法者），投射物仅在施法者
   不可用时兜底。影响 `ActiveSkillsComponent`/`GlobalModifierComponent` 查找、dodge 与 endgame 归因；
   快照查找仍走 `source_entity`，不影响快照命中数值。
2. **事件元素标签按实际结算元素**: `SkillHit/MeleeHit/ProjectileHit/AreaHit/DealDamage/TakeDamage/OnCrit`
   的元素位改为转换后实际结算元素，非元素动作/机制位与 payload 显式元素位保留。下游 Proc / 异常判定与
   实际伤害元素保持一致。
3. **Void（及任何池索引 ≥6）守恒**: suppressor / frost amp 覆盖全池，`total_damage == sum(final_pool)`。
4. `CalculateBatch` 标记 deprecated（仅编译期提示，运行行为不变）。

## 残留风险

- 第 1 项的 Update 级直接回归不可构造（原因见上），当前由等效 Calculate 回归 + 既有投射物归因测试覆盖；
  若未来修改 `ProjectileSystem::Update` 的快照挂载时机，需同步复核该归因分支。
- `resolved_element_tags` 在减免前累计；若未来引入跨元素转换或减免会改变元素属性，需重新审视重映语义。
- 重映只替换「元素类型位」，依赖 `kAllDamageTypeTags` 覆盖全部元素位；新增伤害类型标签时必须同步更新
  该掩码，否则新位会被当作非元素位保留。
- 性能数字为 `RelWithDebInfo` 下采集，仅供相对对照，非 Release 基线。

## 命令与结果

| 命令 | 结果 |
| --- | --- |
| `build.bat` | exit=0，All steps completed successfully |
| `ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure` | 100% tests passed, 0 failed / 1（7.70s） |
| 同上（连跑第二次） | 100% tests passed, 0 failed / 1（7.92s） |
| `ctest --test-dir build -C RelWithDebInfo -L integration --output-on-failure` | 100% tests passed, 0 failed / 6（4.90s） |
| P3 归因 / P4 事件标签 / 守恒 / payload / Baker / EventConsistency | 全部 pass（详见 `review3-targeted.log`） |
| BladeWard 470 单用例 x15 | 15/15 exit=0 |
| `[Performance] DamagePipeline - Single Calculate` | Mean=0.000ms, P99=0.001ms（目标 <0.01ms） |
| `[Performance] DamagePipeline - CalculateBatch 200 Targets` | Mean=0.053ms, P99=0.107ms（目标 <1.0ms） |
| `[Performance] DamagePipeline - Batch Scaling` | 50→0.027/0.038ms；100→0.041/0.146ms；200→0.051/0.097ms；500→0.112/0.273ms（Mean/P99） |
| `[Performance] DamagePipeline - Adaptive Dispatch Scaling` | 1–8→约 0.001–0.002ms；50→0.027/0.041ms；200→0.065/0.300ms；500→0.127/0.315ms（Mean/P99） |

## 未 commit 声明

本轮仅修改源码与测试并生成报告/日志，未执行任何 git commit。
