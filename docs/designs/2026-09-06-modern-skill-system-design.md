# 现代化高可用、易扩展、高性能技能系统架构设计 (Modern Skill System Architecture Design)

- 日期：2026-09-06
- 状态：终审通过 / 准入实施 (Approved & Ready for Implementation)
- 范围：技能系统整体架构重构、动态标签流、装备技能修饰器 (Item Skill Modifiers)、交付与载荷正交管线、统一触发总线、DOD 内存与缓存优化
- 前置流程文档：`docs/workflows/design.md`
- 技术栈规范参考：`conductor/tech-stack.md`、`conductor/code_standard.md` (V2.1)
- 标杆对标：底层玩法对标《最后纪元》(Last Epoch) 技能独立专精树与动态标签，装备与触发对标《恐怖黎明》(Grim Dawn) 装备修饰补丁与自适应触发

---

## 1. 问题陈述与现状代码证据 (Problem Statement & Code Evidence)

当前 NoMoreDay 技能系统处于由原型 Demo 向数据驱动演化的过渡期，各子系统之间耦合严重、硬编码蔓延，具体表现为以下核心缺陷：

### 1.1 基础数据层：词缀枚举与具体技能硬编码绑定
- **代码实锤**：[`src/game/foundation/components/ItemStats.hpp:73-83`](file:///d:/PRJ/NoMoreDay/src/game/foundation/components/ItemStats.hpp#L73-L83)
  ```cpp
  PlusAllSkills,     // 44
  PlusFlowingThrust, // 45 -> 词缀枚举直接绑定技能1（流云刺）
  PlusRendingWave,   // 46 -> 词缀枚举直接绑定技能2（裂地波）
  TitanGrip,         // 47
  FlatDodgeRating,   // 48 ~ PercentBlockRating = 51
  ```
- **危害与存档兼容约束**：词缀系统反向依赖上层业务技能。同时由于项目采用 `ItemPersistenceCodec`（`NMDS` v1 二进制流存档）直接持久化 192 字节 POD 的 `ItemInstance`（含 `CompactAffix::type` 整数值），**直接删除枚举将引发后续所有词缀数值移位（如 47 号 TitanGrip 错位），彻底破坏二进制存档兼容性**。必须保留废弃占位符，新增泛型词缀枚举分配至安全空闲值 `PlusSkillLevelGeneric = 52`，并通过独立的持久化分段保存装备技能修饰器。

### 1.2 施法主干逻辑被业务特例侵入
- **代码实锤**：
  - [`src/game/systems/skill/SkillSystem.cpp:2046-2064`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSystem.cpp#L2046-L2064)：通用施法入口 `SkillSystem::TryCast` 中直接硬编码检测天赋 124（影杀阵）的残影复制逻辑，强制分支计算法力追加与冷却。
  - [`src/game/systems/skill/SkillSystem.cpp:908-998`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSystem.cpp#L908-L998)：全局受击回调 `SkillSystem::OnTakeDamage` 中，硬编码了技能 9（幻影闪）的反击伤害结算与坐标索敌（`:951-954`），以及技能 8（回旋飞剑）的受击冷却减免。
  - [`src/game/systems/skill/SkillSystem.cpp:1213-1500`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSystem.cpp#L1213-L1500)：通用更新循环 `UpdateStates` 中手写了技能 5（万剑归宗）和技能 7（斩心剑）的粒子发射、空间索敌与实体生成，导致单文件膨胀至 **2672 行**。
- **危害**：核心施法管线违背开闭原则（OCP），新增任意联动特性都必须向核心施法主干塞 `if-else`。

### 1.3 字段爆炸与组件状态膨胀 (State Explosion)
- **代码实锤**：[`src/game/foundation/components/SkillDefs.hpp:751-1010`](file:///d:/PRJ/NoMoreDay/src/game/foundation/components/SkillDefs.hpp#L751-L1010)
  - `HeavenlySwordFieldComponent` 包含 **38 个成员字段**（多达 15 个布尔标记用于记录专精节点开启状态，如 `lightning_tribunal`, `frozen_dominion`）。
  - `BloodSeaFieldComponent` 包含 **27 个字段**。
- **外部消费依赖**：外部模块 [`BladeMasteryService.cpp:38, 69`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/BladeMasteryService.cpp#L38)、[`GameUiSnapshotBuilder.cpp:383-400`](file:///d:/PRJ/NoMoreDay/src/game/application/ui/GameUiSnapshotBuilder.cpp#L383-L400)、[`PlayerHudController.cpp:230-233`](file:///d:/PRJ/NoMoreDay/src/game/application/ui/PlayerHudController.cpp#L230-L233) 直接感知这两个专用组件并读取 `has_void_keystone` 等字段渲染“Miasma Pressure”。重构必须在保持 POD 的前提下提供 `owner`、`cast_id` 及对齐的专精状态查询通道。

### 1.4 技能横向强行跨系统穿透
- **代码实锤**：[`src/game/systems/skill/behaviors/FlowingThrust.cpp:158-163`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/FlowingThrust.cpp#L158-L163) 直接 include 并调用 [`SevenStarSlashShared.hpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/behaviors/SevenStarSlashShared.hpp)，并在命中时通过 `RefundRendingWaveCooldown` 强行修改技能 2 的状态。

### 1.5 高频堆分配与快照臃肿 (DOD 性能硬伤)
- **代码实锤**：
  - [`src/game/foundation/components/Projectile.hpp:42`](file:///d:/PRJ/NoMoreDay/src/game/foundation/components/Projectile.hpp#L42)：`std::vector<entt::entity> hitEntities;` 引发每秒上万次 `malloc/free`；
  - [`src/game/foundation/components/Projectile.hpp:15`](file:///d:/PRJ/NoMoreDay/src/game/foundation/components/Projectile.hpp#L15) 与 [`SkillSystem.cpp:2108-2110`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSystem.cpp#L2108-L2110)：投射物与残影实体（`ShadowComponent`）全量深拷贝约 400 字节的 `CombatStats`，破坏 CPU 缓存局部性。

---

## 2. 架构设计理念与标杆对标 (Design Philosophy)

```
                    ┌────────────────────────────────────────────────────────┐
                    │                      标杆架构参考                      │
                    ├──────────────────────────┬─────────────────────────────┤
                    │    最后纪元 (Last Epoch)  │     恐怖黎明 (Grim Dawn)     │
                    ├──────────────────────────┼─────────────────────────────┤
                    │ 1. 独立技能专精树 (20+点)│ 1. 装备技能修饰器 (Item Mod)│
                    │ 2. 标签动态推导 (TagConv)│ 2. 引擎技能模板 (Skill Tmpl)│
                    │ 3. 子技能触发防递归深度  │ 3. 星座触发与CD加权 (Proc)  │
                    │ 4. 技能树决定子技能形态  │ 4. 变质器 (Transmuter) 机制 │
                    └──────────────────────────┴─────────────────────────────┘
```

1. **对标《最后纪元》——技能为本，标签流转**：
   - 每一个技能拥有专属的深度专精树；
   - 技能具备固有标签（Base Tags），专精树节点或转化效果改变标签（Effective Tags）；
   - 所有角色属性、装备通用词缀（`+Cold Damage`）均**面向标签结算**，与具体技能 ID 解耦。
2. **对标《恐怖黎明》——装备修饰，权重自适应**：
   - 装备词缀支持 **Item Skill Modifier**（装备技能修饰器），通过配置补丁改写指定技能的基础参数；
   - 全局触发几率依据技能基础冷却自动加权缩放，消除“高频小技能永远最强”的失衡。
3. **NoMoreDay 引擎落地原则——严守 DOD、POD 与 ECS 安全规范**：
   - 所有组件严格保持 **POD / Standard Layout**，严禁在组件内部嵌入 `std::vector` 等堆分配容器；
   - 触发派发采用 **Collect-Then-Execute（收集再执行）** 两阶段模式，完整携带目标实体、坐标并执行活性检查，严禁跨实体操作持有 EnTT 指针；
   - 全面复用引擎既有的 [`CombatEvents.hpp`](file:///d:/PRJ/NoMoreDay/src/game/contracts/CombatEvents.hpp) 事件总线与 [`CombatConstants.hpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/combat/CombatConstants.hpp) 单向伤害转化链。

---

## 3. 核心设计支柱 (Five Architectural Pillars)

```
                    ┌────────────────────────────────────────┐
                    │          Skill Definition (JSON)       │
                    │   Tags, Delivery Archetype, Payloads   │
                    └───────────────────┬────────────────────┘
                                        │
           ┌────────────────────────────┴────────────────────────────┐
           ▼                                                         ▼
┌───────────────────────┐                                 ┌─────────────────────────┐
│ Talent Tree Modifiers │                                 │ Item Skill Modifiers    │
│ (Skill Spec Contract) │                                 │ (Grim Dawn Overrides)   │
└──────────┬────────────┘                                 └────────────┬────────────┘
           │                                                           │
           └────────────────────────────┬──────────────────────────────┘
                                        ▼
                   ┌─────────────────────────────────────────┐
                   │   Baked Skill Profile (Cached in POD)   │
                   │  - Effective Tags (TagBitset)           │
                   │  - Parameter Overrides (CD, Mana, Mult) │
                   │  - Compact Inlined Injected Payloads    │
                   └────────────────────┬────────────────────┘
                                        │ (On Cast)
               ┌────────────────────────┴────────────────────────┐
               ▼                                                 ▼
┌─────────────────────────────┐                   ┌─────────────────────────────┐
│      Delivery Pipeline      │                   │       Payload Pipeline      │
│  (Spatial, Motion & Entity) │                   │  (Damage, Ailment & Buff)   │
├─────────────────────────────┤                   ├─────────────────────────────┤
│ - ProjectileDelivery        │ ── On Collision ─>│ - DamagePayload             │
│ - MeleeSectorDelivery       │    / On Interval  │ - StatusAilmentPayload      │
│ - AreaFieldDelivery (POD)   │                   │ - BuffPayload               │
│ - BeamChannelDelivery       │                   │ - ForceImpulsePayload       │
└──────────────┬──────────────┘                   └──────────────┬──────────────┘
               │                                                 │
               │            Emit SkillVfxEvent                   │
               └───────────────┐           ┌─────────────────────┘
                               ▼           ▼
                      ┌─────────────────────────────┐
                      │    GPUSkillEffectSystem     │
                      │    (SkillVfxRecipeEngine)   │
                      └─────────────────────────────┘
                                     │ (Combat Events via CombatEvents.hpp)
                                     ▼
                      ┌─────────────────────────────┐
                      │  Unified Proc Engine (POD)  │
                      │  - Two-Phase Safe Dispatch  │
                      │  - Depth Guard (Depth <= 2) │
                      │  - Adaptive Cooldown Weight │
                      │  - UpdateCooldowns (dt)     │
                      └─────────────────────────────┘
```

### 3.1 支柱一：动态标签流转与词缀泛型化 (Tag-Driven System)

#### 3.1.1 词缀解耦与二进制存档兼容保障
为确保 192 字节 `ItemInstance` POD 在 `NMDS` v1 格式下反序列化时不发生枚举错位：
```cpp
// 在 ItemStats.hpp 中锁定枚举数值
enum class AffixType : uint16_t {
    // ...
    PlusAllSkills = 44,
    Deprecated_PlusFlowingThrust = 45, // 占位符：保留旧值，防枚举数值移位
    Deprecated_PlusRendingWave = 46,   // 占位符：保留旧值
    TitanGrip = 47,                    // 保持原有定义不变
    FlatDodgeRating = 48,
    PercentDodgeRating = 49,
    FlatBlockRating = 50,
    PercentBlockRating = 51,
    PlusSkillLevelGeneric = 52,        // 新增：泛型技能加级（安全空闲数值）
    // ...
};

// 泛型技能加级定义
struct SkillLevelBonus {
    uint32_t target_skill_id = 0;      // 0 表示按标签匹配
    Tag target_tag_filter = Tag::None; // 例如 Tag::Melee, Tag::Spell
    int level_delta = 1;
};
```

#### 3.1.2 动态标签推导与既有伤害转化链对齐
技能运行期生效标签结算公式：
$$\text{EffectiveTags} = (\text{BaseTags} \setminus \text{StrippedTags}) \cup \text{GrantedTags}$$
属性转化严格遵守 [`src/game/systems/combat/CombatConstants.hpp:44-66`](file:///d:/PRJ/NoMoreDay/src/game/systems/combat/CombatConstants.hpp#L44-L66) 的单向转化顺序与防回环规则：
$$\text{Physical (0)} \to \text{Lightning (3)} \to \text{Cold (2)} \to \text{Fire (1)} \to \text{Poison (4)} \to \text{Shadow (5)}$$
角色的全局属性词缀通过 `EffectiveTags` 自动对齐结算，技能内无需任何私有特判。

---

### 3.2 支柱二：装备技能修饰器 (Item Skill Modifiers - 恐怖黎明式)

```cpp
struct ItemSkillModifier {
    uint32_t target_skill_id = 0;      // 目标技能 ID
    float flat_cooldown_delta = 0.0f;  // 冷却时间减免
    float mana_cost_delta = 0.0f;      // 蓝耗增减
    int extra_projectiles = 0;         // 投射物增加
    float area_radius_mult = 1.0f;     // 范围倍率
    Tag convert_from = Tag::None;      // 来源属性
    Tag convert_to = Tag::None;        // 目标属性
    float conversion_ratio = 0.0f;     // 转化比例
    uint32_t inject_ailment_id = 0;    // 附加异常状态
    float inject_ailment_chance = 0.0f;
};
```

#### 3.2.1 持久化分段隔离与纯 POD 烘焙表
- **存档安全保障**：装备技能修饰器不侵入 192 字节的 `ItemInstance`，在 [`ItemPersistenceCodec.hpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/item/storage/ItemPersistenceCodec.hpp) 中新增独立的 `SectionType::ItemSkillModifiers = 13`（并分配脏标记 `ContainerDirtyFlags::ItemSkillModifiers = 1 << 13`）。旧存档反序列化时若不存在 Section 13 则直接跳过，零风险实现向前兼容。
- **纯 POD 烘焙属性表**：
  在角色换装/洗点时，烘焙生成纯 POD 的 `BakedSkillProfile` 缓存于实体的 `ActiveSkillsComponent`（[`SkillDefs.hpp:510`](file:///d:/PRJ/NoMoreDay/src/game/foundation/components/SkillDefs.hpp#L510)）。

```cpp
enum class PayloadType : uint8_t {
    Damage = 0,
    Ailment,
    Buff,
    Impulse
};

struct PayloadDefinition {
    PayloadType type = PayloadType::Damage;
    uint32_t ailment_id = 0;
    float value_mult = 1.0f;
    Tag damage_tags = Tag::None;
};
static_assert(std::is_standard_layout_v<PayloadDefinition>);

struct BakedSkillProfile {
    uint32_t skill_id = 0;
    int effective_level = 1;
    float effective_cooldown = 0.0f;
    float effective_mana_cost = 0.0f;
    Tag effective_tags = Tag::None;
    int projectile_count = 1;
    float area_radius = 1.0f;
    float proc_coefficient = 1.0f;
    
    // 定长 POD 载荷数组 (Zero Heap Allocation)
    static constexpr uint8_t kMaxInjectedPayloads = 4;
    std::array<PayloadDefinition, kMaxInjectedPayloads> injected_payloads{};
    uint8_t injected_count = 0;
};
static_assert(std::is_standard_layout_v<BakedSkillProfile>);
```

#### 3.2.2 UI Tooltip 完整消费链路闭环
扩展 [`SkillDisplayPreviewService.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillDisplayPreviewService.cpp)，并同步改造 [`src/game/application/ui/UIRenderer.cpp:1074-1081`](file:///d:/PRJ/NoMoreDay/src/game/application/ui/UIRenderer.cpp#L1074-L1081)，使其从只读静态配置转向消费实体的动态 Preview 数据，实现 Tooltip 实时显示装备带来的冷却缩减、投射物增加和形态转化。

---

### 3.3 支柱三：交付载体 (Delivery) 与命中载荷 (Payload) 正交分离

#### 3.3.1 纯 POD 地表领域组件与外部消费方兼容
彻底废弃 38 字段的 `HeavenlySwordFieldComponent` 与 27 字段的 `BloodSeaFieldComponent`，统一使用满足 Standard Layout 的通用组件：
```cpp
struct AreaFieldComponent {
    entt::entity owner = entt::null;     // 归属施法者 (实体消亡与属性伤害结算必须)
    uint64_t cast_id = 0;                // 战斗归因 ID
    uint32_t source_skill_id = 0;        // 技能标识 (兼容外部系统查询)
    float remaining_duration = 0.0f;
    float pulse_interval = 0.25f;
    float timer = 0.0f;
    float radius = 48.0f;
    uint8_t shape_type = 0;              // 0: 圆形, 1: 环形, 2: 旋转切割线
    
    // 定长 POD 载荷数组 (Zero heap allocation, Standard Layout)
    static constexpr uint8_t kMaxFieldPayloads = 4;
    std::array<PayloadDefinition, kMaxFieldPayloads> payloads{};
    uint8_t payload_count = 0;
};
static_assert(std::is_standard_layout_v<AreaFieldComponent>);
static_assert(std::is_trivially_destructible_v<AreaFieldComponent>);
```
- **外部消费方无缝迁移**：
  - [`BladeMasteryService.cpp:38, 69`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/BladeMasteryService.cpp#L38) 迁移至通过 `AreaFieldComponent::source_skill_id == 11/12` 判定领域激活并读取 `owner` 进行结算；
  - [`GameUiSnapshotBuilder.cpp:383-400`](file:///d:/PRJ/NoMoreDay/src/game/application/ui/GameUiSnapshotBuilder.cpp#L383-L400) 与 [`PlayerHudController.cpp:230-233`](file:///d:/PRJ/NoMoreDay/src/game/application/ui/PlayerHudController.cpp#L230-L233) 迁移：通用进度改由 `AreaFieldComponent` 获取，而血海虚空基石（`has_void_keystone`）与瘴气加成（`miasma_duration_bonus`）直接由施法者 `ActiveSkillsComponent::baked_profiles` 提供，保证 HUD "Miasma Pressure" 状态反馈连贯。

#### 3.3.2 引导技能管线独立
提炼 `BeamChannelDeliverySystem`，从 `SkillSystem::UpdateStates` 中剥离万剑归宗（技能 5）与斩心剑（技能 7）的引导更新逻辑，使核心系统文件行数缩减 40% 以上。

---

### 3.4 支柱四：全事件驱动的统一触发总线 (Unified Proc Engine)

#### 3.4.1 复用既有事件总线与纯 POD 规则定义
直接接入既有的 [`src/game/contracts/CombatEvents.hpp`](file:///d:/PRJ/NoMoreDay/src/game/contracts/CombatEvents.hpp)（使用 `NoMoreDay::CombatEventType`）与 [`CombatEventDispatcher`](file:///d:/PRJ/NoMoreDay/src/game/contracts/impl/CombatEventDispatcher.hpp)：

```cpp
enum class TriggerTargetPolicy : uint8_t {
    Self = 0,
    Victim,
    Attacker,
    GroundTarget
};

struct TriggerRule {
    uint32_t rule_id = 0;
    NoMoreDay::CombatEventType listen_event = NoMoreDay::CombatEventType::OnSkillHit;
    Tag event_tag_filter = Tag::None;
    float base_chance = 1.0f;
    bool use_proc_scaling = true;
    float internal_cooldown = 0.0f;
    float current_cooldown = 0.0f;
    
    uint32_t cast_skill_id = 0;
    float effectiveness = 1.0f;
    TriggerTargetPolicy target_mode = TriggerTargetPolicy::Victim;
};

// 挂载在实体上的纯 POD 组件
struct TriggerRuleComponent {
    static constexpr uint8_t kMaxRules = 8;
    std::array<TriggerRule, kMaxRules> rules{};
    uint8_t rule_count = 0;
};
static_assert(std::is_standard_layout_v<TriggerRuleComponent>);
static_assert(std::is_trivially_destructible_v<TriggerRuleComponent>);
```

#### 3.4.2 两阶段安全派发 (Collect-Then-Execute) 与冷却更新闭环
严守 `code_standard.md` §5.3 与硬否决规则 8，彻底捕获目标实体与坐标，加入实体有效性检查，并建立每帧冷却 Tick 系统：

```cpp
// 两阶段安全派发：完整携带目标实体、坐标与活性检查
void ProcEngine::DispatchEvent(entt::registry& reg, entt::entity listener, const CombatEvent& event) {
    if (event.trigger_depth >= 2) return; // 递归深度保护 (最后纪元模式)
    
    struct PendingAction {
        uint32_t skill_id = 0;
        TriggerTargetPolicy target_mode = TriggerTargetPolicy::Victim;
        entt::entity target_entity = entt::null;
        Vector2 target_pos{0.0f, 0.0f};
        uint8_t depth = 0;
        float effectiveness = 1.0f;
    };
    std::array<PendingAction, 4> pending_actions{};
    uint8_t action_count = 0;
    
    // 阶段一：在局部只读/更新作用域内收集目标上下文
    {
        auto* triggerComp = reg.try_get<TriggerRuleComponent>(listener);
        if (!triggerComp) return;
        
        for (uint8_t i = 0; i < triggerComp->rule_count; ++i) {
            auto& rule = triggerComp->rules[i];
            if (rule.listen_event != event.type || rule.current_cooldown > 0.0f) continue;
            if ((event.tags & rule.event_tag_filter) != rule.event_tag_filter) continue;
            
            float real_chance = rule.base_chance;
            if (rule.use_proc_scaling && event.parent_skill_cd > 0.0f) {
                real_chance *= (1.0f + event.parent_skill_cd * 0.2f); // 恐怖黎明 CD 加权
            }
            
            if (RandomFloat(0.0f, 1.0f) <= real_chance) {
                rule.current_cooldown = rule.internal_cooldown; // 标记冷却
                if (action_count < pending_actions.size()) {
                    entt::entity resolved_target = (rule.target_mode == TriggerTargetPolicy::Attacker) ? event.source : event.target;
                    Vector2 resolved_pos{0.0f, 0.0f};
                    if (reg.valid(resolved_target)) {
                        if (const auto* pos = reg.try_get<Position>(resolved_target)) {
                            resolved_pos = {pos->x, pos->y};
                        }
                    } else if (const auto* listener_pos = reg.try_get<Position>(listener)) {
                        resolved_pos = {listener_pos->x, listener_pos->y};
                    }
                    
                    pending_actions[action_count++] = {
                        rule.cast_skill_id,
                        rule.target_mode,
                        resolved_target,
                        resolved_pos,
                        static_cast<uint8_t>(event.trigger_depth + 1),
                        rule.effectiveness
                    };
                }
            }
        }
    } // triggerComp 指针脱离引用作用域
    
    // 阶段二：安全派生施法，执行严格的实体有效性检查
    for (uint8_t i = 0; i < action_count; ++i) {
        if (!reg.valid(listener)) break; // 施法者消亡立即中断
        const auto& act = pending_actions[i];
        SkillSystem::TriggerCast(reg, listener, act.skill_id, act.target_entity, act.target_pos, act.depth, act.effectiveness);
    }
}

// 每帧冷却计时更新系统 (由 SkillSystem::UpdateCooldowns 调用)
void ProcEngine::UpdateCooldowns(entt::registry& reg, float dt) {
    auto view = reg.view<TriggerRuleComponent>();
    for (auto entity : view) {
        auto& comp = view.get<TriggerRuleComponent>(entity);
        for (uint8_t i = 0; i < comp.rule_count; ++i) {
            if (comp.rules[i].current_cooldown > 0.0f) {
                comp.rules[i].current_cooldown = std::max(0.0f, comp.rules[i].current_cooldown - dt);
            }
        }
    }
}
```

#### 3.4.3 贯通 `parent_skill_cd` 数据生产链路
在 [`DamagePipeline.cpp:254-325`](file:///d:/PRJ/NoMoreDay/src/game/systems/combat/DamagePipeline.cpp#L254-L325)（`DispatchSingleDamageEvents`）构建 `CombatEvent` 时，依据 `request.skill_id` 从施法者实体的 `ActiveSkillsComponent` 或静态配置查询发起技能的有效冷却时间，注入 `CombatEvent::parent_skill_cd`，确保恐怖黎明自适应加权公式具备真实的数据输入。

---

### 3.5 支柱五：高性能 DOD、缓存行对齐与伤害管线贯通

1. **投射物穿透缓存的 SBO 与多重打击保护**：
   - 改造 [`Projectile.hpp#L42`](file:///d:/PRJ/NoMoreDay/src/game/foundation/components/Projectile.hpp#L42)，引入固定 8 槽位栈缓存；
   - **多重打击防护语义**：为单发投射物设置规则级穿透上限 `max_pierce <= 8`；达到上限强制消亡，杜绝溢出后因未记录导致后续帧目标重复被判为“未命中”而遭到连续扣血。
2. **轻量级伤害上下文贯通 (`DamagePayloadContext`)**：
   - 构造 64 字节缓存行对齐的 POD 上下文：
     ```cpp
     struct alignas(64) DamagePayloadContext {
         float base_damage_min = 0.0f;
         float base_damage_max = 0.0f;
         float crit_chance = 0.0f;
         float crit_multiplier = 1.5f;
         float increased_damage = 0.0f;
         float more_damage = 1.0f;
         Tag effective_tags = Tag::None;
         uint32_t source_skill_id = 0;
         uint8_t trigger_depth = 0;
     };
     ```
   - **贯通伤害管线契约**：在 [`DamagePipelineTypes.hpp`](file:///d:/PRJ/NoMoreDay/src/game/contracts/DamagePipelineTypes.hpp) 的 `DamageRequest` 结构中新增 `std::optional<DamagePayloadContext> payload_context;`；`DamagePipeline::Calculate` 优先读取该上下文快照，同步消灭投射物与残影（[`ShadowComponent`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/SkillSystem.cpp#L2108)）内部 400 字节的 `CombatStats` 拷贝。
3. **渲染全面收口至 `SkillVfxRecipe`**：
   - 业务逻辑全面停止直接调用 `GPUParticleSystem::Emit`，统一发射标准的 [`SkillVfxEvent`](file:///d:/PRJ/NoMoreDay/src/engine/render/SkillVfxEvent.hpp)，由既有的 [`SkillVfxRecipe`](file:///d:/PRJ/NoMoreDay/src/engine/render/GPUSkillEffectSystem.hpp) 驱动。

---

## 4. 验收标准与验证方案 (Acceptance Criteria & Verification)

### 4.1 可观察验收标准
1. **数据与存档安全**：`AffixType` 保留占位符，`PlusSkillLevelGeneric = 52` 绝不碰撞 47-51 号词缀，`NMDS` v1 存档反序列化词缀整型值 100% 保持一致；
2. **内存合规**：`AreaFieldComponent`、`BakedSkillProfile`、`TriggerRuleComponent`、`PayloadDefinition` 全部通过 `static_assert(std::is_standard_layout_v<T>)`；
3. **主干特例清空**：`SkillSystem.cpp` 移除技能 8、9、124 的硬编码分支，文件行数降低至 1600 行以下；
4. **外部系统零破坏**：`BladeMasteryService`、`GameUiSnapshotBuilder`、`PlayerHUD`、`UIRenderer` 编译通过且运行时行为完全一致；
5. **触发闭环验证**：反击触发能正确索敌攻击者坐标，且被动冷却按 `dt` 正常倒计时与恢复；
6. **递归安全**：单元测试验证 `depth = 2` 时二次派生被完全截断；
7. **热路径零堆分配**：500 连发穿透弹幕压测下，投射物生成与碰撞判定引发的堆内存分配数为 0。

### 4.2 验证命令集
- 构建命令：`.\build.bat RelWithDebInfo`
- 单元与集成测试：`ctest --test-dir build -R "nmd\.tests\.(skill|combat)" --output-on-failure`
- 性能基准测试：`build/bin/RelWithDebInfo/tests/nmd.tests.benchmark.exe --benchmark_filter="ProjectileSystemBenchmark"`
