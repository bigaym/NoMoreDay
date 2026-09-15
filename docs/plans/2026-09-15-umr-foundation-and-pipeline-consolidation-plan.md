# UMR 统一修饰器运行时基础夯实与工程闭环 — 实施计划

- **状态**：计划已按审查修订 / 待执行
- **文档路径**：`docs/plans/2026-09-15-umr-foundation-and-pipeline-consolidation-plan.md`
- **日期**：2026-09-15
- **设计基线**：`docs/designs/2026-09-15-umr-foundation-and-pipeline-consolidation-design.md`
- **审查依据**：`docs/reviews/2026-09-15-umr-foundation-and-pipeline-consolidation-design-review.md`（结论 `修改`）
- **范围**：
  1. 修复生成脚本源路径并打通构建门禁（`build.bat` precheck 三道防线）
  2. 闭环求值器 `MANA_COST_MULT` 算子（求值 + 存储 + 单测；技能消耗链接线见 scope-out 说明）
  3. 为存储旁表（`ItemSideTableData`）与持久化编解码补充 `modifier_record_ids` 支持
  4. **先解耦生成器与适配器**，再改造 `MapModifierAdapter` 与 `MonsterModifierAdapter` 直连 `ModifierRuntimeRegistry` 消除内存旁路（保留地图滚值与怪物三档过滤语义）
  5. 修正 `talent_modifiers.json` 节点 ID 漂移并全量回归

### 0.1 相对初稿的关键修订（来自第 1 轮审查）
1. **Phase 3 顺序修正**：`gen_map_monster_modifier_v2.py` 的正则**从 `MapModifierAdapter.cpp` 提取 `MapAffixType -> StatType` 映射**。直接删 switch 会让 Phase 1 门禁永久失败。故新增 **Task 3.0（解耦）**，并把适配器改造排在解耦之后。
2. **地图滚值不可丢**：`map_modifiers.json` 存的是 `MapAffixRegistry` 的 valT1 模板值，而运行时用 `Affix.value`（按层数滚动）。改走 registry 必须引入逐次求值请求 `ModifierRecordRequest`（同一词缀出现多次即施加多次、各带滚值），并补齐共鸣词缀记录（node `799999`）。
3. **怪物三档语义**：`EvaluateAffixDelta/Events/BehaviorOps` 是 opcode 过滤出的三档视图，不是三条独立数据；`Monster_Vampiric` 记录含 `LifeSteal` 属性会造成双计，生成器须与运行时对齐。
4. **存储轨落点**：字段加在 `ItemSideTableData` 与 `ItemPersistenceCodec.cpp` Section 3，**不是** 8 字节 POD 的 `CompactAffix`；文件路径为 `src/game/systems/item/storage/...`。
5. **`MANA_COST_MULT` 消费端 scope-out**：仓库无统一技能魔耗结算点，本阶段只交付求值/存储/单测。
6. **`--check` 与默认模式语义**：默认（无参数）与 `--build` 写盘；`--check` 纯内存比对、非 0 退出、绝不写盘。
7. **`.bin` 是 gitignore 的本地产物**：`assets/generated/modifier_runtime_v2.bin` / `.debug.json`（`.gitignore:73`）不随仓库分发。故 `build.bat` 对该产物只能**生成**、不能硬性 `--check`，否则干净检出必然失败；`--check` 仅用于 CI / 跨机比对。

---

## 1. 实施思路与原理

### 1.1 总体策略：零兼容层、契约通电、消除死数据
1. **门禁优先（Phase 1）**：门禁是防止代码与配置漂移的唯一自动化保障。必须先修通 `gen_map_monster_modifier_v2.py` 的头文件引用，改造 `gen_modifier_runtime_v2.py` 的 `--check` 为**真正的只读内存对比**，并在 `build.bat` 中挂载执行。
2. **算子与存储闭环（Phase 2）**：
   - `MANA_COST_MULT` 是连乘模型，`ModifierDelta` 存储技能级和全局（ID 0）的浮点倍率，并在 `ModifierEvaluator.cpp` 的两个求值 switch 中实现。
   - 物品存储轨保持 `ItemInstance`（192 字节 POD）与 `CompactAffix`（8 字节 POD）零堆分配设计，将变长的 `modifier_record_ids` 挂载到专门承载复杂/稀疏修饰器的 `ItemSideTableData` 中，并由 Section 3 / Section 13 编解码器提供二进制持久化。
3. **消除适配器内存旁路（Phase 3，须先解耦）**：
   - 当前 `MapModifierAdapter` 和 `MonsterModifierAdapter` 内部自行组装 `ModifierRecord`，导致 `.bin` 成为死数据。
   - **前置**：`gen_map_monster_modifier_v2.py` 目前从 `MapModifierAdapter.cpp` 提取映射，是生成器的输入契约。必须先 Task 3.0 把映射迁到 `MapAffixDefinition`，生成器改解析 `MapAffixRegistry.hpp`，否则删 switch 会打断 Phase 1 门禁。
   - 改造后，Adapter 仅负责收集 Context（激活词缀/标签/槽位/AffixType），据此取得 Record IDs，然后调用 `ModifierEvaluator::Evaluate(registry, requests, ctx[, categories])` 查表只读求值。
   - **地图**：数值是运行时滚值，必须用 `ModifierRecordRequest` 逐次注入（同一 `MapAffixType` 出现 N 次即施加 N 次，避免按 id 查表只保留首条滚值），不能直接取 JSON 里的 valT1 模板值；共鸣词缀须在数据层补齐记录。
   - **怪物**：必须保留 `includeStats/includeEvents/includeBehaviorOps` 三档过滤语义，并与生成器对齐 `Monster_Vampiric` 的 `LifeSteal` 属性语义（避免双计）。
4. **数据纠偏与确定性重编（Phase 4）**：
   - 纠正 `talent_modifiers.json` 中的 `1100` 假节点为星盘实际起点（如 1001）；
   - 执行生成器重编 `modifier_runtime_v2.bin`，确保与 JSON 字节级对齐，并通过门禁与全量单测。

---

## 2. 伪代码与接口草图

### 2.1 工具链 `--check` 只读比对（`gen_modifier_runtime_v2.py`）
```python
def check_binary_matches(input_dir: Path, output_bin: Path) -> bool:
    # 内存生成，不写磁盘
    new_blob, _ = compile_from_input(input_dir)
    if not output_bin.exists():
        print(f"[modifier-runtime] Check failed: '{output_bin}' does not exist.")
        return False
    current_blob = output_bin.read_bytes()
    if new_blob != current_blob:
        print("[modifier-runtime] Check failed: binary out of sync with modifier_v2 json.")
        return False
    return True
```

### 2.2 求值器魔耗扩展（`ModifierEvaluator.hpp / .cpp`）
```cpp
// ModifierEvaluator.hpp
struct ModifierDelta {
    // ...
    std::unordered_map<uint32_t, float> mana_cost_mult; // 技能 ID -> 连乘系数 (0 为全局通配)

    void AddManaCostMultiplier(uint32_t skillId, float mult) {
        auto it = mana_cost_mult.find(skillId);
        if (it == mana_cost_mult.end()) {
            mana_cost_mult.emplace(skillId, mult);
        } else {
            it->second *= mult;
        }
    }

    [[nodiscard]] float GetManaCostMultiplier(uint32_t skillId) const {
        float globalMult = ReadOr(mana_cost_mult, 0u, 1.0f);
        float skillMult = (skillId != 0u) ? ReadOr(mana_cost_mult, skillId, 1.0f) : 1.0f;
        return globalMult * skillMult;
    }
};

// ModifierEvaluator.cpp switch (opcode)
case ModifierOpCode::MANA_COST_MULT:
    out.AddManaCostMultiplier(op.param_u32, op.param_f32);
    break;
```

### 2.3 存储轨旁表集成（`ItemStorageTypes.hpp` & `ItemPersistenceCodec.cpp`）
```cpp
// ItemStorageTypes.hpp
struct ItemSideTableData {
    std::vector<StatConversion> conversions;
    std::vector<DamageModifier> damage_modifiers;
    std::vector<ItemSkillModifier> skill_modifiers;
    std::vector<uint32_t> modifier_record_ids; // 新增：持久化携带的 UMR 记录 ID 列表
    
    [[nodiscard]] bool empty() const noexcept {
        return conversions.empty() && damage_modifiers.empty() &&
               skill_modifiers.empty() && modifier_record_ids.empty();
    }
};

// ItemPersistenceCodec.cpp (Section 3: buildItemSideTablesSection)
// 写入 modifier_record_ids 数组长度与 uint32_t 元素
// 解码时对应读出并还原至 ItemSideTableData
```

### 2.4 地图修饰器直连 Registry（`MapModifierAdapter.cpp`，含滚值覆盖）
> 关键：`map_modifiers.json` 内是 `MapAffixRegistry` 的 valT1 模板值，运行时数值必须由 `affix.value` 覆盖注入，否则高阶地图词缀会塌陷为 T1 强度。

```cpp
ModifierDelta MapModifierAdapter::EvaluateEnemyAffixDelta(const ActiveDimensionalState &state) {
    if (!state.isActive) return ModifierDelta{};

    const auto &registry = ModifierRuntimeRegistry::Get();
    ModifierEvalContext ctx;
    std::vector<ModifierRecordRequest> requests;

    for (const auto &affix : state.explicitAffixes) {
        const StatType combatStat = MapAffixRegistry::GetDef(affix.type).combatStat;
        if (combatStat == StatType::Count) continue;                    // 无战斗映射的词缀跳过
        const uint32_t recordId = EncodeMapAffixRecordId(affix.type);   // kMapRecordIdBase + affixType
        if (!registry.FindRecordById(recordId)) continue;               // 未登记的词缀跳过
        // 每次出现一个请求：记录形状来自 registry，数值来自运行时滚值；
        // 同一词缀出现 N 次即施加 N 次，避免按 id 查表只保留首条滚值。
        requests.push_back(ModifierRecordRequest{recordId, /*override_percent_mult=*/true, affix.value,
                                                 static_cast<uint32_t>(combatStat)});
        ctx.active_node_ids.push_back(EncodeMapAffixNodeId(affix.type));
    }

    if (state.resonance.totalEnemyDensity > 0.0f) {
        constexpr uint32_t kResonanceRecordId = kMapResonanceRecordId; // 需在 map_modifiers.json 补记录
        if (registry.FindRecordById(kResonanceRecordId)) {
            requests.push_back(ModifierRecordRequest{kResonanceRecordId, true,
                                                     state.resonance.totalEnemyDensity * 0.05f,
                                                     kAllStatTargets});
        }
    }

    return ModifierEvaluator::Evaluate(registry, requests, ctx);
}
```

---

## 3. 原子任务拆分（按序推进）

### Phase 1: 工具链与构建门禁通电 (P0)
- [ ] **Task 1.1**: 修正 `scripts/gen_map_monster_modifier_v2.py:10-14` 的失效头文件路径，映射到实际位置：`src/game/foundation/data/MapAffix.hpp`、`src/game/foundation/data/MonsterAffixRegistry.hpp`、`src/game/foundation/components/Stats.hpp`（`MapAffixRegistry.hpp` 与 `MapModifierAdapter.cpp` 两个常量本就正确，勿改）。
- [ ] **Task 1.2**: 改造 `scripts/gen_modifier_runtime_v2.py`：`--check` 为纯内存编译 + 字节级（CRC32 与逐字节）比对，不一致打印 `[modifier-runtime] Check failed: binary out of sync with modifier_v2 json` 并非 0 退出，**绝不写盘**；默认（无参数）与 `--build` 保留写盘语义。修正现有 `if not args.check and not args.check_determinism: args.check = True`（:267-268）避免默认路径语义混乱。
- [ ] **Task 1.3**: `build.bat` 的 UMR 防护网分两类：**硬门禁** = `gen_map_monster_modifier_v2.py --check`（受版本管理 JSON ↔ C++ 源码一致性）与 `check_monster_behavior_dispatch.py`；**生成步骤** = `gen_modifier_runtime_v2.py`（默认写盘）。原因：`assets/generated/modifier_runtime_v2.bin` 已被 `.gitignore:73` 忽略，属本地生成产物，若当作硬失败门禁会让任何干净检出失败。
- [ ] **Task 1.4**: 在 `build.bat` 的 `ENABLE_PRECHECKS` 区块按上述两类接入三步，沿用现有 `:run_quiet_step` 调用约定；并以写盘模式运行一次生成器，产出与 JSON 对齐的 `.bin` / `.debug.json`。
- [ ] **Task 1.5**: 门禁负例验证：正常状态三步退出码为 0；手工篡改 `map_modifiers.json`（或 `monster_modifiers.json`）应被第 1 步拦截；删除 `.bin` 后再构建应由第 3 步重新生成而不是失败。负例须用仓库外临时副本，测试后原样还原。

### Phase 2: 求值器算子闭环与存储轨对齐 (P0)
- [ ] **Task 2.1**: 在 `src/game/systems/modifier/ModifierEvaluator.hpp` 的 `ModifierDelta` 中新增 `std::unordered_map<uint32_t, float> mana_cost_mult` 容器及 `AddManaCostMultiplier`（连乘）/`GetManaCostMultiplier`（0 为全局通配，结果 = global × specific）方法。
- [ ] **Task 2.2**: 在 `ModifierEvaluator.cpp` 的两个 `switch`（约 :197-248 与 :276-327）中接入 `ModifierOpCode::MANA_COST_MULT`，消除 `default: break` 的静默丢弃。
- [ ] **Task 2.3**: 在 `tests/unit/ModifierEvaluatorTests.cpp` 补充 `MANA_COST_MULT`（全局通配、单技能特化、两者连乘）用例。用例可与 Task 2.4~2.7 并行编写；编译与执行统一放到 Phase 4 的集中验证阶段，避免子任务并发构建。
- [ ] **Task 2.4**: 在 `src/game/systems/item/storage/ItemStorageTypes.hpp` 的 `ItemSideTableData` 中新增 `std::vector<uint32_t> modifier_record_ids`，并同步 `empty()`/`clear()`。
- [ ] **Task 2.5**: 在 `src/game/systems/item/storage/ItemPersistenceCodec.cpp` 的 Section 3 `buildItemSideTablesSection`（约 :154-198）与解码分支 `SectionType::ItemSideTables`（约 :719-766）中编码/解码 `modifier_record_ids`。
- [x] **Task 2.6**: 版本决定（已落地）：Section 3 新增 `modifier_record_ids` 后，`ITEM_STORE_BINARY_VERSION`（`ItemPersistenceCodec.hpp:27`）已由 `1` 递增为 `2`，且不兼容 v1。依据：解码器在 FileHeader 阶段即校验 `fileHeader.version != ITEM_STORE_BINARY_VERSION` 并直接失败（`ItemPersistenceCodec.cpp:601-602`），故递增版本可安全拒绝旧档而非让其进入 Section 3 静默错位；本阶段不做 v1 -> v2 迁移，已用往返测试 + 版本拒绝用例覆盖。
- [ ] **Task 2.7**: 在 `tests/unit/ItemPersistenceCodecTests.cpp` 补充含 `modifier_record_ids` 的往返读写用例。
- [ ] **Task 2.8 (scope-out 记录)**: `MANA_COST_MULT` 的技能层消耗接线**不在本计划内**：仓库无统一技能魔耗结算点（无 `SkillSystem::CalculateManaCost`），耗蓝散落于 `BeamChannelDeliverySystem` 与个别行为。待确立唯一结算函数后另立任务，验收需含"倍率生效前后基础耗蓝对比"。

### Phase 3: 契约解耦与适配器闭环 (P1)
- [ ] **Task 3.0 (前置，阻塞 3.2)**: 解耦生成器与适配器：在 `src/game/systems/world/MapAffixRegistry.hpp` 的 `MapAffixDefinition` 增加战斗属性字段（复用 `StatType`），把 `MapModifierAdapter.cpp` 中 `Enemy_ExtraHealth -> MaxHealth`、`Enemy_ExtraDamage -> PhysicalDamage`、`Enemy_Fast -> MoveSpeed` 的映射迁入该字段；`gen_map_monster_modifier_v2.py` 改为从 `MapAffixRegistry.hpp` 读取映射，**删除对 `MapModifierAdapter.cpp` 的正则解析**（`_parse_map_adapter_enemy_templates`）。同步修正 `_parse_affix_definitions` 的 `fields[5]` 字段索引（:309-316）。
- [ ] **Task 3.1**: 改造 `src/game/systems/modifier/MonsterModifierAdapter.cpp` 直连 `ModifierRuntimeRegistry`：以 `MonsterAffixType` 映射 RecordId（`5000000 + affixType`）取记录求值；**保留三档过滤语义**（`EvaluateAffixDelta` = stats+behavior 且排除吸血属性、`EvaluateAffixEvents` = events、`EvaluateBehaviorOps` = behavior），不得让任一入口返回全部字段。
- [ ] **Task 3.2 (依赖 3.0)**: 重构 `src/game/systems/modifier/MapModifierAdapter.cpp`：记录形状来自 registry，滚值以 `ModifierRecordRequest` 逐次注入（同一词缀出现多次即施加多次）；为共鸣词缀补齐 `map_modifiers.json` 记录（node `799999`）并纳入生成器；record/node 常量以 `MapModifierIds` 暴露并加映射一致性测试。
- [ ] **Task 3.3**: 在 `src/game/systems/modifier/ModifierEvaluator.{hpp,cpp}` 新增 `Evaluate(registry, std::span<const ModifierRecordRequest>, ctx, ModifierOpCategory categories = All)` 重载（`struct ModifierRecordRequest { uint32_t record_id; bool override_percent_mult; float param_f32; uint32_t target_stat; }`），语义为按请求顺序逐次施加、仅覆盖 `op.param_u32 == target_stat` 的 `ADD_STAT_PERCENT_MULT` 数值，记录形状仍只读 registry；`ModifierOpCategory` 位掩码供窄入口跳过无关算子组。
- [ ] **Task 3.4**: 数值等价性回归：`tests/unit/MapModifierAdapterTests.cpp` 与 `tests/unit/MonsterModifierAdapterTests.cpp` 断言改造前后数值一致（含吸血不双计、地图分层滚值、共鸣倍率）。
- [ ] **Task 3.5**: 与 Task 3.0 对齐生成器：怪物生成器对 `Vampiric` 不再产出 `LifeSteal` 属性 op，使数据与运行时语义一致，并加负例测试。

### Phase 4: 数据纠偏与终态全量回归 (P1)
- [ ] **Task 4.1**: 修正 `assets/data/modifier_v2/talent_modifiers.json` 的 `node_id_whitelist`：先以 `assets/data/profession_talents.json` 实际内容确认合法节点，再取值（当前拟改为 `1001`，不得臆测）。
- [ ] **Task 4.2**: 以写盘模式重编 `assets/generated/modifier_runtime_v2.bin` 与 `modifier_runtime_v2.debug.json`，使与 JSON 字节级对齐。
- [ ] **Task 4.3**: 执行完整构建 `./build.bat RelWithDebInfo`，验证 precheck 门禁全部亮绿。
- [ ] **Task 4.4**: 运行全套相关测试集：
  - `NoMoreDayTests.exe --test-case="*Modifier*"`
  - `NoMoreDayTests.exe --test-case="*MonsterAffix*"`
  - `NoMoreDayTests.exe --test-case="*ItemPersistence*"`
  - `NoMoreDayTests.exe --test-case="*ItemStore*"`
  - `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure`

---

## 4. 测试方法与回归命令

### 4.1 门禁命令与工具链测试
```powershell
# 1. 验证地图与怪物修饰器生成一致性
python scripts/gen_map_monster_modifier_v2.py --check

# 2. 验证怪物行为 OpCodes 分发覆盖闭环
python scripts/check_monster_behavior_dispatch.py

# 3. 验证二进制运行库与 JSON 数据一致（无漂移）
python scripts/gen_modifier_runtime_v2.py --check
```

### 4.2 单元与集成测试命令
```powershell
# 编译构建 (含自动执行上述预检查)
.\build.bat RelWithDebInfo

# 核心 UMR 与适配器单元测试
.\bin\NoMoreDayTests.exe --test-case="*Modifier*"

# 怪物词缀行为分发与系统测试
.\bin\NoMoreDayTests.exe --test-case="*MonsterAffix*"

# 物品存储轨与持久化编解码测试
.\bin\NoMoreDayTests.exe --test-case="*ItemPersistence*"
.\bin\NoMoreDayTests.exe --test-case="*ItemStore*"

# CTest 全量单元测试套件
ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure
```

---

## 5. 验证任务完成 (DoD 验收清单)

> 执行结果：**全部通过**（构建 `RelWithDebInfo` 退出码 0；`ctest -L unit` 100% / 8 项通过）。证据见下方各项括注。

- [x] **门禁守护成立**：`build.bat` precheck 含硬门禁（Map/Monster 生成校验、怪物行为分发合同）+ 产物生成（`gen_modifier_runtime_v2.py` 写盘）。实测：篡改 `map_modifiers.json` → precheck 中止、退出码 1；删除 `.bin` → 构建退出码 0 且自动重生成。
- [x] **`--check` 语义正确**：`gen_modifier_runtime_v2.py --check` 只读不写盘、不一致时打印 `[modifier-runtime] Check failed: ...` 并非 0 退出；默认与 `--build` 写盘。（既有 `ModifierCompilerDeterminismTests.cpp` 已改用 `--build`。）
- [x] **生成器解耦成立**：`gen_map_monster_modifier_v2.py` 不再解析 `MapModifierAdapter.cpp`，改由 `MapAffixRegistry.hpp` 的 `MapAffixDefinition.combatStat` 提供 `MapAffixType -> StatType` 映射，`--check` 退出码 0（map=4 / monster=25）。
- [x] **求值器无遗漏**：`MANA_COST_MULT` 已求值/存储并有 5 条用例覆盖；技能层接线登记为后续任务（§Task 2.8）。
- [x] **存储轨不丢字段（范围收窄）**：`ItemSideTableData.modifier_record_ids` 经 `ItemPersistenceCodec` Section 3 往返保留；**`ITEM_STORE_BINARY_VERSION` 递增为 2，显式不兼容 v1**（v1 被 Header 阶段拒绝，避免静默错位），v1 拒绝用例已覆盖。本阶段仅以旁表编解码往返用例覆盖 **codec 层**；ECS→存储轨的生产端桥接（`Affix.modifier_record_ids` 落入/还原旁表）**尚未实现**，待双轨统一任务 T-P3-3，该字段当前为前向兼容预留。
- [x] **消除死数据旁路且无数值回归**：两个 Adapter 不再手写 `ModifierRecord`，改由 `ModifierRuntimeRegistry` 查出并执行；地图滚值经 `ModifierRecordRequest` 逐次注入（同词缀重复出现各施加一次）、共鸣（id `4_099_999` / node `799_999`）已入数据；怪物吸血不双计（数据层移除 LifeSteal op），三档评估经 `ModifierOpCategory` 掩码保持不变且窄入口不为无关算子组做容器插入。`*Modifier*` 60/60、`*MonsterAffix*` 30/30、`*MapModifier*` 6/6、`*MonsterModifier*` 8/8、`*AttributePipeline*` 6/6。
- [x] **数据源合法**：星盘节点由非法 `1100` 修正为 `1001`，已按 `profession_talents.json` 实体核验（Minori/tier1、`{MaxHealth, Flat, 10.0}`×5 = 记录 `MaxHealth Flat 50.0` 吻合）。
- [x] **全量回归通过**：`*Modifier*`/`*MonsterAffix*`/`*MapModifier*`/`*MonsterModifier*`/`*AttributePipeline*`/`*ItemPersistence*`/`*ItemStore*`/`*Nemesis*`/`*MapAffix*` 全绿；`ctest --test-dir build -C RelWithDebInfo -L unit` → 100% / 8。

## 6. 后续任务（本计划外，已登记）

- **技能魔耗结算链接线**：确立唯一魔耗结算函数后，由 `EquipmentModifierAdapter` 注入 `mana_cost_mult`；验收需含"倍率生效前后基础耗蓝对比"。
- **地图词缀数值量纲核查（既有缺陷，非本次回归）**：`MapAffixCalculator.cpp:100/174` 写入的 `affix.value` 为"百分数点"（如 `Enemy_ExtraHealth` T1=20），而 `MapModifierAdapter` 将其原样作为 `ADD_STAT_PERCENT_MULT` 参数（求值语义为 `1 + value`），疑似应为 `value * 0.01`。改造前旧实现同样未归一化，故本次**忠实保留**了该行为；修正会改变地图词缀强度（玩法平衡），需单独评估后处理。
- **地图 tier 插值测试覆盖**：已补齐 `MapAffixRegistry::CalculateValue` 4 条直接用例（钳制/线性/单调/常量），并更正原误导性用例为 `- rolled value drives intensity across tiers`。
- **存储旁表编码侧对称约束**：`kMaxModifierRecordIdsPerItem = 64` 当前仅在 `ItemPersistenceCodec` 解码侧校验；建议在编码/写入侧增加对称上限，避免写出超限数量导致该物品后续无法解码。
- **第 3 轮代码审查发现项**：已全部修复（override 语义改为每次出现一条 `ModifierRecordRequest` + `target_stat`；新增 `ModifierOpCategory` 掩码；`Reload` + 一次性告警使失败可观测；`ItemSideTableData` 相等性纳入 `modifier_record_ids`；常量守护用例）。详见 `docs/reviews/2026-09-15-umr-foundation-and-pipeline-consolidation-design-review.md` §7。
