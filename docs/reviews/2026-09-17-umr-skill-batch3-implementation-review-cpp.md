# UMR-SKILL-BATCH-3 C++ 实现面审查报告

- 批次：UMR-SKILL-BATCH-3（技能 7 心剑·无影 / 8 御剑·回旋 / 9 绝影绝剑）
- 审查范围：C++ 实现面（Baker、SkillDefs、单测、功能测试）与三项自报风险复核
- 审查类型：对抗性代码审查（只读，未修改任何被审查文件）
- 结论：**提交**
- 阻塞项：无

## 1. 审查依据与范围

### 1.1 文档依据

| 文件 | 用途 |
| --- | --- |
| `docs/designs/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-design.md` | 设计 §3.2 步骤 1/2/4、§4.3 键退役、§4.4 保留键、§5.3 单测职责、§5.4 功能测试隔离、§6 风险表、§7 DoD |
| `docs/plans/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-plan.md` | Task 6/7/8 的拆分与验收口径 |
| `docs/reviews/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-review.md` | 设计阶段审查（F-01..F-05 已闭环进 v1.1） |
| `assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json` | 新增 11 条权威数值 |
| `assets/data/skill_mechanics.json` | 退役 8 迁移键 + 3 死键 |
| `scripts/validate_skill_spec_modifiers.py` | 迁移等价性校验（KEPT/DEAD/MIGRATION_EQUIVALENCE） |

### 1.2 代码审查范围

- `src/game/systems/skill/SkillSpecializationBaker.cpp`
- `src/game/foundation/components/SkillDefs.hpp`
- `tests/unit/SkillBatch3DeliveryOpTests.cpp`（新增）
- `tests/unit/SkillSpecializationBakerTests.cpp`（新增 Batch3 用例）
- `tests/functional/{MindBladeNodes,BladeBoomerangNodes,PhantomTranceNodes}.cpp`
- `src/game/application/states/GameplayState.cpp`（仅自报风险 c 的 TODO 复核）

### 1.3 方法与限制

- 手段：`rg` 精确定位 + `Read` 读源 + `git diff --unified=0 HEAD` / `git show HEAD:<file>` 做迁移前后逐行比对 + 手工公式复算。
- 未执行编译与测试运行（子代理并行约束）。因此「可编译性」「测试通过」两类结论为静态推断，须由主代理以 `build.bat`（RelWithDebInfo）与测试套件实测确认。
- 已静态核对新增单测的类型/接口可编译性：`ModifierRecord{filter, ops}`、`ModifierOp{opcode,param_u32,param_f32}`、`NodePointEntry{node_id,points}`、`ModifierEvalContext.node_points`、`Evaluate(std::span<const ModifierRecord>, const ModifierEvalContext&)` 全部存在且类型匹配（`src/game/systems/modifier/ModifierContext.hpp:100-157`、`ModifierEvaluator.hpp:92`）；`tests/CMakeLists.txt:4-5` 用 `GLOB_RECURSE ... CONFIGURE_DEPENDS` 收集 `tests/**/*.cpp`，新增文件自动纳入构建，无需改清单。

### 1.4 git 基线澄清（影响「等价性」判定的前提）

HEAD（`5320b277`）**已包含本批次的一部分实现**，`git diff HEAD` 只反映最后一段增量。经 `git show HEAD:` 对照确认，以下内容在 HEAD 即存在、非本批次新增：

- case 7 步骤 1 的 `del.range = GetFloat(7,0,"base_range",350.0f)`（HEAD 行 99）
- case 8 步骤 1 的 `del.duration = 0.0f`（HEAD 行 103）
- case 9 步骤 1 的 `del.duration = GetFloat(9,0,"form_duration",3.0f)` 与 `del.trance.duration_sec = del.duration`（HEAD 行 107-108）
- 技能 8 巨剑（854）半径保底迁至步骤 4 的注释（HEAD 行 567-570）

本批次在本工作区**真正新增**的 Baker 改动为：case 7 步骤 1 的 `area_radius = base_radius(60)`；case 7/8/9 节点循环中 700/701/702/703、800/801/810、975/986 手写数值分支的删除（保留 `feature_flags`）；步骤 4 的 8/9 终局块。下文一律以**工作区当前状态**为审查对象。

## 2. 逐项结论

### 2.1 步骤 1 case 7 `area_radius` 基准与幂等性 —— 通过

`src/game/systems/skill/SkillSpecializationBaker.cpp:106-115`：case 7 现于步骤 1 显式写入

- `:112` `del.range = ...GetFloat(7, 0, "base_range", 350.0f)`
- `:114` `out_profile.area_radius = ...GetFloat(7, 0, "base_radius", 60.0f)`

`area_radius` 的全部写入点（`rg area_radius src/`）为 `:51`（通用）、`:52-53`（钳制）、`:76`（case 1）、`:101`（case 6）、`:114`（case 7 新增）、`:206`（步骤 3 乘算）、`:243`（case 3）、`:254`（技能 5 保底）、`:285-286`（装备）。对技能 7 的有效链为 `:51 → :114 → :206`，路径唯一且确定性，不存在第二源写入或读后写竞态，**幂等成立**。`:52-53` 的 `<=0 → 1.0f` 钳制在 `:114` 之前，不会覆盖 60.0f。

数值等价性（关键）：旧实现在 `HEAD` 的 case 7 节点循环中对 702 采用**赋值**语义 `out_profile.area_radius = 60.0f * (1.0f + 0.10f*points)`；新实现为步骤 1 赋值 60.0f、步骤 3 在 `:206` 做 `*= GetSkillAreaMult(7)`（求值层 `ModifierEvaluator.cpp:300-301` 产出 `1.0f + 0.10f*pts`），结果同为 `60*(1+0.1*pts)`，**各点数取值完全一致**。

0 点时段的等价性经消费端复核亦成立：底座 `skills.json` 中不存在 `area_radius` 键，故 `:51` 取得默认 1.0f；而技能 7 的引导消费端 `src/game/systems/skill/BeamChannelDeliverySystem.cpp:224-226` 为

```
const float radius = (profile && profile->area_radius > 1.0f)
                         ? profile->area_radius
                         : mech.GetFloat(7u, 0u, "base_radius", 60.0f);
```

即 0 点旧路径落到回退值 `base_radius = 60.0f`，与新路径烘焙值 60.0f **完全相同**。故 702 全点数区间（含 0 点）游戏行为无变化，设计 §3.2 步骤 1 的选择（60 = 消费端回退值）是正确的对齐点。

### 2.2 步骤 2 手写分支清理彻底性与 feature_flags 保全 —— 通过

清理完整性（`git show HEAD:` 逐块比对）：

| 节点 | 旧实现（HEAD） | 现实现 | 是否有副作用丢失 |
| --- | --- | --- | --- |
| 700 | `effective_mana_cost *= max(0, 1-0.10*pts)` | 仅注释（`:783-784`），数值走 UMR | 无（旧分支无 flag 写入） |
| 701 | `more_damage_mult *= (1+0.10*pts)` | 同上 | 无 |
| 702 | `area_radius = 60*(1+0.10*pts)`（赋值） | 同上 | 无 |
| 703 | `del.range = (range>0?range:200)*(1+0.10*pts)` | 同上 | 无 |
| 732 | `effective_mana_cost *= (1+mana_penalty_pct)` + `flags \|= 128` | 仅 `del.feature_flags \|= 128;`（`:799-800`） | **flag 保留** |
| 800 | `effective_mana_cost = max(0, cost - 1.0*pts)` + `\|= 1u` | 仅 `\|= 1u;`（`:840-841`） | **flag 保留** |
| 801 | `speed *= mult; range *= mult` + `\|= 2u` | 仅 `\|= 2u;`（`:842-843`） | **flag 保留** |
| 810 | `del.duration = hover_duration(0.8f)` + `\|= 16u` | 仅 `\|= 16u;`（`:849-850`） | **flag 保留** |
| 975 | `trance.duration_sec = form_duration + 0.25*pts` + `\|= 1u<<0` | 仅 `\|= 1u<<0;`（`:969-970`） | **flag 保留** |
| 986 | `trance.cooldown_flat_reduce = 1.0*pts` + `effective_cooldown = max(1.0f, cd - reduce)` + `\|= 1u<<16` | 仅 `\|= 1u<<16;`（`:1011-1012`） | **flag 保留** |

逐一核验后，**6 个必保 flag（128 / 1u / 2u / 16u / 1u<<0 / 1u<<16）全部保留**，被删的旧分支均无 `feature_flags` 置位、无其它状态写入，也不存在「同一节点在别处补写」的缺口。节点链首由 `else if (node_id == 710)` 改为 `if (node_id == 710) {`（`:785`）是把删除后悬空的 `else if` 正确改写为链首，语义不变。

同时确认无重复计算：canonical 中 `skill_id_whitelist` 含 7 的记录恰为 5 条（json 行 1747/1790/1833/1876/1919）、含 8 的 4 条、含 9 的 2 条，合计 11 条，无多余记录；手写分支已删，不存在「UMR + 手写」双算。

### 2.3 步骤 4 —— 通过

`src/game/systems/skill/SkillSpecializationBaker.cpp:257-267`（与 `skill_id==3` 的 `:238`、`skill_id==5` 的 `:251` 并列，非嵌套）：

- 技能 8（`:257-261`）：`if ((out_profile.delivery.feature_flags & (1u << 20)) != 0) { out_profile.delivery.sub_count = 0; }`
  - 位值核对：854 分支置 `del.feature_flags |= 1048576u;` 且写 `del.sub_count = 0`（`:893-896`），而 830 分支写 `del.sub_count = 2` 并置 `1024u`（`:864-866`）。`1048576 == 1u << 20`，判定位正确。由于节点遍历顺序取决于 `allocated_points` 容器的键序，830 可能晚于 854 执行，故该终局覆盖**确实消除了顺序依赖**，是必要且正确的修复（与设计 §6 高风险行对应）。
- 技能 9（`:262-266`）：
  - `:264` `trance.duration_sec = delivery.duration;` —— 单源同步。必要性成立：步骤 1（`:128`）写入的是**加了 DurationFlat 之前**的 3.0f，步骤 3（`:211`）才把 `GetSkillDurationFlat(9)` 累加进 `del.duration`，因此必须在步骤 3 之后重新同步，否则 975 加成丢在 trance 侧。旧实现靠节点分支直接写 `trance.duration_sec`，新实现靠终局同步，语义等价且消除了双源。
  - `:266` `effective_cooldown = std::max(1.0f, effective_cooldown);` —— 在步骤 3（`:196-199`，`max(0, (cd+flat)*mult)`）之后，故 986 的负向 COOLDOWN_FLAT（求值层 `ModifierEvaluator.cpp:288-289` 加性累积）无法穿透 1.0s 下限。

消费端唯一性复核：`rg "delivery\.duration|del\.duration|duration_sec" src/game/systems/skill/` 显示 `delivery.duration` 的消费点为 `AreaFieldDeliverySystem.cpp:469`、`behaviors/SwordArray.cpp:132`（技能 6）、`behaviors/BladeBoomerang.cpp:199`（技能 8，`bc.hover_duration = del.duration`）。技能 9 的形态时长只经 `params.duration_sec` 读取（`behaviors/PhantomTrance.cpp:235/236/257/258/745/746/753/754/776`），技能 9 的爆发走内部 `burst_radius`（`PhantomTrance.cpp:517-537`）而非 AreaField 系统，故对技能 9 而言 `delivery.duration` **无其它消费者**，单源同步不会外溢影响别的系统。

### 2.4 迁移前后数值等价性 —— 逐一复算通过

复算基准：`skill_mechanics.json` 7/0 `mana_cost_per_sec=15.0`（`:562`）、`base_radius=60.0`（`:563`）、`base_range=350.0`（`:564`）；技能 8 基础法耗 8.0（由 `tests/functional/BladeBoomerangNodes.cpp:214`「800=4 → 4.0」反推）；技能 9 `form_duration=3.0`、`cooldown` 基础值 15.0。

| 节点 | 旧公式（HEAD） | 新公式 | 4 点复算 | 判定 |
| --- | --- | --- | --- | --- |
| 700 | `cost *= max(0, 1-0.10*pts)` | `cost = (cost+0)*Π(1-0.10*pts)` | 15×0.6=9.0 | 等价 |
| 732 | `cost *= (1+0.50)`（**不含 pts**） | `mult = 1-(-0.50*pts)` | 15×0.9=13.5（pts=1 时 15×0.9×1.5=20.25 两侧一致） | 等价（前提见 F-07） |
| 701 | `more *= (1+0.10*pts)` | `more *= (1+0.10*pts)` | 1.40 | 等价 |
| 702 | `area = 60*(1+0.10*pts)`（赋值） | `area = 60*(1+0.10*pts)` | 84 | 等价 |
| 703 | `range = range*(1+0.10*pts)`，`range=350` | 同 | 490 | 等价 |
| 800 | `cost = max(0, cost-1.0*pts)` | `cost = max(0,(cost-1.0*pts)*1)` | 8-4=4.0 | 等价 |
| 801 | `speed*=mult; range*=mult`，`mult=1+0.15*pts` | 同（乘算按记录累乘） | 400×1.6=640 / 300×1.6=480 | 等价 |
| 810 | `duration = hover_duration`（**不含 pts**） | `duration = 0 + 0.80*pts` | 0.8 | 等价（前提见 F-07） |
| 975 | `trance = 3.0 + 0.25*pts` | `duration = 3.0+0.25*pts` → 同步 `trance` | 4.0 / 4.0 | 等价 |
| 986 | `cd = max(1.0, 15 - 1.0*pts)` | `cd = max(1.0, (15 + (-1.0*pts))*1)` | 14.0 | 等价 |

乘加顺序复核（无漂移的关键点）：

- 求值层对同类乘算记录是**累乘**（`ModifierEvaluator.cpp:401-409/426-434` 的 `emplace`/`*= mult`），对加算记录是**累加**（`:421-424` `+=`）。旧实现中 700 与 732 对技能 7 的连续乘算（0.6 × 1.5 = 0.9）与新实现的累乘 `0.6*1.5` 一致。
- 旧实现 700 的**逐节点钳制** `max(0, 1-red)` 由求值层 `ModifierEvaluator.cpp:305` 的 `std::max(0.0f, 1.0f - paramF32*pts)` 承担，逐记录独立钳制，语义一致。
- 旧实现 800 的 `max(0, cost - flat)`（先算 flat 再钳制）与新实现 `max(0, (cost + flat) * mult)` 在 `mult=1` 时一致；技能 8 无 COOLDOWN/MANA 乘算记录，故 mult 恒为 1。
- 缺键语义：`ModifierEvaluator` 的 getter 走「缺键取单位元」（乘算 1.0f / 加算 0.0f），故未点节点不产生偏移，旧实现「分支不命中即不动」等价。

残余的、**已知且不触发**的差异：旧 703 存在 `del.range > 0 ? del.range : 200.0f` 回退（HEAD 行 705），新实现无该回退。若 `base_range` 被配置为 0，新实现 `del.range=0`，但消费端 `BeamChannelDeliverySystem.cpp:211-213` 对 `range <= 0` 会回退到同键默认 350.0f，形成安全网，故当前不可观测。

### 2.5 步骤 4 与步骤 5 的相对顺序（自报风险 a）—— 属实但非本批次缺陷

`SkillSpecializationBaker.cpp:266`（步骤 4，下限 1.0f）先于 `:282`（步骤 5，`effective_cooldown = std::max(0.0f, effective_cooldown + mod.flat_cooldown_delta);`）。因 `:270-307` 位于 `if (spec && ...)` 块（`:268` 闭合）之外，步骤 5 对所有技能无条件执行，装备的 `flat_cooldown_delta` 可将技能 9 冷却压至 1.0s 以下。结论见 F-03。

### 2.6 `SkillDefs.hpp` `cooldown_flat_reduce` 删除 —— 通过

`git diff` 为 0 增 1 删，唯一变化是从 `PhantomTranceParams` 删除 `float cooldown_flat_reduce = 0.0f;`。逐项核验：

- `bool operator==(const PhantomTranceParams &) const = default;`（`SkillDefs.hpp:619`）为默认化比较，按成员逐个生成，删除成员后仍然合法，无语义残留。
- `static_assert(std::is_standard_layout_v<PhantomTranceParams>)`（`:621`）继续成立；`BakedDeliveryParams`（`:667-668`）与 `BakedSkillProfile`（`:697-698`）的 static_assert 未受影响。
- 聚合初始化：全仓不存在按位置初始化 `PhantomTranceParams{...}` 的调用点（`rg` 仅见 `PhantomTranceParams params{};`（`:1016`）与成员级写入 `del.trance.*`），故尾字段删除不改变任何既有聚合初始化语义。
- 序列化：`rg "memcpy|Serialize|to_bytes|FromBytes"` 在 `SkillDefs.hpp` 中无对 `PhantomTranceParams`/`BakedDeliveryParams` 的二进制或文本序列化代码，`std::array<BakedSkillProfile, MAX_SKILL_SLOTS> baked_profiles{}`（`:713`）只是内存常驻数组，`sizeof` 变化不落盘，无版本兼容问题。
- 外部读取者：仅 `behaviors/PhantomTrance.cpp:87`、`:100`（`return profile->delivery.trance;`），其消费字段为 `params.duration_sec`，与已删字段无关。
- 删除后无悬空引用：`rg "cooldown_flat_reduce"` 在 `src/` 下零命中。

结论：字段删除对聚合初始化、序列化、`operator==`、`static_assert` 与外部模块均无编译语义变化。

### 2.7 单测断言质量（设计 §5.3 六项覆盖矩阵）

| # | 设计 §5.3 条目 | 覆盖位置 | 判定 |
| --- | --- | --- | --- |
| 1 | 700 `MORE_DAMAGE_MULT` 技能 7 线性 | `SkillBatch3DeliveryOpTests.cpp` 701 用例；`SkillSpecializationBakerTests.cpp:2129`（1.40） | 通过 |
| 2 | 702 `AREA_MULT` + 703 `RANGE_MULT` 与技能 7 步骤 1 基准 | DeliveryOpTests 702/703 用例；BakerTests `:2097-2099`（60/350/15）、`:2109`（84）、`:2119`（490） | 通过 |
| 3 | `MANA_COST_FLAT` 技能 8 负向平减 **+ 下限 0 保护** | DeliveryOpTests 800 用例（-1/-4/0）；`BladeBoomerangNodes.cpp:214`（4.0）；**下限 0 仅由技能 2 用例 `SkillSpecializationBakerTests.cpp:448-468` 覆盖，技能 8 无下限用例** | **部分**（见 F-01） |
| 4 | 801 `SPEED_MULT`+`RANGE_MULT` 技能 8 复合 | DeliveryOpTests 两个 801 用例；`BladeBoomerangNodes.cpp:220-221`（640/480） | 通过 |
| 5 | 810 `DURATION_FLAT` 0.8s + 975 `DURATION_FLAT` 0.25s | DeliveryOpTests 810/975 用例；`BladeBoomerangNodes.cpp:233`（0.8）；`SkillSpecializationBakerTests.cpp:2173-2174`（4.0 且 trance 同步 4.0） | 通过 |
| 6 | 986 `COOLDOWN_FLAT` 缩减 + 1.0s 终局下限 | DeliveryOpTests 986 用例；BakerTests `:2184`（14.0）、`:2197`（注入 -20 后仍 1.0） | 通过 |

其余质量判定：

- **无循环论证**：DeliveryOpTests 全部断言落在 `ModifierDelta` 的 getter 上（求值层输出），不读取 Baker 中间量、不 mock 求值器；BakerTests 的合成断言落在 `BakedSkillProfile` 终值上，属于消费层，两层分工与计划 v1.1 的 F-04 决定一致，不存在「用实现算出的值断言实现」。
- **无「把消费端行为算在求值层」的实际断言**，但存在该倾向的注释与一处恒真断言（F-01）。
- **断言值溯源**：BakerTests 的 60/350/15/84/490/1.40/9.0/4.0/14.0/1.0 均可用设计公式 + canonical 的 per-point 幅度独立推导（见 §2.4 复算表），未发现抄自实现的痕迹。
- 单测的 per-point 幅度为**字面量常量**，未从 canonical 读取（F-02）。

### 2.8 功能测试隔离与断言保全 —— 通过

`git diff` 核对（三文件均为 **+3/-0**，无任何删除）：

- `tests/functional/MindBladeNodes.cpp`：`EnsureSkillMechanics()` 内 `(void)systems::AilmentRegistry::Get().EnsureLoaded();` 之后插入 2 行注释 + `REQUIRE(ReloadModifierRuntimeFromAsset());`
- `tests/functional/BladeBoomerangNodes.cpp`：同上
- `tests/functional/PhantomTranceNodes.cpp`：同上

即**既有 100% 断言无一被弱化或删除**（设计 §5.4 满足）。附带确认：`MindBladeNodes.cpp:520-540` 的 703 用例（`CHECK(range4 == doctest::Approx(490.0f)); CHECK(range4 > 350.0f);`）为既有用例且与现实现一致；`:591-598` 场景二以 `profile.area_radius` 构造几何，现实现给出 84，与用例守卫 `(> 1.0f) ? ... : 60.0f` 兼容。

### 2.9 自报风险评估复核（要求逐项给出属实/不属实 + 严重度 + 是否阻塞）

| # | 自报风险 | 判定 | 严重度 | 阻塞 | 依据 |
| --- | --- | --- | --- | --- | --- |
| a | 冷却 1.0s 下限仅置于步骤 4，可被步骤 5 装备路径压低 | **属实**（但非本批次引入，且设计 §6 的风险仅针对 986，986 已被闸住） | 低 | 否 | `SkillSpecializationBaker.cpp:266` 与 `:282` |
| b | 700 法耗乘算从步骤 2 折叠到步骤 3 `(base+flat)*mult`，若技能 7 存在 `SKILL_MANA_COST_FLAT` 则新旧不等价 | **属实（条件性）**：当前全量 canonical 中 `SKILL_MANA_COST_FLAT` 仅出现在技能 2 节点 201 与技能 8 节点 800，技能 7 无该记录，故**当前不可触发**；一旦未来为技能 7 增设，结算顺序将从「仅乘」变为「先加后乘」 | 低 | 否 | `asset canonical`（仅 201/800 两处）+ `SkillSpecializationBaker.cpp:191-195` |
| c | `GameplayState.cpp:876` 渲染圈 TODO 是否仍准确 | **属实且准确**：`:877` 仍读 `skills::GetMech(7, 0, "base_range", 350.0f)`，`:873-881` 绘制半径未乘 703 的放大量；注释已改写为「703 的 `SKILL_RANGE_MULT` 专精放大未接入，渲染圈未随节点放大」，与代码一致。该缺口为既有问题，设计与计划均未将其纳入本批次范围 | 低 | 否 | `GameplayState.cpp:873-881` |

## 3. 问题清单

### F-01（中·非阻塞）§5.3 第 3 项的技能 8 法耗下限未落实，且存在一处恒真断言

- 证据：`tests/unit/SkillBatch3DeliveryOpTests.cpp:158-160` 注释声明「法耗下限 0 由消费端（Baker 步骤 3 的 `max(0, base + flat)`）承担」，随后断言 `CHECK(ModifierDelta{}.GetSkillManaCostFlat(kSkill) == doctest::Approx(0.0f));`。这是对**默认构造的空 delta** 取值，`ModifierDelta` 缺键返回单位元 0.0f 属构造性事实，恒真，不构成任何下限验证。
- 证据：技能 8 的法耗下限在任何测试中均未被驱动到 0。负向平减有覆盖（`SkillBatch3DeliveryOpTests.cpp` 的 800 用例、`tests/functional/BladeBoomerangNodes.cpp:214` 的 800=4 → 4.0），共享钳制路径由 `tests/unit/SkillSpecializationBakerTests.cpp:448-468`（技能 2 节点 201，注入 -100 → 0.0）覆盖，但那是技能 2 而非技能 8。设计 §5.3 第 3 项明确要求「在技能 8 上的负向平减**与下限 0 保护**」。
- 影响：行为本身由共享钳制路径保证（`SkillSpecializationBaker.cpp:191-195` 对全部技能同构），风险有限；缺口在**验收闭环**与**断言有效性**，即一条恒真断言掩盖了未落实的设计条目。
- 期望修复方向：删除该恒真断言；在 `SkillSpecializationBakerTests.cpp` 的 Batch3 用例中补一条技能 8 下限用例（例如 800 点数使 `8 - pts <= 0`，或注入 `BuildManaRuntimeBlob` 负值，断言 `effective_mana_cost == 0.0f`），使其真正覆盖消费端钳制。

### F-02（低·非阻塞）DeliveryOpTests 的幅度字面量与 canonical 无耦合

- 证据：`tests/unit/SkillBatch3DeliveryOpTests.cpp` 全部用例以字面量书写 per-point 幅度（0.10 / -0.50 / -1.0 / 0.15 / 0.80 / 0.25），未从 `assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json` 读取。
- 影响：若 canonical 数值发生漂移，该单测不会失败，设计「asset 为单一事实源」的承诺在 C++ 侧缺少守卫。设计 §5.2 已把数据侧校验交给 `tests/python/SkillSpecBatch3GateTest.py` 与 `scripts/validate_skill_spec_modifiers.py`，职责上可接受。
- 期望修复方向：在文件头注明与其 Python gate 测试的分工，或对关键幅度做一次 canonical 读取比对；至少避免未来读者误以为该测试覆盖了数据源。

### F-03（低·非阻塞）步骤 5 装备冷却可穿透步骤 4 的 1.0s 下限

- 证据：`src/game/systems/skill/SkillSpecializationBaker.cpp:266`（步骤 4 设下限 1.0f）先于 `:282`（步骤 5 `std::max(0.0f, effective_cooldown + mod.flat_cooldown_delta)`），且步骤 5 位于 `if (spec && ...)` 块之外（`:268` 闭合、`:270-307` 独立），对所有技能无条件执行。
- 判定：属实。但（1）装备平减对全部技能一视同仁，属既有全局结算顺序，非本批次引入；（2）设计 §6 的对应风险行（技能 9 冷却缩减穿透下限）针对的是 986 算子，986 经 `ModifierEvaluator.cpp:288-289` 累积后在 `:266` 被钳制，风险已闭合。
- 期望修复方向：若设计意图是「技能 9 冷却任何来源都不得低于 1.0s」，应把下限钳制移到装备结算之后（`:312` 附近）；否则建议在 `:266` 补注释说明「该下限仅约束专精侧，装备平减按全局规则可继续下压」，以免后续误读为绝对下限。

### F-04（低·非阻塞）700 与 `MANA_COST_FLAT` 的结算顺序假设未在代码中声明

- 证据：`SkillSpecializationBaker.cpp:191-195` 的统一结算为 `(base + flat) * Πmult`；技能 7 当前无 `SKILL_MANA_COST_FLAT` 记录（canonical 中该算子仅出现于技能 2 节点 201 与技能 8 节点 800）。
- 判定：属实但当前不可触发。旧实现 700 只有纯乘算、无 FLAT 语义，因此现状等价；若未来为技能 7 增设 FLAT 记录，结算从「仅乘」变为「先加后乘」，行为将不同于任何历史口径。
- 期望修复方向：在 canonical 的 2007000 记录旁或 Baker case 7 处加一行注释，声明「700 的乘算以 `(base + flat)` 为底」，把该假设显式化。

### F-05（低·非阻塞）`GameplayState.cpp:876` 渲染圈 TODO 准确但缺口仍在

- 证据：`GameplayState.cpp:877` 读 `skills::GetMech(7, 0, "base_range", 350.0f)`，`:873-881` 的绘制半径未乘 703 的 `SKILL_RANGE_MULT`，注释与代码一致。
- 判定：自报风险 c 属实；该缺口为既有问题，设计与计划均未纳入本批次，不构成本批次违约。
- 期望修复方向：保持 TODO；建议在后续批次中改为读取已烘焙 profile 的 `delivery.range`，与 `BeamChannelDeliverySystem.cpp:211-212` 的实际射程保持单一事实源。

### F-06（低·非阻塞）变更集混入与本批次无关的 `settings.json` 改动

- 证据：`git diff HEAD -- assets/data/settings.json` 仅有两处：`renderQualityAutoDetect.benchmarkScore` 124.29000091552734 → 124.26000213623047，`updatedAtUtc` 2026-08-16T13:24:07Z → 2026-09-17T04:45:11Z（自动检测写入）。
- 影响：与技能 7/8/9 迁移无关，混入会污染批次变更集并可能引发跨机器噪声 diff。
- 期望修复方向：提交前将该文件还原，或将此类自动生成字段纳入 ignore/重置流程。

### F-07（低·非阻塞）732 与 810 的数值等价以「Keystone rank 恒为 1」为前提

- 证据：旧实现在 `HEAD` 对 732 写 `effective_mana_cost *= (1.0f + GetFloat(7,732,"mana_penalty_pct",0.50f))`、对 810 写 `del.duration = GetFloat(8,810,"hover_duration",0.8f)`，**均不含 `* points`**；新 UMR 公式为 `1 - (-0.50*pts)` 与 `0.80*pts`，含 `pts`。二者仅在 `points == 1` 时逐值一致。
- 判定：属实。732 与 810 在 `assets/data/skills.json` 中均为 Keystone（`:4783-4784`、`:5429-5430`），设计口径为 Keystone 单点。当前实现下新公式在合法加点上与旧公式等价；但若加点或存档层对 Keystone 缺乏 rank 上限校验，越界加点会让新实现线性放大而旧实现不放大。
- 期望修复方向：确认加点/存档校验层对 Keystone 的 rank 上限为 1（若有该约束，本条可降级为记录性说明），并在设计或注释中固定该前提。

## 4. 已核对且未发现问题的点（审查覆盖面声明）

1. 步骤 1 case 7 `area_radius` 与 702 的手写赋值语义逐一等价，且 0 点时段与消费端回退值 60.0f 完全对齐（`BeamChannelDeliverySystem.cpp:224-226`）。
2. 步骤 2 清理彻底：10 个节点的旧分支全部删除，6 个必保 flag 全部保留，无遗留双算（canonical 记录数与节点一一对应）。
3. 步骤 4 技能 8 巨阙覆盖的位判定正确（`1u<<20` ↔ `1048576u`），遍历顺序无关性成立。
4. 步骤 4 技能 9 单源同步必要且 `delivery.duration` 对技能 9 无其它消费者；1.0s 下限对 986 有效。
5. 11 条 canonical 记录与设计 §4.2 的算子/幅度逐条一致；退役键与保留键与 `scripts/validate_skill_spec_modifiers.py` 的 `KEPT_MECHANICS_KEYS`/`DEAD_MECHANICS_KEYS` 一致；`src/` 下无对已退役键的悬空引用。
6. `PhantomTranceParams::cooldown_flat_reduce` 删除对聚合初始化、序列化、`operator==`、三项 `static_assert` 与外部模块无影响。
7. §5.3 六项中五项完整覆盖；恒真断言仅一处（F-01）；无循环论证、无把消费端行为记在求值层、断言值均可由设计独立推导。
8. 三个功能测试文件均为纯增量（+3/-0），既有断言零删改，`ReloadModifierRuntimeFromAsset()` 隔离到位。
9. 新增单测的接口/类型与构建注册（`GLOB_RECURSE`）静态可编译。

## 5. 最终结论

**提交。**

本批次 C++ 实现面在迁移等价性、幂等性、遍历顺序无关性、单源同步与测试隔离五个维度上均经逐行复算与代码核对通过，未发现内存安全、并发、数值漂移或架构违约类缺陷。设计 §6 的两项高风险（技能 8 巨阙顺序依赖、技能 9 延命双字段不同步）已被步骤 4 正确闭合；自报三项风险中 (a)(b) 属实但均为非本批次引入或当前不可触发，(c) 属实且注释准确。

**阻塞项：无。** 建议随批次一并处理的两项为 F-01（补技能 8 法耗下限用例并删除恒真断言）与 F-06（还原 `settings.json` 无关改动）；其余为可选加固。另请主代理以 `RelWithDebInfo` 构建与单元/功能套件实测确认编译与绿灯结论（本报告为静态审查，未执行编译与测试）。
