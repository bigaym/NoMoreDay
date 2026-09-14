# A-01 技能抽象化 Track 复审报告

- 审查目标：A-01「技能 1~12 专精实现抽象化/模块化」改动集（Wave E，未提交）
- 审查轮次：第 2 轮（跟进核验）
- 结论：**提交**（第 1 轮 2 项 High、3 项 Medium、2 项 Low 已逐条关闭；剩余 L-2 为低风险可选改进）
- 日期：2026-09-14
- 审查标准：`docs/workflows/review.md`

---

## 1. 输入

| 类别 | 路径 |
|---|---|
| 审查流程/标准 | `docs/workflows/review.md` |
| 设计 | `docs/designs/2026-09-14-skill-abstraction-track-a01-design.md` |
| 实施计划 | `docs/plans/2026-09-14-skill-abstraction-track-a01-plan.md`（§7 各批实施记录、§8 收尾） |
| D9 盘点 / R2 裁定 | `docs/designs/2026-09-14-skill-abstraction-a01-d9-inventory.md` |
| 上游决策 | `docs/plans/2026-09-13-skill-followup-plan.md:188-194`（Wave E）、`docs/designs/2026-09-13-skill-abstraction-and-b2-cleanup-design.md`（方案 B） |

**方法说明**：本轮未编译、未跑测试、未修改 `src/`/`assets/`/`tests/`（仅本报告）。源码检索以 `git show HEAD:<file>`、`git diff`、`rg` 与直接读文件取证。`codebase-memory-mcp` 图谱工具在本次子代理环境中不可用，故调用链/影响面核对退化为 `rg` + 逐文件阅读；此为方法限制，相关结论已尽量以 `文件:行` 证据支撑。计划所报的构建/测试/生成器门禁结论（`plan.md:100,145,182-190,229,240,251,293-300`）未在本轮独立复现，按其自报证据采信并标注为未复现项。

---

## 2. 变更文件边界

`git status --short` 工作区包含 A-01 改动与多组**无关改动**（B2-18 / 技能 12 BranchD / ailment 契约）。区分如下。

### 2.1 A-01 相关（本轮检视）

新增（未跟踪）：
- `src/game/systems/skill/SkillProfileResolve.hpp` / `.cpp`（28 / 41 行）
- `src/game/systems/skill/SpecStateTable.hpp`（71 行）
- `src/game/systems/skill/behaviors/BladeBoomerangSpecState.hpp`（87 行）
- `src/game/systems/skill/behaviors/generated/*.gen.hpp`（12 个）
- `assets/data/skill_specstate/skill_01.json` ~ `skill_12.json`（12 个 descriptor）
- `tests/unit/SkillProfileResolveTests.cpp`、`SpecStateTableTests.cpp`、`GeneratedSpecStateTests.cpp`

修改（跟踪）：
- `CMakeLists.txt`、`src/game/systems/skill/CMakeLists.txt`
- `scripts/gen_skill_contracts.py`（+441）、`scripts/gen_skill_mechanics_schema.py`（+14）、`assets/data/skill_mechanics_schema.json`（重新生成）
- `src/game/systems/skill/behaviors/{FlowingThrust,RendingWave,BladeFormation,BladeWard,InfiniteBlades,SwordArray,MindBlade,PhantomTrance,BladeBoomerang}.cpp`
- `tests/integration/GameplaySystems.cpp`
- 设计/计划/D9 三份文档（新增）

### 2.2 无关改动（本轮不评价）

- B2-18 / ailment 契约：`assets/data/ailment_contracts.json`、`src/game/systems/combat/{AilmentEngine.cpp,.hpp,HazardSystem.cpp}`、`src/game/systems/skill/SummonAISystem.cpp`、`tests/unit/AilmentEngineTests.cpp`、`tests/unit/SkillMechanicsKeySchemaTests.cpp`、`src/game/foundation/data/TagRegistry.hpp`（注释明写 B2-18）。
- 技能 12 BranchD 数据对齐：`assets/data/mastery_skill_trees.json`（节点 1220/1223/1224 的 `desc_key` 改写，见 untracked `docs/designs/2026-09-13-skill12-branchD-gdd-data-alignment-design.md`）。
- 技能 1 冰缓契约：untracked `docs/designs/2026-09-13-skill1-slow-chill-ailment-contract-design.md`；`FlowingThrust.cpp` 的 `ApplyFrostSlowDebuff`（B2-18）与 A-01 迁移混在同一文件，已按 hunk 区分。

> 结论：工作区存在多 Track 混合改动，A-01 与 B2-18/技能 12 的改动边界清晰，未发现 A-01 越界触碰其他权限文件（`TagRegistry.hpp`/`mastery_skill_trees.json` 属他 Track）。

---

## 3. 范围对齐

- 设计 §2.1 范围内（技能 1~9 效果层信封化、R1 样板唯一化、`SpecStateTable<State>` 模板、`--gen-specstate` 生成器）与实现一致：12 descriptor → 12 生成头，技能 1~8 各恰 1 处模板 `ResolveSpecState`（`FlowingThrust.cpp:62`、`RendingWave.cpp:55`、`BladeFormation.cpp:39`、`BladeWard.cpp:39`、`InfiniteBlades.cpp:32`、`SwordArray.cpp:39`、`MindBlade.cpp:20`、`BladeBoomerang.cpp:53`）。
- 设计 §2.2 范围外（Baker 两个 `switch`、技能 10~12 重做、`skill_mechanics.json` 平衡）未被触碰：`SkillSpecializationBaker.cpp` 无 diff；D-A3 命中 0。
- D7=b（技能 10~12 仅回归）遵守：`SevenStarSlashShared.hpp`/`HeavenlySwordDescent.hpp`/`BloodSea.hpp` 的生产解析器未改。
- 偏差（详见发现项）：D-A5 的「单一模板、无重复 PointBinding/FlagBinding」按字面未达成；技能 10 未按设计 §11 E1b「回填模板」执行。

---

## 4. 质量与风险评估

### 4.1 行为等价性（重点核查项 1）——未发现等价性回归

逐字对照 HEAD 与当前实现：

- **FlowingThrust（技能 1）**：原 `flags = profile ? profile->delivery.feature_flags : 0` + `(flags & 位) || active_nodes`（HEAD `:132-136,188`）已改为 `specState.<flag>` + `active_nodes`（`FlowingThrust.cpp:93-99,151`）。Baker case 1 的位与节点一一对应（`SkillSpecializationBaker.cpp:253,256,263,267,272,275`），`HasNode`（点数>0）= 置位，等价。
- **BladeWard（技能 4）**：`nodePoints`/`nodeActive`/`nodeFlag` 三 lambda（`BladeWard.cpp:52-63`）逐字复刻原 `getPoints`（点数>0 取点数，否则 `active_nodes ? 1 : 0`）。`hasStaticField`（`:232`）替原 `staticPts>0`（HEAD `:262`）、`counter_spin`（`:146`）替 `flag&2048`，均与 Baker 节点 472/473 对应，等价。
- **SwordArray（技能 6）**：`nodePoints`/`nodeActive`（`SwordArray.cpp:53-60`）覆盖原 `getPoints` 与 `profile ? flag : getPoints` 两条路径；逐 flag 复核（`has_slow:149`、`has_execute:163`、`has_chain_lightning:202` 等）与 HEAD `:182-254` 语义一致。
- **BladeFormation（技能 3）**：`HasNodeFlag`（`BladeFormation.cpp:48-50`）把原 `profile ? (feature_flags & flag) : active_nodes` 改为 `profile ? 指定态 : active_nodes`。9 处调用（`:83-99,114,120,129,134,156`）的节点与 Baker case 3 的位映射逐一核对（330/351/353/313/314/354/355/312/315/350/352/311），等价。原 `flag&256`（HEAD `:273`）改 `specState.elementalCorePoints > 0`（当前 `:226`），因处于 `pts303<=0` 的 else 分支，两式均不触发，等价（B2 属既有特化，未动）。
- **PhantomTrance（技能 9）合成回退**：`ResolveParams`（`PhantomTrance.cpp:78-99`）保留「缓存命中短路 → 否则按 `active_nodes` 合成 `SpecializedSkill` → 同一 `Bake` 路径」语义；`kPhantomTranceNodes`（`:36-47`）与 HEAD 的 29 个 id 逐值一致（902,913,914,934,935,954,955,972-993）。`ResolveBakedProfile(...,&synthesized)` 的 Bake 分支与 HEAD 直接 `Bake` 等价（`SkillProfileResolve.cpp:21-25`）。
- **跨技能读点**：技能 1 读技能 8 的 814 仍走 `SkillSystem::HasAllocatedNode(reg, actualAttacker, 8, 814)`（`FlowingThrust.cpp:346-348`），**未并入**技能 1 信封，符合设计 §12.2-3 与计划 §7.2。
- **MindBlade（技能 7）**：原 `HasNode(spec,770/772)` 选择元素（HEAD diff）→ `specState.glacialShards/orbitalStrike`（`MindBlade.cpp:20,48-52`），等价。

### 4.2 内存/生命周期（重点核查项 2）——未发现 UB/泄漏

- `ResolveSpecState` 返回值语义，`State{}` 缺省（`SpecStateTable.hpp:45-69`），`static_assert` 限 POD（`:48-50`）。
- `ResolveBakedProfile` 返回 `&scratch`（调用方栈对象）或缓存指针；全部 9 个行为调用点均先声明 `BakedSkillProfile localProfile` 再使用（`SwordArray.cpp:48-49`、`RendingWave.cpp:53-54`、`MindBlade.cpp:41-43`、`InfiniteBlades.cpp:110-111`、`BladeWard.cpp:47-48`、`BladeBoomerang.cpp:70-71`、`FlowingThrust.cpp:77-78`、`BladeFormation.cpp:57-58`、`PhantomTrance.cpp:82-99`），scratch 生存期覆盖使用期。
- `SpecStateTable` 的 `std::span` 指向生成头中的 `inline constexpr std::array`（静态生存期），无悬垂。
- 无裸 `new`/`delete`、无 `dynamic_cast`、无 C 式转换、无 `std::thread`、热路径无字符串键分支（`review.md` 硬否决 6/7/9/10/11 均不触发）。

### 4.3 契约/单源（重点核查项 4）——命名空间别名无 ODR

- 生成头使用独立 `<Pascal>NodesGen` 命名空间；行为文件以 `namespace SwordArrayNodes = SwordArrayNodesGen;`（`SwordArray.cpp:35`，另 `RendingWave.cpp:31`、`BladeFormation.cpp:32`、`BladeWard.cpp:32`、`FlowingThrust.cpp:34`）重导出，旧标识符名不变。`constexpr` 命名空间常量默认内部链接，跨 TU 无 ODR 冲突。
- descriptor 仅承载命名，结构事实仍来自 `talent_tree`/compact/`skill_mechanics`；生成器对 descriptor 节点集与 `talent_tree` 做**闭合校验**（`gen_skill_contracts.py:878-886`），符合 R2 裁定。

### 4.4 生成器健壮性（重点核查项 3）

- 严格项：节点全集 `missing/extra`（`:878-886`）、重复 node/identifier/member（`:873-876,918-923`）、binding 目标必须已命名（`:905-908`）、转质节点必须为 flag 绑定（`:909-917`）、机制键闭合（`:927-934`）、无 descriptor 的残留生成物报错（`:1079-1089`）。
- 不足项：`--check-determinism` 为空转（见发现项 H-1）；无「每个技能必有 descriptor」的全覆盖断言（M-2）。

### 4.5 测试充分性（重点核查项 5）

- `SkillProfileResolveTests.cpp`：命中/回退 Bake（与直接 `Bake` 逐字对比 `:78-88`）/无来源/显式 fallback 优先/按 owner 解析，5 用例非自证。
- `SpecStateTableTests.cpp`：模板 vs 技能 10 手写解析器（`：147-204`，含异技能槽位前置）、技能 8 逐字段 == 运行期 `ReadPoints`（`:243-247`）、槽位选择/缺组件/首槽胜出、技能 11/12 对拍。
- `GeneratedSpecStateTests.cpp`：技能 8/10/11/12 生成物 vs 生产手写表（`:170-209`），技能 1~7/9 生成物 vs 测试侧独立期望表（`:213-668`，含补名节点常量断言）。
- 未发现 `REQUIRE(true)`、被注释/弱化的既有用例。`-L functional` 等行为回归由计划自报全绿（未独立复现）。

**已核验通过的门禁**（本轮只读复核）：D-A3 `rg SkillSpecializationBaker::Bake src/.../behaviors/` 命中 **0**；D-A2 `rg feature_flags &` 仅 `BladeBoomerang.cpp:142-143`（`Magnet`/`Giant`，交付参数，已注释）；行为层无 `allocated_points.(find|contains|count)` 直读；无 `skills::primitives::*`（D-A6 未新增基元）。

---

## 5. 发现项（按严重度排序）

### H-1（High）SpecState 的确定性自检为空转，D-A7「确定性」证据失效

- 证据：`scripts/gen_skill_contracts.py:1094` — `if check_determinism and _render_specstate_header(model) != rendered:`。`rendered` 即由同一 `model` 在 `:1093` 渲染得到，`_render_specstate_header` 是 `model` 的确定性纯函数（`model.point_bindings`/`model.nodes` 为有序 tuple，无 set 迭代、无随机），两次渲染恒等。
- 为何成问题：设计 D-A7（`design.md:261`）与计划 §9.1 把「幂等/确定性通过」列为验收证据，而该「确定性」检查在结构上不可能失败，构成对一项必要验证证据的无效覆盖（对照 `review.md` 判定规则 1「必要验证证据失效」与硬否决 4 的精神）。幂等检查（`:1096-1099`，从磁盘重载后重渲染）是有效的，确定性项不是。
- 修复建议：把确定性检查改为真正跨调用的比较，例如对 `model` 做 JSON/`dataclass` 往返（`deepcopy` + 重新 `_load_specstate_model`）后再渲染比较，或对同一 descriptor 连续两次完整加载—渲染并断言相等（与 `gen_skill_contracts.py` 既有 `_verify_determinism:572-598` 的 `deepcopy` 范式对齐）；并补一条负向用例（人为打乱字段后断言检查失败）。

### H-2（High）D-A5 按字面未达标，却以「迁移范围内成立」宣告通过，设计 DoD 未同步修订

- 证据：设计 D-A5（`design.md:259`）要求「无重复 `PointBinding`/`FlagBinding` 定义」，且设计 §11 E1b（`design.md:396`）明确要求「技能 10 回填为黄金样本」；实际 `SevenStarSlashShared.hpp:318/323`、`HeavenlySwordDescent.hpp:110/115`、`BloodSea.hpp:132/137` 仍各自保留手写绑定结构，技能 10 未回填模板（计划 P2.4 未执行，`plan.md:140`）。计划在 `plan.md:282-285` 以「本轮迁移范围（技能 1~9 + 生成表）内成立」重新解释 D-A5。
- 为何成问题：D7=b（技能 10~12 仅回归）与 D-A5（收敛其手写形状）在设计层彼此冲突，实施选择了 D7 而未回改 D-A5 的验收措辞。DoD 表仍保留「达标」，与实测不符，属验证/账目层面的偏差（`review.md:109-113` 禁止隐瞒相对设计的偏差、判定规则 2「缺失的必备行为/未验证关键路径」）。
- 修复建议（二选一，均须落在文档）：(a) 在设计 D-A5 条目显式写入「技能 10~12 手写绑定结构为 D7 授权的保留例外」，使 DoD 与实测一致；或 (b) 按设计 E1b 执行技能 10 回填（属行为无关的结构替换，可在 D7 约束下单独回归）。推荐 (a)，成本最低且不触碰 D7 代码。

### M-1（Medium）技能 8 存在双 SpecState（生产用手写、生成物仅测试）

- 证据：生产 `BladeBoomerang.cpp:53` 用 `kBladeBoomerangTable`（定义于 `BladeBoomerangSpecState.hpp:71-85`）；生成头 `generated/BladeBoomerangSpecState.gen.hpp` 的 `kBladeBoomerangTableGen` 仅被 `GeneratedSpecStateTests.cpp:170-179` 消费。技能 1~7/9 生产均已切到生成表。
- 为何成问题：与 D-A1「每技能唯一 POD SpecState」的字面要求不符；两处节点映射存在漂移面（现有 `GeneratedSpecStateTests` 的对拍与 `GameplaySystems.cpp` 的 id 校验提供缓解，故不升级为 High）。
- 修复建议：在计划 §8.1 已接受差异清单中补充该条（当前只列了 A5 与 clamp），或后续批次把 `BladeBoomerang.cpp` 切到 `kBladeBoomerangTableGen` 并退役手写头；二者择一即可。

### M-2（Medium）生成器缺少「技能全覆盖」断言，缺失 descriptor 可静默漏覆盖

- 证据：`scripts/gen_skill_contracts.py:1057-1077` 仅遍历 `descriptor_dir.glob("skill_*.json")`，未断言 descriptor 的 `skill_id` 集合等于 `skills.json`+`mastery` 中全部技能；`:1079-1089` 的「无 descriptor 的生成物报错」只在生成物仍存在时触发——若 descriptor 与对应 `.gen.hpp` 同时被删，该技能将无声明地脱离覆盖。
- 修复建议：在 `generate_specstate` 中收集 `model.skill_id` 并与 `entry_by_skill` 键集合比对，缺失即报错；同时校验 descriptor 间 `skill_id`/`pascal_skill` 唯一。

### M-3（Medium）D-A4「无硬编码机制数值」无可复现证据

- 证据：设计 D-A4（`design.md:258`）与计划 §9.1 以「`rg` 数字字面量 + 人工分类」为验证手段，但仓库无对应脚本或输出；本轮无法据现有产物复现该结论。
- 影响：本轮迁移为机械替换（读点改由生成表承载），未发现**新增**硬编码机制值；但 D-A4 的验收仍属未验证项，不应以「已达标」结案。
- 修复建议：补充一个可执行扫描（或在报告中记录逐文件人工分类清单）作为 D-A4 证据；否则在 DoD 映射表将 D-A4 标为「未验证/残余风险」。

### L-1（Low）`SpecStateTable.hpp` 注释与实现/生成器约定不一致

- 证据：`src/game/systems/skill/SpecStateTable.hpp:10` 写「非点读/转质节点（如技能 10 的 1021/1022、**技能 6 的 973**）不得入表」。节点 973 实为技能 9 `OverloadShield`（`PhantomTranceSpecState.gen.hpp`/`skill_09.json`），非技能 6；且「不得入表」与生成器明确授权的「技能 12 转质节点走 flag 绑定入表」（`gen_skill_contracts.py:911-917`）自相矛盾。
- 修复建议：改为「技能 10 的 1021/1022 由 `GetActiveTransmuterNode` 在表外叠加；技能 12 转质节点按 flag 绑定入表；节点 973 属技能 9」，或删除具体举例。

### L-2（Low）D-A8「生成表 == 运行期 `ReadPoints`」仅对技能 8/10 直接断言

- 证据：直接 `ReadPoints`/`HasNode` 断言见 `SpecStateTableTests.cpp:195-203`（技能 10）与 `:243-247`（技能 8）；技能 1~7/9 经独立期望表由同一模板解析比较（`GeneratedSpecStateTests.cpp:113-166`）。后者是有效等价证明，但与 D-A8 的措辞不完全对应。
- 修复建议：对每个生成表补一条「逐绑定成员值 == `ReadPoints`/`HasNode`」的直接断言，或在 DoD 措辞中承认「经独立期望表等价验证」。

### L-3（Low）`SpecStateTableTests.cpp` 的测试辅助使用可变静态量

- 证据：`tests/unit/SpecStateTableTests.cpp:86-87` 在 `MakeSkill10Table()` 内使用函数级 `static std::vector` 并在每次调用清空重填。单线程 doctest 下无碍，但存在重入/并行测试下的隐晦状态耦合。
- 修复建议：改为局部 `static const` 表或在调用方持有 vector，避免可变全局状态。

---

## 6. 最佳实践建议（对应 `修改`）

1. 优先修 H-1：把 SpecState 的确定性检查改成能失败的形式，并补负向用例；这是本轮唯一影响「验证完整性」的实质缺陷。
2. 处理 H-2：以设计小改（D-A5 例外注记）或技能 10 回填二选一，消除 DoD 与实测的偏差；不要保留「D-A5 达标」的字面表述。
3. 将 M-1/M-2/M-3 记入计划 §8.1 已接受差异或补齐实现；`L-1` 顺手修正注释。
4. 生成器可增设 descriptor 全覆盖与 skill_id 唯一断言；D-A8 直接断言按 L-2 补齐。

---

## 7. 已接受差异确认（D7 / clamp / 技能 9 / D-A5）

| 差异 | 判断 | 说明 |
|---|---|---|
| **D7=b**（技能 10~12 仅回归、不重做） | **认可** | 设计已裁定，且本轮确未改 `SevenStarSlashShared.hpp`/`HeavenlySwordDescent.hpp`/`BloodSea.hpp` 与 Baker。 |
| **技能 11/12 clamp 口径**（手写 `std::max(0, ReadPoints)` vs 模板裸赋值） | **认可为接受差异，但残留风险成立** | 正常分配非负时逐字等价；测试只覆盖正值，clamp 路径无覆盖。可在 Phase 6 统一，不建议本轮为它触碰生产路径。 |
| **技能 9 无运行时消费节点集** | **认可（有保留）** | 行为仅消费 `delivery.trance`，无 `ResolveState` 属合理显式偏差（`plan.md:287-291`）；但 29 条 flag 绑定中除替换手写 id 列表的节点常量外，其余绑定在生产中无消费者，属测试用数据。建议在描述中明确「生成表 flag 绑定为覆盖节点全集的验证载荷，非运行期读点」。 |
| **D-A5 的 10~12 例外** | **代码层认可（D7 授权），但「达标」措辞不认可** | 见 H-2：例外本身合理，问题在未回改 DoD 验收措辞。 |

---

## 8. 剩余风险

1. 计划自报的构建/测试/数据门禁结果（`build.bat` 0 警告、`ctest`、`[Functional]*`、生成器 `--check`）本轮未独立复现；若其证据为真则风险低，均标为**未复现**。
2. 行为等价性结论建立在「Baker 置位 ⇔ `HasNode`（点数>0）」之上；对有「点数为 0 仍置位」等边角数据的路径未覆盖（现有数据与测试均为正点数）。
3. D-A4 无脚本化证据，理论与既有实现均未见新增硬编码机制值，但未逐字核验全部行为文件数字字面量。
4. D-A8 对技能 1~7/9 为「经独立期望表等价」而非直接 `ReadPoints` 断言（L-2）。
5. 技能 8 双源残余漂移面（M-1），现有测试对拍缓解。

---

## 9. 下一步动作

1. **修改** H-1（确定性自检）与 H-2（D-A5 DoD/例外落文）。
2. 处理或登记 M-1/M-2/M-3，修正 L-1。
3. 补 H-1 负向用例与 D-A8 的直接断言（L-2）后，重新执行数据门禁与 `-L ci`/`-L skill`/`[Functional]*`，在**同一文件**追加跟进审查轮次小节，复查本节发现项的关闭情况，再判最终结论。

> 说明：本轮未发现 CRITICAL 级内存安全/并发/热路径违规，也未发现技能 1~9 的行为等价性回归；`修改` 结论主要由验证完整性与 DoD 账目问题（H-1/H-2）驱动，均可用小改动闭环。

---

## 10. 第 2 轮复审（跟进核验，2026-09-14）

- 复审范围：第 1 轮发现项 H-1/H-2/M-1/M-2/M-3/L-1/L-3 的修复核验，以及修复是否引入新问题。
- 方法：只读核验（`rg`/`git status`/直接读文件）。仍**未编译、未跑测试、未提交**；计划自报的构建/测试/门禁结果（`plan.md:309-315`）本轮未独立复现，继续标注为未复现项。
- 最终结论：**提交**。

### 10.1 逐条核验

| 发现项 | 声称修复 | 核验证据 | 判定 |
|---|---|---|---|
| **H-1** 确定性自检空转 | 新增 `_reload_and_render_specstate`/`_verify_specstate_determinism`/`_self_test_specstate_determinism` | `scripts/gen_skill_contracts.py:1037-1043`（从 `model.source_path` 重新 `_load_specstate_model` 后独立渲染）、`:1046-1053`（重载渲染与 baseline 比较，不等即 `raise`）、`:1055-1068`（把 baseline 加哨兵后调 `_verify_specstate_determinism`，若未抛 `ValueError` 则 `raise`）；门禁循环 `:1156-1163` 每技能执行 `_verify`、首技能再执行 `_self_test` | **真实成立** |
| **H-2** D-A5 记账 | 设计/计划改为「迁移范围内达标 + 10~12 为 D7 授权例外」并注明技能 10 未回填 | `design.md:259`（明确「在迁移范围内（技能 1~9 + 全部 12 个生成表）…；技能 10~12 保留既有手写绑定结构，为 D7 授权已接受差异」）；`design.md:396`（E1b：D-A5 在迁移范围内达标；技能 10 未回填，黄金样本实为技能 8，按用户 Phase 2 指令收窄）；`plan.md:283-285`、`:311-312` 同步 | **真实成立** |
| **M-1** 技能 8 双表 | 生产切生成表、删手写头、测试改独立期望表 | `BladeBoomerang.cpp:25` include 生成头、`:42` 别名、`:51-53` 用 `BladeBoomerangSpecStateGen`+`kBladeBoomerangTableGen`、`:112,256` 消费；`Test-Path` 手写头 = **False**（已删除）；`GeneratedSpecStateTests.cpp:186` 技能 8 改 `CheckGeneratedMatchesExpected`；`GameplaySystems.cpp:287-288` 清单改指生成头；全仓 `rg 'BladeBoomerangSpecState\.hpp'` 仅命中本报告与计划历史记录，源码/测试/CMake **0 残留**；`rg 'kBladeBoomerangTable(?!Gen)'` **0 命中` | **真实成立** |
| **M-2** 全覆盖断言 | 断言 1~12 每技能必有 descriptor | `gen_skill_contracts.py:1116-1127`（重复 `skill_id` 报错）、`:1128-1132`（重复 `pascal_skill` 报错）、`:1133-1141`（`missing_descriptors`/`unknown_descriptors` 非空即报错，输出 `missing=[..] unknown=[..]`）；负向 `skill_07` → `missing=[7]` 属实施方自报 | **真实成立**（负向自报，未独立复现） |
| **M-3** D-A4 证据口径 | 标注为「未脚本化门禁（review 级证据）」并给可复现 rg | `design.md:258`（D-A4 行改为「未脚本化门禁，仅有 review 级证据」）、`plan.md:294-296`（M-3 证据口径）、`plan.md:336`（DoD 映射表同步）；去除了「已脚本化」的误读 | **真实成立**（转为已声明的残余风险，符合原建议二选一之「标为未验证/残余风险」） |
| **L-1** 注释错误 | 更正 973 归属 + 转质 point/flag 语义 | `SpecStateTable.hpp:10-13`：明确「转质/非点读节点不得作为 **point** 绑定；技能 10 的 1021/1022 由 `GetActiveTransmuterNode` 在表循环外叠加；技能 12 转质节点按 **flag** 绑定；节点 973 属技能 9 `OverloadShield`」，与生成器 `gen_skill_contracts.py:909-917` 约定一致 | **真实成立** |
| **L-3** 可变 static | 去 `SpecStateTableTests.cpp` 可变 static | `SpecStateTableTests.cpp:86-105`：`MakeSkill10Table()` 改为 `static const auto points/flags = []{...}()` 不可变、线程安全初始化并保留静态生存期（span 安全）；`rg 'static std::vector'` **0 命中** | **真实成立** |
| **L-2** D-A8 直接断言 | （未在本次修复清单内） | 直接 `ReadPoints`/`HasNode` 断言仍仅见 `SpecStateTableTests.cpp:195-203`（技能 10）与 `:243-247`（技能 8）；技能 1~7/9 仍为 `GeneratedSpecStateTests.cpp` 独立期望表间接验证 | **未修（保留为 Low）** |

### 10.2 重点判断

- **H-1 负向自检是否真能捕获不确定性**：`_self_test_specstate_determinism`（`:1055-1068`）把 baseline 人为篡改后调用校验函数，并断言其**必须抛错**；只有比较逻辑为活（非 `if False`/恒真）时才能通过。因此自检证明校验器非空转，属有效的“元验证”。正向校验 `:1046-1053` 现在真正重跑 `_load_json → _load_specstate_model → _render_specstate_header`（`_load_specstate_model:767-820` 全程按 descriptor 有序列表构造 `tuple`，无 set/随机来源），可捕获同一进程内的加载/渲染不确定性。**残余（低）**：两次加载在同一 Python 进程内哈希种子相同，理论上无法暴露跨进程 `PYTHONHASHSEED` 导致的集合/字典序差异；但渲染顺序来源为有序 JSON 列表，实际风险可忽略。
- **M-1 删除手写头后的残留/ODR 风险**：全仓（除历史文档）对 `BladeBoomerangSpecState.hpp` 与裸 `kBladeBoomerangTable` 均 **0 引用**，生成头 `BladeBoomerangSpecStateGen`/`kBladeBoomerangTableGen` 由 `constexpr`/内联常量承载，内部链接，无 ODR 冲突；生产、集成守卫、单测三处引用一致。**无残留、无 ODR 风险**。唯一文档层面残留：`plan.md:145` 的 Phase 2 历史验证记录仍提到旧手写头（历史记录，非现行事实，不建议改动）。

### 10.3 新引入问题复核

- 未发现新增 CRITICAL/High/Medium 缺陷。
- 新增的「descriptor 全覆盖」硬门禁（`gen_skill_contracts.py:1133-1141`）会使任何缺少 SpecState descriptor 的技能触发生成失败——这是 D-A1「每技能唯一 SpecState」的预期策略，非缺陷；但今后若要新增无 SpecState 的技能，需同步更新该断言口径（记录为口径约束）。
- 由 A-01 变更引起的 `assets/data/skill_mechanics_schema.json` 重新生成属预期副产物，未见异常。
- 既有无关改动（B2-18 ailment、技能 12 BranchD、`TagRegistry.hpp`）仍与 A-01 分离，未被本次修复污染。

### 10.4 剩余风险（不阻塞提交）

1. 计划自报的 `build.bat`/`ctest`/`[Functional]*`/生成器门禁结果本轮**未独立复现**（受本次复审「不编译、不跑测试」约束）；若其证据为真则风险低。
2. D-A4 明确为「未脚本化、仅 review 级证据」的已声明残余（`plan.md:294-296`）。
3. L-2：技能 1~7/9 的「生成表 == 运行期 `ReadPoints`」为经独立期望表的等价验证，非逐绑定直接断言（Low，可选）。
4. H-1 的确定性校验为同进程内两轮加载比较，不覆盖跨进程哈希种子序（低）。

### 10.5 最终结论

第 1 轮的 2 项 High（H-1/H-2）、3 项 Medium（M-1/M-2/M-3）与 2 项 Low（L-1/L-3）均已**真实修复或转为已声明残余风险**，行为等价性与内存/生命周期结论维持第 1 轮判断（无回归、无 UB/泄漏），未发现新引入缺陷。剩余 L-2 属低风险可选改进，不构成阻塞。

**结论：`提交`**（前提：实施方自报的构建/测试/数据门禁在合并前保持全绿；本报告未独立复现该部分）。

---

## 11. 第 3 轮复审（跟进核验，2026-09-14）

- 复审目标：以当前工作树状态独立核验第 2 轮结论与全部修复证据，并**补上前两轮「未独立复现」的数据/生成器门禁缺口**。
- 方法：只读核验（`rg`/`Test-Path`/读文件）+ **本轮实际执行了数据/生成器门禁脚本**。仍未编译、未跑 `ctest`、未提交。
- 结论：**提交**（维持第 2 轮结论，且证据链补强）。

### 11.1 前轮发现项在当前工作树的核验

| 发现项 | 核验证据（本轮独立取证） | 判定 |
|---|---|---|
| **H-1** 确定性自检空转 | `scripts/gen_skill_contracts.py:1046`（`_verify_specstate_determinism`）、`:1055`（`_self_test_specstate_determinism`）、`:1160`（门禁循环内逐技能调用 `_verify`）均在当前文件真实存在 | **修复在位** |
| **M-1** 技能 8 双表 | `Test-Path src/game/systems/skill/behaviors/BladeBoomerangSpecState.hpp` = **False**（手写头已删）；`generated/` 12 个 `.gen.hpp` 与 `assets/data/skill_specstate/` 12 个 descriptor 均在 | **修复在位** |
| **M-2** 全覆盖断言 | `gen_skill_contracts.py:1119-1125`（duplicate skill_id）、`:1128-1132`（duplicate pascal_skill）、`:1135-1141`（`missing_descriptors`/`unknown_descriptors` 非空即报错）均在 | **修复在位** |
| **L-1** 注释错误 | `src/game/systems/skill/SpecStateTable.hpp:10-13`：已更正为「技能 10 的 1021/1022 表外叠加；技能 12 转质节点按 flag 绑定；节点 973 属技能 9 `OverloadShield`」 | **修复在位** |
| **L-3** 可变 static | `tests/unit/SpecStateTableTests.cpp:87,95` 为 `static const auto = []{...}()`，无 `static std::vector` | **修复在位** |
| **L-2** D-A8 直接断言 | 未修（第 2 轮已声明保留为 Low，可选改进） | **维持** |

### 11.2 本轮独立复现的门禁（补强第 1/2 轮「未复现」缺口）

以下命令由本轮直接执行，全部 EXIT=0：

| 命令 | 结果 |
|---|---|
| `python scripts/gen_skill_contracts.py --gen-specstate --check --check-idempotency --check-determinism` | `[OK] skill_contract blocks are up to date.` + `[OK] SpecState artifacts unchanged.` |
| `python scripts/gen_skill_mechanics_schema.py --check` | `OK: 472 entries, 80 unreferenced` |
| `python scripts/sync_skill_node_icon_ids.py --check` | `Nodes unchanged: 76 / missing 0` |

### 11.3 DoD 只读抽查（本轮 rg 实测）

- **D-A3**：`rg "SkillSpecializationBaker::Bake" src/game/systems/skill/behaviors/` → **0 命中**。
- **D-A2**：`rg "feature_flags &|flags &"` 仅 `BladeBoomerang.cpp:142-143`（Magnet/Giant 交付参数位）。
- 行为层 `allocated_points.(find|contains|count)` 直读 → **0 命中**；`skills::primitives` → **0 命中**（D-A6：未新增基元，与计划 §8.1 一致）。
- `SpecStateTable.hpp` 复核：POD `static_assert`（`:50-52`）、`ResolveSpecState` 首个匹配槽位填表后 `break`、无槽位返回 `State{}`，与设计 §8.1 及第 1 轮结论一致。

### 11.4 剩余风险（不阻塞提交）

1. `build.bat` 0 警告与 `ctest`（`-L ci`/`-L skill`/`[Functional]*`）仍未由复审独立复现（本轮亦未编译）；实施方自报见 `plan.md:319-325`。
2. D-A4「无硬编码机制数值」仍为已声明的 review 级证据项（M-3 转化）。
3. L-2（D-A8 直接断言仅技能 8/10）为可选改进。
4. H-1 确定性校验为同进程内两轮加载比较，不覆盖跨进程哈希种子序（低，第 2 轮已记录）。

### 11.5 最终结论

当前工作树与第 2 轮复审通过时的状态一致：前轮全部 High/Medium 修复证据真实在位，数据/生成器门禁由本轮独立复现通过，无新引入缺陷。**结论：`提交`**（剩余前提：构建/测试门禁由实施方保持全绿；如需完全闭环，可在提交前跑一次 `ctest -L ci` + `[Functional]*`）。
