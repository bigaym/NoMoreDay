# A-01 技能 1~12 专精抽象化 — 实施计划（Wave E）

- 日期：2026-09-14
- 设计：`docs/designs/2026-09-14-skill-abstraction-track-a01-design.md`（状态：已裁定）
- 上游计划：`docs/plans/2026-09-13-skill-followup-plan.md:188-194` §7 Wave E（E1=设计、E2=迁移技能 1~9）
- 决策基线：D1=b / D2=b / D3=b / D4=b / D5=b / D6=b / D7=b / D8=a / D9=先盘点再定
- 基线提交：`4d9a1fe2`（工作树含未提交的技能 1/12 后续改动，见 §8 前置）
- 范围：技能 1~9 效果层信封化 + R1 profile 样板归零 + `SpecStateTable<State>` 模板 + `--gen-specstate` 生成器；技能 10~12 仅回归验证（D7）

## 1. 实施思路与原则

1. **盘点先行（D9）**：生成器的输入源与产物落点未定，Phase 0 先产出决策记录，禁止在 Phase 0 结论前写死生成器接口。
2. **前哨去重再信封化**：R1（profile 样板）改动小、收益确定、可独立验证，先做（Phase 1）以减少后续每批的重复结构。
3. **模板 + 黄金样本**：先用 `SpecStateTable<State>` 模板在一个**小爆炸半径**技能上跑通（Phase 2），再生成器接管（Phase 3），最后批量迁移（Phase 4）。
4. **风险升序分批**：批次按「文件行数 / flag 数 / 跨技能耦合」升序排列，每批独立可回退、独立审查。
5. **能力型 DoD（D1=b）**：不设行数门禁；行数只用 `scripts/count_code_lines.py` 观测并写入审查报告（D-A12）。
6. **单源优先**：点数值走 `ReadPoints`、点亮走 `HasNode`、机制数值走 `GetMech`、交付参数走 Baker（设计 §5.2）。
7. **零兼容层**：不保留旧字段别名（`docs/workflows/planning.md:27`）。
8. **串行约束**：`SkillDefs.hpp`、`SevenStarSlashShared.hpp`、`FlowingThrust.cpp`、`SkillSystem.cpp`、`scripts/gen_skill_contracts.py` 为高冲突文件，同批内串行改动，不并行编译（沿用 `2026-09-13-skill-followup-plan.md:15`）。

## 2. 改动面总览（预估）

| Phase | 目标 | 新增/修改文件 | 预估改动点 |
|---|---|---|---|
| 0 | D9 盘点决策 | `docs/` 决策记录（新） | 只读盘点 + 1 份记录 |
| 1 | R1 profile 基元 | `src/game/systems/skill/SkillProfileResolve.{hpp,cpp}`（新）+ 9 个行为文件 | `FlowingThrust.cpp:97-107`、`RendingWave.cpp:82-90`、`BladeFormation.cpp:95-103`、`BladeWard.cpp:66-76`、`BladeBoomerang.cpp:97-105`/`:293`、`InfiniteBlades.cpp:90`/`:142-150`、`SwordArray.cpp:67-76`、`MindBlade.cpp:39-45`、`PhantomTrance.cpp:69-82` |
| 2 | `SpecStateTable<State>` 模板 + 黄金样本 | `src/game/systems/skill/behaviors/SpecStateTable.hpp`（新）+ `BladeBoomerang.cpp` | 模板 1 个 + 技能 8 信封 1 套 |
| 3 | `--gen-specstate` 生成器 | `scripts/gen_skill_contracts.py`（扩展）+ 生成物 | 生成器 + `--check` |
| 4 | 批量迁移技能 1~9 | 8 个行为文件（分 3 批） | 每技能 1 套信封 |
| 5 | 收尾与终态验证 | flag 二分收尾 + 基元盘点 + 回归 | 见 Phase 5 |

## 3. Phase 0 — D9 盘点（生成器输入源与产物落点）

> **产出物**：一份决策记录，落在 `docs/designs/2026-09-14-skill-abstraction-a01-d9-inventory.md`（或并入本计划附录，实施时二选一，推荐独立成文）。
> **规则**：本 Phase 为只读盘点；未产出决策记录前，Phase 3 的生成器设计**不得**写死输入源。

### 3.1 盘点对象（可执行方法：图工具 + `rg` + 读文件）

| # | 盘点对象 | 目的 | 证据锚点 |
|---|---|---|---|
| I1 | 紧凑契约输入 | 是否含「节点 id + 角色 + 名称」的权威列表，可作 SpecState 生成输入 | `assets/data/skill_contracts_compact.json`（`gen_skill_contracts.py:22`） |
| I2 | 技能主数据 | 节点契约块（role/resist/scope/trigger_window）现状 | `assets/data/skills.json`（`gen_skill_contracts.py:20`）、角色常量 `:25-67` |
| I3 | 精通树结构 | 节点 id 与依赖/坐标，可提供节点全集与顺序 | `assets/data/mastery_skill_trees.json`（`gen_skill_contracts.py:21`）、`scripts/merge_skill_tree_coords_and_deps.py` |
| I4 | 机制数值表 + schema | 数值来源与「代码读点 ↔ JSON」对账机制 | `assets/data/skill_mechanics.json`、`assets/data/skill_mechanics_schema.json`、`scripts/gen_skill_mechanics_schema.py:43-45` |
| I5 | 角色枚举 | 生成器可复用的节点角色取值域 | `src/game/foundation/data/SkillContract.hpp:12-18`（Passive/Keystone/Trigger/Synergy/Transmuter） |
| I6 | 节点契约消费面 | SpecState 需覆盖的节点属性 | `src/game/foundation/data/SkillContract.hpp:60` `NodeContractData`；`SkillRegistry.hpp:46` `GetNodeContract` |
| I7 | 交付参数结构 | 交付层字段全集（判断哪些 flag 属交付参数，D6） | `src/game/foundation/components/SkillDefs.hpp:625-699` |
| I8 | 生成器家族现状 | 复用其 `--check`/幂等/确定性范式，避免造轮 | `scripts/gen_skill_contracts.py`、`gen_skill_mechanics_schema.py`、`gen_skill_spec_modifier_contract.py:13-35`、`sync_skill_node_icon_ids.py` |
| I9 | 现有手写镜像 | 退役对象与对拍基线 | `tests/unit/SpecStateMappingTests.cpp`、`src/game/systems/skill/behaviors/SevenStarSlashShared.hpp:318-378` |
| I10 | 技能树数据形态 | 节点「语义名」是否数据化（决定生成声明能否含标识符） | `rg` 各行为文件的 `*Nodes` 命名空间（`FlowingThrust.cpp:32-61` 等） |

### 3.2 盘点任务

- [ ] P0.1 用 `search_graph`/`get_code_snippet` 确认 `NodeContractData` 字段全量与 role 分布。
- [ ] P0.2 用 `rg` 统计 12 个技能节点 id 的来源：`compact` vs `mastery trees` vs C++ 命名空间，判定**权威源**。
- [ ] P0.3 判定生成物落点（新建 `*.gen.hpp` vs 就地扩展现有文件），并列出对编译单元/包含关系的影响。
- [ ] P0.4 判定节点「语义名」（如 `RidingTheWind`）是否有数据来源；若无，记录「生成器只能产出 id→成员映射，命名仍需手写枚举」的约束。
- [ ] P0.5 产出决策记录：**输入源 + 产物落点 + 命名来源 + 与现有生成器的共享范式**。
- **DoD**：决策记录含明确结论与 `文件:行` 证据；若结论为「无合适输入源」，则回退到 D2 的退化方案（手写 SpecState + 生成器仅做校验），并回报用户。

## 4. Phase 1 — R1 profile 解析基元（前哨去重）

### 4.1 原理

当前 9 处行为层样板形状为：

```
profile = GetBakedSkillProfile(reg, owner, skillId)
if (!profile) { for spec in specialized_slots: if spec.skill_id==skillId { Bake(...,&spec,local,&profile); break; } }
```

其中 `local` 是栈上临时 profile，`profile` 指针被重指向它。该形状与「数据所有权」一致，可收敛为单一基元（D5=b：归 skill 层）。

### 4.2 伪代码（接口草图）

```cpp
// 新文件：先落点于 src/game/systems/skill/（D5=b），实现阶段按 Phase 0 结论微调
// 语义：优先返回缓存；miss 时由调用方提供栈存储回退 Bake，避免返回悬垂。
[[nodiscard]] const BakedSkillProfile *ResolveBakedProfile(
    entt::registry &registry, entt::entity owner, uint32_t skillId,
    BakedSkillProfile &scratch);   // scratch 由调用方栈上声明

// 便捷重载：行为层 DoCast/DoHit 内声明 scratch 的宏或局部包装，
// 保证 scratch 生命周期覆盖 profile 使用期。
```

约束：
- 不改变 `SkillSystem::GetBakedSkillProfile`（`SkillSystem.cpp:2753`）语义。
- `DoHit` 中 `actualAttacker` 可能为召唤物 owner（`FlowingThrust.cpp:334-337`），基元需接受已解析的 owner。
- 非行为消费点（`ElementPathSystem.cpp:69`、`AreaFieldDeliverySystem.cpp:447/468`、`SkillDisplayPreviewService.cpp:24`、`SkillSystem.cpp:1801/2000`）本轮**可选**纳入，不强制。

### 4.3 任务

- [x] P1.1 新增 `ResolveBakedProfile` 基元（落点按 Phase 0 结论）。
- [x] P1.2 替换行为层 9 处样板（清单见 §2 Phase 1）。
- [x] P1.3 单测：缓存命中 / miss 回退 / 召唤物 owner 三种路径。
- **依赖**：无前置 Phase；可与 Phase 0 并行（不触碰生成器）。
- **回退**：还原 9 处调用点即可，基元文件独立删除。
- **DoD**：`rg -n "SkillSpecializationBaker::Bake" src/game/systems/skill/behaviors/` 命中 0（D-A3）；`-L ci` 全绿。
- **验证记录（2026-09-14）**：`.\build.bat RelWithDebInfo` EXIT=0、0 编译警告/错误；`-L ci` 1533 用例/113921 断言全绿；`-L skill` 194 用例/5629 断言全绿；`[Functional]*` 178 用例/2553 断言全绿；新增基元单测 5 用例/29 断言全绿。D-A3 命中 0。

## 5. Phase 2 — `SpecStateTable<State>` 模板 + 黄金样本

### 5.1 原理

把技能 10 手写的绑定表结构（`SevenStarSlashShared.hpp:318-378`）泛化为模板（D3=b），并在一个**小爆炸半径**技能上验证。黄金样本选 **技能 8 御剑·回旋（`BladeBoomerang.cpp`，378 行）**：它是技能 1~9 中最小、且交付耦合最典型（`feature_flags` + `BakedDeliveryParams` 技能 8 专项字段 `SkillDefs.hpp:642-661`）者，能同时验证模板机制与 D6 flag 二分，风险最低。

> 假设（待 Phase 2 启动时确认）：若盘点（I6）显示技能 8 的节点语义过薄、不足以验证模板，则改选技能 6（`SwordArray.cpp`，498 行，14 处 flag，最能验证 flag 二分）。

### 5.2 伪代码（结构草案）

```cpp
template <typename State>
struct SpecStateTable {
  std::span<const typename State::PointBinding> points; // {node, int State::*}
  std::span<const typename State::FlagBinding>  flags;  // {node, bool State::*}
};

// 单点解析：首个匹配槽位填表；转质/激活节点在表循环外叠加（不变量 §5.4-2）
template <typename State>
[[nodiscard]] State ResolveSpecState(const entt::registry &registry,
                                     entt::entity owner, uint32_t skillId,
                                     const SpecStateTable<State> &table);

// 技能 8 黄金样本声明（形状示意，命名由生成器统一）
struct BladeBoomerangSpecState {
  using PointBinding = ...; using FlagBinding = ...;
  int returnDamagePoints = 0; /* ... */
};
inline constexpr SpecStateTable<BladeBoomerangSpecState> kBladeBoomerangTable{...};
```

技能 10 回填为第二个调用点，验证模板对已信封技能的兼容（不改行为）。

### 5.3 任务

- [x] P2.1 新增 `SpecStateTable<State>` + `ResolveSpecState<State>`（POD 约束）。
- [x] P2.2 技能 8 信封化：DoCast/DoHit 散装读点改为单点 `ResolveSpecState`。
- [x] P2.3 技能 8 的 flag 判定按 D6 二分：节点语义 → `HasNode`；交付参数 → 留 Baker 并加 `delivery-param` 注释。**实测结论：技能 8 的 `feature_flags` 仅 810/850/854 三位，且 850/854 位由 Baker 写入、被交付层（`Projectile.hasPull`/体积）消费，属交付参数位；为保持烘焙快照语义不变，保留位读取并加 D6 注释，未迁移为 `HasNode`。**
- [ ] P2.4 技能 10 回填模板（行为不变，作为对照组）。**（本轮用户指令：P2 只做一个黄金样本、不动其它技能，故未执行；技能 10 仅以对拍测试覆盖模板等价性。）**
 - [x] P2.5 单测扩展：技能 8/10 的「生成表逐节点 == 运行期 `ReadPoints`」断言（手写黄金样本，暂由 Phase 3 接管）。**扩展：技能 11/12 手写解析器也纳入逐字段对拍（生产绑定表搬入模板后逐绑定比较），覆盖 10~12 全部手写表。**
- **依赖**：Phase 1（减少样板干扰）。
- **回退**：技能 8/10 各自独立；模板文件可保留（无调用即无影响）。
- **DoD**：D-A1/A2/A5 在技能 8 上达标；`-L ci` + `-L functional` 绿；技能 10 行为零变化。
 - **验证记录（2026-09-14）**：`.\build.bat RelWithDebInfo` EXIT=0、0 编译警告/错误；`ctest -L ci` 全绿（1538 用例/114235 断言）；`-L skill` 2 测试全绿（unit 160/4555、integration 39/1388）；`[Functional]*` 178 用例/2553 断言全绿；新增 `SpecStateTableTests` 5 用例/314 断言（技能 8 信封 + 技能 10/11/12 手写表对拍）；技能 8 定向（`*BladeBoomerang*,*SpecStateMapping*,*SpecStateTable*`）44 用例/1915 断言全绿。D-A3 命中 0；D-A2 技能 8 仅剩 2 处 `feature_flags &`（Baker 交付参数位，已加 D6 注释）。数据门禁 `gen_skill_mechanics_schema.py --check` / `gen_skill_contracts.py --check` / `sync_skill_node_icon_ids.py --check` PASS。集成守卫 `GameplaySystems.cpp` 的节点 id 校验清单同步改指 `BladeBoomerangSpecState.hpp`（沿用技能 10 迁移先例）。

## 6. Phase 3 — `--gen-specstate` 生成器

### 6.1 原理

在 Phase 0 决策记录确定的输入源上，生成 SpecState 结构声明 + 绑定表；沿用 `gen_skill_contracts.py` 既有 `--check`/`--check-idempotency`/`--check-determinism` 范式（`gen_skill_contracts.py:721-740`）。

### 6.2 伪代码（生成器骨架）

```
parse args: --skills/--compact/...  + --gen-specstate  --check  --check-idempotency  --check-determinism
inputs (由 P0 决策): <权威节点源>
for skill in 1..12:
    state = build_specstate_model(skill)      # 字段名 + 节点 id + int/bool 类型
    emit_decl(state)                          # 结构体 + 绑定表
    emit_resolve_specialization(state)        # 或共用模板实例
compare with on-disk generated artifact:
    --check → 有差异则非零退出并打印 diagnostics
```

### 6.3 任务

- [x] P3.1 实现生成器（输入源按 P0 决策记录）。
- [x] P3.2 幂等/确定性自检（重复运行无 diff；顺序稳定）。
- [x] P3.3 对拍：技能 8（Phase 2 手写黄金样本）+ 技能 10（既有）生成物与手写一致。
- [~] P3.4 逆向校验：对每技能断言「生成表逐节点 == 运行期 `ReadPoints`」（D-A8）。
      已覆盖技能 8/10/11/12（生成表 vs 手写表在同一专精分配下逐绑定解析值对拍）；1~9 全量留待 Phase 4 各技能信封化时逐批补齐。
- [~] P3.5 `SpecStateMappingTests.cpp` 改为引用生成物，退役手写镜像。
      延后：当前生成器只覆盖 8/10/11/12，且退役独立镜像会削弱对拍强度；待 Phase 4 覆盖 1~9 后再执行。
      **Phase 5 复核结论（2026-09-14）：评估后不退役、保持现状**——该文件已非「解析逻辑镜像」（自身注释声明直接调用生产 `ResolveSpecState`），其独立期望值断言与技能 10 转质节点（`GetActiveTransmuterNode`）选取、槽位过滤等运行期语义，`GeneratedSpecStateTests` 并未等价覆盖（后者仅比对「生成表 ↔ 手写绑定表」，不调用技能 10 生产解析器）；且 D7 要求技能 10~12 仅回归，保留该唯一独立运行期回归用例更稳妥。D-A8 的 1~12 覆盖以 `GeneratedSpecStateTests` 为准（见 §9.1）。
- **依赖**：Phase 0 决策记录（硬前置）；Phase 2 黄金样本。
- **回退**：删除生成物与生成器分支，回到 Phase 2 手写状态。
- **DoD**：D-A7 达标（`--check`/幂等/确定性 PASS）；D-A8 覆盖技能 1~12。
- **实际范围说明**：命名 descriptor 与生成物覆盖技能 8/10/11/12（即已有手写 SpecState 表、可对拍的四个技能）。
  技能 1~7/9 无手写表可对拍，且 `BladeWard.cpp`/`MindBlade.cpp`/`PhantomTrance.cpp` 目前不存在完整节点标识符
  （技能 3/10 亦各缺 1 个），补齐属 Phase 4 逐技能迁移工作；不提前臆造约 84 个无消费方标识符。
- **验证记录（2026-09-14，Phase 3）**：`.\build.bat RelWithDebInfo` EXIT=0、零警告/错误；
  `ctest -L ci` 全绿（1 测试）；`ctest -L skill` 2 测试全绿（unit 2.50s、integration 1.04s）；
  非性能全量 1542 用例/114407 断言全绿；`[Unit]*Skill*` 164/4727、`[Integration]*Skill*` 39/1388、
  `[Functional]*` 178/2553 全绿；新增 `GeneratedSpecStateTests` 4 用例/172 断言（技能 8/10/11/12 生成物 vs 手写表对拍）；
  技能 8 定向 `*BladeBoomerang*` 30 用例/435 断言全绿。
  生成器门禁：`gen_skill_contracts.py --gen-specstate --check --check-idempotency --check-determinism` EXIT=0；
  负向用例：descriptor 缺节点 → `missing=[800]` 报错、多节点 → `extra=[899]` 报错、无 descriptor 的生成物 → 报错，均非零退出。
  数据门禁：`gen_skill_mechanics_schema.py --check`（472 entries, 80 unreferenced）、`sync_skill_node_icon_ids.py --check` PASS。
  CMake 新增 `GenerateSpecState` 目标（镜像 `GenerateTags`，不强制加入其它目标依赖）。

## 7. Phase 4 — 批量迁移技能 1~9

按风险升序分 3 批；每批 1~3 技能，独立验证、独立审查。技能 8 已在 Phase 2 完成。

### 7.1 批次划分

| 批 | 技能 | 文件（行数） | 主要风险点 |
|---|---|---|---|
| **4a** | 6 剑阵诛仙、5 万剑归宗 | `SwordArray.cpp`(498)、`InfiniteBlades.cpp`(513) | 14 处 flag、引导交付 |
| **4b** | 4 剑气护体、2 裂空斩、3 御剑术 | `BladeWard.cpp`(590)、`RendingWave.cpp`(702)、`BladeFormation.cpp`(726) | 反击口径（`DamageInterceptors.hpp:55`）、331 门控（`BladeFormation.cpp:746-750`）、弹速倍率（`SkillSpecializationBaker.cpp:68-73`） |
| **4c** | 9 绝影绝剑、7 心剑无影、1 流云刺 | `PhantomTrance.cpp`(762)、`MindBlade.cpp`(80)、`FlowingThrust.cpp`(1034) | 形态时长（`SkillSpecializationBaker.cpp:104-110`）、射程 `base_range`（`:92-95`）、流血爆发口径（`FlowingThrust.cpp:434-462`）、跨技能 814 读点（`:406-408`） |

### 7.2 单技能变更模板（每技能一致）

- [ ] 声明 `XSpecState`（POD）+ `SpecStateTable` 实例（优先由生成器产出）。
- [ ] `DoCast`/`DoHit` 首部单点 `ResolveSpecState`，删除内联 `specialized_slots` 循环与 `getPts` lambda（反例 `FlowingThrust.cpp:342-371`）。
- [ ] flag 二分（D6）：节点语义 → `HasNode`；交付参数保留并注释。
- [ ] 转质/激活/非点读节点保留在绑定表循环外（不变量 §5.4-2）。
- [ ] 跨技能读点保留为对目标技能的 `HasNode`（如 `FlowingThrust.cpp:406-408`），**不**并入本技能信封。
- [ ] 每批 `-L ci` + `-L functional` + 技能专项绿；出复审报告。

### 7.3 回退方式

每技能信封化是纯增量重排（新增声明 + 改读取点），**按技能独立回退**；同一批内技能不回退，只回退当前技能。

### 7.4 批次 DoD

D-A1/A2/A4/A9 在每个迁移技能上达标；D-A10 不回归。

### 7.5 Phase 4a 实施记录（技能 6 剑阵诛仙 / 技能 5 万剑归宗）

- [x] descriptor：`assets/data/skill_specstate/skill_06.json`（28 节点 / 23 绑定 = 13 point + 10 flag）、`skill_05.json`（28 节点 / 12 绑定 = 8 point + 4 flag）；标识符自各文件原 `*Nodes` 常量机械提取。
- [x] 生成物：`src/game/systems/skill/behaviors/generated/SwordArraySpecState.gen.hpp`、`InfiniteBladesSpecState.gen.hpp`（`--gen-specstate`）。
- [x] 迁移：两技能各新增唯一 `ResolveState()` 调用点；删除内联 `specialized_slots` 循环与 `getPoints` lambda；`feature_flags` 读取清零（D6），交付参数仍走 `profile`。
- [x] 特化保留：技能 6 原 `getPoints` 的 `exec.active_nodes` 回退以 `nodePoints`/`nodeActive` lambda 保留（守卫测试 `SkillBehaviorGuardTests.cpp:462-487` 无专精组件路径依赖它）；技能 5 无需回退。
- [x] 数据集清单：`tests/integration/GameplaySystems.cpp` 扫描命名空间后缀增至 `NodesGen`，两文件条目改指生成头。
- [x] 生成器配套：`scripts/gen_skill_mechanics_schema.py` 支持 `*NodesGen` 与 `namespace A = B;` 别名解析（否则技能 6 的 26 条机制键丢失）。
- [x] 验证（实测）：`build.bat RelWithDebInfo` EXIT=0 / 0 warning / 0 error；`ctest -L ci`、`-L skill` EXIT=0；非性能全量 1544 用例 / 114555 断言 0 失败；`[Functional]*` 178/2553；`*SpecState*` 21/1815；`*SwordArray*`+`Skill 6*` 10/219；生成器 `--check --check-idempotency --check-determinism` EXIT=0；`gen_skill_mechanics_schema.py --check`、`sync_skill_node_icon_ids.py --check` EXIT=0。
- P3.4 对拍覆盖提升至技能 5/6/8/10/11/12；P3.5 仍延后。回退：按技能独立回退（删生成头 + descriptor + 还原读取点）。

### 7.6 Phase 4b 实施记录（技能 2 裂空斩 / 技能 3 剑气护体 / 技能 4 御剑护体）

- [x] descriptor：`assets/data/skill_specstate/skill_02.json`（28 节点 / 24 绑定 = 12 point + 12 flag）、`skill_03.json`（28 节点 / 27 绑定 = 17 point + 10 flag）、`skill_04.json`（29 节点 / 25 绑定 = 19 point + 6 flag）；标识符自各文件原 `*Nodes` 常量机械提取，3 个未被读取节点（335 巨剑裂空、414 御剑防壁、450 幻影步）由 talent_tree `name_key` 补名。
- [x] 生成物：`src/game/systems/skill/behaviors/generated/RendingWaveSpecState.gen.hpp`、`BladeFormationSpecState.gen.hpp`、`BladeWardSpecState.gen.hpp`（`--gen-specstate`）。
- [x] 迁移：三技能各新增唯一 `ResolveState()` 调用点；删除内联 `specialized_slots` 循环与 `getPoints` lambda；`feature_flags` 读取清零（D6）。转质节点 270/272、370/372、472/474 按 flag 绑定，保留在绑定表内、语义经生成表读取。
- [x] 特化保留：技能 4 原 `getPoints` 的 `exec.active_nodes` 回退与「点数 > 0 取点数」语义以 `nodePoints`/`nodeActive`/`nodeFlag` 三个 lambda 逐字保留（`specPtr` 独立扫描由 `ResolveSpecState` 同槽等价取代，取值集合不变）；技能 3 的 `HasNodeFlag` 由读 `feature_flags` 改为读生成表点亮位、`profile` 为空时仍回退 `exec.active_nodes`（兼容绕过 TryCast 的直接行为调用），DoHit 的 `ReadAttackerPoints` 改由 `ResolveState(reg, attacker)` 承载；技能 2 原 `getPts` 无 `active_nodes` 回退，直接读生成表。B2-15（`BladeFormation.cpp` 经 `GetFloat(3,351,...)` 消费 351 机制键）未触碰。
- [x] 数据集清单：`tests/integration/GameplaySystems.cpp` 三文件条目改指生成头（技能 2/3/4）。
- [x] 对拍扩展：`tests/unit/GeneratedSpecStateTests.cpp` 新增技能 2/3/4 生成物 vs 测试侧独立期望表对拍（含 3 个补名节点常量断言）。
- [x] 验证（实测）：`build.bat RelWithDebInfo` EXIT=0 / 0 warning / 0 error；`ctest -L ci`、`-L skill` EXIT=0；非性能全量 1547 用例 / 114878 断言 0 失败；`[Functional]*` 178/2553；`*SpecState*` 24/2134；`*BladeWard*,*RendingWave*,*BladeFormation*` 31/238；生成器 `--check --check-idempotency --check-determinism` EXIT=0；`gen_skill_mechanics_schema.py --check`（472 entries / 80 unreferenced）、`sync_skill_node_icon_ids.py --check` EXIT=0。
- P3.4 对拍覆盖提升至技能 2/3/4/5/6/8/10/11/12；P3.5 仍延后。回退：按技能独立回退（删生成头 + descriptor + 还原读取点）。

### 7.7 Phase 4c 实施记录（技能 9 绝影绝剑 / 技能 7 心剑 / 技能 1 流云刺）

- [x] descriptor：`assets/data/skill_specstate/skill_01.json`（28 节点 / 16 绑定 = 12 point + 4 flag）、`skill_07.json`（28 节点 / 2 绑定 = 0 point + 2 flag）、`skill_09.json`（29 节点 / 29 绑定 = 0 point + 29 flag）。技能 1 标识符自 `FlowingThrustNodes` 机械提取、零补名；技能 7 沿用 770/772 既有标识符、其余 26 节点按 `name_key` 语义补名；技能 9 全 29 节点按 `name_key` 语义补名（行为无节点读取点，故选 flag 绑定以覆盖节点全集）。
- [x] 生成物：`src/game/systems/skill/behaviors/generated/FlowingThrustSpecState.gen.hpp`、`MindBladeSpecState.gen.hpp`、`PhantomTranceSpecState.gen.hpp`（`--gen-specstate`）。
- [x] 迁移：技能 1 新增唯一 `ResolveState()` 调用点（DoCast/DoHit/幻影护盾更新共用一个静态 helper），删除 DoCast 的 `momentumPoints`/`infernalPoints` 内联 `specialized_slots` 循环与 DoHit 的 `getPts` lambda，`feature_flags` 读取清零（D6）；技能 7 新增文件内 `ResolveState()`，770/772 两处 `HasNode` 读取改由生成表；技能 9 行为仅消费 `delivery.trance`、无节点读取点，故不新增 `ResolveState`，改以生成头节点常量替换手写 id 列表（D-A8 覆盖，逐字等价）。
- [x] 特化保留：**B2-18**（`FlowingThrust.cpp` `ApplyFrostSlowDebuff` 走 `AilmentAdapter::BuildMoveSpeedDebuff` + `AddOrRefresh` + `StatsDirty`）原样保留；**跨技能 814** 仍走 `SkillSystem::HasAllocatedNode(reg, actualAttacker, 8, 814)`，未并入技能 1 信封；`exec.active_nodes` 兜底（御风/留影/影袭/移形/势如破竹/幻影护盾）逐字保留；**技能 9** 的缓存命中短路 + `synthesized` 经 `fallbackSpec` 烘焙语义逐字保留。
- [x] 数据集清单：`tests/integration/GameplaySystems.cpp` 三文件条目改指生成头（技能 1/7/9）。
- [x] 对拍扩展：`tests/unit/GeneratedSpecStateTests.cpp` 新增技能 1/7/9 生成物 vs 测试侧独立期望表对拍（含补名节点常量断言）。
- [x] 验证（实测）：`build.bat RelWithDebInfo` EXIT=0 / 0 warning / 0 error；`ctest -L ci`、`-L skill` EXIT=0；非性能全量 1550 用例 / 115141 断言 0 失败；`[Functional]*` 178/2553；`*SpecState*` 27/2372；`*PhantomTrance*,*MindBlade*,*FlowingThrust*` 54/587；生成器 `--check --check-idempotency --check-determinism` EXIT=0；`gen_skill_mechanics_schema.py --check`（472 entries / 80 unreferenced）、`sync_skill_node_icon_ids.py --check` EXIT=0。
- P3.4 对拍覆盖提升至技能 1/2/3/4/5/6/7/8/9/10/11/12（12 技能全覆盖）；P3.5（手写镜像退役）仍待 Phase 5。回退：按技能独立回退（删生成头 + descriptor + 还原读取点）。

## 8. Phase 5 — 收尾与终态验证

- [x] P5.1 **flag 二分全量核对（D6）**：`rg -n "feature_flags &|flags &" src/game/systems/skill/behaviors/` 仅命中 `BladeBoomerang.cpp:142-143`（`Magnet`/`Giant` 两个交付参数位，`SkillSpecializationBaker.cpp:859-873` 在写 850/854 时同步写入 `sub_count=0`/`giant_armor_scale=0.05f`，属交付参数），已就地注释；技能 1~9 其余迁移点节点语义位读取为 0（D-A2）。
- [x] P5.2 **机制族基元盘点（D4=b）**：见 §8.1 基元清单；本轮沉淀 3 个 ≥3 调用面的公共基元（`ResolveBakedProfile` / `SpecStateTable` / `--gen-specstate` + `NodesGen`），无额外逐字重复 ≥3 的逻辑需抽 `skills::primitives::*`（D-A6）。
- [x] P5.3 **技能 10~12 回归（D7=b）**：生产代码零改动；`-L ci`/`-L functional` 全绿（见 §8.2）。
- [x] P5.4 **行数观测（D-A12）**：见 §8.2，仅记录、不作门禁。
- [x] P5.5 **终态验证**：见 §8.2 全量命令实测（性能采证不在本批，按 `docs/workflows/performance.md` 另行执行）。
- [ ] P5.6 复审报告，结论 `提交`（按 `docs/workflows/review.md`）——待用户执行复审。

### 8.1 Phase 5 基元盘点（D-A6）与已接受差异

**本轮沉淀的公共基元及其使用面**

| 基元 | 位置 | 使用面（≥3 达标） |
|---|---|---|
| `skills::ResolveBakedProfile(registry, owner, skillId, scratch, fallbackSpec=nullptr)` | `src/game/systems/skill/SkillProfileResolve.hpp/.cpp` | 9 个技能行为文件各 1 处（技能 1~9）；并入 `SkillSystem::GetBakedSkillProfile` 缓存命中 + `SkillSpecializationBaker::Bake` 回退 |
| `skills::SpecStateTable<State>` + `skills::ResolveSpecState<State>(...)` | `src/game/systems/skill/SpecStateTable.hpp` | 9 处模板调用（技能 1~9 各恰 1，经 `ResolveState()`）；12 张生成表；被 12 个生成头 + 2 个单测引用 |
| `--gen-specstate` 生成器 + 命名 descriptor + `<Pascal>NodesGen` 命名约定 | `scripts/gen_skill_contracts.py`、`assets/data/skill_specstate/*.json`、`generated/*.gen.hpp`、`CMakeLists.txt` `GenerateSpecState` | 12 个 descriptor → 12 个生成头，覆盖技能 1~12 |
| `skills::ReadPoints` / `HasNode` / `GetMech`（既有，`SkillPointAccess.hpp`） | `src/game/foundation/components/SkillPointAccess.hpp` | 被 `SpecStateTable.hpp` 与全部生成/手写绑定表复用 |

- D-A6 门槛（每个基元 ≥3 调用点）：三者分别 9 / 9 / 12，均达标。`rg "skills::primitives|namespace primitives" src/ tests/` → 0 命中：本轮未新增 `skills::primitives::*`，因未发现超出上述三者之外的「逐字重复 ≥3」技能逻辑（`ResolveBakedProfile`/`SpecStateTable` 已吸收 R1 样板与解析样板）。
- **D-A1 偏差记录**：技能 1~8 各恰 1 处模板 `ResolveSpecState`；技能 9（`PhantomTrance.cpp`）行为仅消费 `delivery.trance`、无节点读取点，故未设 `ResolveState`，改以生成头节点常量替换手写 id 列表（D-A8 覆盖）；技能 10 使用其手写 `ResolveSpecState(registry, owner)`（`SevenStarSlashShared.hpp:383`，`SevenStarSlash.cpp:362` 调用），技能 11/12 使用各自手写 `ResolveHeavenlySwordCastSpec`/`ResolveBloodSeaCastSpec`——均按 D7 仅回归、不重做。

**已接受差异：技能 11/12 clamp 口径**

- 现象：技能 11/12 手写解析器对点数节点取 `std::max(0, ReadPoints(spec, node))`；模板 `ResolveSpecState<State>` 为裸赋值（技能 8/10 与全部生成表亦为裸赋值），二者在「负分配点」输入下取值不同。
- 裁定：**接受该差异，不改技能 11/12 生产代码**（D7=b：技能 10~12 仅回归、不重做）。理由：① `ReadPoints` 语义为原始存储值，正常运行期分配点数非负，clamp 与裸赋值在既有数据下逐字等价；② 改动会触碰技能 11/12 生产路径，违反 D7；③ 生成器仅承载 `int`/`bool` 映射，`clamp` 属行为层策略而非映射层，纳入模板会引入隐式语义。若后续需要统一，应在 Phase 6（如启用）以独立决策处理。

**已接受差异：D-A5 单模板的 10~12 例外**

- 现象：`skills::SpecPointBinding`/`SpecFlagBinding`（`SpecStateTable.hpp`）为唯一模板，技能 1~9 与全部 12 张生成表均使用之；但技能 10/11/12 仍各自保留手写 `SevenStarSlashPointBinding`/`HeavenlySwordPointBinding`/`BloodSeaPointBinding`（及对应 Flag 版，共 3 组，`SevenStarSlashShared.hpp:318/323`、`HeavenlySwordDescent.hpp:110/115`、`BloodSea.hpp:132/137`）。
- 裁定：**接受**（D7=b 仅回归、不重做）。生成表已统一到单模板；10~12 的重复定义属既有代码，重做会违反 D7。D-A5 在「本轮迁移范围（技能 1~9 + 生成表）」内成立。
- **M-1 收口（2026-09-14 复审后）**：技能 8 原同时存在手写 `BladeBoomerangSpecState.hpp`（生产）与生成 `BladeBoomerangSpecState.gen.hpp`（仅测试对拍），构成双源。复审认定 M-1；评估为低风险 drop-in（仅改 include/类型别名/表引用，节点集合与 `{node,member}` 完全一致）并执行：生产改用 `kBladeBoomerangTableGen`、删除手写头、`GeneratedSpecStateTests` 改为测试侧独立期望表对拍、集成守卫清单改指生成头。至此技能 1~9 生产全部使用生成表，双源消除。

**已接受差异：D-A1 的 9/10/11/12 说明**

- 技能 1~8：各恰 1 处模板 `ResolveSpecState` 调用（经 `ResolveState()`），达标。
- 技能 9：行为无节点读取点（仅消费 `delivery.trance`），未设 `ResolveState`；以生成头节点常量替换手写 id 列表，属显式偏差。
- 技能 10/11/12：分别使用手写 `ResolveSpecState(registry, owner)` / `ResolveHeavenlySwordCastSpec` / `ResolveBloodSeaCastSpec`（D7 不动）。

**M-3（D-A4）证据口径**

- D-A4 无脚本化门禁，属**未脚本化验证项，仅有 review 级证据**（行为文件 `GetMech`/`GetFloat`/`GetInt` 读取计数 + 人工分类数字字面量）。已在设计 D-A4 行与 §9.1 表同步标注，避免「已脚本化」的误读。

**复审（2026-09-14）修复记录**

- **H-1**（`gen_skill_contracts.py` 确定性自检空转）：`_render_specstate_header(model) != rendered` 改为 `_verify_specstate_determinism()`——从磁盘重载 descriptor 后独立渲染再与基线比较（跨调用）；并新增 `_self_test_specstate_determinism()` 负向自检，用篡改基线证明校验链路确实会失败（防空转）。运行门禁时自动执行，通过即证明非空转。
- **H-2**（D-A5 账目）：设计 D-A5 行与设计 §11 E1b 行修订为「迁移范围内（技能 1~9 + 全部生成表）达标；技能 10~12 手写绑定结构为 D7 授权例外」；本节 §8.1 同步。未执行技能 10 回填。
- **M-2**（生成器技能覆盖）：`generate_specstate` 增加断言——1~12 技能必须各有 descriptor（缺失/多余/重复 skill_id/重复 pascal_skill 即报错），并已用负向用例验证（省略 `skill_07.json` → `descriptor coverage mismatch; missing=[7]`）。
- **M-1**（技能 8 双表）：见上「M-1 收口」。
- **M-3**（D-A4 证据）：见上「M-3 证据口径」。
- **L-1**（`SpecStateTable.hpp:10` 注释）：更正「973 属技能 9 `OverloadShield`」，并明确「转质节点不得作为 **point** 绑定、可作 **flag** 探测（技能 12 授权）」。
- **L-3**（`SpecStateTableTests.cpp` 函数级可变 `static`）：`MakeSkill10Table()` 的 `static std::vector` 改为 lambda 初始化的 `static const`。

### 8.2 Phase 5 终态验证实测

- `build.bat RelWithDebInfo`：EXIT=0 / 0 warning / 0 error。
- `ctest -C RelWithDebInfo -L ci`：1/1 Passed（12.14s）；`-L skill`：2/2 passed（unit 2.62s + integration 1.10s）；`-L integration`：6/6 passed。注：仓库无 `functional` CTest 标签（`ctest -N -L functional` = 0），以 doctest `[Functional]*` 等价执行。
- 非性能全量 `NoMoreDayTests --test-case-exclude=*Performance*,*GPU-Diagnostic*`：**1550 用例 / 115143 断言 / 0 失败 / 94 skipped**。
- `[Functional]*`：**178 用例 / 2553 断言 / 0 失败**。
- 数据/生成器门禁（均 EXIT=0）：`gen_skill_contracts.py --gen-specstate --check --check-idempotency --check-determinism`（`[OK] skill_contract blocks are up to date.` + `[OK] SpecState artifacts unchanged.`）、`gen_skill_mechanics_schema.py --check`（472 entries / 80 unreferenced）、`gen_skill_spec_modifier_contract.py --check`（`[OK] skill_spec runtime contract is up to date.`）、`sync_skill_node_icon_ids.py --check`（unchanged 76 / missing 0）、`validate_json.py`（assets/data 全部合法）、`check_module_boundaries.py`（`PASS: ledger and observed reverse edges match`）、`gen_asset_registries.py`（运行后 `src/engine/resource/` 无 diff，等价 check 通过）。
- 行数观测（D-A12，`count_code_lines.py`）：全仓 4446 文件 / 1,224,320 有效代码行 / 316,033 含注释行。A-01 触碰面：改动 14 个受版本控制文件（+849 / -727，含 `gen_skill_contracts.py` +441）；新增文件 `SkillProfileResolve.hpp`(28)/`.cpp`(34)/`SpecStateTable.hpp`(63)、单测 3 个（127/272/627 行）、生成头 12 个（1079 行）、descriptor 12 个。**仅观测，非门禁**。

### 8.3 复审修复后终态验证实测（2026-09-14 复审）

- `build.bat RelWithDebInfo`：EXIT=0 / 0 warning / 0 error。
- `ctest -C RelWithDebInfo -L ci`：1/1 passed；`-L skill`：2/2 passed（unit + integration）。
- 非性能全量 `NoMoreDayTests --test-case-exclude=*Performance*,*GPU-Diagnostic*`：**1550 用例 / 115151 断言 / 0 失败 / 94 skipped**。
- `[Functional]*`：**178 用例 / 2553 断言 / 0 失败**。
- 定向 `*BladeBoomerang*,*SpecState*`（技能 8 双表收口后）：**57 用例 / 2817 断言 / 0 失败**。
- 数据/生成器门禁（均 EXIT=0）：`gen_skill_contracts.py --gen-specstate --check --check-idempotency --check-determinism`（含 H-1 新增的跨调用确定性校验与自检）、`gen_skill_mechanics_schema.py --check`（472 entries / 80 unreferenced）、`sync_skill_node_icon_ids.py --check`（unchanged 76 / missing 0）。
- H-1/M-2 负向用例演示：篡改基线 → 确定性校验抛错；省略 `skill_07.json` → `descriptor coverage mismatch; missing=[7]`；内部 `_self_test_specstate_determinism` 通过（证明校验链路非空转）。

## 9. 测试与验收

### 9.1 D-A1~D-A12 映射

| DoD | 可执行验证 | 期望 |
|---|---|---|
| D-A1 信封唯一性 | 逐技能 review + `rg "ResolveSpecState"` 每技能恰 1 调用点 | 1~12 全覆盖 |
| D-A2 flag 归零 | `rg -c "feature_flags &\|flags &" src/game/systems/skill/behaviors/*.cpp` | 0，或仅剩带 `delivery-param` 注释项 |
| D-A3 R1 归零 | `rg -n "SkillSpecializationBaker::Bake" src/game/systems/skill/behaviors/` | 命中 0 |
| D-A4 无硬编码数值 | **未脚本化门禁（review 级证据）**：`rg -n "GetFloat\|GetMech\|GetInt" src/game/systems/skill/behaviors/*.cpp` + 人工分类数字字面量 | 机制数值均来自 `GetMech`/params |
| D-A5 绑定表模板化 | review：**迁移范围（技能 1~9 + 全部生成表）内**无重复 `PointBinding`/`FlagBinding` 定义；技能 10~12 手写定义登记为 **D7 授权例外** | 迁移范围内单模板 |
| D-A6 基元门槛 | 基元清单 + `trace_path` 调用点 | 每个基元 ≥3 调用点 |
| D-A7 生成器 | `python scripts/gen_skill_contracts.py --gen-specstate --check --check-idempotency --check-determinism` | PASS |
| D-A8 生成一致性 | `GeneratedSpecStateTests` 覆盖生成物；`SpecStateMappingTests`/`SpecStateTableTests` 覆盖技能 10 手写解析器 | 生成表 == 运行期 `ReadPoints` |
| D-A9 非点读节点 | 单测覆盖 1021/1022/973 等 | PASS |
| D-A10 不回归 | `ctest -L ci` / `-L skill` / `-L functional` | 全绿 |
| D-A11 构建与数据 | `build.bat` + 数据脚本（见 §9.2） | 0 警告；脚本 PASS |
| D-A12 行数观测 | `python scripts/count_code_lines.py` | 仅记录，不设阈值 |

### 9.2 命令清单

```powershell
build.bat
ctest --test-dir build -C RelWithDebInfo -L ci
ctest --test-dir build -C RelWithDebInfo -L integration
ctest --test-dir build -C RelWithDebInfo -L skill
ctest --test-dir build -C RelWithDebInfo -L functional
python scripts/gen_skill_contracts.py --check --check-idempotency --check-determinism
python scripts/gen_skill_contracts.py --gen-specstate --check --check-idempotency --check-determinism
python scripts/gen_skill_mechanics_schema.py --check
python scripts/gen_skill_spec_modifier_contract.py --check
python scripts/sync_skill_node_icon_ids.py --check
python scripts/gen_asset_registries.py
python scripts/check_module_boundaries.py
python scripts/count_code_lines.py
```

### 9.3 Baker 18 case 与行为不变量覆盖

- **18 个 Baker case**：`SkillSpecializationBaker.cpp:62-113`（交付推导 9 case）+ 节点烘焙段（9 case）。每批迁移后，以技能 1~12 × 施法/命中路径遍历，断言 `BakedSkillProfile.delivery` 与迁移前逐字段一致（回归基线来自迁移前 `-C RelWithDebInfo` 的既有测试）。本轮**不改** Baker（D8=a），故该覆盖为**只读回归**。
- **行为不变量**（设计 §5.4）：读点/点亮/机制数值语义、`TriggerRuleComponent` 去重（`TriggerRuleComponent.hpp:63-75`）、热路径零堆（`SkillDefs.hpp:668-699` 的 `static_assert`）、`BuffKind` 整数比较（`FlowingThrust.cpp:389-401`）、技能 3/4 特定门控（`BladeFormation.cpp:746-750`、`DamageInterceptors.hpp:55`）、技能 10~12 转质叠加（`SevenStarSlashShared.hpp:400-404`）。
- **测试层**：unit（`SpecStateMappingTests.cpp`、`SkillBehaviorGuardTests.cpp`）、functional（`tests/functional/SkillBehaviors.cpp`、`SevenStarSlashNodes.cpp`、`BladeFormationNodes.cpp`）、integration（技能接线/隔离）。

## 10. 完成标准

### 10.1 每批 DoD

1. `build.bat` RelWithDebInfo **0 警告**。
2. `ctest -L ci` 全绿；本批相关 `-L functional`/`-L skill` 绿。
3. 本批 D-A 子集达标（§9.1）。
4. 出复审报告；结论 `提交` 后进入下一 batch。
5. 行数观测已记录（非门禁）。

### 10.2 整体完成判据

- D-A1~D-A12 全部达标。
- `SpecStateTable<State>` 模板 + `--gen-specstate` 生成器 + 技能 1~12 信封化闭环。
- 技能 10~12 零回归（D7）。
- Baker 2×9 switch 未改动（D8）、`skill_mechanics.json` 未改（本轮无平衡调整）。
- 全部批次经复审报告结论 `提交`。

## 11. 风险与未决

| 风险/未决 | 等级 | 缓解 |
|---|---|---|
| **双源漂移**（生成器 × 手写） | 中 | D-A7 `--check`/幂等/确定性 + D-A8 运行期对拍；手写样本仅作 Phase 2 过渡 |
| **漏迁 flag** 致行为悄然变化 | 中 | P5.1 逐处标注 + 每批 `-L functional` 对照 |
| **跨技能读点**（`FlowingThrust.cpp:406-408` 读技能 8 的 814） | 中 | 明确**不并入**本技能信封；保留 `HasNode` 直读；P2/P4 在技能 8 迁移时同步核对 |
| **过早抽机制族基元** | 中高 | D4 ≥3 次逐字重复门槛 + `trace_path` 证据 |
| **D9 未决**：输入源/落点/命名来源 | 高（阻塞 Phase 3） | Phase 0 决策记录为硬前置；退化方案=手写+仅校验 |
| **生成器产出命名标识符**（节点语义名非数据化） | 未决 | P0.4 记录；若无来源则生成器只产 id→成员映射 |
| **技能 10~12 回归** | 低 | D7 仅回归验证 |
| **`FlowingThrust.cpp` 1034 行/含未提交改动** | 中 | 列在 4c 最后；迁移前先确认工作树改动已提交/冻结 |
| 泛型模板增加编译期复杂度 | 低 | 限 POD + `constexpr` 表 |

## 12. 前置与索引

- 工作树含未提交的技能 1/12 后续改动（见设计 §证据索引与 `git status`）；Phase 4c 触碰 `FlowingThrust.cpp` 前需用户确认改动已落定。
- 设计：`docs/designs/2026-09-14-skill-abstraction-track-a01-design.md`
- 先例体例：`docs/plans/2026-09-13-skill-followup-plan.md`
- 上轮抽象化设计/计划：`docs/designs/2026-09-13-skill-abstraction-and-b2-cleanup-design.md`、`docs/plans/2026-09-13-skill-abstraction-and-b2-cleanup-plan.md`
- backlog：`docs/plans/2026-09-12-skill1-9-followup-backlog.md:162`
- 流程：`docs/workflows/planning.md`、`docs/workflows/implementation.md`、`docs/workflows/testing.md`、`docs/workflows/review.md`
