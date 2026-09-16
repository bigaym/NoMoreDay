# UMR 统一修饰器框架完善与工程加固 — 实施计划

- **状态**：计划制定完成（评审修订版 v2）/ 待执行
- **文档路径**：`docs/plans/2026-09-16-umr-framework-completion-and-hardening-plan.md`
- **日期**：2026-09-16
- **设计基线**：`docs/designs/2026-09-16-umr-framework-completion-and-hardening-design.md`（v2）
- **修订说明**：v2 相对 v1 有三处结构性调整——(1) 热循环短路前新增"词缀数据单源化"前置任务；(2) 地图战斗属性挂接新增 `AttributePipeline` 改造任务，否则映射不生效；(3) 移除 Schema V2 constraints 仲裁全部任务（无数据可消费、排序无效），改列前置条件。
- **范围**：
  1. 存档安全：`ItemPersistenceCodec.cpp` Section 3 编码侧补齐 `kMaxModifierRecordIdsPerItem = 64` 上限校验。
  2. 量纲归一化：`MapModifierAdapter.cpp` 注入求值器时对 `affix.value` 施加 `* 0.01f` 缩放，并**修正**固化了 Bug 的测试断言。
  3. 构建图闭环：根目录 `CMakeLists.txt` 接入 `GenerateModifierRuntime` 自定义命令，覆盖 `canonical/` 子目录，并明确与 `build.bat` 的职责边界。
  4. 词缀数据单源化：补全 `kAffixData`（`Vortex`/`Entangler`）+ 编译期对齐自检 + `AffixFlags` 与 UMR 行为算子表交叉校验。
  5. 热循环短路：前置任务完成后，`MonsterAffixSystem.hpp` 使用 `affix.hasUpdate` 快速跳过无 Update 行为怪物。
  6. 技能魔耗消费端：`EquipmentModifierAdapter` 提供 `GetEquippedManaCostMultiplier`，在 `SkillSystem` 与 UI 预览中消费。
  7. 地图战斗属性映射（含 `AttributePipeline` 改造）：暴击 + 全抗 + 六系抗性按"百分点"语义接入 `mapEnemyDelta`；平坦词缀与 `Enemy_CritResist` 推迟。
  8. 生成器断言：生成脚本校验单记录内同目标属性最多一条乘算算子（先核验存量数据）。
- **本轮不做**：Schema V2 `constraints` 仲裁（见设计 §2.7）；`MapModifierAdapter` 每调用堆分配优化。

---

## 1. 实施思路与原理

### 1.1 总体策略
按照**"底座安全与稳定性先行 (P0) -> 构建纪律 (P0) -> 数据单源化与热路径 (P1) -> 核心算子消费闭环 (P1) -> 数据语义挂接 (P1) -> 生成器防御 (P2) -> 全量回归验证 (P0)"**的节奏推进，所有改动均为原子任务，确保任意中间状态均可独立编译与通过测试。

1. **Section 3 编码防御**：在 `ItemPersistenceCodec::encode`（`src/game/systems/item/storage/ItemPersistenceCodec.cpp:466`）计算完 `allowedPtr`（`:510`）后、`processSection` 调用（`:512`）**之前**，遍历 side tables 并跳过 `allowedPtr` 未包含的条目做 fail-closed 预校验：任一 `modifier_record_ids.size() > kMaxModifierRecordIdsPerItem` 即打 `LOG_ERROR` 并 `return false`。不能在 `buildItemSideTablesSection` 内 `return {}`——`processSection`（`:485-498`）仍会登记该分段、写出循环（`:567-572`）仅跳过零长度 payload，结果只是静默丢弃整个 ItemSideTables 分段，既不失败也无法定位；也不能做无差别全仓库校验，否则未被当前脏掩码触及的坏旁表会永久卡死所有分段存盘。
2. **地图数值量纲归一化**：生成器在生成 JSON 模板时已经做过 `/ 100.0`，运行时 `MapModifierAdapter` 覆盖模板时漏乘了 `0.01f`，只需在 `requests.push_back` 处补上 `affix.value * 0.01f`。配套单测断言必须改用归一化后的真实强度（`CalculateValue(type,1) * 0.01f`），而不是把旧断言调成一个能过的新数字。仅适用于百分比语义词缀；平坦词缀 `Enemy_Armor` / `Enemy_Dodge` / `Enemy_ArmorShred` 本批次不挂接。
3. **CMake 自定义命令**：在 `CMakeLists.txt` 中声明 `GenerateModifierRuntime`，以 `modifier_v2/**/*.json`（含 `canonical/`）为输入依赖，输出 `modifier_runtime_v2.bin`，使 `NoMoreDayGameModifier` 成为直接下游。**不改动 `build.bat`**：`build.bat:307/313` 仍是唯一的漂移门禁与生成入口，CMake 仅在产物缺失/过期时兜底生成。
4. **词缀数据单源化**：`MonsterAffixRegistry.hpp:177-179` 的 `kAffixData` 按枚举位置初始化，但定义列表缺失 `Vortex`/`Entangler`，导致 `Avenger` 起所有词缀错移两位。先补全定义并加 `static_assert` 对齐自检，再把 `AffixFlags` 与生成器 `MONSTER_*_BEHAVIOR_OPS` 对齐（以算子表为唯一事实），并加生成期双向交叉校验。前置任务完成前**不得**启用短路。
5. **怪物行为热循环短路**：在每帧 `Update` 中提前过滤无 Update 行为的怪物，消除每帧无谓的 `EvaluateBehaviorOps` 调用。该任务仅降低开销，不改变行为分支；`mirrorCooldown`/`timers` 本就位于 `HasOnUpdate()` 之后。
6. **技能魔耗消费端**：在 `EquipmentModifierAdapter` 中基于 `CollectEquippedRecordIds` 与 `skillId` 计算魔耗倍率，在 `SkillSystem.cpp:2050` 计算 `base_cost` 时乘入；UI 预览需插在 `:24-36` 赋值之后、`:39-42` 提前返回之前。
7. **地图战斗属性挂接**：`AttributePipeline.cpp` 目前仅用 `mapEnemyDelta` 计算最大生命/移速/普攻伤害，抗性与暴击完全未接入。必须新增 `MapPercentPointsBonus` 辅助函数并在敌方分支接入，映射改动才会真正生效。
8. **生成器断言**：单记录同 `target_stat` 最多 1 个 `ADD_STAT_PERCENT_MULT`，先对存量 JSON 核验。

---

## 2. 伪代码与接口草图

### 2.1 存储编码对称校验（`ItemPersistenceCodec.cpp`）
```cpp
// ItemPersistenceCodec::encode：allowedPtr 计算完成后（:510）、processSection 之前（:512）
for (const auto &[idx, data] : service.getStore().getAllSideTables()) {
  if (allowedPtr != nullptr && !allowedPtr->contains(idx)) {
    continue; // 仅校验本次会写出的旁表
  }
  if (data.modifier_record_ids.size() > kMaxModifierRecordIdsPerItem) {
    LOG_ERROR(
        "ItemPersistenceCodec: side-table {} has {} modifier_record_ids, exceeding max limit {}",
        idx, data.modifier_record_ids.size(), kMaxModifierRecordIdsPerItem);
    return false; // fail-closed：拒绝写入，保留旧存档
  }
}
// buildItemSideTablesSection 保持原样：appendBytes(payload, recordIdCount) 后逐个写入
```

### 2.2 地图滚值归一化（`MapModifierAdapter.cpp`）
```cpp
// MapModifierAdapter.cpp: EvaluateEnemyAffixDelta
const float normalizedValue = affix.value * 0.01f;
requests.push_back({recordId, true, normalizedValue, static_cast<uint32_t>(combatStat)});
```

### 2.3 词缀数据对齐自检（`MonsterAffixRegistry.hpp`）
```cpp
// 在 kAffixData 之后
static constexpr bool ValidateAffixDataIds() {
  for (size_t i = 0; i < kAffixData.size(); ++i)
    if (static_cast<size_t>(kAffixData[i].id) != i) return false;
  return true;
}
static_assert(ValidateAffixDataIds(),
              "kAffixData must be positionally aligned with MonsterAffixType");
// 并在 Berserker 与 Avenger 之间补入 Vortex(hasUpdate) / Entangler(hasOnHit) 定义
```

### 2.4 怪物行为快速短路（`MonsterAffixSystem.hpp`）
```cpp
for (auto entity : view) {
  auto &affix = view.get<MonsterAffixComponent>(entity);
  // 前置：hasUpdate 已与 UMR 行为算子表对齐（见 Task 3.1/3.2/3.3）
  if (!affix.hasUpdate) {
    continue;
  }
  const auto behaviorOps = MonsterModifierAdapter::EvaluateBehaviorOps(affix);
  if (!behaviorOps.HasOnUpdate()) {
    continue;
  }
  // 正常执行行为逻辑...
}
```

### 2.5 技能魔耗消费端（`EquipmentModifierAdapter.hpp / .cpp` 与 `SkillSystem.cpp`）
```cpp
// EquipmentModifierAdapter.hpp
[[nodiscard]] static float GetEquippedManaCostMultiplier(
    const entt::registry &registry, entt::entity entity,
    uint32_t skillId, Tag skillTags) {
  const auto recordIds = CollectEquippedRecordIds(registry, entity);
  if (recordIds.empty()) return 1.0f;
  const auto ctx = BuildContextFromCharacter(registry, entity, skillId, skillTags);
  // 三参重载：不存在 (registry, span<uint32_t>, ctx, category) 的四参重载
  const auto delta = ModifierEvaluator::Evaluate(
      ModifierRuntimeRegistry::Get(),
      std::span<const uint32_t>(recordIds.data(), recordIds.size()),
      ctx);
  return delta.GetManaCostMultiplier(skillId);
}

// SkillSystem.cpp: ExecuteCast
float base_cost = raw_mana_cost * (1.0f - std::min(0.9f, rcr));
base_cost *= EquipmentModifierAdapter::GetEquippedManaCostMultiplier(
    registry, entity, slot.id, data->tags);

// SkillDisplayPreviewService.cpp：插在 :24-36 赋值之后、:39-42 提前返回之前
preview.display_mana_cost *= EquipmentModifierAdapter::GetEquippedManaCostMultiplier(
    registry, player, skillId, skillData->tags);
```

### 2.6 地图战斗属性接入（`AttributePipeline.cpp`）
```cpp
// 敌方地图词缀：把百分比 delta 解释为 ×100 刻度上的百分点增量
static float MapPercentPointsBonus(const ModifierDelta &delta, StatType stat) {
  const uint32_t key = static_cast<uint32_t>(stat);
  float points = 0.0f;
  if (auto it = delta.flat.find(key); it != delta.flat.end())
    points += it->second;
  if (auto it = delta.percent_add.find(key); it != delta.percent_add.end())
    points += it->second * 100.0f;
  if (auto it = delta.percent_mult.find(key); it != delta.percent_mult.end())
    points += (it->second - 1.0f) * 100.0f;
  return points;
}

// 敌方分支（:368-396）
calcs[static_cast<size_t>(StatType::CritChance)].base =
    DEFAULT_CRIT_CHANCE * 100.0f +
    MapPercentPointsBonus(mapEnemyDelta, StatType::CritChance);
calcs[static_cast<size_t>(StatType::ResistAll)].base =
    MapPercentPointsBonus(mapEnemyDelta, StatType::ResistAll);
auto applyRes = [&](Tag tag, StatType t) {
  float val = bonus + (HasTag(raceData.resistances, tag) ? NATIVE_RES : 0.0f)
            + MapPercentPointsBonus(mapEnemyDelta, t);
  if (val > 0.0f) ApplyStatModifier(calcs, t, ModifierMode::Flat, val);
};
```

---

## 3. 原子任务拆分（按序推进）

### Phase 1: 存档防御与数值量纲修复 (P0)
- [ ] **Task 1.1**: 在 `src/game/systems/item/storage/ItemPersistenceCodec.cpp:466` 的 `ItemPersistenceCodec::encode` 中，计算完 `allowedPtr`（`:510`）后、`processSection` 之前，遍历 `getStore().getAllSideTables()` 并对 `allowedPtr` 未包含的条目 `continue`，做 `modifier_record_ids.size() > kMaxModifierRecordIdsPerItem` 预校验，超限打 `LOG_ERROR` 并 `return false`。**不要**在 `buildItemSideTablesSection` 内 `return {}`（会被 `processSection` 登记、写出时静默跳过，导致整段旁表丢失），也不要无差别校验全仓库旁表（会卡死无关的轻量存盘）。
- [ ] **Task 1.2**: 在 `tests/unit/ItemPersistenceCodecTests.cpp` 补充编码侧超限防御用例：向某 side table 的 `modifier_record_ids` 压入 65 条数据，断言 `encode(...)` 返回 `false` 且目标流未产生任何分段；64 条数据成功往返。
- [ ] **Task 1.3**: 在 `src/game/systems/modifier/MapModifierAdapter.cpp:98` 注入 `ModifierRecordRequest` 时，将 `affix.value` 乘以 `0.01f` 进行归一化（仅百分比语义词缀）。
- [ ] **Task 1.4**: 更新 `tests/unit/MapModifierAdapterTests.cpp:107-118`：现有断言 `100.0f * (1.0f + t1Value)`（`t1Value` 未归一化）**固化了量纲 Bug**，必须改为基于 `MapAffixRegistry::CalculateValue(type, 1) * 0.01f` 的真实强度断言（T1=20 → 120.0f）。
- [ ] **Task 1.5（可选，防御性）**: 复核确认真实字节流预算解码路径不适用于 Section 3 后，在 `buildItemSkillModifiersSection` 编码侧补一个与解码 `:1007` 对齐的 `modCount > 100` 上限断言（Section 13 非 P0 缺陷，见设计 §2.1.3，不阻断验收）。

### Phase 2: 构建图集成 (P0)
- [ ] **Task 2.1**: 在根目录 `CMakeLists.txt` 中添加 `GenerateModifierRuntime` 自定义命令与自定义目标，输入依赖使用 `file(GLOB_RECURSE ... CONFIGURE_DEPENDS "${MODIFIER_DATA_DIR}/*.json")`（必须覆盖 `canonical/`），并 `add_dependencies(NoMoreDayGameModifier GenerateModifierRuntime)`（对齐现有 `GenerateTags` 模式）。
- [ ] **Task 2.2**: 保持 `build.bat:307/313` 不变（唯一漂移门禁 + 生成入口），在 CMake 段落就近写注释说明职责边界与"两次输出必须字节一致"的确定性要求。
- [ ] **Task 2.3**: 验证：删除本地 `assets/generated/modifier_runtime_v2.bin` 后，仅通过 CMake 构建（不经 `build.bat`）会自动重新生成该文件，且生成的 `.bin` 与 `build.bat` 生成的字节一致。

### Phase 3: 词缀数据单源化与热循环短路 (P1)
- [ ] **Task 3.1**: 在 `src/game/foundation/data/MonsterAffixRegistry.hpp` 的 `kAffixData` 中补入缺失的 `Vortex`（`hasUpdate=true`）与 `Entangler`（`hasOnHit=true`）定义，插入位置为 `Berserker`（`:381`）与 `Avenger`（`:393`）之间；并增加 `ValidateAffixDataIds()` + `static_assert`，保证 `kAffixData[i].id == MonsterAffixType(i)`。
- [ ] **Task 3.2**: 以生成器的 `MONSTER_*_BEHAVIOR_OPS` 为唯一事实，修正不一致的 `AffixFlags`：`Suppressor` → `hasUpdate=true`（`:450`）、`Avenger` → `hasOnDeath=true`（`:399`）、`MirrorImage` → `hasUpdate=false`（`:424`）、`SoulEater` → `hasUpdate=false`（`:437`）。
- [ ] **Task 3.3**: 在 `scripts/gen_map_monster_modifier_v2.py` 中增加双向交叉校验：对每个词缀，`flags.hasUpdate == (A ∈ MONSTER_UPDATE_BEHAVIOR_OPS)`、`hasOnHit == (A ∈ ON_HIT)`、`hasOnDeath == (A ∈ ON_DEATH)`，不等即 `RuntimeError`。
- [ ] **Task 3.4**: 在 `src/game/systems/combat/MonsterAffixSystem.hpp:94` 的 `for (auto entity : view)` 循环头部增加 `if (!affix.hasUpdate) continue;` 短路判断（必须在 Task 3.1-3.3 完成后）。
- [ ] **Task 3.5**: 补充测试：`kAffixData` 对齐（`Vortex`/`Entangler` 查询正确）；对无 Update 词缀怪物验证跳过求值；对 `Suppressor` 验证其 Update 行为仍执行（回归防护）。

### Phase 4: 技能魔耗消费端接入闭环 (P1)
- [ ] **Task 4.1**: 在 `src/game/systems/modifier/EquipmentModifierAdapter.hpp` 和 `.cpp` 中实现 `GetEquippedManaCostMultiplier(registry, entity, skillId, skillTags)`（空 recordIds 返回 `1.0f`）。
- [ ] **Task 4.2**: 在 `src/game/systems/skill/SkillSystem.cpp:2050` 接入装备魔耗乘算结算；在 `src/game/systems/skill/SkillDisplayPreviewService.cpp` 的 `:24-36` 赋值之后、`:39-42` 提前返回之前接入 UI 预览展示；两个文件补充 `EquipmentModifierAdapter.hpp` include。
- [ ] **Task 4.3**: 在 `tests/unit/EquipmentModifierAdapterTests.cpp`（或对应单测文件）补充魔耗倍率结算用例，并覆盖"未装备词缀时为 1.0f"。

### Phase 5: 地图词缀战斗属性挂接（含 `AttributePipeline`）(P1)
- [ ] **Task 5.1**: 在 `src/game/systems/world/MapAffixRegistry.hpp` 中为 `Enemy_CritChance` → `StatType::CritChance`、`Enemy_ResistAll` → `StatType::ResistAll`、`Enemy_ResistPhys` → `ResistPhysical`、`Enemy_ResistFire` → `ResistFire`、`Enemy_ResistCold` → `ResistCold`、`Enemy_ResistLight` → `ResistLightning`、`Enemy_ResistPois` → `ResistPoison`、`Enemy_ResistVoid` → `ResistShadow` 赋值。`Enemy_Armor` / `Enemy_Dodge` / `Enemy_ArmorShred` 平坦语义不改；`Enemy_CritResist` 无 `StatType`，不改。
- [ ] **Task 5.2**: 在 `src/game/foundation/stats/AttributePipeline.cpp` 中新增 `MapPercentPointsBonus` 辅助函数，并在敌方分支（`:368-396`）接入 `CritChance`、`ResistAll` 与六系抗性（见计划 §2.6）。**这是本 Phase 的关键任务，缺它则映射改动零效果。**
- [ ] **Task 5.3**: 运行 `python scripts/gen_map_monster_modifier_v2.py` 更新 `assets/data/modifier_v2/map_modifiers.json`，再由 `python scripts/gen_modifier_runtime_v2.py --build` 重编 `.bin`。
- [ ] **Task 5.4**: 在 `tests/unit/MapModifierAdapterTests.cpp` 补充端到端生效断言：施加 `Enemy_ResistPhys`（T1=25）后敌方 `CombatStats::resistances[Physical]` 相比基线 +0.25`（cap 内）；施加 `Enemy_CritChance`（T1=20）后 `crit_chance` 由 0.05 → 0.25。

### Phase 6: 生成器单记录不变量防御 (P2)
- [ ] **Task 6.1**: 先对存量 JSON 跑一次不变量核验（`--check` 语义），确认 `assets/data/modifier_v2/*.json` 不存在同 `target_stat` 多条 `ADD_STAT_PERCENT_MULT`。若存在，先修数据或降级为 warning 并登记待清理项。
- [ ] **Task 6.2**: 在 `scripts/gen_map_monster_modifier_v2.py` 与 `scripts/gen_modifier_runtime_v2.py` 中增加断言，校验单条 record 内同目标属性最多 1 条 `ADD_STAT_PERCENT_MULT`（与设计 §2.8 一致）。

### Phase 7: 门禁检查与全量集中验证 (P0)
- [ ] **Task 7.1**: 运行 `python scripts/gen_map_monster_modifier_v2.py --check` 验证为 0 退出（含行为 flag 交叉校验、单记录乘算不变量）。
- [ ] **Task 7.2**: 运行 `python scripts/gen_modifier_runtime_v2.py --check` 验证为 0 退出。
- [ ] **Task 7.3**: 执行完整构建 `build.bat RelWithDebInfo`，验证 precheck 门禁与编译零警告/零报错。
- [ ] **Task 7.4**: 运行全套相关测试组：
  - `NoMoreDayTests.exe --test-case="*Modifier*"`
  - `NoMoreDayTests.exe --test-case="*MonsterAffix*"`
  - `NoMoreDayTests.exe --test-case="*ItemPersistence*"`
  - `NoMoreDayTests.exe --test-case="*ItemStore*"`
  - `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`
  - 同时确认新增用例已挂 `unit` 标签（若与既有约定不同，需先确认真实标签名）。

---

## 4. 测试方法与命令

```powershell
# 1. 门禁与代码防漂移校验
python scripts/gen_map_monster_modifier_v2.py --check
python scripts/check_monster_behavior_dispatch.py
python scripts/gen_modifier_runtime_v2.py --check

# 2. 编译与预检
.\build.bat RelWithDebInfo

# 3. 定点单元测试
.\bin\NoMoreDayTests.exe --test-case="*ItemPersistence*"
.\bin\NoMoreDayTests.exe --test-case="*MapModifierAdapter*"
.\bin\NoMoreDayTests.exe --test-case="*EquipmentModifierAdapter*"
.\bin\NoMoreDayTests.exe --test-case="*ModifierEvaluator*"
.\bin\NoMoreDayTests.exe --test-case="*MonsterAffix*"

# 4. 全量 CTest 单元套件
ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure
```

---

## 5. 验证任务完成 (DoD 验收清单)

- [ ] `ItemPersistenceCodec::encode` 编码侧超限防御生效：65 条用例返回 `false` 且不写出任何分段，未造成坏档或旁表静默丢失。
- [ ] `MapModifierAdapter` 输出的数值量纲正确，T1 怪物血量增加 20%（输出 120.0f），绝无 21 倍暴涨；测试断言基于归一化后的真实强度。
- [ ] 仅通过 CMake 构建（不经过 `build.bat`）时，`.bin` 能够自动按需生成，且与 `build.bat` 产物字节一致。
- [ ] `kAffixData[i].id == MonsterAffixType(i)` 编译期断言通过，`Vortex`/`Entangler` 定义不再缺失。
- [ ] `AffixFlags` 与 UMR 行为算子表一致，且生成器交叉校验生效。
- [ ] `MonsterAffixSystem` 对无 Update 词缀的怪物执行跳过；`Suppressor` 的 Update 行为仍会执行（无回归）。
- [ ] `SkillSystem` 释放技能时，装备的 `MANA_COST_MULT` 词缀正确降低扣除法力；未装备时倍率为 1.0f。
- [ ] 地图词缀库中的暴击与全抗/六系抗性词缀**在 `CombatStats` 上真实生效**（非仅产生 Delta）：抗性百分点 +25（cap 内），暴击 0.05 → 0.25。
- [ ] 生成器单记录乘算不变量断言生效，且存量数据已通过核验。
- [ ] `ctest -L unit` 保持 100% 绿灯。
