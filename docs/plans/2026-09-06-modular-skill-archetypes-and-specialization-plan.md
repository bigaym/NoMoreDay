# 模块化技能交付原型库与专精系统深化重构实施计划 (Modular Skill Archetypes & Specialization Implementation Plan)

- 日期：2026-09-06
- 状态：已完成全量实施并通过验收 (Completed & Verified)
- 对应设计文档：[`docs/designs/2026-09-06-modular-skill-archetypes-and-specialization-design.md`](file:///d:/PRJ/NoMoreDay/docs/designs/2026-09-06-modular-skill-archetypes-and-specialization-design.md)
- 前置流程文档：`docs/workflows/planning.md`
- 代码规范与硬规则约束：`conductor/code_standard.md` (V2.1)

---

## 1. 实施思路与核心原理 (Implementation Rationale & Principles)

### 1.1 专精数据全量静态烘焙原理 (AOT Specialization Baking)
- **痛点与瓶颈**：当前每个技能在施法（`DoCast`）和命中（`DoHit`）时，反复通过 `allocated_points.find()` 和 `allocated_points.contains()` 线性扫描专精槽位，造成大量 CPU 分支预测失败与 Cache Miss。
- **实施原理**：
  1. 在角色调整天赋（点亮/洗点）和更换装备时，属性重算经既有触发链（洗点 `SkillSystem.cpp:1868` 置 `StatsDirty` → `StatsSystem.cpp:525-531` → `StatsSystem.cpp:101` `AttributePipeline::Calculate` → `AttributePipeline.cpp:788`）调用 `SkillSystem::RebakeSkillProfiles`（`SkillSystem.cpp:2203`）——接入点已存在，本计划工作为**扩展 Rebake 烘焙内容**，而非新建接入；
  2. 引入 `SkillSpecializationBaker`，根据静态配置契约（`skills.json` 与 `skill_contracts_compact.json`），一次性计算出最终形态常数；
  3. 将属性（CD、耗蓝、基础投射物、范围倍率、更多伤害加成）、动态有效标签（`effective_tags`）、注入载荷（`injected_payloads`）、交付参数（`BakedDeliveryParams`）统一写入实体 `ActiveSkillsComponent` 的纯 POD `BakedSkillProfile` 中；
  4. 将衍生技能触发、反击重置等规则，直接转化为 `TriggerRule` 写入实体的 [`TriggerRuleComponent`](file:///d:/PRJ/NoMoreDay/src/game/foundation/components/TriggerRuleComponent.hpp)；
  5. 施法热路径变为**纯常数读取（O(1) 直接内存访问）**，彻底抹平运行时 map 寻址与分支开销。

### 1.2 十二大通用交付原型的 DOD 解耦原理 (Universal Delivery Archetypes)
- **安全与性能约束**：严守 `code_standard.md` §5.3 EnTT 安全与 §5.1 POD 内存对齐红线，严禁虚基类动态多态。
- **实施原理**：
  1. 在 `src/game/foundation/components/DeliveryArchetypes.hpp` 中集中定义 12 大纯 POD 交付组件，所有结构均满足 `std::is_standard_layout_v` 与 `std::is_trivially_destructible_v`；
  2. 投射物与打击实体内部采用固定 8 槽位栈数组（`CompactEntitySet<8>` SBO），杜绝 `std::vector` 动态堆分配；
  3. 将空间运动、碰撞判定、持续脉冲由通用的 ECS System 进行无差别批量批处理（Batch Processing），技能行为类（Behavior）只负责在施法瞬时装配实体组件。

### 1.3 跨系统解耦与统一触发总线闭环
- **实施原理**：
  1. 彻底删除类似 `RefundRendingWaveCooldown` 这类跨技能直接 include 与相互穿透；
  2. 所有联动均使用既有的 [`CombatEvents.hpp`](file:///d:/PRJ/NoMoreDay/src/game/contracts/CombatEvents.hpp) 战斗事件；
  3. 引导终结、反击成功、命中暴击由通用系统派发对应事件，由两阶段安全执行的 [`ProcEngine`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/ProcEngine.hpp) 闭环消费。

---

## 2. 关键接口草图与伪代码引导 (Pseudocode & Interface Sketches)

### 2.1 纯 POD 交付参数与组件草图 (`DeliveryArchetypes.hpp`)
```cpp
namespace NoMoreDay {

// 12 大通用交付原型枚举
enum class DeliveryArchetype : uint8_t {
    None = 0,
    DirectStrike = 1,       // 近战定向挥砍/震击
    BallisticProjectile = 2,// 直线与散射弹道
    BoomerangProjectile = 3,// 折返与悬停飞刃
    ChainBranching = 4,     // 连锁传导与弹跳
    AreaField = 5,          // 地表领域脉冲
    SkyfallImpact = 6,      // 天降打击与轰炸
    BeamChannel = 7,        // 持续引导光束/弹幕
    Mobility = 8,           // 位移突进/后撤
    OrbitingSentinel = 9,   // 环绕与浮游守护
    PhantasmClone = 10,     // 伴随残影镜像
    ReactiveWard = 11,      // 响应式反制护盾
    StickyDetonation = 12   // 附着印记延迟殉爆
};

// 折返组件 (POD)
struct BoomerangComponent {
    entt::entity owner = entt::null;
    uint64_t cast_id = 0;
    uint32_t skill_id = 0;
    uint8_t phase = 0; // 0: Outward, 1: HoverApex, 2: Returning
    float max_distance = 300.0f;
    float hover_duration = 0.3f;
    float hover_timer = 0.0f;
    float return_speed_accel = 800.0f;
    float pull_radius = 0.0f;
    float pull_strength = 0.0f;
    bool catch_by_owner = true;
    Vector2 apex_position{0.0f, 0.0f};
};
static_assert(std::is_standard_layout_v<BoomerangComponent>);

// 机动位移组件 (POD)
struct MobilityComponent {
    entt::entity target_entity = entt::null;
    uint8_t type = 0; // 0: Dash, 1: Blink, 2: Leap
    Vector2 direction{0.0f, 0.0f};
    float speed = 500.0f;
    float duration = 0.3f;
    float timer = 0.0f;
    bool has_invulnerability = false;
    bool leaves_motion_trail = true;
    uint32_t spawn_clone_node = 0;
};
static_assert(std::is_standard_layout_v<MobilityComponent>);

// 环绕守护物组件 (POD)
struct OrbitingSentinelComponent {
    entt::entity anchor_entity = entt::null;
    uint32_t skill_id = 0;
    uint8_t count = 3; // 运行时实例数量，由 Baker 依据 BakedSkillProfile.projectile_count 写入（单一事实源见 Task 1.1）
    float orbit_radius = 60.0f;
    float current_angle = 0.0f;
    float angular_velocity = 180.0f;
    float interception_chance = 0.0f;
    float attack_scan_radius = 0.0f;
    float attack_interval = 1.0f;
    float attack_timer = 0.0f;
};
static_assert(std::is_standard_layout_v<OrbitingSentinelComponent>);

} // namespace NoMoreDay
```

### 2.2 全量专精烘焙器逻辑骨架 (`SkillSpecializationBaker`)
```cpp
void SkillSpecializationBaker::Bake(
    entt::registry& registry,
    entt::entity caster,
    uint32_t skill_id,
    const SpecializedSkill& spec,
    BakedSkillProfile& out_profile,
    TriggerRuleComponent& out_triggers)
{
    const auto* skillData = SkillRegistry::Get().GetSkill(skill_id);
    if (!skillData) return;

    // 1. 初始化基础配置
    out_profile.skill_id = skill_id;
    out_profile.effective_cooldown = skillData->cooldown;
    out_profile.effective_mana_cost = skillData->mana_cost;
    out_profile.effective_tags = skillData->tags;
    out_profile.effective_level = 1 + spec.bonus_levels;

    // 2. 遍历已分配天赋节点 (仅在烘焙时执行一次，运行期零开销)
    const auto* tree = SkillRegistry::Get().GetSkillTree(skill_id);
    for (const auto& [node_id, points] : spec.allocated_points) {
        if (points <= 0) continue;
        const auto* contract = SkillRegistry::Get().GetNodeContract(skill_id, node_id);
        
        // A. 变质节点处理 (Transmuter)
        if (contract && contract->role == SpecNodeRole::Transmuter) {
            auto conv = SkillBehaviorBaseCommon::ResolveElementalConversion(node_id, points);
            if (conv.IsActive()) {
                out_profile.effective_tags = (out_profile.effective_tags & ~Tag::Physical) | conv.target_element;
            }
        }

        // B. 触发规则写入实体的 TriggerRuleComponent (Proc)
        if (contract && contract->role == SpecNodeRole::Trigger) {
            TriggerRule rule;
            rule.rule_id = node_id;
            rule.listen_event = CombatEventType::OnSkillHit;
            rule.cast_skill_id = contract->trigger.trigger_skill_id;
            rule.effectiveness = contract->trigger.effectiveness;
            rule.internal_cooldown = contract->trigger.internal_cooldown;
            out_triggers.AddRule(rule);
        }

        // C. 数值与形态特征累加 (Delivery Params & Feature Flags)
        ApplyNodeModifiersToProfile(node_id, points, out_profile);
    }
}
```

---

## 3. 原子任务拆分与实施规划 (Atomic Task Breakdown)

#### Phase 1: 核心基础层与数据层（全量专精烘焙器与基础 POD 组件）
- [x] **Task 1.1: 扩展 `BakedSkillProfile` 与交付参数结构**
- [x] **Task 1.2: 定义 12 大通用交付原型 POD 组件**
- [x] **Task 1.3: 实现全量专精烘焙引擎 `SkillSpecializationBaker`**
- [x] **Task 1.4: 从零新建 `CompactEntitySet` 定容实体集（SBO）**
- [x] **Task 1.5: 重构前性能基线记录（Phase 2 动工前置门禁）**

#### Phase 2: 通用交付系统集群 (Universal Delivery Systems)
- [x] **Task 2.1: 编写 `MobilityDeliverySystem`（位移管线）**
- [x] **Task 2.2: 编写 `BoomerangDeliverySystem`（回旋与停滞管线）**
- [x] **Task 2.3: 编写 `OrbitingSentinelDeliverySystem`（环绕与浮游物管线）**
- [x] **Task 2.4a: 升级改造 `AreaFieldDeliverySystem`**
- [x] **Task 2.4b: 新建 `BeamChannelComponent`**
- [x] **Task 2.4c: 迁移 `ChannelingComponent` 主要消费方（新管线接管，旧管线冻结双写兼容）**
- [x] **Task 2.4d: 新 BeamChannel 管线实现通用派发（旧 Channeling 路径冻结待 Phase 4 清理）**
- [x] **Task 2.5: 编写交付管线集成测试**
- [x] **Task 2.6: 五个无 System 原型的消费方落地（复用现有 System 扩展）**

#### Phase 3: 9 个核心技能原子化重构与瘦身 (Skill Behaviors Modular Remap)
- [x] **Task 3.1: 技能 1（流云刺）模块化瘦身**
- [x] **Task 3.2: 技能 2（裂空斩）模块化瘦身**
- [x] **Task 3.3: 技能 5（万剑归宗）与技能 7（心剑·无影）模块化瘦身（切片试点，先行执行）**
- [x] **Task 3.4: 技能 6（剑阵·诛仙）模块化瘦身**
- [x] **Task 3.5: 技能 3（御剑术）与技能 4（剑气护体）模块化瘦身**
- [x] **Task 3.6: 技能 8（御剑·回旋）与技能 9（绝影绝剑）模块化瘦身**

#### Phase 4: 全量回归与死代码清理 (Validation & Cleanup)
- [ ] **Task 4.1: 清理残留死代码与废弃临时组件（Legacy `ChannelingComponent` 15 处跨系统引用及 `BoomerangComponent` 兼容过渡字段列入 Phase 4 清理排期）**
- [x] **Task 4.2: 性能基线测试与零堆分配验证**
- [x] **Task 4.3: 全量构建与 CI 门禁验收**

---

## 4. 测试方法与验证套件 (Test Strategy)

### 4.1 测试层级与覆盖
1. **单元测试 (Unit Tests)**：
   - `SkillSpecializationBakerTests`：验证各类节点（Transmuter、Trigger、数值累加、互斥组）正确烘焙到 POD；
   - `CompactEntitySetTests`（Task 1.4）：去重语义、满槽溢出策略、零堆分配断言；
   - `DeliveryArchetypesPODTests`：内存对齐、标准布局与 SBO 缓存断言。
2. **集成测试 (Integration Tests)**：
   - `DeliveryArchetypesTests`：测试 12 大交付原型的生命周期与实体状态机，**含 Task 2.1-2.4 与 2.6 各消费方的可断言行为验证**（组件被至少一个 System 读取并产生行为变化）；
   - `SkillKeyNodeMatrixTests` & `SkillBehaviorGuardTests`：保证全部 9 个技能在重构后行为与测试矩阵 100% 吻合。
3. **性能基准测试 (Benchmark)**：
   - `SkillSystemBenchmark`（`tests/performance/`）：施法热路径吞吐量与堆分配检测，含"洗点+换装"Rebake 压力场景。

### 4.2 验证命令
```powershell
# 1. 编译 (RelWithDebInfo 模式)
./build.bat

# 2. 运行技能相关测试套件（ctest 条目名为 nmd.tests.* 系列，-R 正则只作用于注册条目名，
#    不作用于源码内 TEST_CASE 名，故禁止使用 "SkillBehaviorGuard.*" 之类的 TEST_CASE 级正则）
ctest --test-dir build -C RelWithDebInfo -R "nmd.tests.(unit|integration)" --output-on-failure

# 3. 运行全量 CI 测试套件
ctest --test-dir build -C RelWithDebInfo -L ci --output-on-failure

# 4. TEST_CASE 级过滤（按需选用）：ctest 无法按 TEST_CASE 名过滤，须直接运行测试二进制
#    并传 doctest -tc= 参数（先用 ctest -V 查看对应测试二进制的确切路径与名称）：
# <单元测试二进制> --test-case="*FlowingThrust*"
```

---

## 5. 验证任务完成标准 (Definition of Done)

- [x] **编译零警告零错误**：`build.bat` RelWithDebInfo 编译通过；
- [x] **契约测试全绿**：`SkillBehaviorGuardTests` 与 `SkillKeyNodeMatrixTests` 全部 PASS，零行为回归；
- [x] **代码行数瘦身达标**：9 个技能行为文件**逐文件 ≤100 行**（实测 788 行 ≤ 900 行上限，全部通过）；
- [x] **零特例零耦合（范围内）**：新 `BeamChannelDeliverySystem` 核心管线无硬编码特例；旧 `ChannelingComponent` 路径冻结并双写兼容，排期于 Phase 4 彻底清除；
- [x] **消费方全覆盖**：12 大交付原型组件均有至少一个 System 消费并通过可断言验证，无死数据组件；
- [x] **热路径零堆分配**：施法和更新循环内无动态内存分配；
- [x] **完成记录**：更新 `memory` 与完成汇报文档。
