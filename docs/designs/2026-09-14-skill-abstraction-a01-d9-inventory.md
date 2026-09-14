# A-01 Phase 0 / D9 盘点：SpecState 生成器输入源与产物落点决策记录

- 日期：2026-09-14
- 归属：Track A-01（Wave E 技能抽象化）· Phase 0 · D9
- 状态：已盘点（只读）；本文件为 Phase 3 生成器的硬前置
- 上游：`docs/plans/2026-09-14-skill-abstraction-track-a01-plan.md`（Phase 0 §3，I1~I10）、`docs/designs/2026-09-14-skill-abstraction-track-a01-design.md`（D2/D9、§5.2 数据所有权）
- 本文件性质：**只读盘点 + 1 份决策记录**。盘点过程未编译、未跑测试、未修改 `src/`/`assets/`/`tests/`/backlog/plan/GDD，未产生 git 提交。

---

## 1. 结论速览

| 决策项 | 结论 |
|--------|------|
| **`--gen-specstate` 输入源（结构）** | `assets/data/skill_contracts_compact.json`（节点角色/触发/排除组/转质分类）∪ `assets/data/skills.json`（技能 1~9 节点全集/顺序/`max_points`）∪ `assets/data/mastery_skill_trees.json`（技能 10~12 同上）∪ `assets/data/skill_mechanics.json`（机制键与数值）。 |
| **输入源（命名）** | **无现有数据源**：节点「语义名」不以可复用标识符形式存在于任何数据文件；`skills.json`/`mastery_skill_trees.json` 只有中文 `name_key`/`desc_key`，`skill_node_prompts.json` 只有英文美术提示串且与 C++ 标识符**不一致**（已核实，见 §3.4）。 |
| **产物落点** | 新建 **C++ 生成头** `src/game/systems/skill/behaviors/generated/<PascalSkill>SpecState.gen.hpp`（POD + 绑定表 + `ResolveSpecState`），由行为头 `#include` 引用，CMake 以 `add_custom_command(OUTPUT ...)` 挂接。**不**采用「写回 `assets/data/*.json`」的原地落点（SpecState 需要 C++ 成员指针，JSON 无法承载）。 |
| **命名来源** | 必须由「① 新增薄的每技能命名清单 descriptor」或「② 手写枚举」提供；生成器只负责 `node_id → 成员` 的结构绑定与一致性校验。**若两者都不引入 → 走退化方案（§6）**。 |
| **是否存在合适输入源** | **结构输入源充足，命名输入源缺失**。建议采用「现有数据为结构事实源 + 新增 descriptor 为命名事实源」的混合方案；若拒绝新增数据文件，则退化为「手写 SpecState + `--gen-specstate` 仅校验」。 |
| **与现有生成器共享范式** | 复用 `gen_skill_contracts.py:710-744` 的 `--check`/`--check-idempotency`/`--check-determinism` 三范式；复用 `gen_tags.py` + `CMakeLists.txt:176-192` 的「`add_custom_command` 生成 C++ 头」落点范式；复用 `gen_skill_mechanics_schema.py` 的「源码读点↔数据」双向对账作为 D-A8 反向校验。 |

---

## 2. 逐项盘点表（I1~I10）

| 编号 | 对象 | 现状（`文件:行`） | 能否作为 SpecState 生成输入源 | 产物落点建议 |
|------|------|-------------------|-------------------------------|--------------|
| I1 | 紧凑契约 `assets/data/skill_contracts_compact.json` + 读取点 `scripts/gen_skill_contracts.py:22` | 顶层 `version=1` + `skills[]`，覆盖技能 **1~12**；每项键为 `skill_id / min_nodes / max_nodes / max_transmuters / max_triggers / has_sword_intent_node / has_synergy_node / transmuter_node_ids / synergy_node_ids / sword_intent_node_ids / sword_step_node_ids / keystone_node_ids / passive_node_ids / trigger_nodes / keystone_exclusion_groups / resist_models / scope_policies / cost_affixes`（实测 union，`scripts/gen_skill_contracts.py:22,639-644`） | **是（角色/分类权威源）**。它是「节点 id + 角色 + 排除组 + 触发」的权威列表；但**不含纯被动节点的 id 全集**，也不含节点语义名 | 作为生成器的**角色输入**；其分类结果物化进 `skills.json`/`mastery_skill_trees.json` 的 `skill_contract.nodes` |
| I2 | 技能主数据 `assets/data/skills.json` + 角色常量 `scripts/gen_skill_contracts.py:25-67` | 技能 **1~9** 有 `talent_tree` + `skill_contract`；节点字段实测为 `id / name_key(中文) / desc_key(中文) / max_points / x / y / prerequisites / stat_modifiers / icon_id`，部分含 `add_tags`/`remove_tags`；`skill_contract.nodes[].role ∈ {Passive,Keystone,Trigger,Synergy,Transmuter}`（`skills.json` 内 `skills[1].skill_contract.nodes`）。**`skill_contract.nodes` 是 `talent_tree` 的真子集**：技能 1~9 分别遗漏 16/14/15/16/15/15/17/15/15 个纯被动节点（实测，`gen_skill_contracts.py:485-498` 的 `emits_non_default` 决定省略） | **是（节点全集 + 顺序 + max_points 权威源）**；`skill_contract` 仅为「非默认节点」注解，**不可单独当全集** | 生成器以 `talent_tree[].id` 为节点全集；角色取 `skill_contract.nodes`，缺省回落 `Passive`（与 `SkillRegistry.cpp:255-261` 运行期默认一致） |
| I3 | 精通树 `assets/data/mastery_skill_trees.json` + 合并脚本 `scripts/merge_skill_tree_coords_and_deps.py` | 技能 **10~12**：`skill_id 10/11/12`、`mastery_id sword_saint/heavenly_sword/demon_blade`，各含 `talent_tree`（26/25/25 节点）+ `skill_contract`（13/10/10 节点，遗漏 13/15/15）。合并脚本把 `skill_x_tree.json` 的 `x/y/prerequisites` 合并进 `skills.json`（`merge_skill_tree_coords_and_deps.py:31-78`），仅有写回/`--dry-run`，**无 `--check`** | **是（技能 10~12 节点全集/顺序/坐标/前置权威源）**；与 I2 合并即为 12 技能节点全集 | 生成器需**同时**读取 I2 与 I3（技能 1~9 在 `skills.json`，10~12 在 mastery）；`merge_skill_tree_coords_and_deps.py` 保持现状不动 |
| I4 | 机制数值 `assets/data/skill_mechanics.json` / `_schema.json` + 对账 `scripts/gen_skill_mechanics_schema.py:43-45` | `skill_mechanics.json` 结构为 `{ "1": { "112": {key: value}, ...}, ... }`（skill→node→键值）；schema 结构 `{version, entries, dynamic_keys, unreferenced}`（`gen_skill_mechanics_schema.py:13-19`）；生成器扫描 `src/` 的 `GetMech(/GetFloat(/GetInt(` 调用点静态解析三元组，并**识别 `*Nodes::Name` 常量与 `std::array<*MechBinding,N>` 绑定表**（`gen_skill_mechanics_schema.py:21-27`） | **是（机制键/数值/默认值权威源）**，并**可直接复用于 D-A8 反向校验** | SpecState 的 `mech` 字段绑定 `(node_id, key, default)` 可直接由该文件+schema 校验；空值回退语义见 `SkillPointAccess.hpp:38-43` |
| I5 | 角色枚举 `src/game/foundation/data/SkillContract.hpp:12-18` | `enum class SpecNodeRole : uint8_t { Passive=0, Keystone=1, Trigger=2, Synergy=3, Transmuter=4 }`；同文件另有 `ResistModel:20-27`、`ScopePolicy:29-33`、`CostAffixPreset:35-39`、`TriggerWindow:42-46`（`SkillContract.hpp:12-46`） | **是（取值域）**：与 `gen_skill_contracts.py:25-67` 的字符串常量一一对应，生成器沿用字符串域即可 | 不需要新产物；生成器输出必须与其枚举序数对齐（`Passive=0…Transmuter=4`） |
| I6 | 节点契约消费点 `SkillContract.hpp:60` `NodeContractData`；`SkillRegistry.hpp:46` `GetNodeContract` | `NodeContractData` 全量字段：`node_id / role / resist_model / scope_policy / affects_sword_intent / affects_sword_step / keystone_exclusion_group / cost_affix / trigger`（`SkillContract.hpp:60-70`）；`static_assert(sizeof(NodeContractData)<=40)`、`sizeof(SkillContract)<=32`（`:88-91`）。访问器 `SkillRegistry.hpp:43-49`：`GetSkillContractDefinition / GetSkillContract / GetNodeContract / ValidateSkillContract`。运行期默认：`BuildDefaultContract` 对未注解节点令 `max_points==1 ? Keystone : Passive`（`SkillRegistry.cpp:255-261`） | **是（SpecState 需覆盖的属性域判定依据）**：SpecState 覆盖的节点属性 ⊆ 该结构字段；可用于生成时对拍 | 不直接生成；作为生成器**覆盖度判据**（哪些角色需要进入 point/flag 分类） |
| I7 | 交付参数结构 `src/game/foundation/components/SkillDefs.hpp:625-699` | `BakedDeliveryParams`（`:625-667`，含 `feature_flags` 位掩码 `:626` 与 `return_damage_mult`…`element_shield_pct` 等显式语义字段）+ `BakedSkillProfile`（`:672-699`，含 `delivery{}`、`injected_payloads` 定长数组、`static_assert` 标准布局/平凡析构 `:668-669,698-699`）；序列化仅落 `slots/specialized_slots/available_talent_points`（`:717-729`） | **是（D6 flag 二分的判据源）**：交付参数 → Baker/`delivery`；节点语义 → `HasNode`。生成器据此排除交付参数，不将其写入 SpecState | 不改结构；生成器产出中**不得**出现交付参数成员（D6） |
| I8 | 生成器家族 | `gen_skill_contracts.py`（JSON→JSON 原地，含 `--check`/幂等/确定性 `:710-744`）；`gen_tags.py`（JSON→**C++ 头** `TagRegistry.hpp`，`gen_tags.py:1-52`）；`gen_skill_mechanics_schema.py`（源码读点→schema JSON）；`gen_skill_spec_modifier_contract.py:13-35`（canonical→发布 JSON，含 schema 校验）；`sync_skill_node_icon_ids.py`；校验脚本 `check_module_boundaries.py:1-8`（四层边界）、`check_legacy_reintroduction.py:8`（对基线清单查回潮） | **是（范式复用源）**；注意：现有生成器中**无 C++ SpecState 产物先例**，最接近的是 `gen_tags.py` | 生成器扩展 `gen_skill_contracts.py`；产物落点参考 `gen_tags.py` + `CMakeLists.txt:176-192` |
| I9 | 现有手写镜像 | `tests/unit/SpecStateMappingTests.cpp`（382 行；独立手写 `kExpectedPoints(17)/kExpectedFlags(6)` `:43-80`、全成员清单 `:82-112`，对拍生产绑定表 `:134-158`）；生产侧 3 份手写 SpecState：`SevenStarSlashShared.hpp:288-405`（技能 10：17 point + 6 flag）、`HeavenlySwordDescent.hpp:50-260`（技能 11：16 point + 9 flag + 20 mech）、`BloodSea.hpp:130-290`（技能 12：18 point + 7 flag + 40 mech）。绑定结构三连：`{node, member}` / `{node, key, coeff, default}` | **是（对拍基线）**：3 份手写实现即 Phase 3 的黄金样本；`SpecStateMappingTests.cpp` 是待退役的「镜像的镜像」 | Phase 3 生成物须逐字段等于技能 10/11/12 手写表；随后 `SpecStateMappingTests.cpp` 改引用生成物（E4） |
| I10 | 技能树数据形态（节点语义名是否数据化） | 11 个 `*Nodes` 命名空间**全部手写于 C++**：`SevenStarSlashShared.hpp:258-284`（`TargetLock=1000`…`ReturningStep=1025`）、`FlowingThrust.cpp:32-61`（`SwiftBlade=100`…`ResidualElements=175`）等（实测 `rg "^namespace \w+Nodes"` 命中 11 处）。数据侧节点只有中文 `name_key`（实测 `skill1 node 100 name_key='迅捷之刃'`、`skill10 node 1000 name_key='星位校准'`）；`skill_node_prompts.json` 只有英文美术提示串（`"1000":"…representing 'Star Alignment'…"`） | **否（关键否定结论）**：节点「语义名」**未数据化**，且提示串与 C++ 标识符**不一致**（节点 1000：C++ `TargetLock` vs 美术名 `Star Alignment`；节点 114 恰好 `Riding the Wind ≈ RidingTheWind`，属巧合而非规则） | 生成器**不能**产出可靠的 `*Nodes::Name`/SpecState 成员名；命名须由新增 descriptor 或手写提供 |

> 补充（节点 id 区间约定，实测）：技能 1~12 的节点 id 以百位分带——s1 `100-175`、s2 `200-275`、s3 `300-375`、s4 `400-476`、s5 `500-555`、s6 `600-675`、s7 `700-775`、s8 `800-876`、s9 `900-993`、s10 `1000-1025`、s11 `1100-1124`、s12 `1200-1224`。可作为生成器分组与命名规则的辅助依据，但**不可**用于反推语义名。

---

## 3. 权威源判定（数据流）

### 3.1 节点集合与顺序 → `talent_tree`
- 技能 1~9：`assets/data/skills.json` 的 `skills[].talent_tree`（28/28/28/29/28/28/28/29/29 节点）。
- 技能 10~12：`assets/data/mastery_skill_trees.json` 的 `skills[].talent_tree`（26/25/25 节点）。
- 合计 331 个节点。`merge_skill_tree_coords_and_deps.py:31-78` 只回写 `x/y/prerequisites`，不改变节点 id 集合。
- **已核实**：`skill_contract.nodes` 始终是 `talent_tree` 的真子集（`extra=0`），故节点全集只能取 `talent_tree`。

### 3.2 角色与分类 → `skill_contracts_compact.json`（物化进 `skill_contract.nodes`）
- `gen_skill_contracts.py:210-526` 从 compact 计算每个节点的 `role/resist_model/scope_policy/affects_sword_intent/affects_sword_step/trigger/keystone_exclusion_group/cost_affix`，并对「非默认节点」物化进 `skills.json`/`mastery_skill_trees.json` 的 `skill_contract`（`:529-550,691-704`）。
- 角色分布（实测，物化契约内）：基础 1~9 → `Passive 53 / Keystone 29 / Transmuter 18 / Synergy 8 / Trigger 9`；精通 10~12 → `Passive 13 / Keystone 9 / Transmuter 4 / Synergy 3 / Trigger 4`。
- **已核实**：节点角色缺省为 `Passive`（`SkillRegistry.cpp:255-261` 运行期默认、`gen_skill_contracts.py:444-455` 生成期默认一致）。因此生成器可「以 `talent_tree` 为全集 + `skill_contract` 覆盖 + 缺省 Passive」精确复算角色。

### 3.3 机制数值与键 → `skill_mechanics.json` + schema
- 数值按 `(skill_id, node_id, key)` 读取（`SkillPointAccess.hpp:38-43` → `SkillMechanicsRegistry`）。
- 对账机制：`gen_skill_mechanics_schema.py` 静态扫描源码读点生成 schema（`:13-27`）；运行期 `SkillMechanicsRegistry::ValidateAgainstSchema()` 双向比对「JSON 有而 schema 无」与「schema 读点而 JSON 无」（`SkillMechanicsRegistry.cpp:280-328`）。这是本项目**已有的「代码↔数据」对账范式**，可直接复用为 D-A8。

### 3.4 命名标识符 → **无数据源（关键否定）**
- C++ 侧 `SevenStarSlashNodes`/`FlowingThrustNodes` 等 11 个命名空间**均为手写**（`SevenStarSlashShared.hpp:258-284`、`FlowingThrust.cpp:32-61` 等）。
- 数据侧能表达「名字」的字段只有：
  1. 中文显示名 `name_key` / 描述 `desc_key`（`skills.json`、`mastery_skill_trees.json` 节点内；实测 `'迅捷之刃'`、`'星位校准'`）；
  2. 英文美术提示串 `assets/data/skill_node_prompts.json`（以节点 id 为键的散文，实测 `"1000":"…'Star Alignment'…"`、`"114":"…'Riding the Wind'…"`）。
- **已核实不一致**：节点 1000 的 C++ 标识符为 `TargetLock`（`SevenStarSlashShared.hpp:259`），而美术名为 `Star Alignment`、中文名为 `星位校准`。三者语义相关但字面不同，无法机械映射。
- **结论（已核实）**：生成器**不能**从现有数据文件产出可靠的 C++ 标识符（`*Nodes::Name`、SpecState 成员名）。命名是 D9 的唯一硬缺口。

---

## 4. `--gen-specstate` 决策

### 4.1 输入源
分两层，互不替代：

1. **结构输入（现有数据，可直接用）**
   - 节点全集/顺序/`max_points`：`skills.json`（1~9）+ `mastery_skill_trees.json`（10~12）的 `talent_tree`。
   - 角色/排除组/触发/转质分类：`skill_contracts_compact.json`（或其对拍物 `skill_contract.nodes`），缺省 `Passive`。
   - 机制键/数值/默认值：`skill_mechanics.json`（+ `skill_mechanics_schema.json` 对账）。
   - 交付参数边界（用于 D6 排除）：`SkillDefs.hpp:625-699`。
2. **命名输入（现有数据缺失，必须新增或手写）**
   - 推荐：**新增每技能命名清单 descriptor**（例如 `assets/data/skill_specstate/<skill>.json`），每节点显式给出 `{node_id, identifier, kind: point|flag|mech, member, key?, default?}`。descriptor 成为**命名的唯一事实源**，现有数据文件对其做完整性/一致性校验（节点集合必须等于 `talent_tree`，角色必须等于 compact，mech key 必须存在于 `skill_mechanics.json`）。
   - 备选：不新增文件，命名继续手写，生成器退化为只读校验器（见 §6）。

### 4.2 产物落点
- **形式**：**C++ 生成头**（**不**写回 `assets/data/*.json`）。理由：SpecState 依赖 C++ 成员指针（`int State::*` / `bool State::*` / `float State::*`，见 `SevenStarSlashShared.hpp:318-326`、`HeavenlySwordDescent.hpp:110-126`），JSON 无法承载。
- **路径与命名规则**：`src/game/systems/skill/behaviors/generated/<PascalSkill>SpecState.gen.hpp`，例如 `SevenStarSlashSpecState.gen.hpp`、`HeavenlySwordSpecState.gen.hpp`。`<PascalSkill>` 与行为文件名同源（`SevenStarSlash.cpp` → `SevenStarSlashSpecState.gen.hpp`）。节点 id 常量若需生成，落 `.../generated/<PascalSkill>Nodes.gen.hpp`。
- **内容**：POD `struct <Skill>SpecState{...}` + `PointBinding/FlagBinding/MechBinding` 结构 + `k<Skill>PointBindings/k<Skill>FlagBindings/k<Skill>MechBindings` 常量表 + `ResolveSpecState(...)`（非点读的转质节点如技能 10 的 `1021/1022` 保留在表循环外，见 `SevenStarSlashShared.hpp:400-404`）。
- **C++ 引用方式**：手写行为头（如 `SevenStarSlashShared.hpp`）改为 `#include ".../generated/SevenStarSlashSpecState.gen.hpp"`，行为 `.cpp` 与单测共用同一生成物（延续「行为与单测共享同一定义」原则，`SevenStarSlashShared.hpp:287`）。
- **构建挂接**：在 `CMakeLists.txt` 仿 `gen_tags.py` 的 `add_custom_command(OUTPUT <header> COMMAND Python3::Interpreter <script> --gen-specstate DEPENDS <script> <data...> <descriptor...>)` + `add_custom_target(GenerateSkillSpecStates DEPENDS ...)`，令 `SkillBehaviors`（`CMakeLists.txt:199`）依赖之（参见 `CMakeLists.txt:176-192`）。
- **校验**：`NoMoreDay` 的 POST_BUILD 或 PR 门禁中执行 `--check`，防止「改了 descriptor/数据却忘记重生成」。

### 4.3 命名来源与约束
- **来源顺序**：① descriptor 的 `identifier/member` 字段（推荐）；② 手写枚举（退化）。
- **约束**：生成器**必须**对 descriptor 与现有数据做闭合校验，禁止生成「数据中不存在的 node_id」或「角色不一致」的绑定；不得生成交付参数成员（D6）。
- **推断（未核实）**：`skill_node_prompts.json` 的英文名可作为 descriptor 作者的**命名建议来源**（人工校对后写入 descriptor），但**不可**作为自动映射输入（§3.4 已证不一致）。

---

## 5. 与现有生成器共享范式（落点细化）

### 5.1 复用 `gen_skill_contracts.py` 的 `--check`/幂等/确定性
- **CLI**：扩展 `gen_skill_contracts.py:710-744`，新增 `--gen-specstate`，与既有 `--check/--check-idempotency/--check-determinism` 组合（计划 §6 已约定同一处扩展）。
- **`--check`**：仿 `:679-689`——对生成物与磁盘内容逐字节比对，落后即返回非零并打印漂移对象（现为 `changed_skill_ids`，C++ 产物改为文件路径列表）。
- **幂等**：仿 `_verify_idempotency`（`:581-599`）——在「已生成结果」上再跑一次，第二次必须零变更。
- **确定性**：仿 `_verify_determinism`（`:553-578`）——对同一输入跑两次 deepcopy，序列化必须相等；C++ 产物序列化 = 渲染后的头文件文本，需固定键序/成员序（当前生成器已用 `sorted(nodes_by_id.keys())`，`:438`）。
- **注意差异（已核实）**：现生成器产物是 `assets/data` 下的 JSON（`:691-707`），新产物在 `src/` 下；`--check` 需同时覆盖两类落点。

### 5.2 复用 `gen_tags.py` + CMake 的「生成 C++ 头」落点范式
- `gen_tags.py` 从 `assets/data/tags.json` 生成 `src/game/foundation/data/TagRegistry.hpp`（`gen_tags.py:1-52`），并通过 `CMakeLists.txt:176-192` 的 `add_custom_command`/`add_custom_target` 纳入构建；该头被 `TriggerRuleComponent.hpp:9` 直接 `#include`。
- **可用性对照**：`tags.json` 的 `tags` 数组存的是**标识符字符串**（`"Physical","Fire",…`，实测），故可整表生成；技能节点数据**不含**标识符 → 这正是必须新增 descriptor 的原因，也是二者范式的关键差别。
- **缺口（已核实）**：`gen_tags.py` 自身**无 `--check`**；因此 `--gen-specstate` 应采 `gen_skill_contracts.py` 的校验框架 + `gen_tags.py` 的落点/构建范式，两者互补。

### 5.3 复用 `gen_skill_mechanics_schema.py` 的「代码读点↔数据」对账（D-A8 反向校验）
- 该脚本静态扫描 `src/` 的机制读点，产出 schema 并支持 `--check`（`:13-32`）；运行期 `ValidateAgainstSchema()` 双向比对（`SkillMechanicsRegistry.cpp:280-328`）。
- **复用方式**：生成 SpecState 后，反向解析生成头中的 `*MechBinding{node,key,default}` 与 point/flag 绑定，断言其节点集合/角色与 `talent_tree`+compact 一致（D-A8「生成表 == 运行期 `ReadPoints`/`HasNode` 所覆盖节点」）。
- **退役衔接（E4）**：`SpecStateMappingTests.cpp`（`:43-112,134-158`）改为断言「生产绑定表 == 生成物」，退役独立手写镜像；测试经 `tests/CMakeLists.txt:4` 的 `GLOB_RECURSE` 自动纳入，无需单独注册。

---

## 6. 退化方案（手写 + 仅校验）与触发条件

**触发条件（满足其一即退化）**：
1. 不接受新增命名 descriptor 数据文件（维护/评审成本）；或
2. 无法在数据层稳定表达 C++ 标识符（即放弃 §4.1 「命名输入」）。

**退化形态**：
- SpecState POD、绑定表、`ResolveSpecState` **继续手写**（沿用技能 10/11/12 现有形态）。
- `--gen-specstate` 退化为**只读校验器**（无 C++ 产物）：
  - 断言手写绑定的节点集合 == `talent_tree` 全集；
  - 断言手写绑定的角色/分类与 `skill_contract`/compact 一致；
  - 断言 mech 绑定 `(node,key,default)` 与 `skill_mechanics.json`+schema 一致；
  - 输出 `--check` 退出码与漂移报告，纳入现有 CI 门禁。
- D2=b 的「生成+校验」仍成立，只是「生成」限定为**结构/一致性校验产物**，而非可编译的 C++ 声明。
- **推断**：退化方案能消除「数据改了表没改」的漂移，但**不能**消除「命名手写」成本；与 D3=b（统一模板）兼容，模板由 Phase 2 单独落地。

---

## 7. 未决假设与风险

| # | 项 | 类型 | 说明 / 缓解 |
|---|----|------|-------------|
| R1 | **命名标识符无数据源** | **已核实** | §3.4。D9 核心缺口；须由 descriptor 或手写补足，否则无法「生成含命名声明的 SpecState」 |
| R2 | descriptor 的引入是否违背「零新数据文件/单一事实源」倾向 | 推断（需用户裁定） | 建议：descriptor **只承载命名**，结构事实仍归现有数据文件，并加闭合校验，避免第二事实源漂移 |
| R3 | `skill_contract.nodes` 非全集易被误当权威 | 已核实 | 生成器必须以 `talent_tree` 为全集、缺省 `Passive`；`skill_contract` 仅覆盖层 |
| R4 | 生成 C++ 头 vs 就地扩展现有行为头 | 推断 | 建议独立 `generated/*.gen.hpp`，避免与手写内容冲突；Phase 2/3 落地时确认 `check_module_boundaries.py:1-8` 四层边界允许 behaviors 层生成头 |
| R5 | 生成物与 `SpecStateMappingTests.cpp` 的接替顺序 | 推断 | 先对拍（生成物 == 手写表）再退役镜像，避免双源窗口期（设计 §11「双源漂移」中风险） |
| R6 | 转质非点读节点（如技能 10 `1021/1022`、技能 11/12 同类）不能被表循环覆盖 | 已核实 | 保留在 `ResolveSpecState` 表循环外（`SevenStarSlashShared.hpp:400-404`）；生成器须显式建模为「非点读」类别 |
| R7 | `skills.json` 中技能 1~9 与 mastery 中 10~12 的读取路径分裂 | 已核实 | 生成器须双源加载（`SkillRegistry.cpp:661-719` 运行时亦如此）；单测/生成器若只读 `skills.json` 会漏 10~12 |
| R8 | 交付参数误入 SpecState（D6 违反） | 推断 | 以 `SkillDefs.hpp:625-699` 字段集为排除清单；生成器输出做负向断言 |
| R9 | `gen_tags.py` 无 `--check` 先例，易误以为「生成头无需校验」 | 已核实 | `--gen-specstate` 必须补齐 `--check`（§5.1） |

---

## 8. 关键证据清单

**生成器与范式**
- `scripts/gen_skill_contracts.py:20-22`（默认输入路径）、`:25-67`（角色/枚举常量域）、`:210-526`（契约构造）、`:485-498`（`emits_non_default` 省略纯被动）、`:553-578`（确定性）、`:581-599`（幂等）、`:679-689`（`--check`）、`:691-707`（写回）、`:710-744`（CLI）
- `scripts/gen_tags.py:1-52`（JSON→C++ 头范式）、`CMakeLists.txt:176-192`（`add_custom_command`/`GenerateTags`）、`src/game/foundation/components/TriggerRuleComponent.hpp:9`（生成头 `#include` 先例）
- `scripts/gen_skill_mechanics_schema.py:2-32`（源码扫描↔数据对账 + `--check`）、`:21-27`（识别 `*Nodes::Name` / `*MechBinding`）
- `src/game/foundation/data/SkillMechanicsRegistry.cpp:280-328`（运行期双向对账）
- `scripts/merge_skill_tree_coords_and_deps.py:31-78,88-153`（坐标/前置合并；无 `--check`）
- `scripts/check_module_boundaries.py:1-8`、`scripts/gen_skill_spec_modifier_contract.py:13-35`、`scripts/check_legacy_reintroduction.py:8`

**数据文件（实测结构）**
- `assets/data/skill_contracts_compact.json`：`version=1` + `skills[]`，覆盖技能 1~12；键集合见 §2/I1
- `assets/data/skills.json`：技能 1~9；节点字段 `id/name_key(中文)/desc_key(中文)/max_points/x/y/prerequisites/stat_modifiers/icon_id`（+可选 `add_tags/remove_tags`）；`skill_contract.nodes[].role` 实测 `Passive 53/Keystone 29/Transmuter 18/Synergy 8/Trigger 9`
- `assets/data/mastery_skill_trees.json`：技能 10/11/12（`mastery_id sword_saint/heavenly_sword/demon_blade`）；`skill_contract` 角色 `Passive 13/Keystone 9/Transmuter 4/Synergy 3/Trigger 4`
- `assets/data/skill_mechanics.json`：`{skill:{node:{key:value}}}` 结构
- `assets/data/skill_node_prompts.json`：节点 id → 英文美术提示串（实测 `"1000":"…'Star Alignment'…"`、`"114":"…'Riding the Wind'…"`）
- 节点 id 区间：`skill_1_tree.json`…`skill_9_tree.json` 存在；技能 10~12 树仅在 mastery 文件内

**C++ 契约与消费点**
- `src/game/foundation/data/SkillContract.hpp:9-46,48-58,60-70,72-91`
- `src/game/foundation/data/SkillRegistry.hpp:43-49`
- `src/game/foundation/data/SkillRegistry.cpp:255-261`（默认角色）、`:265-298`（建树+契约）、`:661-730`（加载，含 mastery `:714-719`）
- `src/game/foundation/components/SkillDefs.hpp:625-699,717-729`
- `src/game/foundation/components/SkillPointAccess.hpp:14-43`（`ReadPoints`/`HasNode`/`GetMech`）

**手写镜像与黄金样本**
- `src/game/systems/skill/behaviors/SevenStarSlashShared.hpp:258-284`（id 常量）、`:288-314`（POD）、`:318-326`（绑定结构）、`:328-378`（point/flag 表）、`:382-405`（`ResolveSpecState`）
- `src/game/systems/skill/behaviors/HeavenlySwordDescent.hpp:46,50-106,110-126,128-260`
- `src/game/systems/skill/behaviors/BloodSea.hpp:130-236,271-290`
- `tests/unit/SpecStateMappingTests.cpp:1-11,31-80,82-112,134-158`（382 行）
- `src/game/systems/skill/behaviors/FlowingThrust.cpp:32-61`（`*Nodes` 手写命名空间示例）

---

## 9. R2 裁定（用户拍板，2026-09-14）

**结论：采用混合方案。** 新增每技能「薄命名 descriptor」，只承载「节点 id → SpecState 标识符」的命名；结构与拓扑事实仍归现有数据文件，生成器据此产出标识符并做闭合校验。

### 9.1 裁定内容

| 项 | 裁定 |
|---|---|
| 是否新增数据文件 | **是**：每技能一份薄命名 descriptor（命名清单） |
| descriptor 承载范围 | 仅「命名」：`node_id` → C++ 标识符 / SpecState 成员名 / mech key 命名 |
| 结构/拓扑事实源 | **不变**：仍为 `talent_tree`（节点全集/顺序/`max_points`）、`skill_contracts_compact.json`（角色/触发/转质）、`skill_mechanics.json`（机制键/数值） |
| descriptor 与现有数据关系 | 生成期**闭合校验**：descriptor 节点集合必须等于 `talent_tree`，角色必须等于 compact，mech key 必须存在于 `skill_mechanics.json`；禁止 descriptor 成为「第二结构事实源」 |
| 交付参数（D6） | descriptor 不得声明 `BakedDeliveryParams`/`feature_flags` 成员；生成器对其做负向断言（沿用 §7 R8） |

### 9.2 对 D9 结论的影响

- §4.1「输入源」由原先的两选一（现有数据 / 新中间文件）**改为组合**：
  - **结构输入** = 现有数据（`talent_tree` + compact + `skill_mechanics`，§3.1~§3.3 已核实）；
  - **命名输入** = 新增薄命名 descriptor（§3.4 已核实：现有数据无法提供可靠 C++ 标识符，`TargetLock` vs `Star Alignment` vs `星位校准`）。
- §3.4 的「关键否定」结论**仍然成立**：命名是硬缺口，descriptor 正是为补此缺口而引入；它与「零新数据文件」倾向的冲突（R2）以「descriptor 只命名、不承载结构 + 闭合校验」化解。
- §6 退化方案（不新增文件、生成器退化为只读校验器）**不再作为默认路径**，保留为兜底（当 descriptor 维护成本不可接受时）。
- §4.2 产物落点、§4.3 命名来源顺序（① descriptor `identifier/member`）**不变**；§5.1 的 `--check`/幂等/确定性要求**不变**。

### 9.3 未受影响的既有结论

R1（命名无数据源）、R3（`skill_contract.nodes` 非全集）、R6（转质节点不入表循环）、R7（技能 1~9 与 10~12 双源加载）、R8（交付参数不得入 SpecState）、R9（须补 `--check`）均维持原判。
