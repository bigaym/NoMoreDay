# UMR-SKILL-BATCH-3 实施审查报告（数据资产 · 门禁脚本 · Python 测试面）

- 审查日期：2026-09-17
- 审查对象：技能 7（心剑 MindBlade）/ 8（回旋 BladeBoomerang）/ 9（绝影 PhantomTrance）批次三迁移的**数据资产、离线门禁脚本与 Python 门禁测试**
- 审查性质：独立对抗性审查（只读，未修改任何源码/资产/测试，未执行 git commit）
- 审查依据：
  - 设计说明 `docs/designs/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-design.md`（v1.1，重点 §4 数据契约与单一事实源治理、§5 校验与测试防线）
  - 实施计划 `docs/plans/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-plan.md`（v1.1，Task 1~8）
  - 历史门槛参照：`docs/reviews/` 下 Batch 1 / Batch 2 审查报告
- 结论：**修改**（详见 §5；无数据正确性阻断项，但存在 1 项中等级门禁覆盖缺口与 1 项必须清理的越界脏改动）

---

## 1. 验证命令与实测结果（均实际运行，记录退出码）

| # | 命令 | 退出码 | 关键输出 |
|---|---|---|---|
| 1 | `python scripts/validate_skill_spec_modifiers.py` | 0 | 6/6 门禁 `[OK]`：`id_decode` / `projectile_integer` / `skill_delivery_domain` / `migration_equivalence` / `retired_keys_absent` / `registry_reverse`；末行 `[OK] skill_spec modifier offline gates passed: 6/6 passed.` |
| 2 | `python tests/python/SkillSpecBatch3GateTest.py` | 0 | `Ran 17 tests ... OK` |
| 3 | `python tests/python/SkillSpecBatch2GateTest.py` | 0 | `Ran 14 tests ... OK` |
| 4 | `python scripts/gen_modifier_runtime_v2.py --check` | 0 | `[modifier-runtime] Check passed: binary is in sync with modifier_v2 json.` |
| 5 | `python scripts/gen_skill_mechanics_schema.py --check` | 0 | `[gen-schema] OK: skill_mechanics_schema.json up to date (445 entries, 62 unreferenced)` |

补充只读验证：

| # | 命令 | 退出码 | 关键输出 |
|---|---|---|---|
| 6 | `python scripts/gen_skill_spec_modifier_contract.py --check` | 0 | `[OK] skill_spec runtime contract is up to date.` |
| 7 | `git check-ignore assets/generated/modifier_runtime_v2.bin` | 0 | 命中忽略规则（`assets/generated/*` 为构建产物，除已跟踪图片外全部忽略） |

---

## 2. 逐项结论

### 2.1 Canonical 记录（11 条）— 通过

文件：`assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json`

- 文件结构 `{"schema_version":1,"records":[...]}`，当前共 **51 条**记录（HEAD 为 40 条，本批次 +11），**51 个 modifier_id 全不重复**。
- 11 条新增记录 `2007000/2007010/2007020/2007030/2007320/2008000/2008010/2008011/2008100/2009750/2009860` 均存在（记录体起始行约 1724–2196；id 行：`2007000@1728`、`2007010@1771`、`2008000@1943`、`2008011@2029`、`2008100@2072`、`2009860@2158`）。
- 逐字段程序化核对设计 §4.2 对照表与 §4.1 通用头约束，结果 **`field mismatches: NONE`**。逐条校验项包括：`runtime.skill_id_whitelist==[skill]`、`runtime.node_id_whitelist==[node]`、`runtime.opcode`、`record.value==runtime.param_f32`、`record.stat_path==runtime.target`、`record.operation∈{add,mul}`、`record.stacks==runtime.param_u32==skill_id`、`conditions.all_skill_ids==[skill]`、`conditions.min_player_level==1`、`record.tags==["skill"]`、`runtime.debug_name`、`runtime.debug_source=="skill_spec_node"`、`runtime.priority==200`、`profession_mask==1`、`weapon_class_mask==65535`、`equip_slot_mask==0`、`exclusive_group==0`、`max_active==0`、`required_skill_tags_all==0`、`forbidden_skill_tags_any==0`。
- **ID 解码**：对全部 11 条验证 `(id-2000000)//10 == node_id_whitelist[0]`；`op_index=(id-2000000)%10` 语义正确。同节点 801 的两条：`2008010`（op_index 0，`SKILL_SPEED_MULT`）与 `2008011`（op_index 1，`SKILL_RANGE_MULT`）op_index 与 opcode 映射正确且不冲突；其余 10 条 op_index 均为 0（单一 opcode 节点）。
- 记录未按 id 全局排序（沿用既有追加顺序），**非缺陷**：`scripts/gen_modifier_runtime_v2.py` 编译时按 `(priority, id)` 排序，`skill_spec_modifiers.json` 亦按 canonical 顺序直出，排序不影响产物确定性。
- schema 字段无缺失：记录键与 `assets/data/modifier_v2/canonical/skill_spec_modifier_record.schema.json` 的 `required`（10 键）一致，生成器逐条做 schema 校验并通过。

> 唯一可讨论的点：`check_record_id_decode` 只校验 `//10` 对应的节点，从不校验 `%10` 的 op_index（G-07）。本批次数据本身无问题。

### 2.2 生成产物与 `--check` 一致性 — 通过

- `assets/data/modifier_v2/skill_spec_modifiers.json`（schema_version 2，51 条 `ops[]`）已随 canonical 同步刷新（`+374/-0`），`gen_skill_spec_modifier_contract.py --check` 退出码 0。
- `gen_modifier_runtime_v2.py --check` 退出码 0（二进制与 JSON 同步）。该二进制位于 `assets/generated/`，被 `.gitignore` 忽略，由构建链（`CMakeLists.txt:229-243` / `build.bat`）重新生成，因此不存在“产物漂移被提交”的风险。
- 生成链**真实两段式**：canonical → `gen_skill_spec_modifier_contract.py`（→ catalog JSON）→ `gen_modifier_runtime_v2.py`（读取 `modifier_catalog.json` 全量条目 → 编译 `.bin`）。`scripts/migrate_skill_spec_modifier_slice.py:353` 亦声明 `runtime_generated_by: scripts/gen_skill_spec_modifier_contract.py`。

### 2.3 机制表退役与死键清理 — 通过

文件：`assets/data/skill_mechanics.json`、`assets/data/skill_mechanics_schema.json`

- `git diff` 显示 `skill_mechanics.json` 恰为 `+1/-26`，删除内容精确等于：整节点 `7/700`、`7/701`、`7/702`、`7/703`、`8/810`（`hover_duration` + `hover_tick_interval`）、`9/975`、`9/986`，以及 `7/732.mana_penalty_pct`、`9/0.form_move_pct`、`9/0.weaken_duration`。即 **8 个迁移键 + 3 个死键**，无多删、无漏删。
- `skill_mechanics_schema.json` 为 `0/-55`，恰好 11 个 `(skill,node,key)` 元组 × 5 行，**同步删除**，无越界修改。
- **保留键未被误删且仍被 src 消费**（rg 反查证据）：
  - `7/732.move_speed_scale` → `src/game/application/input/InputSystem.cpp:230`
  - `7/0.base_range` → `src/game/application/states/GameplayState.cpp:877`、`src/game/systems/skill/SkillSpecializationBaker.cpp:112`、`src/game/systems/skill/BeamChannelDeliverySystem.cpp:210`
  - `7/0.base_radius` → `SkillSpecializationBaker.cpp:114`、`BeamChannelDeliverySystem.cpp:226`
  - `7/0.mana_cost_per_sec` → `SkillSpecializationBaker.cpp:44`
  - `9/0.form_duration` → `SkillSpecializationBaker.cpp:127`
- 反查确认 8 个退役键与 3 个死键在 `src/` 中**已无消费点**（其余同名命中属于技能 2/4/5/11/12 或无关结构体字段）。

### 2.4 门禁脚本 `validate_skill_spec_modifiers.py` — 通过（含 3 处低级别问题）

- `git diff` 为**纯增量扩展**（+35/-7，删除行仅注释改写），未弱化既有断言。
- **MIGRATION_EQUIVALENCE 11 条**（`scripts/validate_skill_spec_modifiers.py:91-101`）关系与冻结期望值经 git HEAD 基准反查，**全部真实等价**：
  - `700/701/702/703/810/975` 为 `raw`，期望值 `0.1/0.1/0.1/0.1/0.8/0.25`，等于 HEAD mechanics 原值；
  - `732` 为 `flat_negate`，期望 `-0.5` == `-(HEAD mana_penalty_pct 0.5)`，且节点 732 max_points=1（keystone，无条件生效）故等价；
  - `986` 为 `flat_negate`，期望 `-1.0` == `-(HEAD cd_per_point 1.0)`，且 986 max_points=4/单点值 1.0；
  - `800` 为 `literal`，期望 `-1.0` == HEAD `SkillSpecializationBaker.cpp:843-844` 的 `cost - 1.0f*points`；
  - `801` 两条为 `literal`，期望均 `0.15` == HEAD `SkillSpecializationBaker.cpp:846-849` 的 `1.0+0.15f*points`（speed 与 range 共用同一系数）。
  - `check_migration_equivalence`（:302-361）在校验冻结期望常量的同时，退役后若键回归会走 :346-355 的关系比对，**并非删除即空转**。
- **`KEPT_MECHANICS_KEYS`**（:105-129）共 16 项，本批新增 5 项。其中 `(7,732,"move_speed_scale")` **确为 `check_registry_reverse` 必需**（实验：将其余 4 项移除后 `registry_reverse` 仍为空；5 项全移除则仅 `7/732.move_speed_scale` 被标记）。其余 4 项（`7/0 base_radius/base_range/mana_cost_per_sec`、`9/0 form_duration`）位于**非迁移节点**，对 `check_registry_reverse` 不产生作用，但作为仍需保留的单一事实源键，登记本身合理（仅设计文档解释措辞不准，见 G-04）。
- **`DEAD_MECHANICS_KEYS`**（:133-143）共 8 项，本批新增 `(8,810,"hover_tick_interval")`、`(9,0,"form_move_pct")`、`(9,0,"weaken_duration")` 三项，与设计 §4.3 完全一致。
- **非空转验证**（内存内 monitor 探针，只读）：篡改 `2007320` 数值 → 报 `record 2007320: canonical value -0.6 != frozen expected -0.5`；删除 `2008011` → 报缺失 + `registry_reverse` 告警；回填 `7/700.mana_reduction_pct_per_point` → 报退役键回潮；篡改 `node_id_whitelist` → 报解码不一致。**四条负向路径均真实可触发。**
- 复现：`SUPPORTED_OPCODE_TO_OPERATION`、`TARGET_PATTERN` 与 canonical 用值一致；`check_skill_delivery_domain` 遍历 `modifier_catalog.json` 的 `ops[].opcode`，新增记录均落于 `30..42` 窗口内。

### 2.5 Python 门禁测试 `SkillSpecBatch3GateTest.py` — 部分通过（Task 5 第 3 项未落实）

文件：`tests/python/SkillSpecBatch3GateTest.py`（247 行，17 个测试）

| 计划 Task 5 断言项 | 落地情况 |
|---|---|
| (a) 11 条已纳入 canonical 且 index 可检出 | ✅ `BATCH3_RECORDS:20-32` + 测试 69-125 |
| (b) ID 解码 `(id-2_000_000)/10` 与 `node_id_whitelist[0]` | ⚠️ 部分：测试 74-84 校验 `//10` 与 `%10∈{0,1}`，未校验 op_index↔opcode 一致性 |
| (c) **两生成器 `--check` 一致性** | ❌ **未实现**：测试仅调用内存态门禁函数（:149-176），无任何 `subprocess`/`--check` 调用；设计 §5.2:305 要求的 `gen_modifier_runtime_v2.py --check` 退出码恒 0 未被断言 |
| (d) 8 退役键 + 3 死键完全不存在 | ✅ 测试 127-135 |

- 负向用例有效性：`test_retired_key_reintroduction_fails_gate`（:226-235）与 `test_dead_key_reintroduction_fails_gate`（:215-224）通过“注入 → 期望 `False` + 匹配消息”断言，**非恒过**（实测触发如上）。但**无 canonical 数值漂移的负向用例**（G-08）。
- 与 Batch 2 门禁测试无重复覆盖（记录 ID 集合不相交），Batch 3 额外提供全量聚合测试（:149-176）与退役键负向测试，Batch 2 无。未发现 Batch 2 已有断言被删除或弱化。

### 2.6 实现自报复核

- **canonical 实为 40 条（非 44 条）**：设计/计划中**不存在**“44 条”的表述（`rg` 全仓无命中）；HEAD 提交态 canonical 为 40 条，+11 后为 51 条，前后自洽。该自报**不可考且不影响判定**，无需修复（G-09）。
- **额外执行 `gen_skill_spec_modifier_contract.py`**：**必要且属于既定生成链**。计划 Task 2 的“生成 JSON 产物”表述有误（把 catalog JSON 归给 `gen_modifier_runtime_v2.py`），实际由契约生成器产出。该步骤使 `skill_spec_modifiers.json` 与 canonical 同步，`SkillSpecCanonicalGenerationTest`（`tests/python/SkillSpecCanonicalGenerationTest.py:42-52`）会持续守卫该一致性。
- **未被任何门禁覆盖的生成产物**：`assets/generated/modifier_runtime_v2.bin` 仅有手动 `--check`，无自动化测试（G-01）；`.debug.json` 同为构建产物，风险可忽略。

---

## 3. 问题清单

| 编号 | 严重度 | 问题 | 证据（文件:行号） | 期望修复方向 |
|---|---|---|---|---|
| **G-01** | **中** | 计划 Task 5 第 3 项“校验两生成器 `--check` 一致性”与设计 §5.2 第 4 条**未在门禁测试中实现**：`SkillSpecBatch3GateTest.py` 全程只调用内存态校验函数，无 `subprocess` 调用；`gen_modifier_runtime_v2.py --check` 无任何自动化测试覆盖 | `docs/plans/...-plan.md:166`；`docs/designs/...-design.md:305,342`；`tests/python/SkillSpecBatch3GateTest.py:149-176`；`scripts/gen_modifier_runtime_v2.py:294,342`（`check_binary_matches` 仅 `main()` 调用） | 二选一：① 在门禁测试中以 `sys.executable` 调用 `gen_modifier_runtime_v2.py --check` 并断言 `returncode==0`（`.bin` 不存在时跳过并显式标记）；② 修订计划/设计，将该检查降级为人工 DoD 步骤并注明 canonical→JSON 由 `SkillSpecCanonicalGenerationTest` 覆盖 |
| **G-02** | 低 | 计划 Task 2 生成链描述错误：称 `gen_modifier_runtime_v2.py` 生成“二进制 blob 与 JSON”，实际 catalog JSON `skill_spec_modifiers.json` 由 `gen_skill_spec_modifier_contract.py` 生成 | `docs/plans/...-plan.md:136`；`scripts/gen_skill_spec_modifier_contract.py:29-31`；`scripts/gen_modifier_runtime_v2.py:274-291` | 更正计划 Task 2 为“canonical → 契约生成器（JSON）→ 运行时编译（bin）”，消除对实现者的误导 |
| **G-03** | 低 | 设计 §5.1 关系枚举自相矛盾且与实现不符：原文“`732/800/986` 为 `flat_negate`，`800/801` 为 `literal`”，其中 `800` 无 mechanics 键，`flat_negate` 不可能成立 | `docs/designs/...-design.md:296` vs 同文档 `:280`；`scripts/validate_skill_spec_modifiers.py:88-101` | 更正为“`732/986` 为 `flat_negate`，`800/801` 为 `literal`，其余为 `raw`” |
| **G-04** | 低 | 设计 §4.4/§5.1 称 5 项 `KEPT` 用于“保障反向登记无告警”，但实测仅 `(7,732,"move_speed_scale")` 被 `check_registry_reverse` 使用（`7/0`、`9/0` 非迁移节点）；其余 4 项的实际作用是保留单一事实源，理由表述不准确 | `docs/designs/...-design.md:284-289,298`；`scripts/validate_skill_spec_modifiers.py:398-438`（`retired_by_node` 仅由 `MIGRATION_EQUIVALENCE` 构建，KEPT 仅于 :432 生效） | 修订文档措辞；5 项登记保持（它们仍是 `src/` 消费的权威键） |
| **G-05** | 低 | 工作区混入与本批次无关的机器相关脏改动，提交将污染基线 | `git diff settings.json`：`benchmarkScore 124.29000091552734→124.26000213623047`、`updatedAtUtc 2026-08-16→2026-09-17` | 提交前 `git checkout -- settings.json` 还原 |
| **G-06** | 低 | 门禁函数文档漂移：`check_skill_delivery_domain` docstring 写 “OpCode 30..40”，实际常量已扩至 42（模块头已写 30..42） | `scripts/validate_skill_spec_modifiers.py:241` vs `:6,:42` | 将函数 docstring 对齐为 `30..42`（历史遗留，非本批引入） |
| **G-07** | 低 | 门禁盲区：无 canonical **重复 modifier_id** 断言，`_canonical_index` 对重复键静默覆盖，schema 亦无跨记录唯一性约束；当前 51 条虽唯一，但未来重复会静默吞记录 | `scripts/validate_skill_spec_modifiers.py:272-289`；`:180-205`（`check_record_id_decode` 逐条遍历，不查重） | 在 `check_record_id_decode` 中追加 `len(ids)==len(set(ids))` 断言并给出重复项 |
| **G-08** | 低 | `check_migration_equivalence` 缺少负向用例：门禁测试只覆盖死键/退役键回潮，未证伪 canonical 数值漂移路径（该路径实测可触发，但无测试守护，未来重构可能使其失效而不被察觉） | `tests/python/SkillSpecBatch3GateTest.py:215-235`（仅两类负向） | 增加“篡改 `2007320` 数值 → `check_migration_equivalence` 返回 False”的负向用例；Batch 2 同步补齐 |
| **G-09** | 信息 | 自报“计划所述 44 条 canonical”不可考：设计/计划全文无“44”字样；HEAD 40 条 + 本批 11 条 = 51 条自洽 | `rg "44" docs/...design.md docs/...plan.md` 无命中；canonical 实测 51 条 | 无需修复，仅记录 |

---

## 4. 与 Batch 1/2 门槛的对齐说明

- 判定口径沿用 Batch 1/2 审查报告：以 `文件:行号 + 可复现命令输出` 为证据，阻断项限于安全/数据丢失/契约破坏，低级别文档漂移与可清理脏改动计入“修改”。
- Batch 3 相较 Batch 2 的**门禁增益**：新增全量聚合测试、退役键回潮负向测试与 `projectile_integer` 交叉覆盖，未出现断言弱化或旁路。
- Batch 1 已记录的既有约定（Python 门禁测试为手动运行，无 CI/CTest 注册）在本批次延续，本报告不将其作为新增缺陷，但 G-01 的 `--check` 缺口会因“无自动运行器”而更难被发现。

---

## 5. 最终结论

**结论：`修改`**

**阻断项：无。** 数据资产层面未发现任何正确性缺陷——11 条 canonical 记录与设计 §4.2 逐字段全等（程序化核对 `field mismatches: NONE`），8 个迁移键 + 3 个死键精确删除且 schema 同步，5 个保留键仍被 `src/` 真实消费，无重复 ID、无 schema 字段缺失、无产物漂移；门禁脚本关系与冻结期望值与 git HEAD 基准真实等价，负向路径实测可触发，不存在门禁空转。

**需修改后提交的原因（均非阻断）：**

1. **G-01（中）**：计划 Task 5 第 3 项 / 设计 §5.2 第 4 条“两生成器 `--check` 一致性”在门禁测试中未实现，`gen_modifier_runtime_v2.py --check` 无自动化覆盖。请补一条（可跳过式）断言，或修订计划/设计明确其人工 DoD 属性。
2. **G-05（低）**：提交前必须还原与本批次无关的 `settings.json` 基准分/时间戳脏改动。
3. **G-02 / G-03（低）**：计划生成链归属与设计 §5.1 关系枚举存在契约漂移，需对齐实现（实现本身正确）。
4. G-04 / G-06 / G-07 / G-08 为文档措辞与门禁健壮性增强项，建议同批修复，不单独阻塞。

完成上述 1–3 后即可判定 **提交**。
