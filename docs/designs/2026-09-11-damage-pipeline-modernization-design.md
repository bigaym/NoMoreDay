# 伤害管线现代化重构设计（Damage Pipeline Modernization）

- 状态：评审修订完成（已合并架构审计与用户裁决）
- 日期：2026-09-11
- 上游基线：
  - `设计文档/战斗系统与属性设计.md`（Combat vNext 合同）
  - `设计文档/职业被动和技能设置.md` §5–§6（标签体系、7 步伤害执行）
  - `设计文档/统一修饰器运行时系统_UMR.md`（修饰器统一执行层）
  - `docs/designs/2026-09-06-modular-skill-archetypes-and-specialization-design.md`（专精烘焙与交付原型）
  - `docs/plans/2026-03-03-combat-core-vnext-design.md`（CombatV2 分层合同）
  - `docs/reviews/2026-09-11-damage-pipeline-modernization-design-review.md`（设计方案架构审计报告）
- 后续流程：根据本设计更新实施计划 `docs/plans/2026-09-11-damage-pipeline-modernization-plan.md`，进入实现与测试。

---

## 0. 结论摘要

| # | 决策 | 收益 |
|---|------|------|
| 1 | 所有伤害请求（hit / DoT / 触发 / 召唤 / 环境 / 荆棘 / 批量）收敛到唯一执行内核，彻底删除 CombatV2 候选桩（CandidateOnly）与 `×1.05` 旁路 | 修复普攻、DoT、召唤、荆棘等全量修饰被旁路的线上事实偏差，消除双实现漂移 |
| 2 | 伤害结算重构为「三层流水线架构」：Pre-Mitigation 业务拦截 ➔ 纯数学计算内核（DamageKernel） ➔ Post-Mitigation 业务拦截与落地 | 职责完全解耦；无敌/高闪避提前退出节省算力；Blade Ward 等业务机制不侵入数学内核 |
| 3 | 攻守状态正交解耦：攻方轻量快照（`AttackerSnapshot`）+ 守方实时状态掩码（`TargetConditionMask`）快速求值 | 解决冻结/命印/流血等守方动态加成与静态模板的冲突；AoE 场景攻方单次构建、守方极速求值 |
| 4 | 实体生命周期绑定：废弃全局动态 LRU 缓存，投射物/DoT 实体通过挂载快照组件（`DamageSnapshotComponent`）直存 | 彻底消除实体 ID 复用导致的幽灵缓存命中，实现天生无锁并发安全（Thread-Safe by Design） |
| 5 | 快照式转换规范（恐怖黎明规则）：Convert 单源封顶 100% 比例缩放，GainExtra 纯百分比不封顶，强制剥离源元素标签（Tag Stripping） | 语义无环、顺序无关、可单测穷举；标签剥离彻底杜绝转换后伤害享受源类型乘区的双重收益 |
| 6 | 引入 `DamageOrigin` 强类型来源分类，建立武器与技能基础点伤的唯一注入规则 | 彻底根除 `skill_id=0` 以及次级伤害（分裂/DoT/反伤/环境）导致的武器伤害双重叠加污染 |
| 7 | 重入安全体系：快照值拷贝传参 + 线程局部重入守卫 + 延迟副作用队列（Deferred Combat Actions） | 彻底根除反击、触发施法、死亡回调导致的 EnTT 组件池扩容与指针悬挂崩溃（UAF） |
| 8 | 单批并轨与两段式编排：攻方阶段统一计算一次，守方减伤单目标走标量内联，N≥4 保留 `xsimd` 批量向量化 | 兼顾单目标极低调用开销与 200 目标大规模 AoE 的高指令级吞吐 |

本设计不改变 `DamagePipeline::Calculate/Execute` 的对外核心契约与事件一致性要求；改变的是内部执行模型、攻守解耦体系、修饰来源通道与转换语义。

---

## 1. 背景与问题定义

### 1.1 直接触发

9 个技能的专精树已按模块化方案重构，但伤害结算本身仍停留在旧结构：非模拟、`base_pool` 非空的请求会被 `DamagePipeline.cpp:511-538` 拦截并路由到 `CombatV2RuntimeFacade`，而该 facade 的候选实现只是 `sum(base_pool) × 1.05f`（`src/game/foundation/combat_v2/CombatV2RuntimeFacade.cpp:26-31`），且 `Execute` 第一行 `(void)registry;`——不读任何实体、属性、词缀、天赋、buff、抗性。

结果：普攻、荆棘反伤、怪物普攻、异常 DoT、地面陷阱、召唤近战等以 `base_pool` 提交的伤害，实际只得到「基础池求和 ×1.05」，全部乘区与减伤被旁路；测试 `tests/integration/CombatV2CutoverTests.cpp:116-142` 已把 `100 → 105` 这一行为固化为契约。技能伤害（payload 路径）则依赖各行为层「故意留空 base_pool」绕开候选运行时（`ProjectileSystem.cpp:122-127`），契约靠注释而非类型保证。

### 1.2 结构性问题与架构审计发现

| 编号 | 问题 | 表现与危害 | 证据位置 |
|------|------|------------|----------|
| P1 | 三条路径并存且口径漂移 | payload 完整路径 / base_pool 候选桩 / simulation 与 batch 旧内联；模拟与实战严重漂移 | `DamagePipeline.cpp:519-538`；`CombatSystem.hpp:10-12` |
| P2 | 静态物化与守方动态条件冲突 | 冻结增伤、命印叠层、血量阈值暴击等强依赖守方状态，静态预编译将导致跨目标错误或快照失效 | `DamagePipeline.cpp:926-979, 1098-1171` |
| P3 | 转换级联、标签继承与双重收益 | 迭代转换链存在级联；转换后保留源元素 Tag 导致火焰伤害再次享受物理 More 乘区；双权威配置导致 UI 与底层脱节 | `DamagePipeline.cpp:745-862`；技能2 审查报告 C6 |
| P4 | 武器伤害双算与请求缺乏分类 | `skill_id=0` 回退默认普攻带 1.0 武器加成；次级打击若带 `skill_id` 会在 `base_pool` 外重复累加武器均伤 | `SkillRegistry.cpp:752-773`；`DamagePipeline.cpp:701-731` |
| P5 | 重入导致 EnTT 指针失效 (UAF) | 反击（Blade Ward 470）在结算中途直接同步调用 `ResolveDamage`，内层修改组件池导致外层指针悬挂崩溃 | `DamagePipeline.cpp:1324, 1795` |
| P6 | 实体生命周期与缓存竞争 | `source_entity` 作为缓存键在实体销毁复用后引发幽灵命中；全局 LRU 缓存并发读写存在写竞争与锁瓶颈 | `DamagePipeline.cpp:1437, 1711-1720` |
| P7 | 业务机制硬编码侵入核心管线 | Blade Ward 拦截投射物、扣减剑数、伤害清零、触发反击强行塞在伤害结算主函数内 | `DamagePipeline.cpp:1269-1329` |
| P8 | 批量 SIMD 向量化退化风险 | 单批若强行并轨为逐个标量循环，200 目标 AoE 性能将严重退化，无法发挥 `xsimd` 优势 | `DamagePipeline.cpp:1486-1708` |
| P9 | 单位协议混乱与死通道 | 暴击 0–1 与 0–100 混用（批量路径 `/100`）；死通道未清理 | `Stats.hpp:102-111`；`DamagePipeline.cpp:1007-1010` |

---

## 2. 目标与非目标

### 2.1 目标

- **G1 高性能**：单目标命中结算开销降至最低；AoE 200 目标批量维持指令级 SIMD 高吞吐；核心数学内核 0 堆分配、0 锁、0 注册表查询。
- **G2 高稳定性与内存安全**：消除组件指针跨阶段悬挂；反击延迟派发杜绝重入 UAF；消除实体 ID 复用导致的缓存幽灵命中。
- **G3 模块化与三层解耦**：前置拦截、纯数学计算内核、后置业务处理清晰正交；新增技能机制或防御机制不污染数学内核。
- **G4 严密语义与唯一权威**：恐怖黎明式快照转换，Convert 封顶且按比例分配，GainExtra 纯百分比叠加，严格剥离源元素标签；统一 `DamageOrigin` 根除双算。
- **G5 来源全覆盖与可测试性**：人物属性、装备、符文、天赋、专精、buff 统一接入；数学内核作为纯函数支持 100% 确定性单测与回放。

### 2.2 非目标

- 不做数值平衡调整；桩移除与双算修复带来的伤害变化属缺陷修复，平衡调整在实现完成后统一进行（2026-09-11 用户裁决）。
- 不引入独立的转换效率字段（用户裁决：仅保留转换百分比）。
- 不改存档格式、不改 UI、不新增第三方依赖。
- 不重做专精树已有内容节点，只保证其数值经统一规范生效。

---

## 3. 核心设计决策

### D1 唯一权威执行路径与候选桩移除
所有伤害——hit / DoT / 触发 / 召唤 / 环境 / 荆棘 / 批量——必须走统一的管线流程。
- 彻底删除 `DamagePipeline.cpp:511-538` 的候选运行时拦截与 `CombatV2RuntimeFacade` 桩接线；
- `base_pool` 是输入数据池，而非跳过计算的开关；
- `calculateBatch` 钩子必须真实返回计算结果，禁止返回空 vector。

### D2 快照式转换与源标签强制剥离规范
- **Convert（转换）**：单源类型总转换量超 100% 时封顶 100%，多目标按比例缩放；源类型点数扣减；
- **GainExtra（额外获得）**：纯百分比叠加，不设封顶；不扣减源类型点数；
- **无级联与顺序无关**：所有转换规则均只读取转换前初始快照（$S[t]$），互转不产生环，结果与遍历顺序无关；
- **源标签强制剥离（Tag Stripping）**：转换或额外获得的产物，必须剥离所有旧伤害类型标签（`kAllDamageTypeTags`），仅赋予目标类型标签，保留动作/形态标签（Melee, Hit, Projectile, Area 等）。

### D3 乘区顺序裁决
沿用「转换在 Increased/More 之前」，与设计规范及 7 步执行模型一致：
$$\text{Base} \longrightarrow \text{Convert/GainExtra} \longrightarrow \text{Increased} \longrightarrow \text{More} \longrightarrow \text{Crit} \longrightarrow \text{Mitigation} \longrightarrow \text{Taken}$$
转换产物按最终元素类型接受后续 Increased 和 More 的加成，彻底避免源类型乘区的双重受益。

### D4 攻守状态解耦（攻方轻量快照 + 守方条件掩码）
- **攻方快照（`AttackerSnapshot`）**：在施法时刻或近战出手时刻提取，包含点伤、转换比例、无条件 Increased、无条件 More、基础暴击、以及极少数动态条件规则（`ConditionalDamageOp`）；
- **守方条件掩码（`TargetConditionMask`）**：在命中守方时极速提取（位运算检查 Freeze, Bleed, HighHp, Stun, FateMark 等）；
- **动态合并**：在执行 More 与 Crit 阶段时，仅针对匹配守方条件的规则进行轻量加权，既实现攻方数据在 AoE 中的单次复用，又精准响应守方动态状态。

### D5 实体生命周期绑定（废弃全局 LRU 缓存）
- 废弃基于 `(attacker, skill_id, source_entity, version_hash)` 的全局动态 LRU 缓存，消除实体销毁后 ID 复用导致的幽灵命中；
- **投射物 / DoT 实体**：在生成（Spawn/Cast）瞬间，将编译好的 `AttackerSnapshot` 以 `DamageSnapshotComponent` 直接挂载在实体自身；命中时 0 查表、0 哈希计算直接读取；
- **近战 / 普攻**：在栈上直接即时构建快照并计算（耗时数十纳秒，远快于全局哈希表查找与版本比对）；
- 消除多线程 Taskflow 并行段下的任何缓存写竞争与锁同步。

### D6 引入 `DamageOrigin` 强类型来源枚举
在 `DamageRequest` 中明确伤害来源分类，彻底闭环武器基础伤害与技能基础伤害的注入边界：
- `DirectSkillCast`：唯有此类请求允许注入 `(weapon_avg * weapon_damage_mult) + skill_base_damage`；
- `SecondaryProc`（分裂/弹射）、`AilmentTick`（DoT）、`HazardEnvironment`（环境）、`ThornsReflect`（荆棘）、`ItemAffixProc`（装备特效）：一律**仅以 `base_pool` 为基础点伤**，绝不附加武器与技能配置点伤。

### D7 重入安全与防御性隔离
- 严禁在计算核心内部跨阶段持久持有 `registry.try_get` 返回的组件裸指针；
- 计算核心只接收栈上的值拷贝快照（`AttackerSnapshot`, `DefenseSnapshot`）；
- 引入线程局部重入深度限制（`kMaxReentrancyDepth = 2`）；
- 业务副作用（Blade Ward 反击、受击施法）压入延迟队列（`DeferredActionQueue`），待最外层 Apply 与事件收尾后统一安全派发，防止组件池扩容引发 UAF。

### D8 三层流水线架构与业务解耦
- **Layer 1: Pre-Mitigation Interceptors**：守方无敌检查（直接退出）、闪避掷骰（若闪避直接退出）；
- **Layer 2: DamageKernel**：纯数学计算函数（`noexcept`），执行转换、增伤、暴击、抗性、护甲、全局 DR 减伤；
- **Layer 3: Post-Mitigation Interceptors & Apply**：Blade Ward 投射物拦截（消耗剑与伤害清零）、反击入队、生命扣除与一致性事件派发。

### D9 单批两段式并轨与 SIMD 性能保留
- 攻方构建阶段统一计算一次，产出攻击就绪池（`ReadyDamagePool`）；
- 守方减伤阶段自适应分流：$N=1$ 走标量快速内联函数；$N \ge 4$ 使用 `xsimd` 进行 4/8 宽度向量化并行减伤；
- 两者共用底层数学公式，既杜绝双轨逻辑漂移，又保持大规模 AoE 的 SIMD 吞吐。

---

## 4. 目标架构

### 4.1 分层架构图

```text
               ┌────────────────────────────────────────────────────────┐
               │                  DamagePipeline (Facade)               │
               └───────────────────────────┬────────────────────────────┘
                                           │
 ┌─────────────────────────────────────────▼────────────────────────────────────────┐
 │ 【Layer 1: Pre-Mitigation Interceptors】                                          │
 │  1. 守方无敌 (InvulnerableComponent) ──────────▶ 伤害归零，立即返回 (Early-Out)     │
 │  2. 命中与闪避掷骰 (Dodge Roll) ────────────────▶ 若闪避，立即返回 (Early-Out)     │
 │  3. 射程/距离修正 (SuppressorComponent)                                           │
 │  4. 提取守方快照 (DefenseSnapshot) 与条件掩码 (TargetConditionMask)                │
 └─────────────────────────────────────────┬────────────────────────────────────────┘
                                           │ (只读快照传递，无 ECS 指针依赖)
 ┌─────────────────────────────────────────▼────────────────────────────────────────┐
 │ 【Layer 2: DamageKernel (纯数学核心，noexcept，0 堆分配，0 锁)】                   │
 │  1. Build: 按 DamageOrigin 规范注入 base_pool 与武器伤害                           │
 │  2. Convert/Gain: 快照比例缩放，纯百分比，强制源标签剥离 (Tag Stripping)             │
 │  3. Increased/More: 基础乘区 + 守方条件掩码快速匹配动态乘区                         │
 │  4. Crit Resolution: 暴击判定 / 期望倍率                                          │
 │  5. Mitigation: 抗性穿透、护甲减伤、全局 DR (单目标标量 / 批量 SIMD 自适应)          │
 │  6. Finalize: 产出 DamageResult (各元素数值、暴击标志、总伤害)                     │
 └─────────────────────────────────────────┬────────────────────────────────────────┘
                                           │
 ┌─────────────────────────────────────────▼────────────────────────────────────────┐
 │ 【Layer 3: Post-Mitigation Interceptors & Application】                          │
 │  1. 拦截判定 (Blade Ward Interception) ───────▶ 掷骰成功则消耗飞剑并将总伤害清零   │
 │  2. 反击判定 (Blade Ward Counter) ────────────▶ 生成反击请求，压入延迟队列         │
 │  3. CombatSystem::ApplyDamage ────────────────▶ 扣除护盾、生命，判定死亡          │
 │  4. 一致性战斗事件派发 (CombatEventDispatcher) ──▶ reported 与 final_applied 一致  │
 │  5. 弹出并执行延迟动作队列 (Deferred Combat Actions) ──▶ 安全执行反击/二次触发      │
 └──────────────────────────────────────────────────────────────────────────────────┘
```

### 4.2 核心数据结构

```cpp
// 1. 伤害来源强类型分类
enum class DamageOrigin : uint8_t {
    DirectSkillCast,   // 技能直接施法 (允许注入武器与技能配置点伤)
    SecondaryProc,     // 衍生次级打击 (分裂、爆炸、弹射，仅用 base_pool)
    AilmentTick,       // 异常状态 DoT (仅用 base_pool，不暴击)
    HazardEnvironment, // 地面环境/陷阱 (仅用 base_pool)
    ThornsReflect,     // 荆棘反伤 (仅用 base_pool)
    ItemAffixProc      // 装备特效触发 (仅用 base_pool)
};

// 2. 守方实时条件位掩码
enum class TargetCondition : uint32_t {
    None         = 0,
    Frozen       = 1 << 0,  // 处于冻结
    Bleeding     = 1 << 1,  // 处于流血
    HpAbove80    = 1 << 2,  // 生命值 > 80%
    Controlled   = 1 << 3,  // 受控 (眩晕/定身/减速)
    HasFateMark  = 1 << 4,  // 拥有命印
    HasQiBrand   = 1 << 5,  // 拥有气印
};

// 3. 动态条件修饰项 (轻量 POD)
struct ConditionalDamageOp {
    TargetCondition required_condition;
    OpStage stage;         // More, CritDamage
    float base_value;      // 基础增量 (如 +0.50f)
    float per_stack_value; // 叠层增量 (如 +0.03f/层)
    BuffKind stack_source; // 叠层关联类别
};

// 4. 转换规则条目
struct ConversionRule {
    DamageType src_type;
    DamageType dst_type;
    float pct;             // 转换比例 (0.0 ~ 1.0)
    bool is_gain_extra;    // true 为 GainExtra, false 为 Convert
};

// 5. 攻方紧凑快照 (约 160 字节，可高速值拷贝)
struct alignas(32) AttackerSnapshot {
    std::array<float, 6> base_flat = {0.0f};
    std::array<float, 6> increased = {0.0f};
    std::array<float, 6> unconditional_more = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    float base_crit_chance = 0.05f;
    float base_crit_multiplier = 1.50f;
    float armor_pen = 0.0f;
    float accuracy = 0.97f;
    Tag combined_tags = Tag::None;
    float added_effectiveness = 1.0f;
    float trigger_effectiveness = 1.0f;

    FixedVector<ConversionRule, 8> conversions;
    FixedVector<ConditionalDamageOp, 4> conditional_ops;
};

// 6. 守方紧凑快照
struct alignas(32) DefenseSnapshot {
    std::array<float, 6> resistances = {0.0f};
    float armor = 0.0f;
    float dodge_chance = 0.0f;
    float block_chance = 0.0f;
    float block_multiplier = 1.0f;
    float global_dr = 0.0f;
    int cached_area_level = 1;
    uint32_t condition_mask = 0; // TargetCondition 位掩码
};

// 7. 实体快照组件 (用于投射物/DoT，绑定生命周期)
struct DamageSnapshotComponent {
    AttackerSnapshot snapshot;
    DamageOrigin origin;
    uint32_t skill_id;
    entt::entity caster;
};
```

### 4.3 阶段执行规范

| 阶段 | 归属 | 关键逻辑与要点 | 提前返回/异常处理 |
|------|------|----------------|-------------------|
| **S0** | Facade | **无敌与预检**：检查守方 `InvulnerableComponent`；若无敌直接返回 0 | 无敌直接返回 |
| **S1** | Facade | **闪避掷骰**：根据命中/闪避公式掷骰；若判定闪避直接返回 `was_dodged=true` | 闪避直接返回 |
| **S2** | Facade | **快照准备**：若 `source_entity` 挂有 `DamageSnapshotComponent` 则直接取用；否则从攻方提取快照；同时提取守方 `DefenseSnapshot` 与条件掩码 | 实体无效安全降级 |
| **S3** | Kernel | **Build（点伤构建）**：始终载入 `base_pool`；仅当 `origin == DirectSkillCast` 时附加武器与技能配置点伤 | 基础点伤 clamp ≥ 0 |
| **S4** | Kernel | **Convert / GainExtra**：执行 §4.4 快照式转换，Convert 封顶且按比例缩放，Gain 纯百分比；强制剥离源元素标签 | 无级联，守恒保证 |
| **S5** | Kernel | **Increased**：各元素类型独立应用 `× (1 + increased[t])` | — |
| **S6** | Kernel | **More 乘区合并**：`unconditional_more[t]` 与匹配守方条件的 `conditional_ops` 动态乘算；终局 More 乘算 | 负值截断 |
| **S7** | Kernel | **Crit 结算**：根据基础暴击与动态暴击条件，模拟取期望 `1 + P*(m-1)` 或实战掷骰 | 分数制，边界保护 |
| **S8** | Kernel | **Mitigation（减伤）**：抗性（含穿透/暴露）、护甲公式、全局 DR；单目标标量内联，批量走 SIMD | 公式统一来源于 `CombatFormula` |
| **S9** | Kernel | **Finalize**：聚合各元素数值填入 `DamageResult.final_pool`，计算 `total_damage` | NaN/Inf 检查，下限 0 |
| **S10**| Facade | **后置业务拦截**：Blade Ward 判定拦截（伤害清零并减剑）；Blade Ward 反击判定（压入延迟队列） | 业务状态与数学解耦 |
| **S11**| Facade | **落地与事件**：调用 `CombatSystem::ApplyDamage`，派发一致性战斗事件；弹出并执行延迟动作 | 重入深度限制 |

---

## 4.4 转换算法规范（核心数学与标签规则）

### 定义
- 初始快照 $S[t]$：Build 阶段结束后各元素类型的基础点数，$t \in [0, 5]$；
- Convert 规则集 $C_{src} = \{r \mid r.src == src\}$；GainExtra 规则集 $G_{src} = \{r \mid r.src == src\}$；
- 所有百分比 $pct$ 均为归一化分数制（$0.0 \sim 1.0$），不设额外转换效率字段。

### 算法描述（单遍、快照、无级联、标签剥离）

```cpp
// 1. 初始化输出池与标签掩码
DamagePool output_pool;
std::array<float, 6> out_values = {0.0f};

// 2. 转换与保留计算
for (int src = 0; src < 6; ++src) {
    float initial_amount = S[src];
    if (initial_amount <= 0.0f) continue;

    float sum_convert_pct = 0.0f;
    for (const auto& r : C[src]) {
        sum_convert_pct += r.pct;
    }

    // A. 计算源属性未转换保留比例 (封顶 100%)
    float retained_pct = (std::max)(0.0f, 1.0f - sum_convert_pct);
    out_values[src] += initial_amount * retained_pct;

    // B. 计算转换转出比例 (超过 100% 则按比例归一化缩放)
    float scale = (sum_convert_pct > 1.0f) ? (1.0f / sum_convert_pct) : 1.0f;
    for (const auto& r : C[src]) {
        float converted_amount = initial_amount * (r.pct * scale);
        out_values[r.dst] += converted_amount;
    }

    // C. 额外获得 (GainExtra，纯百分比叠加，不扣源属性，不封顶)
    for (const auto& r : G[src]) {
        float gained_amount = initial_amount * r.pct;
        out_values[r.dst] += gained_amount;
    }
}
```

### 标签继承与剥离规则（Tag Stripping）
当生成目标类型的伤害实例或进行后续乘区匹配时，必须通过以下规范处理标签：

```cpp
// 元素类型标签掩码集合（0~15 位）
static constexpr Tag kAllDamageTypeTags = 
    Tag::Physical | Tag::Fire | Tag::Cold | Tag::Lightning | Tag::Shadow | Tag::Poison | Tag::Void;

// 强制剥离所有伤害类型标签，赋予唯一目标类型标签
inline Tag TransformTagsOnConversion(Tag original_tags, DamageType target_type) {
    Tag non_elemental_tags = static_cast<Tag>(
        static_cast<uint64_t>(original_tags) & ~static_cast<uint64_t>(kAllDamageTypeTags)
    );
    Tag target_element_tag = static_cast<Tag>(1ULL << static_cast<uint8_t>(target_type));
    return non_elemental_tags | target_element_tag;
}
```

---

## 4.5 防御、拦截与重入安全体系

### 4.5.1 拦截器（Interceptors）与业务机制隔离
- **无敌（Invulnerable）**：在管线入口（Pre-Interceptor）立即判定，若目标无敌直接返回 0，阻断后续全部计算开销。
- **剑阵拦截（Blade Ward Interception）**：位于 Post-Mitigation 阶段。仅当命中带有 `Tag::Projectile` 且守方持有飞剑时掷骰判定；若成功拦截，执行 `ward->sword_count--`，并将 `total_damage` 与 `final_pool` 全部清零。

### 4.5.2 延迟动作队列与重入防范
反击（Blade Ward 470）及受击触发施法不得在结算中途同步调用 `ResolveDamage`。

```cpp
void DamagePipeline::QueueDeferredAction(DeferredCombatAction action) {
    if (s_reentrancy_depth >= kMaxReentrancyDepth) {
        LOG_WARN("Combat: Maximum re-entrancy depth reached. Dropping action.");
        return;
    }
    s_deferred_actions.push_back(std::move(action));
}

// 在 Execute 尾部统一弹出执行：
if (s_reentrancy_depth == 0) {
    while (!s_deferred_actions.empty()) {
        auto action = s_deferred_actions.pop_back();
        // 此刻当前伤害结算与 Apply 已完全结束，栈帧干净，安全执行重入
        ExecuteDeferredAction(registry, action);
    }
}
```

---

## 5. 性能设计与批量 SIMD 策略

### 5.1 指标与预算

| 指标 | 目标 | 达成手段 |
|------|------|----------|
| 单目标命中延迟 | ≤ 基线 P95 | 标量快速内联，0 堆分配，栈上轻量快照，0 动态哈希计算 |
| AoE 200 目标批量吞吐 | 优于现有实现基线 | 攻方单次构建，守方保留 `xsimd` 向量化并行减伤 |
| 热路径内存分配 | 0 次 / 命中 | 栈上紧凑 POD，容器固定容量 `FixedVector` |
| 并发数据竞争 | 0 锁，0 争用 | 废弃全局 LRU，投射物挂组件直存快照，天生线程安全 |

### 5.2 两段式自适应编排（Two-Phase Mitigation）

```text
               ┌────────────────────────────────────────────────────────┐
               │    Phase 1: Attacker Build & Output Pool Resolution    │
               │           (攻方阶段：无论目标数多寡，全量仅计算 1 次)          │
               └───────────────────────────┬────────────────────────────┘
                                           │ 产出 ReadyDamagePool (6 元素点数)
                                           ▼
               ┌────────────────────────────────────────────────────────┐
               │     Phase 2: Defender Mitigation Dispatch (自适应分流)   │
               └───────────────┬────────────────────────┬───────────────┘
                               │                        │
                     [Defenders.size() < 4]   [Defenders.size() >= 4]
                               │                        │
                               ▼                        ▼
                   【标量快速路径 Scalar】        【SIMD 向量化并行路径】
                   直接逐目标内联减伤计算          4/8 宽度 xsimd 连续向量处理
```

- **统一底层公式**：标量与 SIMD 减伤函数严格调用 [`CombatFormula.hpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/combat/CombatFormula.hpp) 的内联无状态公式，杜绝两套计算口径。

---

## 6. 影响面与公共接口

### 6.1 公共接口变更

```cpp
// DamageRequest 扩展 DamageOrigin
struct DamageRequest {
    DamageOrigin origin = DamageOrigin::DirectSkillCast; // 来源强类型
    entt::entity attacker = entt::null;
    entt::entity defender = entt::null;
    uint32_t skill_id = 0;
    DamagePool base_pool;
    float added_effectiveness = 1.0f;
    float trigger_effectiveness = 1.0f;
    Tag additional_tags = Tag::None;
    entt::entity source_entity = entt::null;
    bool is_simulation = false;
    bool dispatch_damage_events = true;
    bool skip_mitigation = false;
    bool thorns_like_damage = false;
    std::optional<DamagePayloadContext> payload_context;
};
```
- `DamageResult` 与 `DamageExecutionResult` 保持二进制兼容；
- `DamagePipeline::Calculate` 与 `Execute` 签名保持不变；
- 移除 `CombatV2RuntimeFacade` 相关调用。

### 6.2 行为变化

1. **候选桩移除**：普攻/DoT/反伤从原先简陋的 `×1.05` 变为享受真实抗性与防御减伤（按裁决数值平衡在实现完成后统一调优）；
2. **转换机制规范**：转换产物严格按比例封顶且剥离源元素标签，构筑收益严格可预测；
3. **武器伤害双算修复**：非 `DirectSkillCast` 的次级伤害不再叠加武器基础点伤。

---

## 7. 迁移路线（Phase 划分）

| 阶段 | 内容 | 交付物与 DoD |
|------|------|--------------|
| **P0 止血与分类** | 引入 `DamageOrigin`；审计全仓调用点并归类；移除 `CombatV2` 候选桩拦截；更新桩期测试断言 | 全测试通过；前后数值差异与发布说明归档；彻底清零 `×1.05` 桩 |
| **P1 结构解耦** | 抽离 Pre/Post 拦截器；引入延迟队列解决反击重入；单/批防御逻辑抽离 | 反击重入安全测试通过；无敌/闪避提前退出生效；性能不劣化 |
| **P2 转换与标签规范** | 实现 §4.4 纯百分比快照转换与 `TransformTagsOnConversion` 标签剥离；删除旧级联链 | 转换 C1–C9 性质测试与黄金测试全绿；双重受益消除验证 |
| **P3 状态正交与组件化** | 实现 `AttackerSnapshot` + `TargetConditionMask`；投射物/DoT 挂载组件直存；废弃全局 LRU | 冻结/命印等动态条件测试全绿；多线程 Taskflow 并行压力测试无死锁/数据竞争 |
| **P4 清理与终验** | 收敛 `foundation/combat_v2`；完成兼容性旧代码终检删除；基准性能与一致性门禁验收 | 消除清单为空；基准测试单目标不劣于基线，AoE 优于基线；事件一致率 ≥99.9% |

---

## 8. 测试与验收标准

1. **转换性质测试集**：穷举验证守恒（C1）、100% 封顶（C2）、比例缩放（C4）、无级联（C5）、互转确定性（C6）、源标签剥离（C10）；
2. **守方动态条件测试集**：验证冻结（172）、命印（512）、高血量暴击（150）仅在守方满足条件时精确触发，在 AoE 中多目标间完全互不干扰；
3. **重入与并发安全测试**：高频触发 Blade Ward 反击与 ShadowCast，验证无组件悬挂崩溃，内存检测无 UAF；
4. **武器点伤唯一性测试**：验证分裂投射物、异常 DoT、危险区、反伤在拥有 `skill_id` 时不会被重复注入武器点伤；
5. **性能基准门禁**：运行 `tests/performance/DamagePipelineBenchmark.cpp`，单目标 P95 延迟不劣于基线，200 目标 AoE 优于基线。

---

## 9. 决策记录（2026-09-11）

| # | 决策项 | 裁决与执行结论 |
|---|--------|----------------|
| **Q1** | 转换封顶 | 仅转换（Convert）单源总量封顶 100% 并按比例分配；GainExtra 纯百分比不封顶 |
| **Q2** | 转换效率 | **不设计独立的转换效率字段**，仅保留纯百分比计算，彻底规避归一化抵消矛盾 |
| **Q3** | 攻守解耦 | 采用攻方静态模板 + 守方条件位掩码（ConditionMask）动态求值，放弃完全静态物化 |
| **Q4** | 缓存生命周期 | **废弃全局动态 LRU 缓存**，投射物/DoT 挂载 `DamageSnapshotComponent` 直存，根除 ID 复用与并发竞争 |
| **Q5** | 标签剥离 | 转换后的伤害实例**强制剥离源元素标签**，仅保留目标元素标签及非元素动作标签，杜绝多重收益 |
| **Q6** | 来源分类 | 引入 `DamageOrigin` 强枚举，唯有 `DirectSkillCast` 注入武器与技能配置点伤 |
| **Q7** | 重入安全 | 引入延迟动作队列，反击与受击施法异步延迟执行，结合快照值传递彻底消灭 UAF |
| **Q8** | 批量性能 | 攻方单次构建，守方自适应分流（单目标标量内联，N≥4 保留 `xsimd` 向量化减伤） |
| **Q9** | 代码量预期 | 认可模块化与完善度带来的总体代码量增加，以架构稳健与性能为第一优先级 |
| **Q10**| 平衡后置 | 桩移除与去双算引起的数值跳变属于缺陷修复，数值平衡在重构完成后统一调整 |

---

## 附录 A：现状证据索引

- 候选桩拦截：`src/game/systems/combat/DamagePipeline.cpp:511-538`
- 桩实现 ×1.05：`src/game/foundation/combat_v2/CombatV2RuntimeFacade.cpp:26-31`
- 反击重入同步调用：`src/game/systems/combat/DamagePipeline.cpp:1324, 1795`
- 守方动态特判：`src/game/systems/combat/DamagePipeline.cpp:926-979, 1098-1171`
- 批量 SIMD 实现：`src/game/systems/combat/DamagePipeline.cpp:1486-1708`
- 转换链实现：`src/game/systems/combat/DamagePipeline.cpp:745-862`
- 标签定义：`src/game/foundation/data/TagRegistry.hpp:12-55`
- 属性与伤害类型：`src/game/foundation/components/Stats.hpp:13-22, 64-140`
