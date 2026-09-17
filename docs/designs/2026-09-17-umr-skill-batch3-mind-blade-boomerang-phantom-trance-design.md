# UMR 技能专精改造第三批（技能 7 心剑·无影、技能 8 御剑·回旋、技能 9 绝影绝剑）设计说明

- **文档状态**：已批准待实施（v1.2：依实施后独立审查报告做文档对齐与门禁补强）
- **文档路径**：`docs/designs/2026-09-17-umr-skill-batch3-mind-blade-boomerang-phantom-trance-design.md`
- **设计日期**：2026-09-17
- **系统代号**：`UMR-SKILL-BATCH-3` (Unified Modifier Runtime - Skill Batch 3)
- **修订记录**：
  - v1.0：初稿（11 条 Canonical 记录 + 0 新增算子论证 + 8 退役键与 3 死键清理）。
  - v1.1：响应首轮独立审查报告全面修订：
    - 明确步骤 4 巨阙判定消除裸数字，改用具名位移 `(1u << 20) /* 854 Giant */`（解 F-02）；
    - 澄清步骤 4 技能 9 冷却保底为“全局硬下限 1.0s（含 986 平减防穿透）”（解 F-03）；
    - 强化声明物理修改 `SkillDefs.hpp` 彻底剔除 `PhantomTranceParams::cooldown_flat_reduce` 死字段（解 F-01）；
    - 登记 `BeamChannelDeliverySystem.cpp:224` 历史三元回退表达式技术债（采纳 F-05）。
  - v1.2：依第三批实现后独立审查报告做文档对齐（G-02/G-03/G-04）与门禁补强（G-01/G-06/G-07/G-08）。
  - v1.3：依外部审查反馈将技能 9 冷却硬下限从步骤 4 后移至步骤 5 装备平减折叠之后（R-01），并令 Python 门禁测试在 `.bin` 产物缺失时跳过而非误报失败（R-02）。
- **输入来源**：
  - `设计文档/统一修饰器运行时系统_UMR.md`
  - `设计文档/职业设计草案_剑修.md`（§3.7 心剑·无影、§3.8 御剑·回旋、§3.9 绝影绝剑）
  - `docs/designs/2026-09-13-skill-baker-consumer-map.md`（B2-20 烘焙→消费单源映射表）
  - `docs/designs/2026-09-16-umr-skill-batch1-rending-wave-blade-formation-design.md`（Batch 1 批次基线）
  - `docs/designs/2026-09-17-umr-skill-batch2-blade-ward-infinite-blades-sword-array-design.md`（Batch 2 批次基线）
  - 代码实况：
    - [`src/game/systems/skill/SkillSpecializationBaker.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSpecializationBaker.cpp)
    - [`src/game/foundation/components/SkillDefs.hpp`](file:///d:/PRJ/NoMoreDay/src/game/foundation/components/SkillDefs.hpp)
    - [`src/game/systems/skill/BeamChannelDeliverySystem.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/BeamChannelDeliverySystem.cpp)
    - [`src/game/systems/skill/behaviors/BladeBoomerang.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/BladeBoomerang.cpp)
    - [`src/game/systems/skill/behaviors/PhantomTrance.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/PhantomTrance.cpp)
    - [`assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json`](file:///d:/PRJ/NoMoreDay/assets/data/modifier_v2/canonical/skill_spec_modifiers.canonical.json)
    - [`assets/data/skill_mechanics.json`](file:///d:/PRJ/NoMoreDay/assets/data/skill_mechanics.json)
    - [`scripts/validate_skill_spec_modifiers.py`](file:///d:/PRJ/NoMoreDay/scripts/validate_skill_spec_modifiers.py)

---

## 1. 背景与目标

### 1.1 背景与现状
在顺利完成 Batch 1（技能 2 裂空斩、技能 3 灵剑决）与 Batch 2（技能 4 剑气护体、技能 5 万剑归宗、技能 6 剑阵·诛仙）的 UMR 交付算子并轨后，基础算子体系（OpCodes 30..42）已覆盖增伤、冷却增减、充能、暴击率、范围缩放、法耗平减/折扣、弹道追加、暴伤、射程缩放、持续时间平加以及弹速乘算。

然而，在技能 7（心剑·无影）、技能 8（御剑·回旋）与技能 9（绝影绝剑）的现存专精代码与数据配置中，依然存在显著的架构双源、字面量硬编码与数据债：

1. **技能 7（心剑·无影）烘焙层重算与范围基准隐式依赖**：
   - [`SkillSpecializationBaker.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSpecializationBaker.cpp) `case 7` 仅显式初始化了 `del.range` 与 `del.sub_interval`，未显式赋予 `out_profile.area_radius` 确定性初值（仅依赖 skills.json 默认 1.0f）；
   - 节点 702（裂空）在 Baker 内部通过 `mech.GetFloat(7, 0, "base_radius", 60.0f)` 重新读取基准并进行手写乘算覆写，破坏了“步骤 1 初始化基准、步骤 3 一次性合成”的幂等性；
   - 节点 700（引导减耗）、701（物理增伤）、703（射程缩放）、732（步影随行增耗惩罚）均为纯正的线性或阶梯数值修正，目前仍停留在 `ApplyNodeModifiersToProfile` 中手写 C++ 分支并查阅 `skill_mechanics.json`。

2. **技能 8（御剑·回旋）Baker 大量字面量硬编码与孤立参数**：
   - Baker `case 8` 中，节点 800（轻巧，法力平减 -1.0/点）、节点 801（疾速，速度与最远距离同比例 +15%/点）全部采用 C++ 字面量计算，在 `skill_mechanics.json` 中甚至未登记对应机制键（为典型的无配置硬编码）；
   - 节点 810（滞空切割）作为核心滞空参数，在 Baker 中直接将 `del.duration` 赋值为 `hover_duration = 0.8f`；同时机制表中残留未被引用的历史死键 `8/810/hover_tick_interval`；
   - 技能 8 的多数专有参数（`return_damage_mult`、`side_angle_mult`、`pull_radius_mult` 等）在 `BakedDeliveryParams` 中存在专用字段，需要与通用 UMR 交付参数界定清晰的系统边界。

3. **技能 9（绝影绝剑）双重参数镜像与死字段残留**：
   - 绝影形态参数统一由结构体 `PhantomTranceParams` 承载，但形态时长存在双源隐患：步骤 1 既写入 `del.duration` 又镜像写入 `del.trance.duration_sec`；节点 975（延命）在步骤 2 中仅修改了 `del.trance.duration_sec`，导致 `del.duration` 未能同步延长；
   - 节点 986（缩地成寸，冷却平减 -1.0s/点）在 Baker 中将平减值存入 `del.trance.cooldown_flat_reduce` 并手写改写 `out_profile.effective_cooldown`，然而根据 `docs/designs/2026-09-13-skill-baker-consumer-map.md` §5-D5 验证，`trance.cooldown_flat_reduce` 在生产代码中**全仓零消费**，属于纯冗余死字段；
   - 机制表 `skill_mechanics.json` 技能 9 节点 0 中沉淀了历史死键 `form_move_pct` 与 `weaken_duration`（全仓零引用，代码中已硬编码具名常量）。

### 1.2 核心目标（Goals）

1. **0 新增算子、100% 复用既有 OpCodes 30..42（P0）**：
   - 经全面梳理，技能 7、8、9 的所有纯数值专精需求完全落在既有算子空间内（`SKILL_MORE_DAMAGE_MULT: 30`, `SKILL_COOLDOWN_FLAT: 31`, `SKILL_AREA_MULT: 35`, `SKILL_MANA_COST_MULT: 36`, `SKILL_MANA_COST_FLAT: 38`, `SKILL_RANGE_MULT: 40`, `SKILL_DURATION_FLAT: 41`, `SKILL_SPEED_MULT: 42`）；
   - **无需扩充任何新 OpCode、无需修改底层二进制头结构**，最大化保障运行时稳定性。

2. **规范化接入 11 条 Canonical 专精记录（P0）**：
   - **技能 7（5 条）**：
     - 700（神识凝聚）：`SKILL_MANA_COST_MULT`（法耗 -10%/点，param_f32: 0.10）；
     - 701（无影无形）：`SKILL_MORE_DAMAGE_MULT`（物理增伤 +10%/点，param_f32: 0.10）；
     - 702（裂空）：`SKILL_AREA_MULT`（基础范围半径 +10%/点，param_f32: 0.10）；
     - 703（心念映射）：`SKILL_RANGE_MULT`（追踪范围 +10%/点，param_f32: 0.10）；
     - 732（步影随行 Keystone）：`SKILL_MANA_COST_MULT`（引导法耗惩罚 +50%，param_f32: -0.50）。
   - **技能 8（4 条）**：
     - 800（轻巧）：`SKILL_MANA_COST_FLAT`（基础法力消耗平减 -1.0/点，param_f32: -1.0）；
     - 801（疾速-飞行速度）：`SKILL_SPEED_MULT`（飞行速度 +15%/点，param_f32: 0.15）；
     - 801（疾速-飞行距离）：`SKILL_RANGE_MULT`（最远距离 +15%/点，param_f32: 0.15）；
     - 810（滞空切割 Keystone）：`SKILL_DURATION_FLAT`（滞空时长 0.8s，param_f32: 0.80）。
   - **技能 9（2 条）**：
     - 975（延命）：`SKILL_DURATION_FLAT`（绝影形态时长 +0.25s/点，param_f32: 0.25）；
     - 986（缩地成寸）：`SKILL_COOLDOWN_FLAT`（基础冷却平减 -1.0s/点，param_f32: -1.0）。

3. **彻底退役 8 项机制键并清理 3 项历史死键（P0）**：
   - 退役上述数值在 [`assets/data/skill_mechanics.json`](file:///d:/PRJ/NoMoreDay/assets/data/skill_mechanics.json) 中的 8 个键：`7/700/mana_reduction_pct_per_point`、`7/701/phys_damage_pct_per_point`、`7/702/radius_pct_per_point`、`7/703/range_pct_per_point`、`7/732/mana_penalty_pct`、`8/810/hover_duration`、`9/975/duration_per_point`、`9/986/cd_per_point`；
   - 彻底删除 3 项经静态代码证明零引用的历史死键：`8/810/hover_tick_interval`、`9/0/form_move_pct`、`9/0/weaken_duration`；
   - 登记独立死键门禁（`DEAD_MECHANICS_KEYS`），断言上述 3 项死键永不回潮。

4. **烘焙基准显式化与单源终局同步（P0）**：
   - 技能 7 在 Baker 步骤 1 显式初始化 `out_profile.area_radius = data::SkillMechanicsRegistry::Get().GetFloat(7, 0, "base_radius", 60.0f)`，使 702 的 `SKILL_AREA_MULT` 具备确定性基准，杜绝步骤 2 破坏性重写；
   - 技能 8 在步骤 1 明确 `del.duration = 0.0f`（未点 810 立即折返），810 经 `SKILL_DURATION_FLAT` 自然累加为 0.8s；
   - 技能 9 在步骤 4 实施单源同步：`del.trance.duration_sec = del.duration`，统一两字段数据源；冷却硬下限 `out_profile.effective_cooldown = std::max(1.0f, out_profile.effective_cooldown)` 于步骤 4 之后的装备折叠收尾处统一施加，并废除死字段 `del.trance.cooldown_flat_reduce`。

5. **离线门禁与全量测试闭环（P0）**：
   - 扩充 [`scripts/validate_skill_spec_modifiers.py`](file:///d:/PRJ/NoMoreDay/scripts/validate_skill_spec_modifiers.py) 的 6 项门禁，覆盖 11 条新增记录、等价性断言与死键隔离；
   - 新增 `tests/python/SkillSpecBatch3GateTest.py` 与 `tests/unit/SkillBatch3DeliveryOpTests.cpp`，保持全仓 100% 构建与回归测试全绿。

### 1.3 明确非目标（Non-Goals）

1. **非目标**：不将复杂形态、机制分支与触发逻辑迁入 UMR：
   - 技能 7：710 引导叠层、711 碎空爆引爆机制、712 虚无牵引、714 寂灭触发器、715 中心暴击判定、730/731 多开撕裂、734 瞬移、750 牵引、751 破甲、753 剑意消耗、754 剑意穿刺协同、755 击杀回蓝、770/772 冰雷转质、775 抗性上限压制；
   - 技能 8：802 附加点伤、811 放血异常、812 流血条件暴伤、813 剑鸣破甲、814 拔血协同、815 风眼治疗、830 侧翼飞剑生成与角度、832 接剑回蓝、833 连击攻速 buff、834 剑步延长、850 磁力场牵引、854 巨阙护甲转化与硬直、870/872 路径元素转质、871 路径增伤、873 高压电弧、874 尾迹几何缩放、875 路径穿透、876 路径护体；
   - 技能 9：902 形态移速闪避 buff、913 穿行虚弱、914 每秒回蓝/减伤、934 攻速 buff、935 逆命触发、954 时光倒流冷却返还、955 其他技能减耗、972/989 冰雷转质、973 过载电弧与雷盾、974 恢复加速、976 气旋爆发、977 免死持有、978 浴血重生、979 绝影护盾、980 虚灵潜行、981 逆脉锁血、982 残血暴伤、983 死亡螺旋、984 嗜血回复、985 瞬移、987 剑意自回、988 剑步衰减、990 冰缓增幅、991 意念穿透、992 附魔刷新、993 影剑回响；
   上述逻辑继续由 C++ 机制层（`BeamChannelDeliverySystem`、`BladeBoomerang`、`BoomerangDeliverySystem`、`PhantomTrance`）与 `feature_flags` 承载。
2. **非目标**：不将技能 9 形态内专用 buff 包（`PhantomTranceParams`）泛化为通用 UMR 算子，防止 OpCode 体系恶性膨胀；
3. **非目标**：本批次不推进技能 10~12 的改造，维持严格的 3 技能批次边界。

---

## 2. 架构拓扑与职责边界

```text
┌─────────────────────────────────────────────────────────────────────────────┐
│                 SpecializedSkill 专精加点 (allocated_points)                 │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │
                 ┌─────────────────────┴─────────────────────┐
                 ▼                                           ▼
    【UMR 纯数值交付层】 (数据驱动)                 【SpecState / 机制形态层】 (代码驱动)
    - SKILL_MORE_DAMAGE_MULT (30)               - 元素转质 (770/772, 870/872, 972/989)
    - SKILL_COOLDOWN_FLAT    (31)               - 触发契约 (TriggerRule: 714, 814, 935)
    - SKILL_AREA_MULT        (35)               - 机制标志位 (711爆, 850磁, 854巨, 980虚, 981逆)
    - SKILL_MANA_COST_MULT   (36)               - 回旋弹道三阶段管理 (BoomerangComponent)
    - SKILL_MANA_COST_FLAT   (38)               - 元素路径系统 (ElementPathSystem)
    - SKILL_RANGE_MULT       (40)               - 绝影形态核心状态机 (PhantomTranceComponent)
    - SKILL_DURATION_FLAT    (41)               - 巨阙禁用侧刃覆盖 (854 sub_count=0 步骤4)
    - SKILL_SPEED_MULT       (42)               - 死亡螺旋/穿行/免死/附魔穿透逻辑
                 │                                           │
                 └─────────────────────┬─────────────────────┘
                                       │ 汇流 (SkillSpecializationBaker::Bake)
                                       ▼
                      【BakedSkillProfile / BakedDeliveryParams】
                                       │
        ┌──────────────────────────────┼──────────────────────────────┐
        ▼                              ▼                              ▼
【BeamChannelDeliverySystem】   【BladeBoomerang / Boomerang】  【PhantomTrance / SkillSystem】
- 读 profile->area_radius       - 读 profile->effective_mana    - 读 profile->effective_cooldown
- 读 delivery.range             - 读 delivery.speed             - 读 delivery.duration
- 读 profile->more_damage_mult  - 读 delivery.range             - 读 delivery.trance.duration_sec
- 读 profile->effective_mana    - 读 delivery.duration (810)    - 读 delivery.trance.* (形态包)
```

---

## 3. 详细技术设计

### 3.1 交付算子复用性分析（0 新增算子论证）

Batch 3 所需的所有交付修饰算子均已在 Batch 1 与 Batch 2 中完成定义与工程落地：

| 算子枚举 | OpCode 值 | 类别 | 参数语义 | Batch 3 使用点 |
|---|---|---|---|---|
| `SKILL_MORE_DAMAGE_MULT` | 30 | `SkillDelivery` | `param_u32`: skill_id; `param_f32`: 每点 More 增伤倍率偏移 | 技能 7: 701 (+10%) |
| `SKILL_COOLDOWN_FLAT` | 31 | `SkillDelivery` | `param_u32`: skill_id; `param_f32`: 每点冷却绝对秒数偏移（负值为平减） | 技能 9: 986 (-1.0s) |
| `SKILL_AREA_MULT` | 35 | `SkillDelivery` | `param_u32`: skill_id; `param_f32`: 每点范围半径倍率偏移 | 技能 7: 702 (+10%) |
| `SKILL_MANA_COST_MULT` | 36 | `SkillDelivery` | `param_u32`: skill_id; `param_f32`: 每点法耗折扣倍率偏移（负值为惩罚） | 技能 7: 700 (+10%), 732 (-50%) |
| `SKILL_MANA_COST_FLAT` | 38 | `SkillDelivery` | `param_u32`: skill_id; `param_f32`: 每点法耗绝对数值偏移（负值为平减） | 技能 8: 800 (-1.0) |
| `SKILL_RANGE_MULT` | 40 | `SkillDelivery` | `param_u32`: skill_id; `param_f32`: 每点射程/最远距离倍率偏移 | 技能 7: 703 (+10%); 技能 8: 801 (+15%) |
| `SKILL_DURATION_FLAT` | 41 | `SkillDelivery` | `param_u32`: skill_id; `param_f32`: 每点持续时间增减秒数 | 技能 8: 810 (+0.8s); 技能 9: 975 (+0.25s) |
| `SKILL_SPEED_MULT` | 42 | `SkillDelivery` | `param_u32`: skill_id; `param_f32`: 每点弹速/飞行速度倍率偏移 | 技能 8: 801 (+15%) |

无需扩充 `ModifierOpCode` 枚举，亦无需在 `ModifierDelta` 中新增容器。

### 3.2 烘焙管线（Baking Pipeline）强化与基准显式化

在 [`SkillSpecializationBaker.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSpecializationBaker.cpp) 的六步烘焙流中，针对技能 7、8、9 进行重构：

#### 步骤 1：基础参数显式初始化（消除隐式依赖）

```cpp
case 7: // 心剑·无影
  del.sub_interval = 0.3f;
  // 射程基准外置于技能级键 base_range (350.0f)
  del.range = data::SkillMechanicsRegistry::Get().GetFloat(7, 0, "base_range", 350.0f);
  // 【新增显式基准】范围半径基准外置于技能级键 base_radius (60.0f)，消除 702 手写覆盖与隐式 1.0 依赖
  out_profile.area_radius = data::SkillMechanicsRegistry::Get().GetFloat(7, 0, "base_radius", 60.0f);
  // 持续引导基准每秒法耗 15.0f 已由步骤 1 头部统一写入
  break;

case 8: // 御剑·回旋
  del.speed = skillData->GetParam("speed", 400.0f);
  del.range = skillData->GetParam("max_distance", 300.0f);
  del.duration = 0.0f; // 滞空时长默认为 0（未点 810 立即折返），810 通过 UMR 算子加算
  break;

case 9: // 绝影绝剑：突进形态
  del.speed = skillData->GetParam("dash_speed", 600.0f);
  del.duration = data::SkillMechanicsRegistry::Get().GetFloat(9, 0, "form_duration", 3.0f);
  del.trance.duration_sec = del.duration;
  // 基础冷却 15.0f 与基础法耗 30.0f 已由步骤 1 头部从 skillData 统一写入
  break;
```

#### 步骤 2：专精节点分支清理

清理 [`ApplyNodeModifiersToProfile`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSpecializationBaker.cpp) 中已转由 UMR 承载的纯数值计算：
- **技能 7**：移除 700（`mana_reduction_pct_per_point`）、701（`phys_damage_pct_per_point`）、702（`radius_pct_per_point`）、703（`range_pct_per_point`）以及 732 中的 `mana_penalty_pct` 计算（732 仅保留 `del.feature_flags |= 128`）；
- **技能 8**：移除 800（`effective_mana_cost -= 1.0f * points`）、801（`del.speed *= mult; del.range *= mult`）、810（`del.duration = hover_duration`）的手写计算，仅保留必要的机制 flag（810 的 `del.feature_flags |= 16u`）；
- **技能 9**：移除 975（`del.trance.duration_sec` 累加）与 986（`cooldown_flat_reduce` 与 `effective_cooldown` 改写），保留 975/986 的 feature_flags。

#### 步骤 3：UMR 交付增量一次性合成

复用既有合成序列（涵盖法耗平减/乘算、冷却平减/乘算、充能、投射物、More 增伤、暴击、暴伤、范围、射程、弹速与持续时间）：
```cpp
// 既有通用合成逻辑自动生效，完全覆盖技能 7、8、9 的 11 条增量
out_profile.effective_mana_cost = std::max(
    0.0f,
    (out_profile.effective_mana_cost + specDelta.GetSkillManaCostFlat(skill_id))
        * specDelta.GetManaCostMultiplier(skill_id));
out_profile.effective_cooldown = std::max(
    0.0f, (out_profile.effective_cooldown + specDelta.GetSkillCooldownFlat(skill_id))
        * specDelta.GetSkillCooldownMult(skill_id));
out_profile.more_damage_mult *= specDelta.GetSkillMoreDamageMult(skill_id);
out_profile.area_radius *= specDelta.GetSkillAreaMult(skill_id);
del.range *= specDelta.GetSkillRangeMult(skill_id);
del.speed = std::max(0.0f, del.speed * specDelta.GetSkillSpeedMult(skill_id));
del.duration = std::max(0.0f, del.duration + specDelta.GetSkillDurationFlat(skill_id));
```

#### 步骤 4：确定性终局覆盖与形态单源同步

置于普通节点与 UMR 增量之后，消除遍历顺序依赖：

```cpp
if (skill_id == 8) {
  // 854 巨阙与 830 侧刃互斥：巨阙禁用侧刃，锁定 sub_count = 0（消除裸魔法数字）
  if ((out_profile.delivery.feature_flags & (1u << 20) /* 854 Giant */) != 0) {
    out_profile.delivery.sub_count = 0;
  }
} else if (skill_id == 9) {
  // 975 延命单源同步：将步骤 3 合成完成的 del.duration 单向同步至 trance.duration_sec，
  // 彻底消除 del.duration 与 del.trance.duration_sec 双重计算与漂移
  out_profile.delivery.trance.duration_sec = out_profile.delivery.duration;
}
```

#### 步骤 5：装备折叠后的终局硬下限

装备专精修饰器在 `Bake` 步骤 5 折叠，其 `flat_cooldown_delta` 仅以 `std::max(0.0f, ...)` 收口
（`SkillSpecializationBaker.cpp` 步骤 5）。若把技能 9 的 1.0s 硬下限置于步骤 4，装备平减会在之后
再次把冷却压到 1.0s 以下，使“全局硬下限”名不副实。因此该下限必须置于装备折叠之后、`Bake` 返回之前：

```cpp
// 步骤 5 装备折叠结束后（Bake 收尾）
if (skill_id == 9) {
  // 技能 9 全局冷却硬下限 1.0s：晚于装备平减折叠，防止 986/装备穿透底线
  out_profile.effective_cooldown = std::max(1.0f, out_profile.effective_cooldown);
}
```

---

## 4. 节点数据配置契约与单一事实源治理

### 4.1 记录 ID 规则与通用头约束
严格遵循 UMR 规范：`modifier_id = 2,000,000 + node_id * 10 + op_index`。
- `priority`: `200`
- `profession_mask`: `1` (BladeAscendant)
- `weapon_class_mask`: `65535`
- `equip_slot_mask`: `0`
- `exclusive_group`: `0`
- `max_active`: `0`
- `debug_source`: `"skill_spec_node"`
- `min_player_level`: `1`
- 契约约束：`record.stacks == runtime.param_u32 == skill_id`，`record.stat_path == runtime.target`。

### 4.2 详细 Canonical 记录对照表（11 条）

| 记录 ID | 技能 | 节点 | OpCode | param_u32 (`stacks`) | param_f32 (`value`) | Canonical stat_path | operation | debug_name |
|---|---|---|---|---|---|---|---|---|
| `2007000` | 7 (心剑) | 700 (神识凝聚) | `SKILL_MANA_COST_MULT` | 7 | `0.10` | `skill.mana_cost` | `mul` | `MindBlade_Node700_ManaCost` |
| `2007010` | 7 (心剑) | 701 (无影无形) | `SKILL_MORE_DAMAGE_MULT` | 7 | `0.10` | `skill.more_damage` | `mul` | `MindBlade_Node701_MoreDamage` |
| `2007020` | 7 (心剑) | 702 (裂空) | `SKILL_AREA_MULT` | 7 | `0.10` | `skill.area_mult` | `mul` | `MindBlade_Node702_AreaMult` |
| `2007030` | 7 (心剑) | 703 (心念映射) | `SKILL_RANGE_MULT` | 7 | `0.10` | `skill.range_mult` | `mul` | `MindBlade_Node703_RangeMult` |
| `2007320` | 7 (心剑) | 732 (步影随行) | `SKILL_MANA_COST_MULT` | 7 | `-0.50` | `skill.mana_cost` | `mul` | `MindBlade_Node732_ManaPenalty` |
| `2008000` | 8 (回旋) | 800 (轻巧) | `SKILL_MANA_COST_FLAT` | 8 | `-1.00` | `skill.mana_cost_flat` | `add` | `BladeBoomerang_Node800_ManaCostFlat` |
| `2008010` | 8 (回旋) | 801 (疾速) | `SKILL_SPEED_MULT` | 8 | `0.15` | `skill.speed_mult` | `mul` | `BladeBoomerang_Node801_SpeedMult` |
| `2008011` | 8 (回旋) | 801 (疾速) | `SKILL_RANGE_MULT` | 8 | `0.15` | `skill.range_mult` | `mul` | `BladeBoomerang_Node801_RangeMult` |
| `2008100` | 8 (回旋) | 810 (滞空切割) | `SKILL_DURATION_FLAT` | 8 | `0.80` | `skill.duration_flat` | `add` | `BladeBoomerang_Node810_HoverDuration` |
| `2009750` | 9 (绝影) | 975 (延命) | `SKILL_DURATION_FLAT` | 9 | `0.25` | `skill.duration_flat` | `add` | `PhantomTrance_Node975_DurationFlat` |
| `2009860` | 9 (绝影) | 986 (缩地成寸) | `SKILL_COOLDOWN_FLAT` | 9 | `-1.00` | `skill.cooldown_flat` | `add` | `PhantomTrance_Node986_CooldownFlat` |

### 4.3 机制表（`skill_mechanics.json`）退役清单与死键清理（11 项）

| 技能/节点 | mechanics 键名 | 迁移前用途 | 处置与唯一权威源 |
|---|---|---|---|
| 7/700 | `mana_reduction_pct_per_point` | Baker case 7 直接乘算 | **删除**（唯一源：canonical `2007000`） |
| 7/701 | `phys_damage_pct_per_point` | Baker case 7 直接乘算 | **删除**（唯一源：canonical `2007010`） |
| 7/702 | `radius_pct_per_point` | Baker case 7 直接覆写 | **删除**（唯一源：canonical `2007020`，步骤 1 取 `base_radius`） |
| 7/703 | `range_pct_per_point` | Baker case 7 直接覆写 | **删除**（唯一源：canonical `2007030`，步骤 1 取 `base_range`） |
| 7/732 | `mana_penalty_pct` | Baker case 7 直接乘算 | **删除**（唯一源：canonical `2007320`） |
| 8/810 | `hover_duration` | Baker case 8 直接赋值 | **删除**（唯一源：canonical `2008100`） |
| 8/810 | `hover_tick_interval` | 历史死键（全仓 0 引用） | **删除**（独立死键防回潮门禁） |
| 9/0 | `form_move_pct` | 历史死键（全仓 0 引用） | **删除**（独立死键防回潮门禁） |
| 9/0 | `weaken_duration` | 历史死键（全仓 0 引用） | **删除**（独立死键防回潮门禁） |
| 9/975 | `duration_per_point` | Baker case 9 直接赋值 | **删除**（唯一源：canonical `2009750`） |
| 9/986 | `cd_per_point` | Baker case 9 直接赋值 | **删除**（唯一源：canonical `2009860`） |

> **注**：技能 8 节点 800 与 801 在原 `skill_mechanics.json` 中本未建键（为 Baker 历史硬编码字面量），本次迁移通过创建 canonical `2008000`、`2008010` 与 `2008011` 实现数据驱动归正，无需在 mechanics 中执行删除。

### 4.4 保留键（`KEPT_MECHANICS_KEYS`）清单与理由说明

已迁移节点下仍作为单一事实源保留的机制键：
- `(7, 0, "base_radius")`: 技能 7 步骤 1 范围半径基准初值（60.0f）；
- `(7, 0, "base_range")`: 技能 7 步骤 1 追踪射程基准初值（350.0f）；
- `(7, 0, "mana_cost_per_sec")`: 技能 7 步骤 1 引导法耗基准（15.0f）；
- `(7, 732, "move_speed_scale")`: 步影随行 Keystone 微步移动速度系数（0.30f），由输入系统在移动中直接消费；
- `(9, 0, "form_duration")`: 技能 9 步骤 1 绝影形态基础持续时长（3.0f）。

> **注**：上述 5 项中，实测仅 `(7, 732, "move_speed_scale")` 是 `check_registry_reverse` 反向登记的必需项（缺省会报 `unregistered mechanics key under migrated node`）；其余 4 项位于非迁移节点（7/0、9/0），对反向登记无告警消除作用，登记目的是将这些仍被 `src/` 消费的单一事实源键显式声明为保留键。

---

## 5. 离线校验与自动化测试防线

### 5.1 门禁扩展（`scripts/validate_skill_spec_modifiers.py`）
1. **MIGRATION_EQUIVALENCE 扩展**：追加上述 11 条记录，严格标定转换关系（700/701/702/703/810/975 为 raw，732/986 为 flat_negate，800/801 为 literal）；
2. **DEAD_MECHANICS_KEYS 扩展**：追加 `(8, 810, "hover_tick_interval")`、`(9, 0, "form_move_pct")`、`(9, 0, "weaken_duration")`，断言不得回潮；
3. **KEPT_MECHANICS_KEYS 扩展**：登记上述 5 项基准保留键，保障反向登记无告警。

### 5.2 独立门禁测试（`tests/python/SkillSpecBatch3GateTest.py`）
创建独立的 Python 门禁单元测试，断言：
- Batch 3 11 条记录已正确纳入 canonical 并在 index 中检出；
- 记录 ID 与 node_id_whitelist[0] 符合 `(id - 2_000_000) / 10` 解码规则；
- 8 项退役键与 3 项死键在 `skill_mechanics.json` 中完全不存在；
- 生成运行时产物校验 `gen_modifier_runtime_v2.py --check` 与 `validate_skill_spec_modifiers.py` 退出码恒为 0。

### 5.3 C++ 单元测试（`tests/unit/SkillBatch3DeliveryOpTests.cpp`）
- 测试 `SKILL_MORE_DAMAGE_MULT` 在技能 7 上的线性放大；
- 测试 `SKILL_AREA_MULT` 与 `SKILL_RANGE_MULT` 结合技能 7 步骤 1 基准的求值；
- 测试 `SKILL_MANA_COST_FLAT` 在技能 8 上的负向平减与下限 0 保护；
- 测试 `SKILL_SPEED_MULT` 与 `SKILL_RANGE_MULT` 在技能 8 上的复合缩放；
- 测试 `SKILL_DURATION_FLAT` 在技能 8（滞空 0.8s）与技能 9（形态 0.25s）上的累加；
- 测试 `SKILL_COOLDOWN_FLAT` 在技能 9 上的平减与保底 1.0s 终局覆盖。

### 5.4 功能测试回归与隔离防线
- 在 `tests/functional/MindBladeNodes.cpp`、`BladeBoomerangNodes.cpp`、`PhantomTranceNodes.cpp` 用例初始化时，显式保证 `ReloadModifierRuntimeFromAsset()` 调用，杜绝进程级单例在随机测试顺序下的状态污染；
- 确保既有 100% 断言无一被弱化或删除。

---

## 6. 风险评估与缓解策略

| 风险项 | 严重度 | 潜在影响 | 缓解与防范措施 |
|---|---|---|---|
| 技能 7 引导法耗基准双源 | Medium | 若未在步骤 1 取 `mana_cost_per_sec`，技能 7 法耗会错误回退至 0.0 | 步骤 1 显式继承既有 `7/0/mana_cost_per_sec` 提取逻辑，且单元测试断言 700 减耗生效 |
| 技能 7 范围半径基准不幂等 | Medium | 702 若直接乘以 60.0 会抹除装备范围修饰 | 在步骤 1 将 60.0 赋予 `out_profile.area_radius`，步骤 3 统一由乘法算子缩放 |
| 技能 8 巨阙与回旋侧刃顺序依赖 | High | 若遍历先巨阙后侧刃，可能导致侧刃未被清空 | 步骤 4 终局覆盖强制检查 854 flag 并置 `sub_count = 0` |
| 技能 9 延命双字段未同步 | High | 若只改 `del.duration`，`PhantomTrance` 读取 `trance.duration_sec` 会导致延命失效 | 步骤 4 强制执行单向赋值 `trance.duration_sec = duration` |
| 技能 9 冷却缩减穿透下限 | Medium | 986 点满或装备平减可能使冷却低于 1s 导致高频连放 | 步骤 5 装备平减折叠之后统一施加 `std::max(1.0f, out_profile.effective_cooldown)` |
| 历史死键回潮风险 | Low | 误将 `weaken_duration` 等键加回配置表 | 门禁 `DEAD_MECHANICS_KEYS` 严格防护并在 CI 中阻断 |

---

## 7. 验收标准与退出准则（DoD）

1. **配置与契约**：
   - 11 条 Canonical 记录已写入 `skill_spec_modifiers.canonical.json`；
   - 8 个迁移键与 3 个死键从 `skill_mechanics.json` 中彻底移除；
2. **离线门禁**：
   - `python scripts/validate_skill_spec_modifiers.py` 退出码 0，全量断言通过；
   - `python tests/python/SkillSpecBatch3GateTest.py` 退出码 0；
   - `python scripts/gen_modifier_runtime_v2.py --check` 产物同步一致；
3. **编译构建**：
   - `build.bat RelWithDebInfo` 编译成功，0 错误、0 新增警告；
4. **测试套件**：
   - `tests/unit/SkillBatch3DeliveryOpTests.cpp` 全绿；
   - 全量回归测试（包含 `MindBladeNodes`、`BladeBoomerangNodes`、`PhantomTranceNodes`）全部通过，既有用例无任何弱化或旁路。
