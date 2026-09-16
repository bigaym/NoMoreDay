# UMR 技能专精改造第一批（裂空斩/灵剑决）实施评审报告

- **审查目标**：UMR 技能专精改造第一批（裂空斩 / 灵剑决）实施包的最终实施评审。
- **结论**：**修改**
- **审查轮次**：第 1 轮（实施评审判定）
- **审查性质**：独立审查（未参与实现），只读检视，未修改任何被测源码/数据/脚本/测试。

---

## 1. 输入

| 类别 | 路径 |
| --- | --- |
| 审查流程与判定规则 | `docs/workflows/review.md` |
| 代码标准 | `conductor/code_standard.md`（V2.1）、`conductor/code_styleguides/` |
| 设计规格 | `docs/designs/2026-09-16-umr-skill-batch1-rending-wave-blade-formation-design.md`（354 行，状态「待审阅 v2」） |
| 实施计划 | `docs/plans/2026-09-16-umr-skill-batch1-rending-wave-blade-formation-plan.md`（213 行，状态「待审阅 v2」） |
| 先例 | `docs/designs|plans/2026-09-16-umr-skill-spec-foundation-and-skill1-slice-*.md`、`docs/reviews/2026-09-16-umr-skill-spec-foundation-and-skill1-slice-review.md` |

**实施方提供的验证证据（本次逐项独立复核，见 §5）**：`build.bat RelWithDebInfo` EXIT=0；C++ 1721 用例 1720 通过（1 项粒子性能用例失败）；5 个 python 门禁全 OK；`ModuleBoundaryCheckerTest` 18 项既有失败（本次未改动相关文件）。

---

## 2. 变更文件边界

`git status --short` 与本包声明清单 **完全一致**，无未声明的权限触及、无禁用消费者改动、无迁移/重构外溢：

修改（13）：
```
src/game/systems/modifier/ModifierContext.hpp
src/game/systems/modifier/ModifierEvaluator.hpp
src/game/systems/modifier/ModifierEvaluator.cpp
src/game/systems/modifier/SkillSpecModifierAdapter.cpp
src/game/systems/modifier/TalentModifierAdapter.cpp
src/game/systems/modifier/MapModifierAdapter.cpp
src/game/systems/skill/SkillSpecializationBaker.cpp
tests/unit/SkillSpecializationBakerTests.cpp
assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json
assets/data/modifier_v2/skill_spec_modifiers.json
assets/data/skill_mechanics.json
assets/data/skill_mechanics_schema.json
scripts/gen_modifier_runtime_v2.py
scripts/migrate_skill_spec_modifier_slice.py
```
新增（4）：`scripts/validate_skill_spec_modifiers.py`、`tests/python/SkillSpecBatch1GateTest.py`、`tests/unit/SkillBatch1DeliveryOpTests.cpp`（第 4 项为上述生成物文件之一）。

`git diff --numstat` 关键事实：`tests/unit/SkillSpecializationBakerTests.cpp +267/-0`（**纯新增，未删除/弱化任何既有断言**）；`SkillSpecializationBaker.cpp +54/-48`；数据侧 `canonical +473/-0`、`skill_spec_modifiers.json +374/-0`、`skill_mechanics.json +1/-22`、`skill_mechanics_schema.json +0/-50`。**变更集内不含** `src/game/fx/`、`tests/performance/`、`scripts/check_module_boundaries.py`、`tests/python/ModuleBoundaryCheckerTest.py`。

**已直接检视的文件**：设计、计划、先例评审报告、上述全部源码/数据/脚本/测试文件（含 `tests/TestCommon.hpp`、`tests/CMakeLists.txt`、`SkillMechanicsRegistry.hpp`、`BladeFormation.cpp`、`RendingWaveNodes.cpp`、`BladeFormationNodes.cpp`、`DamageMitigationService.cpp`、`SkillProfileResolve.cpp`、`modifier_catalog.json`）。

---

## 3. 范围对齐

| 设计目标 | 对齐情况 |
| --- | --- |
| §1.2(1) 4 新算子 37/38/39/40 | ✅ 语义、类别、缩放逐条一致 |
| §1.2(2)(3) 技能 2/3 线性数值迁入 canonical + Baker 硬编码退役 | ✅ 10 个节点分支退役，8 键删除（见 §7.1） |
| §1.2(4) 单一事实源治理（P0，必须机器校验） | ⚠️ 部分未达成（F-1、F-2） |
| §1.2(5) 等价性验证（既有测试 100% 通过 + precheck 三检） | ✅ 已复现 |
| §1.3 非目标（不触碰技能 4~12、不动 Schema/二进制头/紧凑寻址、301 显式例外保留） | ✅ 无越界 |
| §3.3 结算顺序（flat→mult、烘焙步骤 3→4） | ✅ 已固化并被测试锁定 |
| **Task 5.4（原计划标为「可选」）** | ✅ 已纳入且行为等价（先例 N-1 授权项） |

---

## 4. 质量与风险评估（对照 code_standard）

| 维度 | 评估 |
| --- | --- |
| §2.1 性能 / 热路径 | ⚠️ 见 F-4（烘焙路径步骤 4 构建 `std::string`）；其余无新增热路径分配 |
| §2.2 内存安全 / UB | ✅ 无裸 `new/delete`、无 UAF/Dangling、无未初始化读；`reinterpret_cast` 仅出现在测试序列化助手的合法字节视图（`SkillSpecializationBakerTests.cpp:40`，unsigned char 别名合法） |
| §5.3 EnTT 约束 | ✅ 变更未引入「跨组件增删操作持有组件指针」；步骤 4/步骤 3 均为 POD 栈上计算 |
| §6.1 类型转换 | ✅ 无 `dynamic_cast`、无 C 式转换 |
| §7.2 字符串分支 | ⚠️ 见 F-4（技能 2 节点 210 惩罚键由 `std::to_string` 动态拼接）。设计 §3.3 **显式给出该键构造方式**，且 `SkillMechanicsRegistry` 本身即字符串键表 + 透明哈希，故**不构成硬否决第 7 条**，按 Low/Best Practice 记录 |
| §8.1 并发 | ✅ 无裸 `std::thread`；无共享可变状态新增 |
| 确定性 | ✅ 步骤 3/4 全部为按键查表与定点运算，无容器迭代序依赖；`MergeFrom` 语义确定 |
| 数据完整性 | ⚠️ F-2（死键白名单）、F-1（门禁锚点缺失） |
| 验证证据充分性 | ⚠️ F-2、F-3；其余充分（§5） |

**硬否决清单逐条核对：均未触发。** 特别地：无假测试（无 `REQUIRE(true)`/`CHECK(true)`/`CHECK_FALSE(false)`/`doctest::skip`）；本次**未删除或弱化任何既有用例**（`+267/-0`）；未触及他人权限边界；未隐藏失败；无 UB/内存泄漏；无裸线程；无 `dynamic_cast`/C 式转换。

---

## 5. 独立核验的证据（命令与观测）

### 5.1 构建与测试
- `bin\NoMoreDayTests.exe --test-case=*SkillSpecializationBaker*` → **12 用例 / 241 断言全部通过**。
- `--test-case=*SkillBatch1*` → **6 / 6 通过（23 断言）**（确认新用例已编入二进制，总数 1721 与简报一致）。
- `--test-case=*RendingWave*` → **23 / 23 通过（213 断言）**。
- `--test-case=*SkillSpec*` → 31 / 31 通过（535 断言）；`--test-case=*Skill 3*` → 33 / 33 通过（507 断言）。
- **粒子性能用例独立复测**：`--test-case=[Performance] ParticleTrail*` → **4/4 通过**，日志 `Scenario 4 ... Overhead=0.147ms (Target: < 0.2ms)`；`git diff HEAD --stat -- tests/performance/ src/game/fx/` **为空**。⇒ 简报中「全量运行 0.52~0.64ms 失败」判定为**环境/顺序敏感性波动**成立，与本变更正交。

### 5.2 门禁真实性（构造反例证伪，仅内存内变异，未写入仓库）
`validate_skill_spec_modifiers.py --check` 6 项 OK；**逐项注入反例后均被捕获**：
| 检查 | 反例 | 观测结果 |
| --- | --- | --- |
| `check_record_id_decode` | 非豁免记录节点白名单改为 9999 | `record 2001010: decoded node 101 != node_id_whitelist[0] 9999` |
| `check_projectile_value_integer` | `SKILL_PROJECTILES_ADD` param_f32=0.5 | `record 2002100: ... param_f32 0.5 must be an integer` |
| `check_retired_keys_absent` | 回填 `3/310.range_pct_per_point` | `retired mechanics key still present: 3/310.range_pct_per_point` |
| `check_migration_equivalence` | canonical `2003100` 改为 0.25 | `canonical value 0.25 != frozen expected 0.2` |
| `check_registry_reverse` | 新增 `3.310.mystery_key` | `unregistered mechanics key under migrated node: 3/310.mystery_key` |
| `check_skill_delivery_domain` | 合成 catalog 中 talent 记录带 `SKILL_RANGE_MULT`/`debug.source=talent_tree` | `opcode SKILL_RANGE_MULT requires debug.source='skill_spec_node' (got 'talent_tree')` |

⇒ 5 项门禁为**真实有效断言**，非空转；`modifier_catalog.json` 覆盖 5 个域（非局部扫描）。唯一不足见 F-1（锚点覆盖不全）。
补充：`gen_skill_spec_modifier_contract.py --check`、`gen_modifier_runtime_v2.py --check`、`gen_skill_mechanics_schema.py --check`（`OK: 472 entries, 71 unreferenced`）、`validate_json.py`、`python -m unittest tests.python.SkillSpecBatch1GateTest`（OK）、`check_legacy_reintroduction.py`（`PASS: no marker/classification regression detected`）均 EXIT=0。

### 5.3 数值等价性（与 HEAD 逐式比对）
- 技能 2：旧 `200` `*(1+0.1*N)` 半径与射程、`201` `max(0, cost-1.0*N)`、`202` `*(1+0.10*N)`、`210` `projectile_count += N` + 0.75/0.80/0.85 惩罚 → 与 canonical `2002000/2002001/2002010/2002020/2002100` + 步骤 4 读 `damage_penalty_pt1/2/3`（0.25/0.20/0.15）**逐式一致**。
- 技能 3：旧 `300 += N`、`302 (1+0.10N)`、`310 range=200*(1+0.2N)`、`312 max(0,1-0.05N)`、`331 crit += 0.05N`、`332 crit_dmg += 0.25N` → 与 canonical 六条记录一致；旧「先算 311/330 覆盖再算 UMR」的顺序 `(3+N)*2` 与新「步骤 3 `+=` → 步骤 4 `*=2`」**等价**，无双重缩放或缩放丢失。
- **最强证据**：未被本变更修改的既有功能测试原样通过，且断言精确命中迁移后数值 —— `tests/functional/RendingWaveNodes.cpp:98-182`（半径 49.0 / 射程 700.0 / 法耗 7.0 / more 1.50 / 投射物 4 + 0.85）、`tests/functional/BladeFormationNodes.cpp:418-445`（302=0.65、与 330 复合=1.875）、`:607-624`（310 → 320.0）。这构成「无行为回归」的独立证明。

### 5.4 Task 5.4 掩码收紧的等价性
- 全部 11 个 `skill_*` 容器 getter 的消费方**仅** `SkillSpecializationBaker.cpp:182-195`（另有 `EquipmentModifierAdapter.cpp:261` 用 `GetManaCostMultiplier`）。
- `rg ModifierOpCategory::All` 在 `src/` 下**已无任何调用点**；6 个适配器均为显式掩码，技能交付路径仍传 `SkillDelivery`（`SkillSpecModifierAdapter.cpp:199`）。
- `SkillSpecModifierAdapter::ResolveDamageRecordIds` 只收 `ADD_STAT_PERCENT_MULT && param_u32==PhysicalDamage` 且节点白名单相交的记录，11 条新记录永远不被它收集 ⇒ 收紧对 `EvaluateDamageMultiplier` **行为中性**，无漏洞。

### 5.5 机制位与数据完整性
- 技能 2 `case 2`、技能 3 `case 3` 逐节点核对：`feature_flags` 全部机制位（256/1/512/16/32/64/2/1024/2048/4096/8192/4/16384/8/32768/65536）、元素转质（370/372）、触发契约、形态分支（350~355）**均完好，无机制位误删**。
- 删除精确性：`git diff -U0 assets/data/skill_mechanics.json` = 1 插入 / 22 删除，**逐行核对即 9 个目标键（含 `3/312` 仅删 `cost_reduction_pct_per_point`、`3/331` 仅删 `crit_chance_per_point`）**，无连带删除。
- 保留项完好：`2/210 damage_penalty_pt1/2/3 = 0.25/0.20/0.15`（步骤 4 唯一读取）、`3/301 haste_pct_per_point = 8.0`（`SkillSpecializationBaker.cpp:446-449` 读取，逻辑未变）；另核实 `mana_regen_per_sword_per_point`（`BladeFormation.cpp:115`）、`splash_radius`（`BladeFormation.cpp:705`）、`3/300 duration`（`BladeFormation.cpp:245`）均有真实读取方。
- Schema 重生成与源码读取点一致：`dynamic_keys` 7→2（`haste_pct_per_point`、`speed_bonus_pct`），退出项对应「由变量解析的删键调用点」；生成器仅扫描 `src/` 且依赖 `GetMech/GetFloat/GetInt` 调用点，语义自洽，非漂移。
- 无残留双源：被删 9 键在 `src/` 下对技能 2/3 已无任何读取方（其余命中均为技能 5/6/7/10 的同名不同键）。

### 5.6 测试质量与状态隔离
- 新增用例均为真实断言（逐行读过），覆盖设计 §5.1 的算子缩放、累乘、`MergeFrom` 4 容器、烘焙组合（210 投射物 +4 / 0.85x、300=3+4=7、复合 300+311=14、300+330=1 与 80 半径）、基准值（skill3 `del.range==200.0f`）、以及「flat 先于 mult」的顺序锁定用例（8.10 vs 8.00 反推）。
- 状态隔离：`tests/TestCommon.hpp` 的 `TestSetupScope` 在构造/析构两侧都执行 `ResetSkillRegistries()`（含 `SkillMechanicsRegistry::ResetForTests()` + 重载 `assets/data/skills.json`），故 `Node 210 Penalty Reads Mechanics` 写临时机制表不会跨用例泄漏；`Mana Flat Then Mult Order` 用 `LoadFromBytes` 注入后显式 `REQUIRE(ReloadModifierRuntimeFromAsset())` 复原；临时 JSON 用完 `std::filesystem::remove`。**未发现跨用例污染**。

### 5.7 复用与不重复造轮子
- 既有 `tests/python/SkillSpecModifierMigrationTest.py`（迁移脚本）与 `ModifierCanonicalSchemaValidationTest.py`（canonical schema）职责与新校验脚本**不重叠**；`SkillSpecBatch1GateTest.py` 仅为新门禁脚本的薄封装，符合仓库「一门禁脚本一 Test.py」既有约定（现存 10 个同类测试同样为手动 unittest，未接入 CTest）。
- 新校验脚本未重复契约生成器的 canonical↔runtime 一致性职责（脚本内 `RUNTIME_JSON_PATH` 声明后未被任何检查使用）。

---

## 6. 发现项

### F-1（Medium，必修）迁移等价门禁缺少 2 条新记录的锚点
- **位置**：`scripts/validate_skill_spec_modifiers.py:46-58`（`MIGRATION_EQUIVALENCE`，9 行）、`:220-231`（`check_migration_equivalence`）
- **观测**：设计 §4.3 门禁 1 要求「对**每条**新 canonical 记录」给出期望关系。当前表覆盖 9 条，**遗漏 `2002100`（sk2/210 `SKILL_PROJECTILES_ADD` 1.0）与 `2003000`（sk3/300 `SKILL_PROJECTILES_ADD` 1.0）**。两条投射物记录只受 `check_record_id_decode` / `check_projectile_value_integer` 的「ID 正确」与「值为整数」约束，**无任何检查绑定其数值本身**。我以 `2003100` 变异（0.2→0.25）验证门禁会捕获，但把 `2002100/2003000` 的 `value` 改为 2.0 时 6 项检查**全部通过**。
- **违反依据**：设计 §4.3 门禁 1；§6 风险表将「单一事实源被破坏」列为 **High** 并明确「不得仅以文档约定代替机器校验」。
- **建议修法**：为两条补锚定行。`2003000` 可复用 `3/300.swords_per_point`（relation 取 `raw`：`canonical.value == mechanics`，期望 1.0）；`2002100` 旧源为 C++ 字面量，取 relation `literal`（冻结期望 1.0）。注意 `check_retired_keys_absent`（`:278-320`）由该表推导退役键集合，`literal` 行若不携带 mechanics 键，需让退役键检查跳过无键行，避免把检查做成空转。修复后应补一条 `--check` 反例自测（把上面两条的值改 2.0 必须被捕获）。

### F-2（Medium，必修）`3/300.swords_per_point` 成为被门禁「认证保留」的死键，破坏 P0 单一事实源
- **位置**：`assets/data/skill_mechanics.json:162`（`swords_per_point: 1.0`）；`assets/data/skill_mechanics_schema.json:2465`（`unreferenced` 仍登记）；`scripts/validate_skill_spec_modifiers.py:60-68`（`KEPT_MECHANICS_KEYS` 显式把 `(3, 300, "swords_per_point")` 列为**保留活键**）；设计 §4.3 处置表**无 3/300 行**
- **观测**：`rg swords_per_point src tests` 零命中（仅门禁白名单本身引用）；`git grep swords_per_point HEAD` 仅命中 JSON/schema ⇒ 该键在**本批之前即为死数据**。但本批新建立了它的 canonical 替代 `2003000`（设计 §4.3 明确「3/300 由 PROJECTILES_ADD 承载」/ `1.0 = +1/point`），同时把该键写入门禁白名单「保留」集合，等于**用机器门禁固化了双源**：后续维护者可改 `swords_per_point` 而无任何效果，且门禁不会报错 —— 正是设计 §1.2 目标 4 要杜绝的「改数据文件却无效果」。
- **违反依据**：设计 §1.2 目标 4（P0，同类线性数值同名键**必须退役**）；§4.3 门禁 3（反向登记 / 新增遗留键一律报错）；§6「单一事实源被破坏 = High」。
- **建议修法（二选一，需在设计与门禁间取得一致）**：
  1. **推荐**：退役 `3/300.swords_per_point`（同批补 F-1 的 `2003000` 锚定行，即可被 `check_retired_keys_absent` 自动守护）；`3/300.duration` 必须保留（`BladeFormation.cpp:245` 真实读取）。同步删除 `KEPT_MECHANICS_KEYS` 中该项、重生成 schema，并在设计 §4.3 补 3/300 行。
  2. 若确需保留，需在设计 §4.3 将 `swords_per_point` 显式登记为「已退役/保留例外」并说明保留理由与未来消费方；不得仅在门禁白名单中静默保留。
  > 说明：本项为「设计与实现之间的不一致」（设计总则要求删、处置表漏登记），不构成新建设计范围。

### F-3（Medium，必修）设计 §5.1(1) 强制要求的「法耗下限 0」断言缺失
- **位置**：`tests/unit/SkillBatch1DeliveryOpTests.cpp:69-86`（注释 `:72`「下限 0 由消费端（烘焙）负责，本例只断言累加值」、`:85`「减免叠加不在此钳制，消费端再对法耗取 max(0, ...)」）；消费端钳制位于 `src/game/systems/skill/SkillSpecializationBaker.cpp:179-183`
- **观测**：算子级用例**显式声明**不做下限断言，而烘焙侧新增用例的输入（技能 2 基础 10、flat -2；技能 3 基础 25、mult 0.95）均为正，`std::max(0.0f, (cost + flat) * mult)` 的下限分支**无任何用例触达**。设计 §5.1(1) 明确列入「`SKILL_MANA_COST_FLAT` 负值减免**与下限 0**」。
- **违反依据**：设计 §5.1 测试矩阵；计划 DoD「覆盖 §5.1 测试矩阵 100%」；review.md §禁止内容「缺失的必备行为或未验证的关键路径」。
- **建议修法**：补一条烘焙用例（或复用 `BuildManaFlatAndMultRuntimeBlob` 的合成 blob 造 flat=-20 + 基础 10）断言 `effective_mana_cost == 0.0f`。注：该分支当前为**防御性**（节点点数上限下难以自然触达），故风险具界，但设计已将其列为必备覆盖项。

### F-4（Low）步骤 4 惩罚键使用 `std::string` 拼接，落回热路径分配
- **位置**：`src/game/systems/skill/SkillSpecializationBaker.cpp:213-217`
- **观测**：`const std::string key = "damage_penalty_pt" + std::to_string(std::clamp(...));` 产生 17 字符临时串（超出 MSVC SSO 15 字节 ⇒ 堆分配）；被替换的旧代码（`(points==1)?0.75f:...`）零分配，且**同文件相邻调用点**（`:446`、`:763`、`:771`、`:684`、`:687`）均使用字面量键，全仓仅此一处动态拼接。烘焙可达路径：`DamageMitigationService.cpp:136`（每次伤害结算的 `profile` 缺失回退）、`SkillProfileResolve.cpp:22`（缓存未命中）。
- **依据**：`code_standard` §2.1（热路径堆分配）+ §7.2（核心玩法避免字符串键分支）。设计 §3.3 显式给出该键构造写法 ⇒ **非硬否决**，按低风险记录。
- **建议修法**：改为 `static constexpr const char *kPenaltyKeys[4] = {"", "damage_penalty_pt1", "damage_penalty_pt2", "damage_penalty_pt3"};` 取 `kPenaltyKeys[std::clamp(pts,1,3)]`，保留 `GetFloat` 默认值语义；若评审认为需保持设计逐字一致，则在设计中登记该项为已知设计债。

### F-5（Low）范围注释漂移：仍写「opcode 30..36」
- **位置**：`src/game/systems/modifier/SkillSpecModifierAdapter.hpp:32`、`src/game/systems/modifier/SkillSpecModifierAdapter.cpp:194`（**本批已改文件**）、`src/game/systems/modifier/EquipmentModifierAdapter.cpp:131`、`src/game/systems/modifier/ModifierEvaluator.hpp:91`
- **观测**：本批把交付算子扩展到 30..40（`ModifierContext.hpp` 注释已更新为 30..40，`TalentModifierAdapter.cpp:101`/`MapModifierAdapter.cpp:114` 的新增注释也正确写为 30..40），但上述 4 处仍描述旧范围，其中 `SkillSpecModifierAdapter.cpp:194` 落在本次改动文件内。
- **建议修法**：统一改为 30..40（同批顺手修正，不构成阻塞）。

### F-6（Low）设计/计划文档状态未随实施推进更新
- **位置**：`docs/designs/2026-09-16-umr-skill-batch1-rending-wave-blade-formation-design.md:3`、`docs/plans/2026-09-16-umr-skill-batch1-rending-wave-blade-formation-plan.md:3` 均为「待审阅（v2）」
- **观测**：实施已完成并进入评审，而先例（skill1 切片）在评审判定后状态改为「已按评审意见修订（2026-09-16），待复审」。此外设计 §4.3 处置表缺 3/300 行（见 F-2）。
- **建议修法**：本轮按判定结果统一更新设计/计划状态并补齐 §4.3 行；关联 F-1/F-2/F-3 的收敛结论。

### F-7（Low）新门禁未接入自动执行链
- **位置**：`scripts/validate_skill_spec_modifiers.py`（未被 `build.bat` precheck 调用；`tests/CMakeLists.txt` 亦无对应 add_test）
- **观测**：`build.bat` 的 `ENABLE_PRECHECKS` 段仅调用 `check_worktree_mapping / check_legacy_reintroduction / check_module_boundaries / generate_gpu_abi / check_no_manual_abi_structs / validate_json / gen_map_monster_modifier_v2 --check / check_monster_behavior_dispatch / gen_modifier_runtime_v2`；`rg validate_skill_spec_modifiers` 全仓仅命中脚本自身与其 `*Test.py`。设计 §1.2 目标 4 称「新增离线一致性门禁」，目标 5 的「precheck 漂移门禁三检」也未包含它。既有 10 个 python 门禁测试同为手动调用，故**不属本批缺陷**，但门禁仅在人工调用时生效，防回潮力度弱。
- **建议修法**：将 `validate_skill_spec_modifiers.py --check` 与其配套的 `gen_skill_mechanics_schema.py --check` 纳入 `build.bat` precheck 段（低风险、纯只读检查）。

---

## 7. 最佳实践建议（非阻塞）

- **B-1**：`ModifierEvaluator.hpp:97,109` 的 `ModifierOpCategory::All` 缺省参数在本批收紧后**已无任何调用点**，但保留的缺省值会把先例 N-1「SkillDelivery 向非技能调用点泄漏」的隐患留给未来调用者。可考虑将缺省改为 `Stats|Events|Behavior`，或干脆去掉缺省值以强制显式传掩码。
- **B-2**：`ModifierDelta` 容器由 11 增至 15，已在设计 §6 登记为架构设计债，建议继续跟踪（如后续批次做容器分组或 SOA 化）。
- **B-3**：步骤 4 技能 2 惩罚的默认值 `0.0f`（`SkillSpecializationBaker.cpp:216-217`）使「机制表缺键/键名错误」静默表现为「无惩罚」，相对改造前（自足字面量）是行为上的静默弱化；若希望保留告警能力，可在调试配置下对缺键 `WARN` 一次。
- **B-4**：`SkillSpecializationBakerTests.cpp:40` 的 `reinterpret_cast` 属测试二进制序列化边界，建议补一行注释说明理由（`code_standard` §6.1 精神）。

---

## 8. 剩余风险与未验证项

| 项 | 说明 | 状态 |
| --- | --- | --- |
| 粒子性能用例波动 | `[Performance] ParticleTrail` Scenario 4 在全量运行中 0.52~0.64ms 失败、独立运行 0.147ms 通过；`src/game/fx/` 与 `tests/performance/` 本次零改动 | **接受**（既有环境性波动，非本包风险） |
| `ModuleBoundaryCheckerTest` 18 项失败 | 因 `scope.candidate_roots do not match checker policy`；`scripts/check_module_boundaries.py` 与 `tests/python/ModuleBoundaryCheckerTest.py` 本次未改动 | **接受**（既有失败，与本包无关） |
| F-2 死键 | `swords_per_point` 无行为影响（无读取方），但门禁白名单认证其「保留」 | 待收敛（F-2） |
| 节点 201 点数上限 | 未从 `mastery_skill_trees.json` 提取到节点满点，故 F-3 的「下限 0 分支是否可达」未定量确认；不影响结论（无论可达与否，所需断言均缺失） | **未验证项**（低影响） |
| `ModifierDelta` 膨胀 / `All` 缺省 | 设计已登记为债 | **接受** |
| 修订后回归 | F-1~F-3 修复后需重跑：`NoMoreDayTests.exe --test-case=*SkillBatch1*`/`*SkillSpecializationBaker*`、`validate_skill_spec_modifiers.py --check` + 反例自测、`gen_skill_mechanics_schema.py --check`（若动 `skill_mechanics.json`） | 待复验 |

**未触发硬否决**；未发现 Blocker / High。

---

## 9. 下一步动作

判定 **修改**。必需修改项（按优先级）：

1. **F-2**：收敛 `3/300.swords_per_point` 的双源 —— 退役该键（推荐，含 schema 重生成、门禁白名单移除、设计 §4.3 补 3/300 行），或在设计 §4.3 显式登记为保留例外并说明理由。
2. **F-1**：为 `2002100`、`2003000` 补迁移等价锚定行，并加反例自测（值改为 2.0 时必须被门禁捕获）。
3. **F-3**：补「`SKILL_MANA_COST_FLAT` 负值减免与下限 0」用例（烘焙侧断言 `effective_mana_cost == 0.0f`）。
4. **F-5 / F-6**：顺带修正 4 处 `30..36` 注释漂移，并更新设计与计划文档状态（含 §4.3 行补齐）。

修改完成后，请按 §8「修订后回归」复跑相关门禁与用例，并在本报告追加第 2 轮审查小节（保留本轮记录，不得删除）以供复验判定。

---

# 第二轮复核（实施评审判定）

- **复核目标**：独立复核首轮 F-1~F-6 的修订是否真正闭环（不轻信实施方结论）。
- **复核范围**：`git status --short` / `git diff --numstat` 核准的边界（与首轮清单一致，仅 F-5 涉及文件数增加：新增 `EquipmentModifierAdapter.cpp`、`SkillSpecModifierAdapter.hpp` 两处注释修正）；逐条阅读修订后的门禁脚本、数据、Baker 代码与测试；**独立复现篡改/回填反例**。
- **本轮结论**：**提交**

## 1. 逐条复核

### F-1 ✅ 闭环（迁移等价锚点补齐，门禁非空转）
- `scripts/validate_skill_spec_modifiers.py:43-62`：`MIGRATION_EQUIVALENCE` 现为 **11 行**，新增关系文档（`percent` / `flat_negate` / `raw` / `literal`）与两条锚点：`:55` `(2002100, 2, 210, None, "literal", 1.0)`、`:56` `(2003000, 3, 300, "swords_per_point", "raw", 1.0)`。
- **一一对应核验**：按 `_canonical_index` 口径枚举 canonical 中迁入节点 {200,201,202,210,300,302,310,312,331,332} 的记录，得 `[2002000,2002001,2002010,2002020,2002100,2003000,2003020,2003100,2003120,2003310,2003320]`，与表内 11 个 ID **完全相同**（`missing in table: []`、`extra in table: []`），并与设计 §4.2 的 11 条记录一致（无遗漏、无多余）。
- **独立篡改复现**（内存内变异，未写入仓库）：
  - `2002100.value → 2.0` ⇒ `record 2002100: canonical value 2.0 != frozen expected 1.0`
  - `2003000.value → 5.0` ⇒ `record 2003000: canonical value 5.0 != frozen expected 1.0`
  - 基线 11 行全绿（`migration_equivalence: []`）。
- **`key is None` 不漏检**：
  - `check_retired_keys_absent`（`:288-300`）仅对 None 行 `continue`（该行确无 mechanics 源），非 None 行仍逐条断言缺席：回填 `3/300.swords_per_point = 1.0` ⇒ `retired mechanics key still present: 3/300.swords_per_point (replaced by canonical record 2003000)`。
  - `check_registry_reverse`（`:303-343`）对 None 行仍执行 `retired_by_node.setdefault((skill_id, node_id), set())` 登记已迁移节点 2/210，故该节点下未登记残留键**仍被覆盖**：注入 `2/210.mystery_key` ⇒ `unregistered mechanics key under migrated node: 2/210.mystery_key`。
  - `literal` 行仍锚定技能/节点：`2002100.stacks → 3` ⇒ 等价检查报 `registry mismatch: expected skill/node 2/210, got 3/210`，反向登记报 `canonical record 2002100 does not match source literal skill/node 2/210`。
  - `raw` 分支非空转：回填 `3/300.swords_per_point = 2.0` ⇒ `canonical 1.0 not equivalent to mechanics 3/300.swords_per_point=2.0 (raw)`。
  - 保留键无假阳性：`2/210` 三惩罚键与 `3/300.duration` 存在时，`migration_equivalence` / `retired_keys_absent` / `registry_reverse` 均为 `[]`。
- 实跑：`python scripts/validate_skill_spec_modifiers.py --check` ⇒ 6 项全 `[OK]`，`EXIT=0`；`python -m unittest tests.python.SkillSpecBatch1GateTest` ⇒ 8/8 `OK`。

### F-2 ✅ 闭环（死键退役，单一事实源收敛）
- `assets/data/skill_mechanics.json:161-163`：`3/300` 现仅含 `{"duration": 8.0}`；全仓 `rg swords_per_point` **仅命中门禁表自身**（`:56`，作为迁移源锚点，属预期），`src/`、`tests/`、其余数据文件零命中，schema 中亦已消失。
- `KEPT_MECHANICS_KEYS`（`:65-74`）已移除该键并保留 `(3, 300, "duration")`；`duration` 的真实读取点未变（`src/game/systems/skill/behaviors/BladeFormation.cpp:245`）。
- `python scripts/gen_skill_mechanics_schema.py --check` ⇒ `[gen-schema] OK: skill_mechanics_schema.json up to date (472 entries, 70 unreferenced)`（首轮为 71 unreferenced）。
- 设计 §4.3 已补 3/300 行（`design.md:302`，明确「canonical `2003000` 为唯一源；同节点 `duration` 保留」）。

### F-3 ✅ 闭环（法耗下限 0 断言真实触达钳制分支）
- 新增 `tests/unit/SkillSpecializationBakerTests.cpp:348-367`「Mana Cost Floors At Zero」，helper 参数化为 `BuildManaRuntimeBlob(float flat_delta, float mult_scale)`（`:47`）。
- 用例注入 `flat=-100`、`mult=1.0`（有效点数 1）⇒ `(10 - 100) * 1.0 = -90` ⇒ 断言 `effective_mana_cost == doctest::Approx(0.0f)`（`:364`）。**若移除** `src/game/systems/skill/SkillSpecializationBaker.cpp:179-183` 的 `std::max(0.0f, …)`，结果将为 -90 并使断言失败 ⇒ 非空转；退出前 `REQUIRE(ReloadModifierRuntimeFromAsset())`（`:366`）保证无状态泄漏。
- 实跑：`--test-case=*Floors*` ⇒ 1/1 通过（5 断言）；`--test-case=*SkillSpecializationBaker*` ⇒ **13/13 通过（246 断言）**（首轮 12/241，+1 用例 +5 断言）；`--test-case=*SkillBatch1*` ⇒ 6/6（23 断言），无回归。

### F-4 ✅ 闭环（热路径字符串拼接消除）
- `src/game/systems/skill/SkillSpecializationBaker.cpp:214-218`：`static constexpr std::string_view kPenaltyKeys[3] = {"damage_penalty_pt1","damage_penalty_pt2","damage_penalty_pt3"}`，取 `kPenaltyKeys[std::clamp(node210->second, 1, 3) - 1]`，经 `GetFloat` 的 `string_view` 重载查询（`:220`；新增 `#include <string_view>`，`:15`）。
- 与旧 `"damage_penalty_pt" + std::to_string(std::clamp(...))` 在**键值、分档与 clamp 语义上逐点等价**（索引 0..2 对应 pt1..pt3），且不再产生临时堆分配；`GetFloat` 既有 `std::string_view` 重载为异构查找，无二次分配。

### F-5 ✅ 闭环（范围注释统一）
- `rg "30\.\.36" src/` ⇒ **零命中**；`rg "30\.\.40" src/` ⇒ 命中 9 处，含首轮指出的 `SkillSpecModifierAdapter.cpp:194`、`SkillSpecModifierAdapter.hpp:32`、`EquipmentModifierAdapter.cpp:131`、`ModifierEvaluator.hpp:91`。
- 同批小改动逐行核对为**纯注释**：`EquipmentModifierAdapter.cpp`、`SkillSpecModifierAdapter.hpp` 各 `1/1`；`ModifierContext.hpp` 为注释 + 4 个新枚举，无隐藏语义变化。

### F-6 ✅ 闭环（文档状态与修订记录）
- 设计（`design.md:3`）与计划（`plan.md:3`）状态已更新为「已实施（v2.1：按首轮实施审查反馈修订）」，并各附 v2.1 修订记录（`design.md:9`、`plan.md:15`），覆盖 3/300 退役、11 条锚点补齐、法耗下限断言三项。
- 文件为合法 UTF-8（`Read` 工具读取正常；此前终端显示的乱码为控制台编码所致，非文件损坏）。

## 2. 本轮新增发现

1. **Low（可选清理）**：`src/game/systems/skill/SkillSpecializationBaker.cpp:14` 的 `#include <string>` 在 F-4 重构后已无使用点（全文件已无 `std::string` / `std::to_string`），属冗余 include；保留无害。
2. **Info（汇报口径）**：实施方汇报的「schema 471→472 entries」与实测不符 —— 首轮与本轮均为 `472 entries`，变化仅为 `unreferenced 71→70`。属汇报口径误差，不影响仓库状态与结论。
3. **Info（非本包风险，维持既有判定）**：`[Performance] ParticleTrail - Scenario 4`（`tests/performance/ParticleTrailBenchmark.cpp:205`，`CHECK(dispatchOverheadMs < 0.2)`）本轮**隔离运行亦复现失败**：本轮实测 `Baseline=0.155ms, WithSubEmit=0.369ms, Overhead=0.215ms`（首轮隔离实测为 0.147ms 通过），阈值在机器负载下波动幅度超过 1.5x。两轮均确认 `git diff HEAD --stat -- tests/performance/ src/game/fx/` 为空，且 `rg "Modifier|SkillSpecialization|skill_mechanics|SkillSpec" tests/performance/ParticleTrailBenchmark.cpp` 零命中 ⇒ 与本变更集无因果关联。

## 3. 硬否决与回归复检

- 假测试：无（新用例均含真实断言；F-3 用例经反推证明可捕获回归）。
- 既有用例弱化：无（`tests/unit/SkillSpecializationBakerTests.cpp +289/-0`，纯新增）。
- 内存/UB/并发/ECS：无裸 `new`/`delete`、无 `dynamic_cast`/C 式转换、无热路径字符串分配（F-4 已消除）、无 EnTT 指针跨变更违规。
- 门禁与生成物一致性：`validate_skill_spec_modifiers --check`（6/6）、`gen_skill_spec_modifier_contract --check`、`gen_modifier_runtime_v2 --check`、`gen_skill_mechanics_schema --check`、`validate_json`、`SkillSpecBatch1GateTest`（8/8）全部通过；构建 `RelWithDebInfo` EXIT=0（实施方证据，未由本审查复跑全量构建，见「未验证项」）。

## 4. 剩余风险与未验证项（第二轮）

| 项 | 说明 | 状态 |
| --- | --- | --- |
| ParticleTrail 性能阈值 | 隔离运行亦可失败（0.215ms vs 0.2ms），文件与变更集无关联 | **接受**（既有环境性波动，建议后续单独处理该阈值） |
| `ModuleBoundaryCheckerTest` 18 项失败 | 与首轮一致，相关文件本次未改动 | **接受**（既有失败） |
| 全量 `build.bat` / 全量测试 | 本审查未复跑全量构建与 1722 用例全量套件（仅按约束做定向复跑与隔离复测）；构建 EXIT=0 采信实施方证据 | **未验证项**（低影响：定向复跑已覆盖全部改动路径与门禁） |
| 冗余 `#include <string>` | 无行为影响 | 待清理（可选） |

## 5. 最终结论

首轮 F-1~F-6 全部**真正闭环**，关键门禁经独立篡改/回填复现验证为非空转，`MIGRATION_EQUIVALENCE` 11 行与设计 §4.2 的 11 条 canonical 记录一一对应，且 `key is None` 分支未造成任何漏检；未发现阻塞项、高风险缺陷、范围泄漏或行为/数据回归；残留项均为 Low/Info 且与本包目标无关。

**最终结论：`提交`**

下一步：本包可归档并推进后续批次（技能 4~12 或非线性机制迁移）；建议将性能阈值（ParticleTrail Scenario 4）与冗余 include 作为独立小项处理，不纳入本包范围。

---

# 第三轮复核（外部审查反馈处置 + 顺序依赖专项）

## 1. 反馈来源与逐条处置

外部审查提交 6 项发现，逐条判定与修复如下。

| 编号 | 严重度 | 位置 | 判定 | 修复 |
| --- | --- | --- | --- | --- |
| N-1 | Medium | `scripts/validate_skill_spec_modifiers.py:207` | 成立 | `_canonical_index` 改读 `runtime.skill_id_whitelist[0]`，不再用 `record.stacks` 反推归属技能 |
| N-2 | Low（文档） | `scripts/validate_skill_spec_modifiers.py:15` | 成立 | docstring 与 argparse 描述由「5 项」更正为「6 项」，补齐 `registry_reverse` |
| N-3 | Low（文档） | `tests/python/SkillSpecBatch1GateTest.py:89` | 成立 | 注释改为「退役键已在本批移除，本断言用于防回潮」 |
| N-4 | Medium（测试） | `tests/unit/SkillSpecializationBakerTests.cpp` 法耗下限用例 | 成立（断言空转） | 注入参数由 `1.0f` 改为折扣率 `0.0f`，使 0 只可能来自 Baker 生产端钳制；helper 参数更名 `mult_discount_per_point` |
| N-5 | Low | 同文件 `RUNTIME_JSON_PATH` | 成立（死代码） | 删除该常量（全仓无引用） |
| N-6 | Medium（测试） | `tests/unit/SkillSpecializationBakerTests.cpp` 顺序依赖 | 成立 | 为依赖真实运行时的用例补 `EnsureModifierRuntimeForSkillSpec()`（现有 98/117/144/198/265/297/511 等） |

### N-1 语义澄清与反证
`stacks` 承载「叠层数」：对交付类算子恰好等于技能 ID，对其它记录则是目标 stat id（同文件 `2002103` 的 `stacks=9`，而技能为 2）。改读 `skill_id_whitelist` 后：

- `_canonical_index` 对 `2002103` 返回 2（原 9）、`2003000` 返回 3；
- 11 行 `MIGRATION_EQUIVALENCE` 仍全绿，`registry_reverse` 不再产生 `registry mismatch`；
- 篡改回填复现：`2002100 → 2.0`、`2003000 → 2.0` 仍被 `canonical value != frozen expected` 捕获，门禁非空转。

### N-4 空转成因与反证
`SKILL_MANA_COST_MULT` 的 `ApplyOp` 为 `max(0, 1 - paramF32*points)`。原注入 `paramF32=1.0` 时乘性系数自身即为 0，结果 `(10-100)*0 = -0.0`，而 `doctest::Approx(0.0f)` 接受 `-0.0f`，故删除 Baker 的 `std::max` 或整体丢弃 flat 算子都不会失败。改为 `paramF32=0.0` 后系数为 1、结果 `-90`，只有生产端钳制才能得到 0。

## 2. 顺序依赖专项（`--order-by=rand`）

N-6 揭示的是一类问题：`ModifierRuntimeRegistry` 为进程级单例，部分用例经 `LoadFromBytes` 注入合成 blob 后未还原；本批把技能 2/3 专精数值改为由 UMR 记录提供，使更多用例的断言值依赖「真实运行时是否在场」。

处置：沿用 `tests/TestCommon.hpp:18-21` 既有约定（依赖真实生成数据的用例在用例开始时显式重载），在受影响文件的共用初始化函数中补 `REQUIRE(ReloadModifierRuntimeFromAsset())`：

- `tests/functional/RendingWaveNodes.cpp` `LoadSkillData()`（技能 2：200/201/202/210）
- `tests/functional/BladeFormationNodes.cpp` `EnsureSkillMechanics()`（技能 3：300/302/310/312/331/332）
- `tests/functional/Skill3FollowupTests.cpp` `EnsureSkillMechanics()`（331）

`tests/functional/Skill2FollowupTests.cpp` 仅覆盖 251 行为层、不涉及被迁记录，未改动。

### 基线对照（同一 seed，`--test-case-exclude=*Performance*`）

| 版本 | 用例总数 | 通过 | 失败用例 | 失败明细（实测） |
| --- | --- | --- | --- | --- |
| HEAD `4e66910e`（本批之前） | 1624 | 1613 | 11 | `MindBladeNodes:747`、`SwordArrayNodes:295/557/559`、`GraphBindingEquivalenceGLTest:2118`、`RenderGraphV5ContractsIntegrationTest:118`、`MonsterAffixTests:140/141/278/279/324/369/694/944` |
| 本批（现行） | 1636 | 1632 | 4 | `MindBladeNodes:747`、`SwordArrayNodes:295`、`RadianceDirectionalTest:279`、`MonsterAffixTests:444/445` |

三个 seed（20260916 / 777 / 4242）的失败集合完全一致。结论：

- 技能 2/3 的顺序依赖已消除：`RendingWaveNodes`、`BladeFormationNodes` 在随机顺序下全通过；
- 剩余 4 项均位于**无 UMR canonical 记录、无本批代码改动**的子系统：渲染（Radiance）、技能 6（SwordArray）、技能 7（MindBlade）、怪物词缀（MonsterAffix）。它们在 HEAD 基线随机顺序下同样失败（`SwordArrayNodes:295`、`MindBladeNodes:747`、`MonsterAffixTests` 均在基线失败清单内），故与本批无直接关联；本批使随机顺序下失败用例数由 11 降至 4；
- 需说明的测量条件：本次测量期间主机处于高负载（非空载），而残余项中含渲染/计时敏感用例（`RadianceDirectionalTest`、`MonsterAffixTests`、`RenderGraphV5Contracts`），因此其失败集合与次数可能受环境噪声影响。定性为「既有、与本批无直接关联；是否立项治理应先于空载环境复测」；
- 代码层面可确认的既有隐患之一为 `TestSetupScope`（`tests/TestCommon.hpp:62-93`）不复位 modifier runtime。

## 3. 本轮验证数据

| 项 | 结果 |
| --- | --- |
| `build.bat RelWithDebInfo` | EXIT=0，无 error C/LNK |
| 默认顺序全量（非 Performance） | 1636/1636 用例、117070/117070 断言通过 |
| `validate_skill_spec_modifiers.py --check` | 6/6 项通过 |
| `tests/python/SkillSpecBatch1GateTest` | 9/9 通过 |
| `gen_skill_spec_modifier_contract.py --check` | up to date |
| `gen_modifier_runtime_v2.py --check` | binary in sync |

## 4. 独立复核（第三轮）与 Low 项处置

独立审查子代理（cpp-reviewer，全程只读）判定 **`提交`**，无 Critical/High/Medium，提出 5 项 Low，处置如下。

| 编号 | 严重度 | 位置 | 处置 |
| --- | --- | --- | --- |
| L-1 | Low | `tests/unit/SkillSpecializationBakerTests.cpp` `Node 210 Penalty Reads Mechanics` | 该用例把进程级 `SkillMechanicsRegistry` 换成合成表后从未还原，属本批新增的隐性顺序依赖。已改为用例末尾 `ResetForTests()` + 重新 `LoadFromFile("assets/data/skill_mechanics.json")`；修复后默认顺序断言数由 117069 增至 117070（新增 1 条 `REQUIRE`），随机顺序三 seed 结果不变 |
| L-2 | Low（文档） | `scripts/validate_skill_spec_modifiers.py:13-22` | 「6 项」说明字符串位于 imports 之后、不会被绑定为 `__doc__`。已上移至文件首行成为正式模块 docstring |
| L-3 | Low | `scripts/validate_skill_spec_modifiers.py` `check_projectile_value_integer` | 门禁仅校验整数、未拒绝负值，而算子按 `static_cast<int>(param_f32 * 点数)` 截断会产生负弹道数。已增加 `param_f32` 非负校验（负值与非整数给出不同失败信息）；并在 `tests/python/SkillSpecBatch1GateTest.py` 新增防回归用例断言「-1.0 与非整数 1.5 均被拒绝、2.0 通过」，实测两者分别命中 `must be non-negative` 与 `must be an integer` |
| L-4 | Low | `scripts/migrate_skill_spec_modifier_slice.py --check` | 因既有 `docs/reports/four-pillars/phase-4/D4-3/artifacts/migration-report.json` 漂移而 EXIT=1（committed 报告 totals 仅 1 条记录，而 HEAD canonical 已有 10 条）。该 artifact 未被本批改动、脚本未接入构建，属既有问题，不纳入本包；建议后续重生成报告 |
| L-5 | Low | `tests/unit/SkillSpecializationBakerTests.cpp` | 少量纯算式行尾注释未用中文。已仅就**本批新增**的算式注释补中文说明（如 `// 基础 35 × 1.2（2 点 × +10%）= 42`）；全文件另有 70 余处既有英文注释，非本批引入，不在本包范围 |

复核同时确认：`_canonical_index` 改读 `skill_id_whitelist[0]` 对 21 条记录中仅 `2002103` 与旧写法取值不同（9 → 2），三项门禁结果前后一致；4 个新容器中性值（0 / 0.0f / 0.0f / 1.0f）与未命中行为正确；Baker 步骤 1/3/4 与设计 §3.3 一致；11 条 canonical 记录逐字段与设计 §4.2 一致；退役键无其它读者（`2/210.damage_penalty_pt1/2/3` 与 `3/301.haste_pct_per_point` 为有意保留）；`SkillSpecModifierAdapter` 掩码收窄无回归。反向证据：`tests/unit/SkillSpecDeliveryOpTests.cpp:50-91,211` 注入空 filter 通配 blob（技能 3 `more_damage_mult` ×1.25）且不还原，故功能侧守卫是必要的。

## 5. 剩余风险（不阻塞）

| 项 | 说明 | 处置 |
| --- | --- | --- |
| 既有随机顺序失败 4 项 | 渲染 / 技能 6 / 技能 7 / 怪物词缀；在 HEAD 基线同样失败，与本批无直接关联；测量时主机高负载，含计时敏感用例 | 先于空载环境复测再决定是否立项；代码层可确认的隐患为 `TestSetupScope` 未复位 modifier runtime |
| `migration-report.json` 漂移（L-4） | 既有 artifact 与本包无关，`migrate_skill_spec_modifier_slice.py --check` 因此 EXIT=1 | 后续重生成报告；未接入构建，不影响本包 |
| N-6 的 Baker 侧防护未单独构造反证 | 三处用例已按约定加 `EnsureModifierRuntimeForSkillSpec()`，但未逐一构造「移除防护即失败」的反证（技能 2/3 的功能侧对应项已用随机顺序基线证明） | 记录为未验证项（低影响） |

## 6. 本轮结论

外部反馈 6 项与独立复核 5 项 Low 全部完成处置（L-1/L-2/L-3/L-5 已改并复验，L-4 记为既有不阻塞项）；N-1/N-4 经篡改与反证确认修复后不再空转；顺序依赖专项以 HEAD 基线对照证明技能 2/3 的依赖已消除、剩余失败与本批无直接关联。

**本轮结论：`提交`**
