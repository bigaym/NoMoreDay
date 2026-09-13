# 技能 RD 裁决修订 — 实施计划

- 日期：2026-09-13
- 设计：`docs/designs/2026-09-13-skill-rd-rulings-design.md`（修订首版 `docs/designs/2026-09-13-skill-followup-design.md`）
- 清单对账：`docs/plans/2026-09-12-skill1-9-followup-backlog.md`（执行期同步勾选）
- 范围：RD-01（Keystone 角色分组，全 12 树）/ RD-02（982 封顶）/ RD-03（重施法不重置免死）/ RD-06（455）/ RD-08（153 来源）/ RD-14（确认继承）；不含 A-01、性能类、RD-18
- 基线：`31cc67e6`

## 1. 实施思路与原则

1. **先结构后行为**：Wave A 先落契约字段语义、校验放宽、12 树分组数据与契约重生成；Wave B 再改运行行为（封顶/守卫/一次消耗/来源门控），避免数据返工。
2. **单源与就地**：分组权威源是紧凑契约 `assets/data/skill_contracts_compact.json` 的 `skills[].keystone_exclusion_groups`（`node_id→组号`）；`scripts/gen_skill_contracts.py` 由 compact **生成** `skills.json`/`mastery_skill_trees.json` 内嵌 `skill_contract`（节点 `keystone_exclusion_group` 为产物），反向不成立。因此 A2 改 compact、A3 重生成内嵌契约，**不要直接改内嵌契约或 `talent_tree` 节点字段**（后者根本不被生成器读取）。不新增 `max_keystones` 等冗余字段（裁决：上限由分组自然推导）。设计 §3.1/§4 原有的「数据源在 skills.json 节点字段、生成器产出 compact、脚本在 tools/」表述已同步更正为上述管线。
3. **最小行为面**：RD-14 仅补断言不改代码；RD-06 435/452、RD-02 数据、981 全部不动。
4. **串行约束**：`SkillRegistry.cpp`、`DamagePipeline.cpp`、`PhantomTrance.cpp`、`SkillSystem.cpp`、`AilmentEngine.*`、`skills.json`、`mastery_skill_trees.json`、`skill_contracts_compact.json` 为高冲突文件，同 wave 内串行改动，不并行编译。
5. **验证证据**：每 wave 附 `文件:行` 或测试名；出口 `build.bat`（RelWithDebInfo）+ `ctest -C RelWithDebInfo -L ci` + 契约生成器三连校验。

## 2. 状态表

| RD | 设计裁定 | 现状 | 本计划工作 |
|---|---|---|---|
| RD-01 | 角色分组互斥、上限=组数、全 12 树 | 仅 skill2 全塞 group1；其余树 keystone 无组；skill10 keystone/transmuter 混组 | A1/A2/A3 |
| RD-02 | 982 暴伤总量封顶 +200% | 线性无 clamp，rank4 可达 +400% | B1 |
| RD-03 | 重施法不重置免死 | DoCast 无守卫，会重新武装 | B2 |
| RD-06 | 455 改「下一次攻击」全局 More | 2s 全物理 buff | B3 |
| RD-06b | 455 一次攻击行为覆盖 AoE/多段（设计 §3.4 补裁） | 首个结算实例即消费清除 | D1/D2/D3 |
| RD-08 | 153 仅流云刺来源流血回血，ratio=1.0 | 全来源、0.30 | B4 |
| RD-14 | 接受子投射物继承 ignore_resist | 已继承 | C1（仅断言） |

## 3. Wave A — 契约字段语义 / 校验 / 分组数据

### A1. 校验放宽为「组内≥1」

- **原理**：运行期与生成期两处校验都要求每个组 ≥2 节点，单节点角色组会误报（错误串均为 `has fewer than 2 nodes`）：
  - 运行期 `src/game/foundation/data/SkillRegistry.cpp:485-566`：`:502-504` 计数、`:558-566` 报错。
  - 生成期 `scripts/gen_skill_contracts.py:521-525`。
  放宽为「每组至少 1 个」——因计数完全来自节点本身，等价于删除下限检查，仅保留组号非 0 的既有语义（组号 0 不参与）。
  **由「≥2」改成「≥1」后，`exclusion_group_counts` 即无消费者**：C++ 侧 `:489` 声明、`:502-504` 自增与生成器侧计数映射必须一并删除，否则触发「已赋值未使用」告警（项目要求 0 警告）。
- **伪代码**：
  ```
  # SkillRegistry.cpp：删除 :489 声明、:502-504 自增、:558-566 报错循环
  #   保留 :506-518 角色计数、:521-553 既有 Transmuter 豁免与溢出校验、:568-578 transmuter 角色校验
  # gen_skill_contracts.py：删除 :521-525 的 <2 报错循环及其计数映射（实施期确认该映射仅服务此检查）
  ```
- **任务**：
  - [x] A1.1 删除 `SkillRegistry.cpp:558-566` 下限循环；同步移除 `:489`/`:502-504` 的 `exclusion_group_counts`；确认 `has_transmuter_exclusion`（`:521-536`）不变，构建 0 警告。
  - [x] A1.2 删除 `gen_skill_contracts.py:521-525` 下限循环（及仅服务它的计数），使生成期与运行期口径一致。
  - [x] A1.3 `tests/integration/SkillContractRegistryTests.cpp` 增/改用例：单节点组合法；跨组仍互斥；组号 0 不参与。
- **DoD**：单节点组加载与生成均不报错且有正例断言；无「已赋值未使用」告警；原「≥2」负例相应调整并留注释说明依 RD-01。

### A2. 12 树 Keystone 角色分组数据补齐

- **原理**：权威字段落在紧凑契约 `assets/data/skill_contracts_compact.json` 的 `skills[].keystone_exclusion_groups`（`{"<node_id>": <组号>}`）；生成器按 `_build_contract_for_skill`（`gen_skill_contracts.py:463`：`keystone_exclusion_groups.get(str(node_id), 0)`）写出内嵌契约每个节点的 `keystone_exclusion_group`。**不要直接改 `skills.json`/`mastery_skill_trees.json` 的内嵌 `skill_contract`**（会被 `--check` 判为不同步或被生成覆盖），更不要改 `talent_tree` 节点字段（生成器不读取）。
- **组号规则**：每棵树内自 1 起连续；先给 Keystone 角色组 1..N（下表组1/组2/组3），Transmuter 互斥对另取后续组号 N+1；skill10 例外（keystone1007 与 transmuter1021/1022 同组 1），skill11 无 transmuter。
- **分组映射（权威，照抄落 compact）**：

  | 技能 | Keystone 组1 | Keystone 组2 | Keystone 组3 | Transmuter 组（树内后续号） |
  |---|---|---|---|---|
  | skill1 | 113 | 154 | — | 170,172 → 组3 |
  | skill2 | 213,214,234 | 253 | — | 270,272 → 组3 |
  | skill3 | 330,351 | 313 | — | 370,372 → 组3 |
  | skill4 | 412,433,434 | 470 | — | 472,474 → 组3 |
  | skill5 | 533,550 | 534 | — | 570,572 → 组3 |
  | skill6 | 633,634 | 613 | 653 | 670,672 → 组4 |
  | skill7 | 711,732 | — | — | 770,772 → 组2 |
  | skill8 | 810,854 | 815 | — | 870,872 → 组3 |
  | skill9 | 977,980 | 981 | 954 | 972,989 → 组4 |
  | skill10 | 1007,1021,1022 | 1013 | 1025 | （并入组1，无独立组） |
  | skill11 | 1107,1113 | 1120 | — | 无 transmuter |
  | skill12 | 1207,1213,1220 | — | — | 1221,1222 → 组2 |

- **任务**：
  - [x] A2.1 在 `skill_contracts_compact.json` 对应 skill 的 `keystone_exclusion_groups` 写入上表（覆盖旧值）：skill2 的 253 由组1→组2；skill3/4/5/6/7/8 的 transmuter 组号由现组1 抬到 keystone 组之后（组3/组3/组3/组4/组2/组3）；skill1/skill11 新增 keystone 组；skill9 新增 977/980→组1、981→组2、954→组3 且 972/989 由组2→组4；skill10 修正 1007/1021/1022 由现组3→组1、保留 1013→组2、1025→组3；skill12 的 1221/1222 由现组4→组2。
  - [x] A2.2 复核同文件 `keystone_node_ids`/`transmuter_node_ids` 与上表角色一致（如 skill8 的 `transmuter_node_ids` 需与 870/872 对齐）。
  - [x] A2.3 skill8 前置：按设计 §3.1 定稿解除 815 风眼对 810 滞空切割的前置依赖，确认任意 Keystone 选择下 815 均可达（数据侧前置字段，非本组字段）。
  - [x] A2.4 复核无节点误标（Transmuter 与 Keystone 不混组，skill10 除外）。
- **DoD**：compact 中 12 树每个 keystone 均有非 0 组；组号树内自 1 起连续；无跨组重复 id；transmuter 组与 keystone 组不重叠（skill10 除外）。

### A3. 内嵌契约重生成与图标同步

- **原理**：`skill_contracts_compact.json` 是源；运行 `scripts/gen_skill_contracts.py` 将 `keystone_exclusion_groups` 物化进 `skills.json`/`mastery_skill_trees.json` 内嵌 `skill_contract` 的节点 `keystone_exclusion_group`。
- **任务**：
  - [x] A3.1 运行生成器，重生成 `assets/data/skills.json` 与 `assets/data/mastery_skill_trees.json` 的内嵌契约（**不是**重生成 compact）。
  - [x] A3.2 出口校验：`python scripts/gen_skill_contracts.py --check --check-idempotency --check-determinism` + `python scripts/sync_skill_node_icon_ids.py --check`。
  - [x] A3.3 `tests/integration/SkillContractRegistryTests.cpp` 增「每树分组集合」精确断言（按 A2 映射）。
- **DoD**：内嵌契约 `keystone_exclusion_group` 与 A2 映射一致；三连校验 PASS；无 keystone 组 <2 报错。

### A4. UI 组号展示核对

- **原理**：`src/game/application/ui/UISkillTalentTree.cpp:141-143` 展示「互斥 G%u」按组号；组号树内重置后文案仍正确，无需改逻辑，仅核对不出现跨树误导。
- **任务**：
  - [x] A4.1 核对 `src/game/application/ui/GameUiSnapshotBuilder.cpp:951` / `GameUiSnapshot.hpp:423/445` 传递的互斥集合与分组一致；如组号对玩家可见且易误解，评估是否改展示为角色名（非阻塞，登记）。
- **DoD**：UI 快照的互斥集合与契约一致，有断言或手测记录。

## 4. Wave B — 运行行为

### B1. RD-02 982 暴伤封顶

- **原理**：`PhantomTrance.cpp:340-341` 现 `value = missing_pct * last_stand_crit_pct` 无上限。改为对最终加成总量 clamp 到 200。
- **伪代码**：
  ```
  missing_pct = (1 - hp.cur/hp.max) * 100
  value = missing_pct * pt.params.last_stand_crit_pct
  value = min(value, 200.0f)        # 新增：总量封顶 +200%
  # :351-353 CritDamage PercentAdd 应用不变
  ```
- **任务**：
  - [x] B1.1 `:341` 后加 `min(..., 200.0f)`；`last_stand_crit_pct` 数据与烘焙不变。
  - [x] B1.2 在 `tests/functional/PhantomTranceNodes.cpp`（现含 982 用例 `:578`）增断言：rank4 满损血=+200%；rank1 不受影响；981 锁血下仍封顶。
- **DoD**：满损血 + rank4 加成恰为 200，不再是 400。

### B2. RD-03 形态中重施法不重置免死

- **原理**：`PhantomTrance.cpp:716-750 DoCast` 每次重建组件并重置 `lethal_triggered`、重刷 `ApplyDeathSeal`。改为仅在「未处于形态」时武装免死；已在形态中重施法仅刷新形态时长。
- **伪代码**：
  ```
  DoCast(owner):
    in_form = registry.try_get<PhantomTranceComponent>(owner) && pt.remaining > 0
    if in_form:
        refresh form duration          # 允许刷新时长
        # 不重置 lethal_triggered，不重新 ApplyDeathSeal
    else:
        emplace PhantomTranceComponent
        pt.lethal_triggered = false
        ApplyDeathSeal(owner)          # 仅进入形态授予
  ```
- **任务**：
  - [x] B2.1 重写 `DoCast` 分支（`:716-750`）；避免 `emplace_or_replace` 覆盖既有 `lethal_triggered`。
  - [x] B2.2 在 `tests/functional/PhantomTranceNodes.cpp` 增断言：形态中重施法后 `lethal_triggered` 保持、免死窗口不被刷新；结束再进入可重新武装。
- **DoD**：重施法无法刷免死；`CombatSystem.cpp:546-559` 触发与 977 语义不变。

### B3. RD-06 455「下一次攻击」全局 More

- **原理**：`SkillSystem.cpp:1131-1134`/`:1164-1182` 现挂 2s `PhysicalDamage PercentMult 20` 定时 buff。改为挂一次性 pending 标记，在下次攻击伤害结算时消费。
- **伪代码**：
  ```
  on_dodge_success:
    grant_pending(owner, {more_global=0.20, charges=1})
    grant_sword_intent(owner, 1)        # 保留 :1181
  on_attack_damage_calc(owner):          # 一处消费点
    if pending.charges > 0:
        accumulate more_global 0.20
        pending.charges = 0            # 命中与否都消耗
  ```
  - pending 载体：优先复用现有 on-next-hit/一次性 modifier 通道；无合适通道时在技能4 相关组件加一字段（实施期定，须评估存档）。
- **任务**：
  - [x] B3.1 移除 `:1164-1182` 的定时 `BuffEffect` 写法，改一次性 pending。
  - [x] B3.2 在攻击伤害结算接入消费（命中与否都消耗，依设计 §3.4 定稿）。
  - [x] B3.3 在 `tests/functional/Skill4FollowupTests.cpp` 断言：闪避后下一次攻击 +20% More 且消耗；之后攻击无加成；未命中挥击也消耗；获 1 层剑意保留。
- **DoD**：455 行为=「下一次攻击 More+20% + 1 剑意」；435/452 不改（已实装）。

### B4. RD-08 153 限定流云刺来源

- **原理**：`AilmentEngine.cpp:838-858` 现对任意来源 Bleed tick 回血。需为流血记录来源技能，在回血分支加 `effect.source_skill_id == 1`（skill1 流云刺）门控；`lifesteal_ratio` 0.30→1.0。
- **既有设施（已核实）**：`BuffEffect`（`src/game/foundation/components/Buff.hpp:113`）已有 `int source_skill_id = 0` 且已入序列化（`:140`、`:167-168`，旧存档缺字段默认 0）；`BuildAilmentEffect`（`AilmentEngine.cpp:233-258`）目前只写 `effect.source`，未写 `source_skill_id`。故只需「请求增字段 → 集中传播 → tick 门控」，无需新增存储字段。
- **伪代码**：
  ```
  # AilmentApplyRequest 末尾追加 int source_skill_id = 0（保持既有聚合初始化兼容）
  # BuildAilmentEffect：effect.source_skill_id = request.source_skill_id
  on_bleed_tick(effect):
    if HasFlowingThrustBloodDrinker(effect.source) and effect.kind==Bleed
       and effect.source_skill_id == 1 and tick_damage > 0:
         heal(effect.source, tick_damage * lifesteal_ratio)   # ratio=1.0
    # 非流云刺来源(source_skill_id != 1)不回血
  ```
- **任务**：
  - [x] B4.1 `AilmentApplyRequest`（`src/game/systems/combat/AilmentEngine.hpp:48-54`）**末尾追加** `int source_skill_id = 0`；不可插在中间，既有 `{AilmentType::X, owner, mag, dur, stacks}` 形式的聚合初始化（如 `BladeWard.cpp:368`）依赖前 5 位。
  - [x] B4.2 `BuildAilmentEffect`（`AilmentEngine.cpp:233-258`）集中传播 `effect.source_skill_id = request.source_skill_id`。
  - [x] B4.3 `src/game/systems/skill/behaviors/FlowingThrust.cpp` 全部 4 处施加点（`:504`/`:555`/`:764`/`:903`）传 `source_skill_id=1`；其余技能施加点保持默认 0，无需改动（`BladeBoomerang.cpp:328`、`src/game/systems/skill/BeamChannelDeliverySystem.cpp:98` 等）。
  - [x] B4.4 `AilmentEngine.cpp:838` 门控增加 `effect.source_skill_id == 1`；`HasFlowingThrustBloodDrinker` 保留。
  - [x] B4.5 `assets/data/skill_mechanics.json:15` `lifesteal_ratio` 0.30→1.0。
  - [x] B4.6 断言：流云刺流血回血且 ratio=1.0；血海/回旋/引导等来源零回血；同步更新既有 `tests/integration/Skill1FollowupTests.cpp:62`（用例名/期望值含 "30%"，需随 ratio 调整为 100%）。
- **DoD**：153 仅对流云刺流血回血，ratio=1.0；其余来源零回血；`source_skill_id` 默认 0 且存档兼容。

## 5. Wave C — 文档 / 口径 / 确认项

### C1. RD-14 继承断言

- **任务**：
  - [x] C1.1 在投射物套件（`tests/unit/ProjectileTests.cpp`）增断言：`SpawnSplitProjectiles`（`src/game/systems/skill/ProjectileSystem.cpp:896`，重置点 `:921-923`）与 `SpawnExplosionProjectiles`（`:963`，`:983-994`）克隆后子投射物 `ignore_resist` 继承父值。
  - [x] C1.2 首版设计 §3 D10「不继承」标记作废；本设计为权威。
- **DoD**：断言覆盖继承路径；无代码改动。

### C2. GDD 与文本口径

- **任务**：
  - [x] C2.1 `设计文档/职业设计草案_剑修.md` L156：153 改为「仅流云刺造成的流血回血 100%」。
  - [x] C2.2 修订 GDD 3.5 skill12 节点 id 漂移：1220=虚蚀瘴幕(keystone)、1221=血潮奔流、1222=血环噬身；核对全文 id 引用。
  - [x] C2.3 如分组结论影响 §5.1/§5.4.4 Keystone 表述（如 skill12 三选一、skill11 二选一），同步措辞。
  - [x] C2.4 backlog §1.1 与设计 §6.1 双向引用一致。
- **DoD**：GDD 与数据/契约一致；无残留旧 id/旧比例。

## 6. 测试方法

- **层级**：以 unit（契约加载/校验、单机制断言）为主，辅以 integration（技能行为跨系统：doctrine→bake→结算）。
- **受影响套件**：`tests/integration/SkillContractRegistryTests.cpp`（A1/A3）、`tests/functional/PhantomTranceNodes.cpp`（RD-02/RD-03，含 982/981 现有用例）、`tests/functional/Skill4FollowupTests.cpp`（RD-06）、`tests/integration/Skill1FollowupTests.cpp`（RD-08，注意 `:62` 既有断言随 ratio 更新）、`tests/unit/AilmentEngineTests.cpp`（RD-08）、`tests/unit/ProjectileTests.cpp`（RD-14）、UI 快照断言（A4）。实施期以 `rg`/`ctest -N` 复核名后落断言。
- **命令**：
  - 构建：`build.bat`（RelWithDebInfo，0 警告）。
  - 测试：`ctest -C RelWithDebInfo -L ci`（全绿）；按套件可用 `ctest -C RelWithDebInfo -R <suite>`。
  - 数据：`python scripts/gen_skill_contracts.py --check --check-idempotency --check-determinism`；`python scripts/sync_skill_node_icon_ids.py --check`。
- **回归点**：A1 校验放宽不得放过「组号非法/重复」；A2 分组不得改变 keystone 角色集合；B1/B2 不得改动 981/977；B3 不得影响 435/452；B4 不得影响其它 Ailment 治疗路径。

## 7. 验证任务完成（DoD / 退出标准）

- [x] Wave A：12 树分组数据齐全且与映射一致；契约重生成三连校验 PASS；单节点组加载通过。
- [x] Wave B：982=+200% 封顶；重施法不重置免死；455=一次消耗；153 仅流云刺、ratio=1.0，均有断言。
- [x] Wave C：GDD/设计口径一致；RD-14 断言存在。
- [x] 全量：`build.bat` 0 警告 → `ctest -C RelWithDebInfo -L ci` 全绿 → 数据脚本 PASS。
- [x] backlog 对应项勾选并附 `文件:行`/测试名证据。

### Wave D 补裁（2026-09-13，455 一次攻击范围）

- [x] 455 同一次攻击行为（AoE 多目标/同施法多段）全部实例获得 +20%，不同攻击行为不再加成，有断言；既有 455 用例不回归。


## 8. 风险与依赖

- **skill8**：解除 815 数据前置后，须确认 815 机制不依赖 810 运行时形态状态（设计 §6.2）。
- **RD-08 字段**：`BuffEffect.source_skill_id` 已存在且已序列化（默认 0），复用即可，无存档迁移；未知来源(0)不回血。
- **RD-06 载体**：若新建组件字段，须评估存档与序列化；优先复用一次性 modifier 通道。
- **高冲突文件**：同 wave 串行，禁止并行编译。
- **不阻塞的未验证项**：B1-21/22/23、772 数值、§11.9 运行时采证仍受限，本计划不覆盖。

## 9. 执行记录（2026-09-13）

- 5 个并行子代理按文件所有权实施（Wave A / B1+B2 / B3 / B4 / C），互不冲突；编译与测试由主代理串行执行。
- **构建**：`cmake --build build --config RelWithDebInfo` → EXIT 0。仅暴露 2 条**既有**警告（`PhantomTrance.cpp:257`、`SwordArrayNodes.cpp:40`，均不在本次改动行段）；本次改动行段 0 警告。
- **测试**：`ctest -C RelWithDebInfo -L ci` → 100% passed（0 failed）。
- **数据**：`validate_json.py` PASS；`gen_skill_mechanics_schema.py --check` PASS；`gen_skill_contracts.py --check --check-idempotency --check-determinism` PASS；`sync_skill_node_icon_ids.py --check` PASS。
- **首轮 2 失败已修**：① `tests/functional/Skill4FollowupTests.cpp:427` 455 剑意断言（基线攻击经 `OnDealDamage`（`SkillSystem.cpp:690`）先得 1 层，改为增量断言）；② `assets/data/skill_mechanics_schema.json` 需随 B3 新增的 `GetMech(4,455,"duration"/"more_damage_pct")` 重生成。
- **实施期决定 / 偏差**：
  - B2：`ApplyVoidBody`/`ApplyWard`/`RegisterEchoRule` 一并移入「仅进入形态」分支，严格实现「重施法只刷新时长」，已过回归。
  - B3：消费点落在 `DamagePipeline::Calculate`（`~:807`，闪避早退之前），命中/格挡/挥空均消耗；保留 `blade_ward_dodge_power` 同名同类型纯标记以兼容 `SkillSystemTests.cpp:1355-1363` 既有断言。
  - B4：复用既有 `BuffEffect.source_skill_id`（`Buff.hpp:113`，已序列化、默认 0），无存档迁移。
  - Wave A：skill8 815 前置在权威源 `assets/data/skill_8_tree.json` 解除，经 merge 脚本同步至 `skills.json`；紧凑契约 `keystone_exclusion_groups` 为权威源，生成器物化内嵌契约。
- **未决 / 跟进（不阻塞本批）**：
  1. **B4 合并路径归属**：`AilmentEngine.cpp:657-667`/`:695-714` 合并槽只更新 `source`，不更新 `source_skill_id`，跨源叠加时归属可能陈旧；建议后续裁决「按来源分槽或合并时更新归属」。
  2. **skill8 815 desc**：`skills.json` talent_tree 内 815 仍写「需求：滞空切割」，权威树源无 desc 字段，待补。
  3. **GDD 前置漂移**：血潮奔流/血环噬身需求写「污秽侵蚀 2/4」，数据实为 node1220 1 点。
  4. 既有 2 条警告可另行清理。

## 10. 补裁实施 — Wave D（455 一次攻击范围，2026-09-13）

- 触发：复审将「455 对 AoE 仅加成首个结算目标」登记为 Medium 残余；设计 §3.4 已补裁「一次攻击行为=一次施法或一次普攻挥击，其全部伤害实例共享 +20%」。

### D1. 攻击行为标识与消费状态

- **原理**：现消费点 `DamagePipeline.cpp:817-826` 只要标记存在即 `Remove`，故首个结算目标即清除。改为按攻击行为标识聚合。
- **数据结构（运行期瞬时，不序列化、不改存档）**：
  - `src/game/contracts/DamagePipelineTypes.hpp` 的 `DamageRequest` 末尾新增 `uint64_t attack_key = 0;`（无施法 id 的普攻由发起侧填充）。
  - `src/game/foundation/components/Buff.hpp` 的 `BuffEffect` 新增 `bool offensive_guard_consumed = false; uint64_t offensive_guard_attack_key = 0;`，仿 `snapshot_source` 注明运行期字段、不进 `to_json/from_json`。
- **伪代码**（替换 `DamagePipeline.cpp:812-827`）：
  ```
  attack_key = (source_cast_id != 0) ? source_cast_id : request.attack_key
  if qualifying_attack:                      # 门控串保持：非模拟/非 DoT/非荆棘/非环境/非 AilmentTick
    if marker:
      if not marker.consumed:
        marker.consumed = true
        marker.offensive_guard_attack_key = attack_key
        mult = 1 + more
        if attack_key == 0: Remove(marker)    # 无标识：退回逐实例语义
      elif attack_key != 0 and marker.offensive_guard_attack_key == attack_key:
        mult = 1 + more                       # 同一攻击行为后续实例
      else:
        Remove(marker)                        # 新攻击行为：清理且本次不加成
  ```

### D2. 普攻挥击标识

- **原理**：普攻在 `CombatSystem.cpp:150-207` 的 `grid.query` 内逐目标构造 `DamageRequest`，`source_entity` 为空 → `cast_id=0`，需在同一挥击的所有目标间共享一个标识。
- **伪代码**：在 `if (input.attack && cd<=0)` 分支内、`grid.query` 之前取一次 `const uint64_t swing_key = NextBasicAttackKey();`（函数级 `static std::atomic<uint64_t>` 计数器，初值 1，保证非 0），并在 lambda 内 `damageReq.attack_key = swing_key;`。

### D3. 测试

- `tests/functional/Skill4FollowupTests.cpp` 新增用例：闪避后，对两个目标各发一次 `DamageRequest` 且**共享同一非 0 `attack_key`** → 两次均 `baseline*1.2`，且首个实例后标记仍存在（已消费）；随后一次不同 `attack_key` 的攻击 → 无加成且标记清除。
- 复核既有 B3 三条用例（`attack_key` 默认 0）行为不变：首个攻击加成后即清除、后续无加成、未命中挥击消耗、非攻击伤害不消耗。
- 实施期以 `rg` 核实代表性 AoE 技能（如 skill2 裂空斩 / skill5 万剑归宗）经 `source_entity` 携带 `cast_id`，走 `source_cast_id` 聚合路径。

### D4. DoD / 验证

- [x] D1/D2 落地，构建 0 新增警告。
- [x] 新增 AoE 共享攻击行为用例通过；既有 455 用例不回归（5 passed / 0 failed / 48 assertions）。
- [x] `ctest -C RelWithDebInfo -L ci` 全绿；`gen_skill_contracts.py --check --check-idempotency --check-determinism` 与 `sync_skill_node_icon_ids.py --check` PASS（本次不改数据，作回归）。
- [x] 复审补充：`AddOrRefresh` 刷新复位瞬态字段（`Buff.hpp:212-215`）+ 回归用例 `455 Refresh Resets Consumed State`。
- [x] 残余登记：`source_entity` 为空的技能伤害仍退回逐实例语义（无 `cast_id` 可聚合，如 skill5 场地/owner-only AoE）。
