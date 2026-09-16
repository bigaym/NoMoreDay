# UMR 统一修饰器框架完善与工程加固设计说明

- **文档状态**：设计完成（评审修订版 v2）/ 待审阅
- **文档路径**：`docs/designs/2026-09-16-umr-framework-completion-and-hardening-design.md`
- **设计日期**：2026-09-16
- **系统代号**：`UMR` (Unified Modifier Runtime)
- **修订说明**：v2 依据代码实况复核修订，修正了 v1 中"热循环短路前提错误""地图战斗属性实际不生效""约束仲裁无数据可消费"三处关键缺陷，并补齐了构建分工、数据单源、遗漏词缀与测试契约。
- **输入来源**：
  - `设计文档/统一修饰器运行时系统_UMR.md`
  - `docs/designs/2026-09-15-umr-foundation-and-pipeline-consolidation-design.md`
  - `docs/reviews/2026-09-15-umr-foundation-and-pipeline-consolidation-design-review.md`（特别是 §6.3、§7.3、§8.2 遗留任务）
  - 代码实况：`src/game/systems/modifier/*`、`src/game/systems/item/storage/*`、`src/game/systems/combat/MonsterAffixSystem.hpp`、`src/game/foundation/data/MonsterAffixRegistry.hpp`、`src/game/foundation/stats/AttributePipeline.cpp`、`src/game/systems/skill/*`、`src/game/systems/world/MapAffixRegistry.hpp`、`CMakeLists.txt`、`build.bat`

---

## 1. 背景与目标界定

### 1.1 背景与现状
在最近的 UMR 基础夯实批次中（commit `0572a061`），我们成功将 `build.bat` 门禁通电、确立了以 `ModifierRecordRequest` 为核心的滚值覆盖机制，并升级了物品存储旁表二进制版本（`ITEM_STORE_BINARY_VERSION = 2`）。

需要如实记录一项**未完成的现状**：`MapModifierAdapter::EvaluateEnemyAffixDelta` 目前仍在每次调用时新建 `std::vector<ModifierRecordRequest>` 并逐条 `push_back`（`src/game/systems/modifier/MapModifierAdapter.cpp:52-53`），即"临时堆内存拼装旁路"并未被完全消除。该路径位于敌方属性重算热路径上，本批次将其登记为已知残留（见 §1.3），不在本轮改动范围内。

此外，UMR 框架在**工程健壮性、数值正确性、调用链闭环与热路径性能**上仍留有数处明确的非业务数据类缺陷。为了让 UMR 成为真正稳定、自洽、开箱即用的运行时底座，必须在推进海量装备/专精/星盘业务录入之前，将底座本身的漏洞与断链彻底清理完毕。

### 1.2 目标（Goals）
1. **存档安全防御（P0）**：在 `ItemPersistenceCodec.cpp` 的 Section 3 编码侧补齐 `kMaxModifierRecordIdsPerItem = 64` 的对称校验，杜绝因编码侧无界写入导致解码侧彻底坏档。注意：让 `buildItemSideTablesSection` 返回空字节**并不会**使存盘失败（`processSection` 仍会登记该分段，写出循环仅跳过零长度 payload），因此必须在 `encode()` 中计算完 `allowedPtr` 后、构建分段前，对**本次会写出的旁表子集**做 fail-closed 预校验并 `return false`，而不是靠返回空向量。
2. **地图词缀数值量纲归一化（P0）**：纠正地图词缀在 `ADD_STAT_PERCENT_MULT` 中的量纲膨胀 Bug（如 20% 血量被当成 20.0f 计算得出 21 倍膨胀），建立明确的百分比与系数归一化契约（`value * 0.01f`），并同步修正把该 Bug 固化进断言的既有单元测试。
3. **构建图完备化（P0）**：将 `assets/generated/modifier_runtime_v2.bin` 生成任务纳入根目录 `CMakeLists.txt` 的自定义命令/依赖目标，为 IDE/Ninja 裸编译与 CI 提供兜底；同时明确它与 `build.bat` 既有生成/漂移门禁的职责边界，避免双写与门禁重复。
4. **怪物词缀数据单源化与热循环短路（P1）**：先修复 `MonsterAffixRegistry::kAffixData` 的**按位初始化错移缺陷**（缺失 `Vortex` / `Entangler` 定义导致后续词缀整体错位），再把 `AffixFlags` 与 UMR 行为算子表对齐为**单一事实来源**并加生成期交叉校验；在上述前提成立后，才在 `MonsterAffixSystem::Update` 中复用预计算标志做快速短路。
5. **技能魔耗消费闭环（P1）**：在 `SkillSystem` 确立统一的魔耗结算接口，打通 `EquipmentModifierAdapter` 对 `MANA_COST_MULT` 的倍率消费，使已求值的魔耗词缀真正对技能释放产生消耗影响，并明确 UI 预览的接入位置与边界行为。
6. **地图战斗属性挂接（P1，百分点语义批次）**：扩展 `MapAffixRegistry.hpp` 的 `combatStat` 映射，并**同步改造 `AttributePipeline`**，让敌方暴击率与各系抗性真正消费 `mapEnemyDelta`。V1 曾假定"映射后即生效"，实测代码并不成立（`AttributePipeline.cpp` 中 `mapEnemyDelta` 仅用于最大生命、移速与普攻物理伤害），故本目标包含 `AttributePipeline` 改造。`Enemy_Armor` / `Enemy_Dodge` / `Enemy_ArmorShred` 为平坦语义且当前无承载路径，`Enemy_CritResist`（爆伤减免）在 `StatType` 中无对应枚举，三者明确推迟。
7. **生成器单记录不变量防御（P2）**：在生成脚本中增加同目标属性最多一条乘算算子的防御断言，并先对存量 JSON 做一次合规性核验。

### 1.3 明确非目标（Non-Goals）
- **非目标**：本阶段**不推进** 382 个技能专精节点的批量 JSON 迁移（仍由现有 `SpecState` / `SkillSpecializationBaker` 承载）。
- **非目标**：本阶段**不推进** 全量装备词缀库从 `AffixDispatcher` 的大规模批量搬迁。
- **非目标**：本阶段**不引入** 新的第三方库或重构二进制四表的核心寻址模型。
- **非目标（本轮移除）**：Schema V2 `constraints`（`exclusive_group` / `max_active`）仲裁**推迟到后续批次**，理由见 §2.7。
- **非目标（已知残留）**：`MapModifierAdapter` 的每调用堆分配（见 §1.1）保留现状，登入后续性能批次。

---

## 2. 系统设计与技术方案

### 2.1 存档安全性：持久化编码对称上限防御

#### 2.1.1 现状与隐患
- `src/game/systems/item/storage/ItemPersistenceCodec.cpp:780` 解码时有明确断言：
  ```cpp
  if (recordIdCount > kMaxModifierRecordIdsPerItem) return false;
  ```
  其中 `kMaxModifierRecordIdsPerItem = 64`。
- 但在 `buildItemSideTablesSection`（编码端，`:161`）中，直接 `appendBytes(payload, recordIdCount)` 并遍历写入，**没有任何容量校验**。
- 若外部系统异常挂载了超过 64 个 UMR 记录 ID，存盘能够正常完成，但下一次启动读档反序列化该 Section 时直接返回 `false`，导致整份存档报废且无法恢复。

#### 2.1.2 修复方案（fail-closed 预校验）
先澄清一个实现事实：`buildItemSideTablesSection` 返回空 `std::vector` **不会**使存盘失败。`processSection`（`ItemPersistenceCodec.cpp:485-498`）无论 payload 是否为空都会把分段登记进 `sections`，写出循环（`:567-572`）只跳过零长度 payload。因此"`return {}`"的真实效果是**静默丢弃整个 ItemSideTables 分段**（所有物品的旁表数据一并丢失），既非 fail-closed，也无法留痕定位。

正确做法是在 `ItemPersistenceCodec::encode`（`:466`）计算完 `allowedPtr`（`:510`）之后、`processSection` 调用（`:512`）之前，对**本次实际会写出**的 side tables 做预校验，命中上限即整体拒绝存盘：

```cpp
// ItemPersistenceCodec::encode：allowedPtr 计算完成后、processSection 之前
for (const auto &[idx, data] : service.getStore().getAllSideTables()) {
  if (allowedPtr != nullptr && !allowedPtr->contains(idx)) {
    continue; // 本次脏掩码不会写出该旁表，无需为此阻断
  }
  if (data.modifier_record_ids.size() > kMaxModifierRecordIdsPerItem) {
    LOG_ERROR(
        "ItemPersistenceCodec: side-table {} has {} modifier_record_ids, exceeding max limit {}",
        idx, data.modifier_record_ids.size(), kMaxModifierRecordIdsPerItem);
    return false; // fail-closed：拒绝写入，保留旧存档，避免解码侧坏档
  }
}
```

> 必须按 `allowedPtr` 过滤，只校验本次会编码的子集：若对全仓库旁表做无差别校验，一个未被本次脏掩码触及的坏旁表会让所有分段存盘（包括只存战利品的轻量存盘）永久失败，形成"存档被卡死"的次生故障。

`buildItemSideTablesSection` 内部无需改动；上限判定点收敛到 `encode()`，消除"构建成功但写出为空"的歧义路径。

#### 2.1.3 Section 13（`ItemSkillModifiers`）的对称性结论
同名编码器还有一个变长旁表分段 Section 13（`buildItemSkillModifiersSection`，`:214-252`，登记于 `:518`），其编码侧同样不做上限校验。但复核解码侧（`:993-1033`）后确认它**不存在与 Section 3 同类的坏档路径**：解码对 `count`（`count > bytes.size()/8 + 1 || count > 1000000`，`:998`）与 `modCount`（`modCount > 100`，`:1007`）均以**实际字节流预算**为界，编码器写出的文件必然落在该预算内，不会触发硬失败。

因此 Section 13 不构成本轮的 P0 缺陷。为保持编码/解码对称的可读性，可选做一次防御性上限（复用 `kMaxModifierRecordIdsPerItem` 或新增 `kMaxItemSkillModifiersPerItem = 100`，与解码 `:1007` 对齐），但**不阻断本批次验收**。

同时保留单元测试：构造 65 条记录时 `encode(...)` 返回 `false`；64 条成功往返。

---

### 2.2 地图词缀数值量纲归一化（既有 Bug 根治）

#### 2.2.1 缺陷分析与契约失配
- `MapAffixRegistry.hpp:52` 中声明：
  ```cpp
  MapAffixDefinition{"of the Colossus", "巨像之", "怪物生命值: +{value}%", MapAffixCategory::Debuff, 1.0f, 20.0f, 100.0f, true, StatType::MaxHealth},
  ```
  `valT1 = 20.0f`（表示 +20%）。
- 离线生成脚本 `scripts/gen_map_monster_modifier_v2.py:432` 在写入 `map_modifiers.json` 时执行了：
  ```python
  def _normalize_percent(value: float) -> float:
      return round(value / 100.0, 6)
  ```
  因此 JSON 中的 `param_f32` 正确存放了 `0.2`。
- 然而，运行时 `MapModifierAdapter.cpp:98` 注入滚值时执行了：
  ```cpp
  requests.push_back({recordId, true, affix.value, static_cast<uint32_t>(combatStat)});
  ```
  运行时 `affix.value` 来自 `MapAffixRegistry::CalculateValue`，输出的是 `20.0f` ~ `100.0f`。
- `ModifierEvaluator.cpp` 中乘算公式为：
  ```cpp
  val *= (1.0f + op.param_f32);
  ```
- 结果：`1.0f + 20.0f = 21.0f`！怪物血量瞬间翻了 21 倍，而不是 1.2 倍。

#### 2.2.2 归一化技术方案
保持 `affix.value` 在 `MapAffix` 和 UI 显示层继续作为百分比整数（20~100 方便格式化 `{value}%`），但在适配器层向求值器注入时统一进行归一化：
```cpp
// MapModifierAdapter.cpp: 注入求值器时统一缩放为乘算比率 (0.01)
const float normalizedValue = affix.value * 0.01f;
requests.push_back({recordId, true, normalizedValue, static_cast<uint32_t>(combatStat)});
```
- 共鸣词缀 `state.resonance.totalEnemyDensity * 0.05f` 本身已为比率（0.05 = 5%），保持原样。
- **平坦词缀例外**：`Enemy_Armor` / `Enemy_Dodge` / `Enemy_ArmorShred` 的描述模板是 `+{value}`（平坦值），不适用 `*0.01f`。离线生成器 `_build_map_records`（`scripts/gen_map_monster_modifier_v2.py:597-616`）当前对所有 `combatStat` 词缀统一产出 `ADD_STAT_PERCENT_MULT` 并按 `/100` 归一化，且运行时覆盖仅改写 `ADD_STAT_PERCENT_MULT`（`ModifierEvaluator.cpp:260-266`）。所以本阶段只处理百分比语义词缀，平坦词缀不挂接（详见 §2.6）。

#### 2.2.3 测试契约修正（重要）
`tests/unit/MapModifierAdapterTests.cpp:107-118` 现有断言形如 `100.0f * (1.0f + t1Value)`，其中 `t1Value` 直接取自 `CalculateValue`（未归一化的 20~100）。**该断言本身就是 Bug 的固化**，必须显式改写为归一化后的真实强度，而不是"调整成一个能过的新数字"：

```cpp
// 修正后：'of the Colossus' T1 = 20 → 归一化比率 0.2 → 生命乘区 1.2 → 100 → 120
const float t1Ratio = MapAffixRegistry::CalculateValue(type, 1) * 0.01f;
REQUIRE(result == Approx(100.0f * (1.0f + t1Ratio)));
```

---

### 2.3 构建图完备化：CMake 自定义命令驱动 `.bin` 生成

#### 2.3.1 现状与痛点
`assets/generated/modifier_runtime_v2.bin` 是本地生成物，被 `.gitignore` 排除（`.gitignore:74-75`）。当前它由 `build.bat` 在 precheck 阶段生成：`build.bat:313` 无条件执行 `python scripts\gen_modifier_runtime_v2.py`（生成模式），`:307` 另执行 `python scripts\gen_map_monster_modifier_v2.py --check` 作为数据漂移门禁。

当开发者在 VS / CLion / VS Code 中使用原生 CMake 构建，或者在 CI 中仅执行 `cmake --build build` 时，若本地缺失该文件，`src/app/Game.cpp:274-276` 会抛出 `std::runtime_error("ModifierRuntimeV2 load failed")` 导致应用启动崩溃。

#### 2.3.2 CMake 集成设计
在 `CMakeLists.txt` 中增加对 UMR 运行库的构建依赖链，对齐现有 `GenerateTags` 机制（`find_package(Python3 REQUIRED COMPONENTS Interpreter)` 已在根 `CMakeLists.txt:181` 存在；`NoMoreDayGameModifier` 已在 `src/game/systems/modifier/CMakeLists.txt` 中 `add_dependencies(... GenerateTags)`）：

```cmake
# --- UMR Modifier Runtime Blob Generation ---
set(MODIFIER_GEN_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/gen_modifier_runtime_v2.py")
set(MODIFIER_DATA_DIR "${CMAKE_SOURCE_DIR}/assets/data/modifier_v2")
set(MODIFIER_BIN_OUTPUT "${CMAKE_SOURCE_DIR}/assets/generated/modifier_runtime_v2.bin")
set(MODIFIER_DEBUG_OUTPUT "${CMAKE_SOURCE_DIR}/assets/generated/modifier_runtime_v2.debug.json")

# 注意：必须覆盖 canonical/ 子目录，否则 catalog 一旦引用该目录内的 JSON 会漏依赖。
file(GLOB_RECURSE MODIFIER_JSON_INPUTS CONFIGURE_DEPENDS
     "${MODIFIER_DATA_DIR}/*.json")

add_custom_command(
    OUTPUT ${MODIFIER_BIN_OUTPUT} ${MODIFIER_DEBUG_OUTPUT}
    COMMAND Python3::Interpreter ${MODIFIER_GEN_SCRIPT} --build
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS ${MODIFIER_GEN_SCRIPT} ${MODIFIER_JSON_INPUTS}
    COMMENT "Building UMR binary blob (modifier_runtime_v2.bin) from JSON assets..."
)

add_custom_target(GenerateModifierRuntime DEPENDS ${MODIFIER_BIN_OUTPUT})

# 中间层静态库直接依赖生成目标；上层 NoMoreDayApp / NoMoreDayTests 经链接传递依赖继承构建顺序。
add_dependencies(NoMoreDayGameModifier GenerateModifierRuntime)
```

#### 2.3.3 与 `build.bat` 的职责边界（必须写进实现）
本目标**不是**"取代 `build.bat`"，而是为无脚本环境兜底。为避免双写与门禁重复，约定：

- **唯一漂移门禁**：`build.bat:307/313` 的 `--check` 与生成调用保持不变；CMake 侧只负责"产物缺失/过期即生成"，不承担漂移校验。
- **确定性要求**：`gen_modifier_runtime_v2.py` 的两次输出必须字节一致；若 CMake 与 `build.bat` 在一次构建中先后执行，最终产物仍相同（脚本已按 `(priority, id)` 稳定排序，天然确定）。
- **接受源树产物**：`OUTPUT` 落在源码树 `assets/generated/`（该目录已 gitignore）。原生 CMake 构建下源树产物是最简单可靠的方案，本批次不引入"构建目录产物 + POST_BUILD 拷贝"的路径迁移。

---

### 2.4 怪物词缀数据单源化与热循环短路

#### 2.4.1 现状与两处前提缺陷
V1 曾假定"`AffixFlags.hasUpdate` 与运行时行为判断等价，可直接短路"。复核代码后该前提**不成立**，且存在一个更底层的既有缺陷。

**缺陷 A：`kAffixData` 按位初始化错移。**
`src/game/foundation/data/MonsterAffixRegistry.hpp:177-179` 声明为
```cpp
static constexpr std::array<MonsterAffixDef,
                            static_cast<size_t>(MonsterAffixType::Count)> kAffixData = {{ ... }};
```
但初始izer 列表只有 24 个条目。`MonsterAffixType`（`:20-59`）中 `Vortex`（`:45`）与 `Entangler`（`:46`）**在定义列表中缺失**（定义列表从 `Berserker` 直接跳到 `Avenger`，`:381`→`:393`）。按位聚合初始化因此把 `Avenger` 起的定义整体前移 2 个槽位，`Suppressor`（`:444`）与 `ManaSiphon`（`:456`）落空并被值初始化。而查找是纯按位索引：

```cpp
return kAffixData[static_cast<size_t>(type)].name_en;   // :162-164
const auto &def = MonsterAffixRegistry::GetAffixDef(type); // AddAffix, :499
```

结论：从 `Vortex` 起，`GetAffixDef()` / `GetAffixNameEn()` / `flags` 全部指向错误词缀。生成器 `_parse_monster_affix_defs`（`scripts/gen_map_monster_modifier_v2.py:369-429`）按 `MonsterAffixType::X` 名称解析，不受该错移影响，因此该缺陷只在 C++ 运行期暴露。

**缺陷 B：`AffixFlags` 与 UMR 行为算子表不是同一数据源。**

| 词缀 | `AffixFlags`（`kAffixData`） | UMR 行为算子（生成器 `MONSTER_*_BEHAVIOR_OPS`） | 是否一致 |
| :--- | :--- | :--- | :--- |
| `Suppressor` | `{false,false,false,true}`（:450） | `MONSTER_BEHAVIOR_SUPPRESSOR_UPDATE` | **否**（Update 行为会被短路掉） |
| `Avenger` | `{false,false,false,true}`（:399） | `..._AVENGER_ON_DEATH` | **否** |
| `MirrorImage` | `{true,true,false,true}`（:424） | `..._MIRROR_IMAGE_ON_TAKE_DAMAGE` | **否** |
| `SoulEater` | `{true,false,true,true}`（:437） | `..._SOUL_EATER_ON_DEATH` | **否** |
| `SoulLink` / `Berserker` / `ManaSiphon` / `Molten` / `Storm` / `Frozen` / `Teleporter` / `VoidZone` / `Waller` / `Shielding` | `hasUpdate=true` | 均在 `MONSTER_UPDATE_BEHAVIOR_OPS` | 是 |

现有门禁 `_validate_behavior_affix_classification` 只校验"带 flag 的词缀 ⊆ 已分类集合"，**不校验 flag 与算子表的一致性**，因此这类错位不会被 CI 拦截。

#### 2.4.2 修复方案（先单源化，后短路）
**Step 1（P1，前置）：补全并自检 `kAffixData`。**
- 在 `:381`（`Berserker`）与 `:393`（`Avenger`）之间补入 `Vortex`、`Entangler` 两条定义（`Vortex` → `hasUpdate=true`；`Entangler` → `hasOnHit=true`），使条目数与 `Count` 对齐；
- 增加编译期自检，杜绝再次发生按位漂移：
  ```cpp
  static constexpr bool ValidateAffixDataIds() {
    for (size_t i = 0; i < kAffixData.size(); ++i)
      if (static_cast<size_t>(kAffixData[i].id) != i) return false;
    return true;
  }
  static_assert(ValidateAffixDataIds(), "kAffixData must be positionally aligned with MonsterAffixType");
  ```

**Step 2（P1）：把行为语义收敛为单一事实来源。**
以生成器的 `MONSTER_*_BEHAVIOR_OPS` 算子表为唯一事实，修正上表中不一致的 `AffixFlags`（`Suppressor.hasUpdate=true`、`Avenger.hasOnDeath=true`、`MirrorImage.hasUpdate=false`、`SoulEater.hasUpdate=false`），并在生成器中增加**双向交叉校验**：

```
对每个词缀 A：flags(A).hasUpdate  == (A ∈ MONSTER_UPDATE_BEHAVIOR_OPS)
            flags(A).hasOnHit   == (A ∈ MONSTER_ON_HIT_BEHAVIOR_OPS)
            flags(A).hasOnDeath == (A ∈ MONSTER_ON_DEATH_BEHAVIOR_OPS)
```

不等即 `RuntimeError` 阻断生成。这样 `hasUpdate` 与运行时 `HasOnUpdate()` 由构造保证一致。

> 实现取舍：也可以由生成器直接产出 `MonsterAffixBehaviorTable.gen.hpp`（foundation 层，避免 data→modifier 的层级倒置），让 `AddAffix` 从生成表读取 flag。该方案单源性更强，但会新增一个生成产物；本轮采用"改数据 + 生成期交叉校验"，成本更低且同样能保证一致性。

**Step 3（P1）：热循环短路。**
在 `src/game/systems/combat/MonsterAffixSystem.hpp:94` 的 `for (auto entity : view)` 循环头部增加：
```cpp
auto &affix = view.get<MonsterAffixComponent>(entity);
if (!affix.hasUpdate) {
  continue; // 前置修复后该标志与 UMR 行为算子表一致
}
const auto &pos = view.get<Position>(entity);
const auto behaviorOps = MonsterModifierAdapter::EvaluateBehaviorOps(affix);
if (!behaviorOps.HasOnUpdate()) {
  continue;
}
```
- 短路语义与现状等价：`mirrorCooldown -= dt`（`:110-111`）与 `affix.timers[i] += dt`（`:127`）本就位于 `HasOnUpdate()` 门禁之后。
- **收益声明要诚实**：该优化只省下一次 `EvaluateBehaviorOps` 调用（含 request 构建与 `Evaluate`），不改变任何行为逻辑；Step 1/2 才是本轮的主要风险修复。

---

### 2.5 技能魔耗消费端闭环（`MANA_COST_MULT`）

#### 2.5.1 现状与断链分析
- `ModifierEvaluator` 已经完整实现了 `MANA_COST_MULT` 算子求值，并存入 `ModifierDelta::mana_cost_mult`。
- 装备词缀 `assets/data/modifier_v2/equipment_modifiers.json`（record id `1001001`）配置了 `MANA_COST_MULT`：`skill_id_whitelist=[1]`、`equip_slot_mask=2`、`param_f32=0.9`。
- 但在施法核心 `src/game/systems/skill/SkillSystem.cpp:2050` 与 UI 预览 `src/game/systems/skill/SkillDisplayPreviewService.cpp:24-36` 中，没有任何代码从 `EquipmentModifierAdapter` 查询魔耗倍率。

#### 2.5.2 统一结算接口设计
在 `EquipmentModifierAdapter` 中增加公共求值查询函数：
```cpp
// EquipmentModifierAdapter.hpp
[[nodiscard]] static float GetEquippedManaCostMultiplier(
    const entt::registry &registry, entt::entity entity,
    uint32_t skillId, Tag skillTags);
```
实现逻辑：
- 收集实体装备的 `recordIds`（复用现有 `CollectEquippedRecordIds`），空则直接返回 `1.0f`；
- 构建 `ctx`（`BuildContextFromCharacter(registry, entity, skillId, skillTags)`）；
- 调用三参重载 `ModifierEvaluator::Evaluate(ModifierRuntimeRegistry::Get(), std::span<const uint32_t>(recordIds), ctx)`（`ModifierEvaluator.cpp:426-439`）。注意**不存在** `(registry, span<uint32_t>, ctx, category)` 的四参重载，四参重载只接受 `span<const ModifierRecordRequest>`；此处无 override 需求，直接用三参重载即可；
- 返回 `delta.GetManaCostMultiplier(skillId)`。

`ModifierEvalContext` 的默认掩码是宽松的（`weapon_class_mask=0xFFFFFFFF`、`equip_slot_mask=0xFFFFFFFF`，`ModifierContext.hpp:97-104`），因此 `BuildContextFromCharacter` 不填槽位掩码也能通过 `equip_slot_mask=2` 的过滤。

#### 2.5.3 消费端接入点
1. **技能释放结算**：在 `SkillSystem.cpp:2050`（`base_cost` 计算后、幻影分身与暗影分身 hook 之前，使分身复制到已折算的成本）：
   ```cpp
   float base_cost = raw_mana_cost * (1.0f - std::min(0.9f, rcr));
   // UMR 装备魔耗倍率结算
   base_cost *= EquipmentModifierAdapter::GetEquippedManaCostMultiplier(
       registry, entity, slot.id, data->tags);
   ```
2. **UI 面板预览**：在 `src/game/systems/skill/SkillDisplayPreviewService.cpp` 的 `preview.display_mana_cost` 赋值（`:24-36` 的 if/else）**之后、`:39-42` 的 `CombatStats` 提前返回之前**接入：
   ```cpp
   preview.display_mana_cost *= EquipmentModifierAdapter::GetEquippedManaCostMultiplier(
       registry, player, skillId, skillData->tags);
   ```
   **边界语义（必须写明）**：放在提前返回之前，意味着玩家尚无 `CombatStats` 时预览仍显示含倍率的消耗；这是刻意的，因为 `GetEquippedManaCostMultiplier` 只依赖 `EquipmentComponent` 与 `ModifierRuntimeRegistry`，不依赖 `CombatStats`。若未来要求"无 CombatStats 时不显示词缀影响"，需另开设计。
3. `SkillDisplayPreviewService.cpp` 与 `SkillSystem.cpp` 需补充 `EquipmentModifierAdapter.hpp` 的 include。

#### 2.5.4 数据面覆盖说明
当前仅 `skill 1` 配置了 `MANA_COST_MULT`（`skill_id_whitelist=[1]`），因此本轮闭环的实际可观察效果只覆盖该技能。这是**数据面缺口而非代码缺口**，需在验收记录中注明，避免"闭环已通"的误判。

#### 2.5.5 已知性能注意点
`GetEquippedManaCostMultiplier` 每次施法都会 `CollectEquippedRecordIds`（新建 vector + 排序）并执行一次三参 `Evaluate`（内部使用 `ModifierOpCategory::All`，会遍历全部算子类别）。施法不是每帧路径，本批次接受该开销；若后续需要，可增加带 category 掩码的重载并缓存装备记录集合。

---

### 2.6 地图词缀战斗属性挂接（百分点语义批次）

#### 2.6.1 现状分析（V1 的关键误判）
`MapAffixRegistry.hpp` 中有部分词缀当前被设为 `StatType::Count` 哨兵，无法产出任何实际战斗属性。

**但仅修改映射并不足以生效。** 复核 `src/game/foundation/stats/AttributePipeline.cpp` 后确认：`mapEnemyDelta` 仅在以下位置被消费——
- `:373-376` 最大生命（经 `ModifierEvaluator::ApplyStat`）；
- `:392-395` 移速（同上）；
- `:520-525` 敌方普攻物理伤害。

抗性与暴击率**完全没有接入**：抗性由 `applyRes`（`:382-391`）用 `scaled.resistanceBonus*100 + NATIVE_RES(50)` 以 `ModifierMode::Flat` 写入；暴击率对所有实体统一为 `calcs[CritChance].base = DEFAULT_CRIT_CHANCE*100`（`:344`）。最终取值是 `:773-775`（暴击）与 `:797-805`（抗性，`rall` 为 `ResistAll.Result()`）。因此 V1 的"映射即生效"会导致**怪物 Delta 里有值、游戏内零效果**。

#### 2.6.2 语义决策：地图百分比词缀对"×100 刻度加算属性"按百分点解释
`ADD_STAT_PERCENT_MULT` 的数值语义是**乘算**（`AddPercentMult` 存 `1+value`，`ModifierEvaluator.cpp:282-289`），而覆盖路径只改写该算子（`:260-266`），所以生成器必须继续产出 `ADD_STAT_PERCENT_MULT` 才能保留 tier 滚值能力。但对抗性/暴击这类"`×100` 刻度、基础值可能为 0"的属性，乘算无法正确表达，也无法在基础值为 0 时生效（`(0+0)*1.25 = 0`）。

**决策**：在 `AttributePipeline` 中，敌方地图词缀对**暴击率、六系抗性、全抗性**按"百分点"解释，即把乘算 delta 还原为百分点增量：

```cpp
// 仅用于敌方地图词缀：把百分比 delta 解释为 ×100 刻度上的百分点增量。
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
```

在 `AttributePipeline.cpp` 的敌方分支（`:368-396`）中：
```cpp
// 暴击率：基础 5.0（×100 刻度），地图"精准之 +20%"→ +20 百分点 → 25.0 → 0.25
calcs[static_cast<size_t>(StatType::CritChance)].base =
    DEFAULT_CRIT_CHANCE * 100.0f +
    MapPercentPointsBonus(mapEnemyDelta, StatType::CritChance);

// 全抗性（棱镜之）：结果计入最终汇总的 rall 项（:797）
calcs[static_cast<size_t>(StatType::ResistAll)].base =
    MapPercentPointsBonus(mapEnemyDelta, StatType::ResistAll);

auto applyRes = [&](Tag tag, StatType t) {
  float val = bonus + (HasTag(raceData.resistances, tag) ? NATIVE_RES : 0.0f)
            + MapPercentPointsBonus(mapEnemyDelta, t);
  if (val > 0.0f) ApplyStatModifier(calcs, t, ModifierMode::Flat, val);
};
applyRes(Tag::Physical, StatType::ResistPhysical);
// ... 其余五系同现有写法
```
- 结果与描述模板 `+{value}%` 一致：`花岗岩之 +25%` → 抗性百分点 +25（叠加在原生/加成之上），而非某系为 0 时恒等于 0。
- 玩家不受影响：`mapEnemyDelta` 仅在 `isEnemy` 时求值（`:360-366`），非敌方时为空 delta，上述增量恒为 0。

#### 2.6.3 `combatStat` 映射表
在 `MapAffixRegistry.hpp` 中，将下列词缀的 `combatStat` 指向具体 `StatType`（名称必须与 `src/game/foundation/components/Stats.hpp:224-284` 的枚举逐字一致）：

| 词缀（枚举，`MapAffix.hpp`） | 显示名 | 原 combatStat | 升级后 combatStat | 说明 |
| :--- | :--- | :--- | :--- | :--- |
| `Enemy_ExtraHealth` | of the Colossus | `MaxHealth` | 不变 | 已是乘算语义 |
| `Enemy_ExtraDamage` | of Violence | `PhysicalDamage` | 不变 | 已是乘算语义 |
| `Enemy_Fast` | of Frenzy | `MoveSpeed` | 不变 | 已是乘算语义 |
| `Enemy_CritChance` | of Precision | `Count` | `StatType::CritChance` | 新增（百分点语义） |
| `Enemy_ResistAll` | of Prism | `Count` | `StatType::ResistAll` | 新增（V1 遗漏） |
| `Enemy_ResistPhys` | of Granite | `Count` | `StatType::ResistPhysical` | 新增 |
| `Enemy_ResistFire` | of Embers | `Count` | `StatType::ResistFire` | 新增 |
| `Enemy_ResistCold` | of Frost | `Count` | `StatType::ResistCold` | 新增 |
| `Enemy_ResistLight` | of Storms | `Count` | `StatType::ResistLightning` | 新增 |
| `Enemy_ResistPois` | of Venom | `Count` | `StatType::ResistPoison` | 新增 |
| `Enemy_ResistVoid` | of Null | `Count` | `StatType::ResistShadow` | 新增 |

**明确推迟：**
- `Enemy_Armor`（of Iron，`valT1=500`）、`Enemy_Dodge`（of Mist，`valT1=500`）、`Enemy_ArmorShred`（of Sundering，`valT1=5`）：描述模板为 `+{value}`（平坦语义），当前生成器与覆盖路径无法承载平坦值。
- `Enemy_CritResist`（of Adamant，爆伤减免）：`StatType` 中**不存在**对应枚举，且 `AttributePipeline` 无爆伤减免计算路径，需先补 `StatType` 与该计算，属跨模块改动。
- `Enemy_ExtraBarrier` / `Enemy_BarrierRegen`：不在本批次声明范围内（其 `StatType` 存在，但敌方护盾链路未评估）。

> 注：`Enemy_Dodge` 被推迟还有一个原因——`StatType` 中不存在 `Dodge`，闪避相关枚举为 `DodgeChance`（百分比）与 `DodgeRating`（平坦评级），而词缀描述为平坦。

#### 2.6.4 数据再生成与门禁
运行 `python scripts/gen_map_monster_modifier_v2.py` 更新 `assets/data/modifier_v2/map_modifiers.json`，再执行 `python scripts/gen_modifier_runtime_v2.py --build` 重编 `.bin`。生成器 `_build_map_records`（`:572-648`）只对 `combatStat != Count` 的词缀产出记录，映射改动会自动带来对应记录，且记录的 `param_u32` 与映射一致，覆盖路径可正常命中。

---

### 2.7 Schema V2 Constraints 仲裁（本轮推迟）

V1 计划在本轮落地 `exclusive_group` / `max_active` 仲裁。复核后**推迟到后续批次**，理由如下（均为代码实况）：

1. **没有可消费的数据**：`scripts/gen_map_monster_modifier_v2.py:448-449` 的 `_make_constraints()` 恒定返回 `{"exclusive_group": 0, "max_active": 0}`，map/monster 全部记录的 constraints 均为 0；`assets/data/modifier_v2/equipment_modifiers.json` 亦为 `{0,0}`。落地后不存在任何真实互斥组，机制只能靠手写测试数据触发。
2. **V1 的排序任务无效**：`MapModifierAdapter` 注入的全部地图记录共用 `MAP_PRIORITY = 400`（生成器 `:26`），monster 共用 `MONSTER_PRIORITY = 500`。按 priority 降序排序不会改变任何顺序，无法实现"最高优先级优先"，该任务应删除而非实现。
3. **语义尚未定义**：现有设计未定义"哪些词缀属于同一互斥组""`max_active` 的取值规则"，也未定义该仲裁是"单次求值请求序列内"还是"全局唯一生效"（`active_group_counts` 作为单次 `Evaluate` 的局部状态，跨适配器不共享）。在游戏规则缺位时实现仲裁属于凭空发明规则。
4. **二进制代价可避免**：`ModifierRuntimeRecord` 目前 `static_assert(sizeof == 24)`，`reserved` 字段全为 0。在无数据消费的情况下改写记录布局只会增加一处格式-版本风险（`format_version` 仍为 2，旧文件可被静默解析为新布局）。

**后续批次的前置条件**（本轮不动代码，仅记录）：
- 定义至少一个真实互斥组（例如"抗性类地图词缀互斥"），并让对应生成器产出非零 `exclusive_group` / `max_active`；
- 为地图记录定义可区分的 `priority`（当前全部相同），否则"优先"无意义；
- 明确仲裁范围与跨适配器语义；
- 若要落地，则同步升级 `ModifierRuntimeRecord` 布局（`reserved` → `uint16_t exclusive_group; uint16_t max_active;`，`RECORD_STRUCT` 由 `"<IIIIII"` 改为 `"<IIIIIHH"` 保持 24 字节），并考虑 `format_version` 的处理策略。

---

### 2.8 生成器单记录不变量断言强化

在 `scripts/gen_map_monster_modifier_v2.py` 与 `scripts/gen_modifier_runtime_v2.py` 中增加断言：
- 对任意 record，同一个 `target_stat` 下最多只能有 1 个 `ADD_STAT_PERCENT_MULT` 算子；
- 避免同一个 request 覆盖时引发多重冲突或歧义；
- 若违反断言直接在 precheck 阶段抛出 `RuntimeError` 阻断。

**执行顺序要求**：断言必须先对**存量 JSON** 跑一遍（`--check`）确认不违规。若存量数据已违反该不变量，需先修数据（或把断言降级为 warning 并登记待清理项），否则 Phase 6 门禁会因历史数据直接变红，掩盖本轮真正的修改。

---

## 3. 跨系统影响与兼容性评估

| 系统 | 影响评估 | 兼容性保证 |
| :--- | :--- | :--- |
| **存档系统** | `ItemPersistenceCodec` 编码侧增加 64 上限保护 | 不改变 Section 3 存储布局，完全向前兼容 `ITEM_STORE_BINARY_VERSION = 2` |
| **地图与怪物难度** | 地图词缀滚值从 `* (1 + value)` 变为 `* (1 + value * 0.01)` | 修复了怪物生命和伤害暴涨 20+ 倍的异常，恢复正常设计难度曲线 |
| **怪物战斗属性** | 敌方暴击率与六系抗性新增消费 `mapEnemyDelta` 的百分点路径 | 玩家路径不受影响（`mapEnemyDelta` 仅敌方求值）；抗性受 `Cap::RESISTANCE` 约束，暴击受 `Cap::CRIT_CHANCE` 约束 |
| **怪物词缀行为** | `kAffixData` 补全 + `AffixFlags` 对齐 UMR 算子表 | 修正了 `Vortex` 起的定义错移；`Update` 短路仅省求值，不改变行为分支 |
| **技能系统** | 技能释放时扣除的法力受装备 UMR 词缀乘算影响 | 仅对装备了相关词缀的玩家生效，未装备时倍率为 1.0f，无任何副作用 |
| **构建系统** | CMake 自动生成 `.bin` 产物 | 不破坏既有 `build.bat` 流程；`build.bat` 仍是唯一漂移门禁 |
| **运行时二进制** | 本轮**不修改** `ModifierRuntimeRecord` 布局 | 四表偏移、24 字节 POD、`format_version = 2` 全部保持不变 |

---

## 4. 验证方案与退出准则 (DoD)

1. **门禁校验通过**：
   - `python scripts/gen_map_monster_modifier_v2.py --check` 退出码 0（含新增的行为 flag ↔ 算子表交叉校验、单记录乘算不变量）。
   - `python scripts/gen_modifier_runtime_v2.py --check` 退出码 0。
   - `build.bat RelWithDebInfo` 退出码 0。
2. **单元测试与回归**：
   - **编码侧上限**：向 `ItemSideTableData` 注入 65 条记录后 `ItemPersistenceCodec::encode(...)` 返回 `false`（fail-closed，不产生任何分段写出）；64 条成功往返。
   - **地图量纲**：`Enemy_ExtraHealth`（T1=20）在基数 100 时输出 `120.0f`（而非 `2100.0f`）；断言使用 `CalculateValue(type,1) * 0.01f` 归一化后的真实强度。
   - **地图战斗属性生效**（不再是"仅产生 Delta"）：对带 `Enemy_ResistPhys`（T1=25）的地图施加后，敌方 `CombatStats::resistances[Physical]` 相比基线提升 0.25（在 cap 内）；带 `Enemy_CritChance`（T1=20）时 `crit_chance` 由 0.05 提升至 0.25。
   - **词缀数据对称**：`kAffixData[i].id == MonsterAffixType(i)` 编译期断言生效；`Vortex`/`Entangler` 的 `GetAffixDef` 返回正确名称与 flags。
   - **魔耗结算**：装备带 `MANA_COST_MULT`（0.9）词缀后，技能实际消耗为 `baseCost * 0.9`；未装备时为 `baseCost`。
   - **怪物 Update 短路**：对无 Update 词缀的怪，验证系统跳过行为求值；对 `Suppressor` 验证其 Update 行为**仍会执行**（回归防护）。
3. **全量套件验证**：
   - `ctest --test-dir build -C RelWithDebInfo -L unit --output-on-failure` 100% 通过；新增用例必须挂上 `unit` 标签。若标签与既有约定不一致，需在计划中确认真实标签名，避免漏跑。
