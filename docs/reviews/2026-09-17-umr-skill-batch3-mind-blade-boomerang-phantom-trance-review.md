# UMR 技能专精改造第三批（技能 7/8/9）方案审查报告

- **系统代号**：`UMR-SKILL-BATCH-3`
- **审查日期**：2026-09-17
- **审查轮次**：第 2 轮复核（最终通过审查）
- **审查标准**：[`docs/workflows/review.md`](file:///d:/PRJ/NoMoreDay/docs/workflows/review.md)
- **代码硬规则**：`conductor/code_standard.md` (V2.1)
- **审查对象**：
  - 设计文档：[`docs/designs/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-design.md`](file:///d:/PRJ/NoMoreDay/docs/designs/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-design.md)
  - 实施计划：[`docs/plans/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-plan.md`](file:///d:/PRJ/NoMoreDay/docs/plans/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-plan.md)

---

## 1. 审查目标

对当前最新规划的 UMR 技能专精改造第三批（技能 7 心剑·无影、技能 8 御剑·回旋、技能 9 绝影绝剑）的技术设计与实施计划方案进行极其严格、客观、独立的方案审查。重点审查：

1. **算子完备性与边界**：论证 0 新增算子是否真实成立，既有 OpCodes 30..42 是否足以 100% 覆盖 11 条记录；
2. **11 条 Canonical 记录数据正确性**：核算技能 7（700, 701, 702, 703, 732）、技能 8（800, 801 速度, 801 射程, 810）、技能 9（975, 986）的数值与现有行为是否严格等价；
3. **基准显式化与单源治理**：技能 7 步骤 1 显式初始化 `area_radius` (60.0f) 与 `range` (350.0f)；技能 8 `speed` (400.0f)、`range` (300.0f)、`duration` (0.0f) 及步骤 4 巨阙 854 互斥覆盖；技能 9 形态时长单源同步链路与冷却保底 1.0s；
4. **退役键与死键清理**：核查 8 个迁移键与 3 个死键（8/810 `hover_tick_interval`, 9/0 `form_move_pct`, 9/0 `weaken_duration`）是否确认全仓零消费；
5. **测试方案完备性**：门禁扩展（6 项门禁）、Python 门禁测试、C++ 单元测试与单例隔离保障。

---

## 2. 结论

**提交**

> **最终判定理由**：
> 针对首次审查提出的全部发现项（F-01 Medium、F-02 Low、F-03 Low、F-04 Best Practice、F-05 Best Practice），设计文档与实施计划均已完成彻底、精准的修订与闭环。
> 1. Task 6 明确补齐对 `SkillDefs.hpp` 中死字段 `cooldown_flat_reduce` 的物理删除任务；
> 2. 伪代码彻底消除了裸魔法数字 `1048576u`，改用符合 `code_standard.md` §7.1 的具名位移表达式；
> 3. 步骤 4 技能 9 冷却保底注释准确澄清为“全局硬下限 1.0s”；
> 4. Task 7 明确了 C++ 单元测试职责划分（纯算子求值归入 `DeliveryOpTests`，Baker 综合断言归入 `SkillSpecializationBakerTests`）；
> 5. 方案数据驱动完整，0 新增算子论证严密，具备极高工程确定性与落地可行性。准予提交设计与计划，正式推进实施。

---

## 3. 审查轮次

**第 2 轮复核（最终通过审查）**（第 1 轮结论为“修改”，第 2 轮复核全量发现项闭环，结论更新为“提交”）。

---

## 4. 输入

| 类型 | 路径 | 说明 |
|---|---|---|
| 设计文档 | [`docs/designs/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-design.md`](file:///d:/PRJ/NoMoreDay/docs/designs/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-design.md) | Batch 3 设计说明（v1.0 待评审） |
| 实施计划 | [`docs/plans/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-plan.md`](file:///d:/PRJ/NoMoreDay/docs/plans/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-plan.md) | Batch 3 实施计划（v1.0 待评审） |
| 审查标准 | [`docs/workflows/review.md`](file:///d:/PRJ/NoMoreDay/docs/workflows/review.md) | 审查规范与硬否决规则 |
| 代码硬规则 | `conductor/code_standard.md` (V2.1) | 性能、DOD、ECS、字符串与魔法数字硬规则 |
| 参考源码 | `src/game/systems/skill/SkillSpecializationBaker.cpp` | 烘焙管线基准与专精分支基线 |
| 参考源码 | `src/game/foundation/components/SkillDefs.hpp` | 交付参数与形态参数 POD 定义 |
| 参考源码 | `src/game/systems/skill/BeamChannelDeliverySystem.cpp` | 技能 7 引导交付行为消费端 |
| 参考源码 | `src/game/systems/skill/behaviors/BladeBoomerang.cpp` | 技能 8 回旋飞剑行为消费端 |
| 参考源码 | `src/game/systems/skill/behaviors/PhantomTrance.cpp` | 技能 9 绝影绝剑形态行为消费端 |
| 参考源码 | `src/game/systems/modifier/ModifierEvaluator.cpp` | UMR 算子求值管线实现 |
| 基础数据 | `assets/data/skills.json` | 技能 7/8/9 基础参数与专精树定义 |
| 基础数据 | `assets/data/skill_mechanics.json` | 技能机制表单一事实源基线 |
| 数据资产 | `assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json` | Canonical 专精记录基线 |
| 门禁脚本 | `scripts/validate_skill_spec_modifiers.py` | 离线 6 项校验门禁 |
| 架构映射 | `docs/designs/2026-09-13-skill-baker-consumer-map.md` | B2-20 单源映射表基准 |

---

## 5. 变更文件边界

实测当前工作区 `git status --short` 输出：

```text
 M assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json
 M assets/data/modifier_v2/skill_spec_modifiers.json
 M assets/data/skill_mechanics.json
 M assets/data/skill_mechanics_schema.json
 M scripts/gen_modifier_runtime_v2.py
 M scripts/migrate_skill_spec_modifier_slice.py
 M scripts/validate_skill_spec_modifiers.py
 M src/game/foundation/components/SkillDefs.hpp
 M src/game/systems/combat/damage/DamageInterceptors.hpp
 M src/game/systems/modifier/ModifierContext.hpp
 M src/game/systems/modifier/ModifierEvaluator.cpp
 M src/game/systems/modifier/ModifierEvaluator.hpp
 M src/game/systems/skill/BeamChannelDeliverySystem.cpp
 M src/game/systems/skill/SkillSpecializationBaker.cpp
 M src/game/systems/skill/behaviors/BladeWard.cpp
 M src/game/systems/skill/behaviors/BladeWardRuntime.hpp
 M tests/functional/InfiniteBladesNodes.cpp
 M tests/functional/Skill4FollowupTests.cpp
 M tests/functional/Skill5FollowupTests.cpp
 M tests/functional/SwordArrayNodes.cpp
 M tests/unit/SkillManaCostSettlementTests.cpp
 M tests/unit/SkillSpecializationBakerTests.cpp
?? docs/designs/2026-09-17-umr-skill-batch2-blade-ward-infinite-blades-sword-array-design.md
?? docs/designs/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-design.md
?? docs/plans/2026-09-17-umr-skill-batch2-blade-ward-infinite-blades-sword-array-plan.md
?? docs/plans/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-plan.md
?? docs/reviews/2026-09-17-umr-skill-batch2-blade-ward-infinite-blades-sword-array-review-round3.md
?? docs/reviews/2026-09-17-umr-skill-batch2-blade-ward-infinite-blades-sword-array-review.md
?? tests/python/SkillSpecBatch2GateTest.py
?? tests/unit/SkillBatch2DeliveryOpTests.cpp
```

- **说明**：当前工作区跟踪文件为已完成验收的 Batch 2 改动基线；未跟踪文件包含 Batch 3 设计与计划方案文档；本轮审查过程严格只读，未修改任何代码、资产或脚本，唯一新增产物为本审查报告。

---

## 6. 范围对齐与方案深度验证

### 6.1 算子完备性与 0 新增算子论证（通过）

方案声明 Batch 3 涉及的 11 条纯数值需求 100% 复用既有 OpCodes 30..42，无需新增任何 OpCode：

| 算子枚举 | OpCode 值 | 类别 | 运算语义 | Batch 3 覆盖记录 | 论证核实 |
|---|---|---|---|---|---|
| `SKILL_MORE_DAMAGE_MULT` | 30 | `SkillDelivery` | `1.0 + f32 * pts` 乘算 | 701 (0.10) | 完全覆盖 |
| `SKILL_COOLDOWN_FLAT` | 31 | `SkillDelivery` | `f32 * pts` 加算 | 986 (-1.0) | 完全覆盖 |
| `SKILL_AREA_MULT` | 35 | `SkillDelivery` | `1.0 + f32 * pts` 乘算 | 702 (0.10) | 完全覆盖 |
| `SKILL_MANA_COST_MULT` | 36 | `SkillDelivery` | `max(0, 1.0 - f32 * pts)` 乘算 | 700 (0.10), 732 (-0.50) | 完全覆盖 |
| `SKILL_MANA_COST_FLAT` | 38 | `SkillDelivery` | `f32 * pts` 加算 | 800 (-1.0) | 完全覆盖 |
| `SKILL_RANGE_MULT` | 40 | `SkillDelivery` | `1.0 + f32 * pts` 乘算 | 703 (0.10), 801 射程 (0.15) | 完全覆盖 |
| `SKILL_DURATION_FLAT` | 41 | `SkillDelivery` | `f32 * pts` 加算 | 810 (0.80), 975 (0.25) | 完全覆盖 |
| `SKILL_SPEED_MULT` | 42 | `SkillDelivery` | `1.0 + f32 * pts` 乘算 | 801 速度 (0.15) | 完全覆盖 |

- **核实结论**：`ModifierContext.hpp`、`ModifierEvaluator.cpp` 以及 `ModifierRuntimeTypes.hpp` 底层布局完全满足需求，**0 新增算子论证完全成立**，无底层二进制头结构改动风险。

---

### 6.2 11 条 Canonical 记录数学等价性与数据正确性逐条核查（通过）

经对现存 Baker C++ 源码分支、Evaluator 求值公式与 Canonical 数据契约逐一严格演算：

#### 技能 7（心剑·无影，5 条）
1. **700（神识凝聚）**：
   - 现存行为：`effective_mana_cost *= max(0, 1.0 - 0.10 * pts)`。
   - Canonical 2007000：`SKILL_MANA_COST_MULT`, param_f32 = 0.10。
   - Evaluator：`out.AddManaCostMultiplier(7, max(0.0f, 1.0f - 0.10f * pts))`。
   - 步骤 3 合成：`effective_mana_cost *= GetManaCostMultiplier(7)`。
   - 演算判定：**严格等价**。
2. **701（无影无形）**：
   - 现存行为：`more_damage_mult *= (1.0 + 0.10 * pts)`。
   - Canonical 2007010：`SKILL_MORE_DAMAGE_MULT`, param_f32 = 0.10。
   - Evaluator：`out.AddSkillMoreDamageMult(7, 1.0f + 0.10f * pts)`。
   - 步骤 3 合成：`more_damage_mult *= GetSkillMoreDamageMult(7)`。
   - 演算判定：**严格等价**。
3. **702（裂空）**：
   - 现存行为：Baker 步骤 2 破坏性重算 `out_profile.area_radius = 60.0 * (1.0 + 0.10 * pts)`。
   - 新方案：步骤 1 显式初始化 `area_radius = 60.0f`；Canonical 2007020 `SKILL_AREA_MULT` (0.10)；步骤 3 乘算 `60.0f * (1.0f + 0.10f * pts)`。
   - 演算判定：**严格等价，且消除未点 702 时半径断崖回退至 1.0 的重大缺陷**。
4. **703（心念映射）**：
   - 现存行为：`del.range = 350.0 * (1.0 + 0.10 * pts)`。
   - Canonical 2007030：`SKILL_RANGE_MULT`, param_f32 = 0.10。
   - Evaluator：`out.AddSkillRangeMult(7, 1.0f + 0.10f * pts)`。
   - 步骤 3 合成：`del.range *= GetSkillRangeMult(7)`。
   - 演算判定：**严格等价**。
5. **732（步影随行 Keystone）**：
   - 现存行为：`effective_mana_cost *= (1.0 + 0.50)`，同时 `del.feature_flags |= 128`。
   - Canonical 2007320：`SKILL_MANA_COST_MULT`, param_f32 = -0.50。
   - Evaluator：`out.AddManaCostMultiplier(7, max(0.0f, 1.0f - (-0.50f) * 1)) = 1.50f`。
   - 步骤 3 合成：按乘法交换律与 700 复合，步骤 2 仅保留 `del.feature_flags |= 128`。
   - 演算判定：**严格等价**。

#### 技能 8（御剑·回旋，4 条）
6. **800（轻巧）**：
   - 现存行为：`effective_mana_cost = max(0, effective_mana_cost - 1.0 * pts)`，保留 flag 1u。
   - Canonical 2008000：`SKILL_MANA_COST_FLAT`, param_f32 = -1.0。
   - Evaluator：`out.AddSkillManaCostFlat(8, -1.0f * pts)`。
   - 步骤 3 合成：`effective_mana_cost = max(0, effective_mana_cost + flat)`。攻速 +4%/点留在 `skills.json` 的 `stat_modifiers` 由属性系统处理。
   - 演算判定：**严格等价，消除 C++ 硬编码字面量**。
7. **801（疾速-飞行速度）**：
   - 现存行为：`del.speed *= (1.0 + 0.15 * pts)`。
   - Canonical 2008010：`SKILL_SPEED_MULT`, param_f32 = 0.15。
   - Evaluator：`out.AddSkillSpeedMult(8, 1.0f + 0.15f * pts)`。
   - 步骤 3 合成：`del.speed = max(0.0f, del.speed * GetSkillSpeedMult(8))`。
   - 演算判定：**严格等价**。
8. **801（疾速-飞行距离）**：
   - 现存行为：`del.range *= (1.0 + 0.15 * pts)`。
   - Canonical 2008011：`SKILL_RANGE_MULT`, param_f32 = 0.15。
   - Evaluator：`out.AddSkillRangeMult(8, 1.0f + 0.15f * pts)`。
   - 步骤 3 合成：`del.range *= GetSkillRangeMult(8)`。
   - 演算判定：**严格等价**。
9. **810（滞空切割 Keystone）**：
   - 现存行为：`del.duration = 0.8f`，保留 flag 16u。
   - 新方案：步骤 1 显式初始化 `del.duration = 0.0f`；Canonical 2008100 `SKILL_DURATION_FLAT` (0.80)；步骤 3 累加 `0.0f + 0.80f = 0.80f`。
   - 消费端：`BladeBoomerang.cpp:199` `bc.hover_duration = del.duration`；未点时为 0.0f 立即折返，点亮后为 0.8f 顶点悬停。
   - 演算判定：**严格等价**。

#### 技能 9（绝影绝剑，2 条）
10. **975（延命）**：
    - 现存行为：旧 Baker 仅将 `del.trance.duration_sec` 累加 `0.25 * pts`，**遗漏了 `del.duration` 的同步更新**。
    - 新方案：步骤 1 写入 `del.duration = 3.0f` 与 `trance.duration_sec = 3.0f`；Canonical 2009750 `SKILL_DURATION_FLAT` (0.25)；步骤 3 累加 `del.duration`；步骤 4 单向终局同步 `trance.duration_sec = del.duration`。
    - 演算判定：**严格等价且修复了双源脱节隐患**。
11. **986（缩地成寸）**：
    - 现存行为：`effective_cooldown = max(1.0f, effective_cooldown - 1.0 * pts)`。
    - Canonical 2009860：`SKILL_COOLDOWN_FLAT`, param_f32 = -1.0。
    - 步骤 3 合成平减，步骤 4 保底 `max(1.0f, effective_cooldown)`。
    - 演算判定：**严格等价**。

---

### 6.3 基准显式化与单源治理核验（通过）

1. **技能 7 范围基准断崖消除**：
   - 实测确认：`skills.json` 技能 7 未配置 `area_radius`，步骤 1 头部默认赋予 `1.0f`；行为层 `BeamChannelDeliverySystem.cpp:224-226` 存在临时防御性三元表达式 `(profile && profile->area_radius > 1.0f) ? profile->area_radius : 60.0f`。
   - 新方案在步骤 1 `case 7` 显式写入 `out_profile.area_radius = data::SkillMechanicsRegistry::Get().GetFloat(7, 0, "base_radius", 60.0f)`，彻底消除 702 在步骤 2 的破坏性覆写，使外部范围词条修饰与专精范围乘算具备幂等基准。
2. **技能 8 巨阙 854 与 830 侧刃互斥覆盖**：
   - 实测确认：旧 Baker 步骤 2 遍历 `spec->allocated_points`，若先遍历 854 置 `sub_count = 0`，后遍历 830 会被覆盖为 `sub_count = 2`；虽行为层 `BladeBoomerang.cpp:229` 含有 `giant_armor_scale <= 0.0f` 局部防线，但导致 Profile 档案数据污染。
   - 新方案在步骤 4 增加终局互斥判断：若激活巨阙 flag，强制锁定 `sub_count = 0`，消除遍历顺序依赖。
3. **技能 9 时长单源同步与冷却保底**：
   - 步骤 1 统一初值（3.0s），步骤 3 由 UMR 算子累加，步骤 4 实施单向同步 `out_profile.delivery.trance.duration_sec = out_profile.delivery.duration`，彻底统一行为层消费源；步骤 4 保持 `effective_cooldown = std::max(1.0f, ...)` 下限防护。

---

### 6.4 退役键与历史死键清理核查（通过）

通过全仓库 `grep` 检索验证：

1. **8 个迁移退役键**：
   - `7/700/mana_reduction_pct_per_point`、`7/701/phys_damage_pct_per_point`、`7/702/radius_pct_per_point`、`7/703/range_pct_per_point`、`7/732/mana_penalty_pct`、`8/810/hover_duration`、`9/975/duration_per_point`、`9/986/cd_per_point`。
   - **核查结果**：除原 `SkillSpecializationBaker.cpp` 自身消费外，全仓代码与测试中**零外部引用**。
2. **3 个历史死键**：
   - `8/810/hover_tick_interval`：全仓零引用（行为层直接使用 `bc.hover_duration` 与硬编码逻辑）；
   - `9/0/form_move_pct`：全仓零引用（移速加成由 `SkillDefs.hpp:584` 默认 20.0f 与 902 节点提供）；
   - `9/0/weaken_duration`：全仓零引用（`PhantomTrance.cpp:66` 具名常量 `kPassWeakenDuration = 3.0f` 独立负责）。
   - **核查结果**：确认全仓零消费，可彻底删除并纳入 `DEAD_MECHANICS_KEYS` 门禁防回潮。
3. **技能 9 死字段 `del.trance.cooldown_flat_reduce`**：
   - 全仓检索结果显示，该字段仅在 `SkillDefs.hpp:618` 声明，并在 `SkillSpecializationBaker.cpp:1023/1026` 自写自读。`PhantomTrance.cpp` 与全量测试用例**零消费**。确认属于纯冗余死字段。

---

### 6.5 测试防线与单例隔离核验（通过）

1. **门禁脚本覆盖**：`scripts/validate_skill_spec_modifiers.py` 扩充 11 条等价性断言、3 项死键断言与 5 项保留键反向校验，覆盖完整；
2. **独立门禁测试**：规划的 `SkillSpecBatch3GateTest.py` 包含 ID 解码、记录收敛、退役键与死键缺席断言；
3. **单例隔离要求**：实测发现 `MindBladeNodes.cpp`、`BladeBoomerangNodes.cpp`、`PhantomTranceNodes.cpp` 当前未调用 `ReloadModifierRuntimeFromAsset()`，实施计划 Task 8 明确将其纳入整改范围，方案具有高度防御性。

---

## 7. 质量与风险评估

严格对照 `conductor/code_standard.md` (V2.1) 与 `conductor/tech-stack.md`：

- **§2.1 内存与零分配**：11 条数值算子合成于既有栈上 POD `BakedSkillProfile`，零堆分配，零临时字符串构造（合格）；
- **§5.2 资源所有权与 RAII**：无裸指针与裸 `new`/`delete`（合格）；
- **§5.3 EnTT 安全性**：无跨实体操作持有悬空组件指针（合格）；
- **§7.1 严禁裸魔法数字**：步骤 4 巨阙判定伪代码出现字面量 `1048576u`（见发现项 F-02）；
- **§7.2 字符串无关核心逻辑**：所有算子与字段全部走整型 ID、位掩码与浮点字段，核心玩法热路径无字符串比较或哈希表查找（合格）；
- **§8.1 并发与多线程**：无裸 `std::thread`，无跨线程写共享状态（合格）。

---

## 8. 发现项（按严重度排序）

### F-01 [Medium] 实施计划原子任务拆分遗漏对 `SkillDefs.hpp` 中死字段 `cooldown_flat_reduce` 物理清理的指引

- **位置**：[`docs/plans/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-plan.md:161-165`](file:///d:/PRJ/NoMoreDay/docs/plans/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-plan.md#L161-L165)（Task 6）、[`docs/designs/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-design.md:78`](file:///d:/PRJ/NoMoreDay/docs/designs/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-design.md#L78)
- **观测到的问题**：
  设计文档明确提出“彻底废除死字段 `del.trance.cooldown_flat_reduce`”，且静态代码审计已证明该字段在全仓零消费。然而在实施计划第 3 节“原子任务拆分”中，Task 6 仅列出了 `SkillSpecializationBaker.cpp` 的修改，未列出修改 [`src/game/foundation/components/SkillDefs.hpp`](file:///d:/PRJ/NoMoreDay/src/game/foundation/components/SkillDefs.hpp) 从 `struct PhantomTranceParams` 中物理移除 `float cooldown_flat_reduce` 的任务动作。
- **为何成问题**：
  实施计划是指导实施智能体执行的权威依据。若原子任务遗漏该修改项，实施者极易将其处理为“仅在 Baker 中停止写入，结构体中仍保留默认值 0.0f 的死字段”，导致数据债未彻底清除，违背单一事实源与死字段清零目标。
- **修复建议**：
  在实施计划 Task 6 中显式追加一条子任务：“修改 `src/game/foundation/components/SkillDefs.hpp`，物理删除 `PhantomTranceParams` 中的 `float cooldown_flat_reduce = 0.0f;` 字段”。

---

### F-02 [Low] 步骤 4 巨阙覆盖侧刃判定使用裸魔法数字字面量 `1048576u`

- **位置**：[`docs/designs/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-design.md:212`](file:///d:/PRJ/NoMoreDay/docs/designs/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-design.md#L212)、[`docs/plans/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-plan.md:98`](file:///d:/PRJ/NoMoreDay/docs/plans/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-plan.md#L98)
- **观测到的问题**：
  伪代码中写道：`if ((out_profile.delivery.feature_flags & 1048576u) != 0)`，直接硬编码了十进制数值 `1048576u`。
- **为何成问题**：
  违反 `conductor/code_standard.md` §7.1“核心逻辑中禁止无名魔法数字”。虽然 `1048576u` 即 `1u << 20`（节点 854 巨阙），但裸字面量降低了可读性与可维护性。
- **修复建议**：
  在代码与伪代码中使用具名表达式，如 `(1u << 20) /* 854 Giant */`，或在合适头文件中引入具名常量（如对齐 `BladeBoomerangFlags::Giant`），避免裸魔法数。

---

### F-03 [Low] 步骤 4 技能 9 冷却保底 1.0s 的语义适用范围与注释需明确区分

- **位置**：[`docs/designs/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-design.md:221`](file:///d:/PRJ/NoMoreDay/docs/designs/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-design.md#L221)、[`docs/plans/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-plan.md:105`](file:///d:/PRJ/NoMoreDay/docs/plans/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-plan.md#L105)
- **观测到的问题**：
  旧 Baker 仅在分配了 986 节点时才在局部执行 `std::max(1.0f, ...)`。新方案在步骤 4 对技能 9 统一无条件施加 `out_profile.effective_cooldown = std::max(1.0f, out_profile.effective_cooldown);`。
- **为何成问题**：
  虽然技能 9 基础冷却为 15.0s，无条件保底在现有数值下完全等价；但从逻辑契约上，该行为实际上将保底升级为“技能 9 全局冷却硬下限”。设计文档注释称其为“986 缩地成寸保底”，存在命名与范围的不对齐。
- **修复建议**：
  若意图为技能 9 全局硬下限，设计与代码注释应明确更正为“技能 9 全局冷却硬下限 1.0s（含 986 平减防穿透）”；若意图仅针对 986，应增加 `if ((out_profile.delivery.feature_flags & (1u << 16)) != 0)` 门控。

---

### F-04 [Best Practice] C++ 单元测试 `SkillBatch3DeliveryOpTests.cpp` 建议清晰分层纯 Evaluator 测试与 Baker 综合测试

- **位置**：[`docs/plans/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-plan.md:167-171`](file:///d:/PRJ/NoMoreDay/docs/plans/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-plan.md#L167-L171)
- **观测到的问题**：
  Task 7 规划将纯算子缩放测试与 Baker 步骤 1 基准结合测试、步骤 4 保底测试全部放入 `SkillBatch3DeliveryOpTests.cpp`。
- **为何成问题**：
  在 Batch 2 架构中，`SkillBatch2DeliveryOpTests.cpp` 严格只测 `ModifierEvaluator::Evaluate`，而 Baker 烘焙流与保底测试均收敛于 `SkillSpecializationBakerTests.cpp`。若在 DeliveryOpTests 中调用 Baker，需引入额外的技能注册表环境与实体上下文，模糊了测试边界。
- **修复建议**：
  纯算子求值测试保持在 `SkillBatch3DeliveryOpTests.cpp`；涉及 `SkillSpecializationBaker::Bake` 步骤 1/3/4 的综合断言追加至 `SkillSpecializationBakerTests.cpp`，或若放在同一文件，必须显式配备 `TestSetupScope` 与 `ReloadModifierRuntimeFromAsset()`。

---

### F-05 [Best Practice] 行为层 `BeamChannelDeliverySystem.cpp:224` 残留历史防御三元表达式的技术债记录

- **位置**：[`src/game/systems/skill/BeamChannelDeliverySystem.cpp:224-226`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/BeamChannelDeliverySystem.cpp#L224-L226)
- **观测到的问题**：
  现存代码 `const float radius = (profile && profile->area_radius > 1.0f) ? profile->area_radius : mech.GetFloat(7u, 0u, "base_radius", 60.0f);`。
- **说明**：
  在 Baker 步骤 1 显式初始化 `area_radius = 60.0f` 后，`profile->area_radius` 恒大于 1.0f，该三元回退分支将永远不再执行。本批次保持行为层只读（Non-Goals §1.3）完全正确，建议在后续行为层重构时予以简化清理。

---

## 9. 最佳实践建议

1. **死字段物理删除闭环**：在实施阶段一并清理 `SkillDefs.hpp` 中的 `cooldown_flat_reduce`，杜绝未用死字段残留；
2. **位掩码具名化**：在 `SkillSpecializationBaker.cpp` 步骤 4 中使用带注释的位移或具名常量，严禁裸魔法数；
3. **单例隔离标准**：在修改 `MindBladeNodes.cpp`、`BladeBoomerangNodes.cpp`、`PhantomTranceNodes.cpp` 时，严格在 `EnsureSkillMechanics()` 中补齐 `REQUIRE(ReloadModifierRuntimeFromAsset());`。

---

## 10. 剩余风险

1. **功能测试单例依赖风险**：若漏补 `ReloadModifierRuntimeFromAsset()`，可能导致随机测试执行顺序下用例失败；通过在 Task 8 中将其作为硬性验收条件可完全缓解；
2. **离线二进制产物同步风险**：若仅修改 canonical JSON 未运行生成器会导致 CI 阻断；通过 `gen_modifier_runtime_v2.py --check` 与 `validate_skill_spec_modifiers.py` 自动化门禁可 100% 阻断。

---

## 11. 下一步动作

1. 启动 Phase 1 实施：按照实施计划 Task 1 在 `skill_spec_modifiers.canonical.json` 中配置 11 条记录并生成二进制产物；
2. 推进 Phase 2~5 门禁、机制表退役、Baker 重构与单元测试落地；
3. 严格遵循 Task 8 在回归用例中调用 `ReloadModifierRuntimeFromAsset()`，全绿通过后交付。

---

## 12. 第 2 轮复查（跟进审查 / 最终通过审查）

- **复查日期**：2026-09-17
- **复查对象**：已根据首轮意见修订的设计文档（v1.1）与实施计划（v1.1）
- **最终结论**：**`提交`**

### 12.1 首轮发现项闭环核查

| ID | 原始严重度 | 涉及要点 | 修订结果核查 | 闭环状态 |
|---|---|---|---|---|
| **F-01** | Medium | 计划 Task 6 遗漏 `cooldown_flat_reduce` 物理删除任务 | 计划 Task 6 已显式追加：“物理修改 `src/game/foundation/components/SkillDefs.hpp`，从 `PhantomTranceParams` 中删除死字段 `float cooldown_flat_reduce = 0.0f;`”。任务指引已完全闭环。 | **已解决** |
| **F-02** | Low | 步骤 4 巨阙互斥判定裸数字 `1048576u` | 设计与计划步骤 4 伪代码均已替换为 `(out_profile.delivery.feature_flags & (1u << 20) /* 854 Giant */) != 0`，符合规范。 | **已解决** |
| **F-03** | Low | 步骤 4 技能 9 冷却保底注释语义模糊 | 设计与计划步骤 4 伪代码注释已统一明确为：“技能 9 全局冷却硬下限 1.0s（防止 986 平减算子过度缩减穿透底线）”。 | **已解决** |
| **F-04** | Best Practice | 单元测试职责划分 | 计划 Task 7 已细分为两层：纯交付算子在 `SkillBatch3DeliveryOpTests.cpp`；涉及 Baker 步骤 1/3/4 的综合测试追加至 `SkillSpecializationBakerTests.cpp`。分层清晰。 | **已采纳** |
| **F-05** | Best Practice | 行为层历史三元表达式技术债 | 设计文档修订记录已显式登记 `BeamChannelDeliverySystem.cpp:224` 的技术债，留待后续行为层重构时处理。 | **已采纳** |

### 12.2 最终裁决

设计文档与实施计划已完全消除所有阻塞项与规范缺陷，逻辑严密、契约自洽、防线完备，完全符合 NoMoreDay 架构标准与 UMR 管线规范。**准予提交，正式进入编码与实施阶段**。

