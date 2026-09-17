# UMR Batch 4（技能 10/11/12）实现审查报告 — C++ 独立复核

- **审查轮次**：第 4 轮（针对第 3 轮 F1/F2/F3/F4/F6 修复的复审；独立 C++ 复核，聚焦内存安全 / 空值 / 生命周期 / 正确性）
- **审查角色**：独立审查者（未参与实现）
- **结论**：**修改**
- **审查日期**：2026-09-17

## 1. 审查输入

| 类别 | 文件 |
| --- | --- |
| 规则 | `AGENTS.md`、`docs/workflows/review.md` |
| 设计 | `docs/designs/2026-09-17-umr-skill-batch4-seven-stars-heavenly-sword-blood-sea-design.md`（v1.2，§3.2/§3.3/§5.2/§6 R-01/R-05） |
| 计划 | `docs/plans/2026-09-17-umr-skill-batch4-seven-stars-heavenly-sword-blood-sea-plan.md`（v1.2，§2.2/§2.3、Task 4.1/4.2/5.x/6.3/6.4） |
| 代码（本次焦点） | `src/game/systems/skill/SkillSpecializationBaker.cpp`、`src/game/systems/skill/behaviors/{SevenStarSlash,HeavenlySwordDescent,BloodSea}.cpp` |
| 测试 | `tests/unit/SkillBatch4BloodSeaOrderingTests.cpp`（新增）、`tests/unit/SkillBatch4DeliveryOpTests.cpp`、`tests/unit/SkillBatch4NullProfileFallbackTests.cpp`、`tests/unit/SkillSpecializationBakerTests.cpp` |
| 关联声明 | `src/game/systems/skill/SkillProfileResolve.{hpp,cpp}`、`src/game/systems/skill/SkillSystem.cpp`、`src/game/systems/skill/components/PersistentFieldComponents.hpp`、`src/game/foundation/components/SkillDefs.hpp` |

**已提供证据（未重跑）**：`build.bat RelWithDebInfo` RC=0；全量 1761/1761 用例、141849/141849 断言 0 失败；`*SkillBatch4*` 15/15；`*SkillBehaviorGuard*` 19/19；`ctest -L unit` 8/8。本审查按要求未重跑编译 / ctest，仅做只读代码核对。

## 2. 变更文件边界（`git status --short`）

- 修改：`assets/data/mastery_skill_trees.json`、`assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json`、`assets/data/modifier_v2/skill_spec_modifiers.json`、`assets/data/skill_mechanics.json`、`assets/data/skill_mechanics_schema.json`、`scripts/validate_skill_spec_modifiers.py`、`SkillSpecializationBaker.cpp`、`BloodSea.cpp`、`HeavenlySwordDescent.cpp`、`SevenStarSlash.cpp`、`tests/functional/BloodSeaNodes.cpp`、`tests/functional/HeavenlySwordDescentNodes.cpp`、`tests/unit/SkillMechanicsRegistryTests.cpp`、`tests/unit/SkillSpecializationBakerTests.cpp`
- 未跟踪：设计 / 计划文档、本报告、`tests/python/SkillSpecBatch4GateTest.py`、`tests/unit/SkillBatch4BloodSeaOrderingTests.cpp`、`tests/unit/SkillBatch4DeliveryOpTests.cpp`、`tests/unit/SkillBatch4NullProfileFallbackTests.cpp`
- 第 3 轮后新增仅一个测试文件（F1 修复），边界仍与设计 Phase 1–6 一致，无越界改动。
- 新测试由 `tests/CMakeLists.txt:4` 的 `file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS)` 自动纳入构建，无需显式注册。

## 3. 范围对齐（R-01 核心结论）

`ResolveBakedProfile`（`SkillProfileResolve.cpp:9-39`）语义经核实为：缓存命中 → 指针；`fallbackSpec` 提供 → 现场烘焙；否则扫描 `ActiveSkillsComponent::specialized_slots` 同 ID 槽 → 现场烘焙；否则 `nullptr`。**不会自动烘焙**。`ResolveSpecState`（`SpecStateTable.hpp:50-74`）扫描同一批槽位，故 `profile == nullptr` ⟺ 无同 ID 专精槽 ⟺ 专精点全 0，六个新消费点的回退基准与迁移前等价。

| 技能 | 消费点 | 空档案回退基准 | 与迁移前一致 |
| --- | --- | --- | --- |
| 10 | `SevenStarSlash.cpp:424-427` 无敌时长 | `GetParam("invulnerable_duration",0.5f)`，`std::max(0.0f, …)` | ✅ |
| 10 | `SevenStarSlash.cpp:552-553` 暴击 | `0.0f` | ✅ |
| 11 | `HeavenlySwordDescent.cpp:604-610` 半径 | 比值 1.0 → `base_field_radius` | ✅ |
| 11 | `HeavenlySwordDescent.cpp:656-660` 时长 | `GetParam("field_duration",5.0f)` | ✅ |
| 12 | `BloodSea.cpp:430-435` 时长 | `GetParam("field_duration",4.8f)` | ✅ |
| 12 | `BloodSea.cpp:439-445` 半径 | `GetParam("field_radius",120.0f)` | ✅ |
| 12 | `BloodSea.cpp:494` More | `1.0f` | ✅ |

非空行为等价性：全量 1761/1761 与 `SkillBehaviorGuard` 19/19 通过，且逐点核对确认新守卫仅在哨兵档案（`duration == 0`）时改变分支；正常档案 `duration/area_radius` 恒为正，守卫不触发，故非空路径行为不变。

## 4. 复审结果（第 3 轮发现项的状态）

| 第 3 轮项 | 状态 | 说明 |
| --- | --- | --- |
| F1（中）R-05 偏差量化验收 | **部分修复 → 仍为 F1'（中）** | 新增用例已落地并锁定顺序，但偏差解析式建模错误、数值夸大约 3.64×（见 F1'） |
| F2（低）哨兵档案守卫 | **部分修复 → 仍为 F2'（中）** | 时长守卫有效；半径守卫对哨兵无效（哨兵 `area_radius` 默认为 1.0f，`> 0.0f` 判不中，见 F2'） |
| F3（低）注释不完整 | ✅ 已修复 | `HeavenlySwordDescent.cpp:598-603` 与 `BloodSea.cpp:436-439` 均已注明折叠装备 `area_radius_mult` |
| F4（低）技能 11 字面量 | ✅ 已修复 | `SkillBatch4NullProfileFallbackTests.cpp:90,92` 已锚定 `5.0f` / `140.0f` 字面量 |
| F5（低）技能 10 死写 | ✅ 已澄清 | `SkillSpecializationBaker.cpp:130-134` 增加注释说明设计约定的基准一致性用途，功能不变 |
| F6（低）时长夹取 | ✅ 已修复 | `SevenStarSlash.cpp:424-427` 用 `std::max(0.0f, …)` 夹取，同时覆盖档案与非档案分支 |
| F7（信息）1207 半径数值变更 | 保持登记 | 设计已接受，无变化 |

## 5. 发现项（本轮）

### F1'（中）R-05 偏差量化用例的解析模型错误，数值夸大约 3.64×，且断言自证
- **文件**：`tests/unit/SkillBatch4BloodSeaOrderingTests.cpp:21-47, 135-144`
- **证据**：
  - 用例对 `{{1201,4},{1207,1}}` / `{{1201,4},{1207,1},{1202,4}}` 两组的 `bonus_damage_mult` 精确断言 `baseline*more`（2.01872）与 `baseline*more+K`（2.41872），**顺序锁定本身正确且有效**：若把 More 乘算移到 1202 平加之后，`field1202Positive` 将变为 `(baseline+K)*more`，`:138` 立即失败。
  - 但 `:141-144` 将“旧值”建模为 `oldOrderBonus = (baseline + K) * more`，据此得偏差 `K*(more-1)=0.4*0.364=0.1456`，并把该值写入 `:44,47`。这与迁移前的真实顺序不符。以 HEAD 版本核实：`git show HEAD:src/game/systems/skill/behaviors/BloodSea.cpp` 中旧代码为 `:484-485`（1201 `*=`，**在 1202 平加之前**）、`:486-488`（1202 `+=`）、`:540/542`（环形半径/伤害 `*=`）、`:545`（1207 `*=`）。即旧值 `= ((baseline*1.24 + K) * R) * 1.10`，而非 `(baseline + K) * more`；1201 的 1.24 从未跨越 1202 平加。
  - 因此真实迁移偏差（R=1）`= 0.1*K = 0.04`，正是设计 R-05 的 `0.1*K*ring`；用例却记录为 `0.1456`（夸大约 3.64×）。`:21-24` 的推导注释“旧值 = (baseline + K) * more”亦为错误论述。
  - `:143` 的 `deviation == k1202*(more-1)` 与 `:138` 代数等价（由 `field1202Positive == baseline*more+K` 直接可推），属**自证断言**，不构成对“旧顺序”的独立验证。
- **影响**：顺序回归防护有效；但设计 R-05 要求的“量化并记录该已接受偏差”给出的量级错误，会误导后续数值评审与设计文档回填。
- **修复**：把旧顺序模型改为 `oldOrderBonus = (baseline * (more / more1207) + K) * ring * more1207`（即 `(baseline*1.24 + K)*1.10`，`more1207=1.10`），并把 `kExpectedDeviation` 修为 `0.1f*K = 0.04f`；注释改写为“仅 1207 的 `*1.1` 跨越 1202 平加”。若坚持同时移动 1201 的假想口径，必须明确标注其为“非迁移前口径的上界”，不得称“迁移代价”。

### F2'（中）半径哨兵守卫无效：哨兵 `area_radius` 默认为 1.0f，`> 0.0f` 判不中
- **文件**：`src/game/systems/skill/SkillSystem.cpp:2736-2742`；`src/game/foundation/components/SkillDefs.hpp:678`；`src/game/systems/skill/behaviors/HeavenlySwordDescent.cpp:607`；`src/game/systems/skill/behaviors/BloodSea.cpp:440`
- **证据**：`RebakeSkillProfiles` 在 `skillData == nullptr` 时写入哨兵 `BakedSkillProfile p{}; p.skill_id = skill_id;`。`BakedSkillProfile` 的字段默认值为 `area_radius = 1.0f`（`SkillDefs.hpp:678`）、`more_damage_mult = 1.0f`、`delivery.duration = 0`。因此：
  - `profile->delivery.duration > 0.0f` **能**识别哨兵（0 不通过），时长守卫有效；
  - `profile->area_radius > 0.0f` **不能**识别哨兵（1.0 > 0 通过）。`HeavenlySwordDescent.cpp:606-610` 会算出 `areaMult = 1.0/140 ≈ 0.00714`，使 `field_radius ≈ 1.0`；`BloodSea.cpp:439-445` 会算出半径 `≈ 1.0 + 行为层加项`。
- **可达性**：当技能数据在重烘焙时缺失、其后数据补齐但未再烘焙时，缓存哨兵仍在，`TryCast`（`SkillSystem.cpp:1981-1985`）因当前数据存在而放行，`DoCast` 即消费哨兵；`SkillSystem.cpp:1886` 等直调路径同样绕过前置校验。属防御性边界缺陷，故评“中”：一个看似生效、实则不生效的守卫比无守卫更危险（形成假保证）。
- **测试盲区**：`SkillBatch4NullProfileFallbackTests.cpp` 经 `CreateCaster` 构造无 `ActiveSkillsComponent` 的施法者，走的是 `nullptr` 分支而**非**哨兵分支，因此 1761/1761 全绿并不能证明本守卫有效。
- **修复（建议）**：以可靠哨兵标记判别半径，例如 `(profile && profile->delivery.duration > 0.0f)` 或与已知基准比较；更彻底的做法是在 `RebakeSkillProfiles` 不写入可被 `GetBakedSkillProfile` 命中的哨兵（如 `skill_id = INVALID_SKILL_ID`，或令哨兵 `area_radius = 0`），从源头消除歧义。同时补充一条“存在哨兵档案”的回归用例。

## 6. 已核实无问题项

- **成员/类型无幻影**：`HeavenlySwordFieldComponent` 无 `bonus_damage_mult`（设计已删除），`BloodSeaFieldComponent` 含 `bonus_damage_mult`；`BakedSkillProfile`/`BakedDeliveryParams` 字段路径全部存在（`SkillDefs.hpp:671-698`）。
- **既有精确值断言仍等价**：`BloodSeaNodes.cpp:278/279/293/294/308-310/327/590-600`、`SkillBehaviorGuardTests.cpp:664-676/2227-2228` 所配置的节点均不含 1201/1202/1207，迁移前后数值路径一致；本轮大样本回归通过亦佐证非空路径未变。
- **退役键无悬挂生产读者**：六个迁移键与八处机制读取在生产代码中已无输出；`SkillMechanicsRegistryTests.cpp:81-85`、`BloodSeaNodes.cpp:129/141/181`、`HeavenlySwordDescentNodes.cpp:128/147/182` 的改动是反向断言加强（断言键不存在），非弱化测试。
- **内存安全 / 生命周期**：无裸 `new/delete`、无 `reinterpret_cast`/C 风格转换、无 `const_cast`、无 `dynamic_cast`；`ResolveBakedProfile` 的 `scratch` 为调用栈局部对象，返回指针仅在 `DoCast` 同作用域内使用，无悬垂；`[[nodiscard]]` 生效且所有调用点均接收返回值。新增排序用例 `SkillBatch4BloodSeaOrderingTests.cpp:58-88` 的 `castField` lambda 虽在体内创建局部 `entt::registry`，但返回类型由 `auto` 按值推导（`auto` 剥离引用），`return view.get<BloodSeaFieldComponent>(…)` 是在局部 registry 析构**之前**完成拷贝构造，故 `field1202Zero` / `field1202Positive` 是独立值对象，`:131-144` 的读取无悬垂、无 UAF。（若改为 `auto&` / `decltype(auto)` 则会悬垂——此处未使用，故安全。）
- **测试隔离与空值路径真实**：`CreateCaster`（`tests/SkillKeyNodeMatrixTestHelpers.hpp:321-329`）不装配 `ActiveSkillsComponent`，`ResolveBakedProfile` 确为 `nullptr` 分支；`TestSetupScope` 析构重载技能注册表，无跨用例桩泄漏；无 `REQUIRE(true)` 类平凡断言。
- **注释规范**：新增注释均为中文且描述目的 / 设计依据（含 `cpp F2` 溯源），无 `Task x` 类过程性注释，无 TODO/FIXME。

## 7. 最佳实践建议

1. 在 `ResolveBakedProfile` / `GetBakedSkillProfile` 层面统一排除“哨兵档案”，可一次性消除所有消费点的重复且易错的守卫（F2'）。
2. 将 R-05 偏差解析式与真实旧顺序（`(baseline*1.24 + K)*R*1.10`）写入设计“已量化”栏，并回填测试链接（F1'）。
3. 为“哨兵档案”补一条端到端回归用例（构造 `ActiveSkillsComponent` 含无数据技能槽并触发重烘焙），使防御路径可被测试覆盖。

## 8. 剩余风险

- 半径哨兵守卫失效：数据热更新 / 存档引用缺失技能产生哨兵档案时，技能 11/12 领域半径退化为 ≈1（F2'，当前被施法前置校验部分阻断）。
- R-05 偏差已由测试锁定顺序，但记录量级错误（0.04 记为 0.1456），数值评审依据不可靠（F1'）。
- 装备 `area_radius_mult` 一旦被赋予技能 11/12，将静默改变其领域半径（第 3 轮 F3，潜伏；注释已披露）。

## 9. 结论与下一步动作

**结论：修改。**

必须处理：
1. **F1'（中）**：修正 `tests/unit/SkillBatch4BloodSeaOrderingTests.cpp` 的旧顺序模型与偏差常量（`0.1*K`，非 `K*(more-1)`），消除自证断言与错误注释。
2. **F2'（中）**：修正半径哨兵守卫（`area_radius > 0.0f` 对默认 1.0f 无效），改用可靠的哨兵判别或从 `RebakeSkillProfiles` 源头消除哨兵，并补哨兵回归用例。

已确认第 3 轮 F3/F4/F5/F6/F7 处理正确，且未发现新的非空行为回归、内存/生命周期或空值处理回归。

---

## 10. 第 3 轮复核（F1'/F2' 关闭验证）

> 说明：以下针对本文档第 5 节所列 F1'/F2' 两项（本文档编号为「第 4 轮发现项」）的独立关闭验证。只读复核，未修改任何源码 / 数据文件；作者提供的编译与测试证据仅作合理性抽查，未重跑。

### 10.1 F1'（中）R-05 旧顺序模型 — **已关闭**

**独立重推导（`git show HEAD:src/game/systems/skill/behaviors/BloodSea.cpp`）**：

- `:484-485` 1201：`field.bonus_damage_mult *= 1.0f + pressureTideRisePoints * damage_per_point`（**在 1202 平加之前**）
- `:486-488` 1202：`field.bonus_damage_mult += effective_consumed * bloodthirstEdgePoints * damage_per_point_per_bloodthirst`
- `:544-546` 1207：`if (spec.bottomlessPurgatory) field.bonus_damage_mult *= bottomless_damage_mult`（**在 1202 之后**）

→ 迁移前真实顺序 = `(baseline · 1.24 + K) · 1.10`，与第 4 轮 F1' 的判定一致；作者采用的新模型正确。

**常量与算术独立核对**（数据源而非测试自述）：

| 量 | 来源 | 值 |
| --- | --- | --- |
| 1201 单点乘区 | `assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json:2447-2450` `param_f32=0.06`；4 点 | `1 + 0.06×4 = 1.24` ✅ |
| 1207 乘区 | 同文件 `:2459-2493` `param_f32=0.1`（mul op） | `1.10` ✅ |
| baseline | `assets/data/skills.json:6440` `bloodthirst_damage_bonus=0.12` | `1 + 4×0.12 = 1.48` ✅ |
| K | `assets/data/skill_mechanics.json:951` `damage_per_point_per_bloodthirst=0.025` | `4×4×0.025 = 0.4` ✅ |

新顺序 `1.48×1.364 + 0.4 = 2.41872`；旧顺序 `(1.48×1.24 + 0.4)×1.10 = 2.45872`；偏差 `2.45872 − 2.41872 = 0.04 = 0.1×K`。全部正确。

**测试核对**（`tests/unit/SkillBatch4BloodSeaOrderingTests.cpp`）：

- `:43` `kMore1201Expected=1.24f`、`:45` `kMore1207Expected=1.10f`、`:56` `kExpectedOldOrderWithBonus=2.45872f`、`:58` `kExpectedDeviation=0.04f`，与上表逐一吻合；
- `:152-153` `oldOrderBonus = (baseline * kMore1201Expected + k1202) * kMore1207Expected`，模型已纠正；
- `:155-156` 分别以字面量钉死旧 / 新值；`:157-159` 锁定 `0.1×K`；
- **顺序锁定仍有效**：`:148-149` 断言 `bonus == baseline*more + K`（`field1202Positive`）；若把 More 乘算移到 1202 平加之后，值变为 `(1.48+0.4)×1.364 = 2.56432 ≠ 2.41872`，`:149` 立即失败。`:143-144` 同（`1202=0` 时 `2.01872`）。✅

**关于「自证断言」**：`:157-159` 在 `:149` 与 `:152-153` 模型定义之下**仍是代数恒等式**（旧代码不可执行，偏差断言本质上只能是模型算术，无法独立观测）。但原危害已消除——旧顺序模型已与 HEAD 一致，且唯一「旧值」常量 `2.45872` 在 `:155` 以字面量钉死，不存在以错误模型自证的情形。保留为 10.4 的非阻塞观察项 N1。

**判定：已关闭。** 模型正确、常量正确、顺序回归防护仍有效。

### 10.2 F2'（中）半径哨兵守卫 — **已关闭（并更正我的原前提）**

**我先前的 F2' 前提部分错误，特此更正。** 第 4 轮 F2' 中我写「`delivery.duration = 0`，故 `profile->delivery.duration > 0.0f` 能识别哨兵，时长守卫有效」。实测：

- `src/game/foundation/components/SkillDefs.hpp:631`：`BakedDeliveryParams::duration = 1.0f`（**不是 0**）。
- 因此 `profile->delivery.duration > 0.0f` 对哨兵**同样无效**——与 `area_radius > 0.0f` 一样恒为真。

即：我关于「时长守卫有效」的论断是**事实性错误**，无任何站得住脚的依据；作者「任何正数守卫都无效」的判断成立。

**独立核实**：

- `SkillDefs.hpp:631` `duration=1.0f`；`:678` `area_radius=1.0f`；`:672` `skill_id=0`；`:679` `proc_coefficient=1.0f`；`:680` `more_damage_mult=1.0f`——哨兵默认值全在合法非零域内。
- 哨兵写入：`src/game/systems/skill/SkillSystem.cpp:2735-2742`，仅在 `!skillData`（即 `SkillRegistry::Get().GetSkill(skill_id)==nullptr`）时写入 `BakedSkillProfile p{}; p.skill_id = skill_id;`。
- `GetBakedSkillProfile`（`SkillSystem.cpp:2766-2779`）**仅按 `skill_id` 匹配**，无有效性校验，故哨兵可被命中。
- 可达性：`RebakeSkillProfiles` 由 `src/game/foundation/stats/AttributePipeline.cpp:836` 运行时触发；`SkillRegistry::LoadFromJson` 仅在 `src/app/Game.cpp:258` 调用一次（初始化期）；`assets/data/skills.json:6393`（id 11）、`:6418`（id 12）均在册。故技能 11/12 的哨兵在正常流程中**不可达**，作者陈述成立。

**当前守卫状态**：三个行为文件已无 `profile->delivery.duration/area_radius > 0.0f` 正数守卫（grep 为空），恢复为与计划一致的朴素三元 `profile ? profile->X : fallback`；仅保留 `HeavenlySwordDescent.cpp:605-608` 的 `base_field_radius > 0.0f` 除零守卫（真实必要）。注释已改为准确中文：`BloodSea.cpp:425-430`、`HeavenlySwordDescent.cpp:598-602` 均明确写出「哨兵 `duration`/`area_radius` 默认 1.0f，正数守卫无法区分」。

**是否存在真正有效的守卫？** **值域（positivity）型守卫不存在**——哨兵默认全为非零合法域值（`duration=1.0f` / `area_radius=1.0f`），任何 `> 0.0f` 都无法与正常档案区分。唯一有效的是**结构性**方案：在源头不写入可被 `GetBakedSkillProfile` 命中的哨兵。精确代码（`src/game/systems/skill/SkillSystem.cpp` 约 `:2735-2742`）：

```cpp
if (!skillData) {
  // 不写入带 skill_id 的哨兵，避免 GetBakedSkillProfile 以 skill_id 命中默认档案
  if (!(active->baked_profiles[i] == BakedSkillProfile{})) {
    active->baked_profiles[i] = BakedSkillProfile{};
  }
  continue;
}
```

**判定：已关闭。** 依据：(1) 我的原前提有误，F2' 指控中「时长守卫有效」不成立，所谓「有效守卫被错删」不成立；(2) 回退到计划一致的朴素三元是正确的工程决策，而非弱化；(3) 技能 11/12 哨兵在正常流程中不可达。10.4 的结构性方案降级为非阻塞后续项 N2。

### 10.3 回归复检（通过）

- **无幻影成员**：`src/game/systems/skill/components/PersistentFieldComponents.hpp` `HeavenlySwordFieldComponent:6-45` 无 `bonus_damage_mult`；`BloodSeaFieldComponent:60` 含之。`HeavenlySwordDescent.cpp:475` 的 `bonus_damage_mult` 属 `BeamChannelComponent`（另一类型），非幻影。
- **R-01 回退**：`SevenStarSlash.cpp:424-427`、`HeavenlySwordDescent.cpp:655-658`、`BloodSea.cpp:431-435`、`:439-444` 均为朴素空指针三元，回退基准与迁移前一致。
- **R-05 乘加位置**：`BloodSea.cpp:493`（`*= more_damage_mult`）仍在 `:494-496`（1202 平加）之前，未变。
- **无越界 / 无新增守卫**：三行为文件未再引入正数守卫；修复轮未触及数据文件语义。

### 10.4 新发现（均为非阻塞）

- **N1（信息）**：`SkillBatch4BloodSeaOrderingTests.cpp:157-159` 仍为代数恒等式，不构成对旧顺序的独立观测（见 10.1）。建议后续改为「字面量锚定 + 说明为模型推导」，或直接断言 `newOrderBonus == kExpectedWithBonus` 并保留对 `2.45872` 的独立钉死即可。
- **N2（信息）**：10.2 的结构性哨兵消除仍建议作为独立小任务落地，可一次性消除多个消费点的重复防御逻辑。

### 10.5 复核结论

**结论：提交。**

| 发现项 | 第 4 轮判定 | 本轮状态 |
| --- | --- | --- |
| F1'（中）R-05 旧顺序模型错误 | 修改 | **已关闭** |
| F2'（中）半径哨兵伪守卫 | 修改 | **已关闭**（我的原前提有误并更正） |
| F3 / F4 / F5 / F6 / F7 | 已处理 | 无变化，维持 |

F1' 的模型 / 常量已纠正且顺序回归防护仍有效；F2' 经独立核实，其严重性被我高估（`duration` 默认 1.0f，非 0），回退守卫为正确决策。未发现阻塞性新问题；N1/N2 为非阻塞后续项。
