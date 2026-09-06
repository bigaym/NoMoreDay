# 现代化技能系统重构实施计划 (Modern Skill System Implementation Plan)

- 日期：2026-09-06
- 状态：终审通过 / 准入实施 (Approved & Ready for Implementation)
- 对应设计文档：[`docs/designs/2026-09-06-modern-skill-system-design.md`](file:///d:/PRJ/NoMoreDay/docs/designs/2026-09-06-modern-skill-system-design.md)
- 前置流程文档：`docs/workflows/planning.md`
- 代码规范与硬规则约束：`conductor/code_standard.md` (V2.1)

---

## 1. 实施思路与核心原理 (Implementation Rationale & Principles)

### 1.1 数据层：纯 POD 烘焙、二进制存档兼容与独立分段持久化
- **现有痛点与存档约束**：[`ItemStats.hpp`](file:///d:/PRJ/NoMoreDay/src/game/foundation/components/ItemStats.hpp) 硬编码具体技能；项目采用 `ItemPersistenceCodec`（`NMDS` v1 格式），直接持久化 192 字节 POD 的 `ItemInstance`（含 `CompactAffix::type` 整数值）。直接删除枚举会导致 47 号 `TitanGrip` 及之后词缀数值错乱，破坏二进制存档。
- **解决原理**：
  1. `AffixType` 保留数值占位符（`Deprecated_PlusFlowingThrust = 45`, `Deprecated_PlusRendingWave = 46`），新增泛型词缀分配至安全空闲数值 `PlusSkillLevelGeneric = 52`，彻底锁定枚举数值稳定性；
  2. 装备技能修饰器 `ItemSkillModifier` 接入独立持久化分段 `SectionType::ItemSkillModifiers = 13`（并分配脏标记 `ContainerDirtyFlags::ItemSkillModifiers = 1 << 13`），旧存档反序列化时自动跳过，不污染 192 字节 `ItemInstance` 结构；
  3. 在角色穿脱装备与调整天赋时，生成纯 POD 的 `BakedSkillProfile`，缓存于实体的 `ActiveSkillsComponent`（[`SkillDefs.hpp:510`](file:///d:/PRJ/NoMoreDay/src/game/foundation/components/SkillDefs.hpp#L510)）中。施法路径只读取烘焙常数，**Zero runtime overhead**。

### 1.2 施法与触发总线：两阶段安全派发 (Collect-Then-Execute)、目标追踪与冷却更新闭环
- **安全红线**：严守 `code_standard.md` §5.3（EnTT 安全）与硬否决规则 8，严禁跨实体操作持有组件指针。
- **解决原理**：
  1. 彻底复用 [`src/game/contracts/CombatEvents.hpp:14-59`](file:///d:/PRJ/NoMoreDay/src/game/contracts/CombatEvents.hpp#L14-L59) 的 `NoMoreDay::CombatEventType` 与单例 [`CombatEventDispatcher`](file:///d:/PRJ/NoMoreDay/src/game/contracts/impl/CombatEventDispatcher.hpp)；
  2. 构造纯 POD 的 `TriggerRuleComponent`（使用定长 `std::array`，零堆分配）；
  3. `ProcEngine::DispatchEvent` 采用**两阶段派发模式**：
     - **阶段一**：在局部作用域内安全读取规则，完整解析并捕获目标实体 `target_entity` 与空间坐标 `target_pos`，暂存待触发动作至栈数组 `std::array<PendingAction, 4>`；
     - **阶段二**：脱离组件指针后执行派生施法，并在循环中追加 `if (!reg.valid(listener)) break;` 活性检查，彻底杜绝 UAF 崩溃与目标丢失；
  4. 建立 `ProcEngine::UpdateCooldowns(reg, dt)` 并接入 `SkillSystem::UpdateCooldowns`，确保被动冷却计时器按帧正常递减，杜绝触发一次后永久失效的死锁；
  5. 改造 `DamagePipeline.cpp`（`DispatchSingleDamageEvents`），在派发战斗事件时依据发起技能注入 `parent_skill_cd`，确保恐怖黎明自适应加权公式具备真实的数据输入。

### 1.3 交付与载荷正交管线：纯 POD 组件化与外部消费方闭环
- **解决原理**：
  1. 提炼通用的 `AreaFieldDeliverySystem` 与纯 POD 的 `AreaFieldComponent`（包含 `owner`、`cast_id`、`source_skill_id` 与定长 `std::array<PayloadDefinition, 4>`，满足 Standard Layout 与零堆分配）；
  2. 迁移天剑降临与血海，同步适配 [`BladeMasteryService.cpp`](file:///d:/PRJ/NoMoreDay/src/game/systems/skill/BladeMasteryService.cpp)、[`GameUiSnapshotBuilder.cpp`](file:///d:/PRJ/NoMoreDay/src/game/application/ui/GameUiSnapshotBuilder.cpp) 与 [`PlayerHudController.cpp:230-233`](file:///d:/PRJ/NoMoreDay/src/game/application/ui/PlayerHudController.cpp#L230-L233)，确保 HUD "Miasma Pressure" 等状态反馈连贯；
  3. 提炼 `BeamChannelDeliverySystem`，剥离万剑归宗（技能 5）与斩心剑（技能 7）的引导更新逻辑；
  4. 改造 [`UIRenderer.cpp:1074-1081`](file:///d:/PRJ/NoMoreDay/src/game/application/ui/UIRenderer.cpp#L1074-L1081)，打通 Tooltip 消费动态预览数据。

### 1.4 DOD 性能硬化与伤害管线贯通
- **解决原理**：
  1. 投射物穿透缓存利用固定 8 槽位栈数组（SBO），并明确单发穿透上限 `max_pierce <= 8` 的防护语义，杜绝溢出后的多重打击伤害漏洞；
  2. 引入 64 字节对齐的 `DamagePayloadContext`，在 [`DamagePipelineTypes.hpp`](file:///d:/PRJ/NoMoreDay/src/game/contracts/DamagePipelineTypes.hpp) 的 `DamageRequest` 中正式接入该字段，彻底消除投射物与残影（`ShadowComponent`）中 400 字节 `CombatStats` 的拷贝；
  3. 属性转化统一遵循既有的 `Constants::Combat::Conversion::CONVERSION_ORDER`。

---

## 2. 关键接口草图与伪代码引导 (Pseudocode & Interface Sketches)

### 2.1 词缀、纯 POD 载荷与烘焙表
```cpp
// 在 ItemStats.hpp 中锁定枚举数值
enum class AffixType : uint16_t {
    // ...
    PlusAllSkills = 44,
    Deprecated_PlusFlowingThrust = 45, // 占位符：保留数值稳定
    Deprecated_PlusRendingWave = 46,   // 占位符：保留数值稳定
    TitanGrip = 47,                    // 保持既有定义
    FlatDodgeRating = 48,
    PercentDodgeRating = 49,
    FlatBlockRating = 50,
    PercentBlockRating = 51,
    PlusSkillLevelGeneric = 52,        // 泛型加级 (安全空闲数值)
    // ...
};

// 基础载荷定义 (纯 POD / Standard Layout)
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

// 纯 POD 实体烘焙属性表
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

### 2.2 两阶段安全触发派发 (Collect-Then-Execute) 与冷却更新
```cpp
// 目标选择策略枚举
enum class TriggerTargetPolicy : uint8_t {
    Self = 0,
    Victim,
    Attacker,
    GroundTarget
};

// 纯 POD 触发规则组件
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

struct TriggerRuleComponent {
    static constexpr uint8_t kMaxRules = 8;
    std::array<TriggerRule, kMaxRules> rules{};
    uint8_t rule_count = 0;
};
static_assert(std::is_standard_layout_v<TriggerRuleComponent>);
static_assert(std::is_trivially_destructible_v<TriggerRuleComponent>);

// 两阶段安全派发：完整携带目标实体、坐标与活性检查
void ProcEngine::DispatchEvent(entt::registry& reg, entt::entity listener, const CombatEvent& event) {
    if (event.trigger_depth >= 2) return; // 递归深度保护
    
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
    } // 此时 triggerComp 指针完全脱离生命周期，后续组件池重分配无任何 UAF 风险
    
    // 阶段二：安全派生施法，执行严格的实体有效性检查
    for (uint8_t i = 0; i < action_count; ++i) {
        if (!reg.valid(listener)) break; // 施法者消亡立即中断
        const auto& act = pending_actions[i];
        SkillSystem::TriggerCast(reg, listener, act.skill_id, act.target_entity, act.target_pos, act.depth, act.effectiveness);
    }
}

// 冷却递减系统 (由 SkillSystem::UpdateCooldowns 调用)
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

### 2.3 通用地表领域组件 (AreaFieldComponent)
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

### 2.4 投射物穿透防多重打击与伤害管线贯通
```cpp
// 投射物穿透 SBO 与多重打击防护
struct Projectile {
    static constexpr uint8_t kMaxInlineHits = 8;
    std::array<entt::entity, kMaxInlineHits> hit_cache{};
    uint8_t hit_count = 0;
    uint8_t max_pierce = 8; // 规则级穿透上限
    
    bool HasHit(entt::entity target) const {
        for (uint8_t i = 0; i < hit_count; ++i) {
            if (hit_cache[i] == target) return true;
        }
        return false;
    }
    
    bool RecordHit(entt::entity target) {
        if (hit_count < kMaxInlineHits && hit_count < max_pierce) {
            hit_cache[hit_count++] = target;
            return (hit_count >= max_pierce); // 返回 true 表示达到上限，应销毁
        }
        return true; // 已达上限，强制阻断穿透
    }
};

// 伤害管线 DamageRequest 扩充
struct DamageRequest {
    // 既有字段...
    std::optional<DamagePayloadContext> payload_context; // 优先消费快照上下文
};
```

---

## 3. 原子任务拆分与阶段路线图 (Atomic Task Breakdown)

### Phase 1: 触发机制通用化与主干特例剥离 (Trigger Engine Decoupling)
- [ ] **Task 1.1**: 定义纯 POD 的 `TriggerRuleComponent`、`TriggerTargetPolicy` 并接入既有的 `NoMoreDay::CombatEventType`（`src/game/foundation/components/TriggerRuleComponent.hpp`）；在 `SkillSystem::UpdateCooldowns` 中挂接 `ProcEngine::UpdateCooldowns`；改造 `DamagePipeline.cpp`（`DispatchSingleDamageEvents`）注入发起技能的 `parent_skill_cd`
- [ ] **Task 1.2**: 在受击回调 `SkillSystem::OnTakeDamage` 中剥离技能 9（幻影闪反击）与技能 8（飞剑 CD 减免）的硬编码特例，改写为两阶段安全触发派发，支持受击者与攻击者坐标自动索敌
- [ ] **Task 1.3**: 在 `SkillSystem::TryCast` 中剥离影杀阵（124）硬编码残影复制特例，重构为前置派生施法钩子
- [ ] **Task 1.4**: 实现 `trigger_depth <= 2` 递归深度守卫与自适应 CD 加权几率算法，编写 `tests/unit/TriggerRuleTests.cpp` 验证递归截断、冷却递减与两阶段指针安全

### Phase 2: 装备技能修饰器与数据层解耦 (Item Skill Modifiers & Data Decoupling)
- [ ] **Task 2.1**: 重构 `ItemStats.hpp`，保留 `Deprecated_PlusFlowingThrust = 45` 与 `Deprecated_PlusRendingWave = 46` 占位符，新增 `PlusSkillLevelGeneric = 52`，保证 `NMDS` v1 二进制存档兼容
- [ ] **Task 2.2**: 在 `ItemPersistenceCodec` 中新增独立的 `SectionType::ItemSkillModifiers = 13`（分配 `ContainerDirtyFlags::ItemSkillModifiers = 1 << 13`），支持装备修饰器持久化，保持 192 字节 `ItemInstance` POD 不变
- [ ] **Task 2.3**: 实现 `SkillSystem::RebakeSkillProfiles`，在角色换装与天赋变更时计算并缓存纯 POD 的 `BakedSkillProfile` 至实体的 `ActiveSkillsComponent`
- [ ] **Task 2.4**: 扩展 `SkillDisplayPreviewService.cpp`，并同步改造 `src/game/application/ui/UIRenderer.cpp:1074-1081`，使 UI Tooltip 能够消费动态 Preview 渲染实时冷却缩减、投射物增加和形态转化

### Phase 3: 交付-载荷正交管线重构 (Delivery & Payload Pipeline)
- [ ] **Task 3.1**: 提取通用的 `AreaFieldDeliverySystem` 与纯 POD 的 `AreaFieldComponent`（包含 `owner`、`cast_id`、`source_skill_id` 与定长 `payloads`）
- [ ] **Task 3.2**: 迁移天剑降临与血海至通用地表领域系统，废弃专用冗余组件；同步更新 `BladeMasteryService.cpp`、`GameUiSnapshotBuilder.cpp` 与 `PlayerHudController.cpp:230-233`，确保领域进度与 HUD "Miasma Pressure" 状态反馈连贯
- [ ] **Task 3.3**: 提取 `BeamChannelDeliverySystem`，将引导技能（技能 5、7）的更新逻辑从 `SkillSystem::UpdateStates` 中剥离
- [ ] **Task 3.4**: 剥离 `behaviors/*.cpp` 相互 include 的私有调用，改为通过通用事件与载荷分发交互

### Phase 4: DOD 性能硬化与渲染契约收口 (DOD & VFX Finalization)
- [ ] **Task 4.1**: 改造 `Projectile.hpp`，将 `hitEntities` 替换为固定 8 槽位栈缓存 `hit_cache`，并引入 `max_pierce <= 8` 多重打击防护
- [ ] **Task 4.2**: 引入 64 字节对齐的 `DamagePayloadContext`；扩展 `DamageRequest` 并在 `DamagePipeline::Calculate` 中优先读取；将投射物与 `ShadowComponent` 中的 400 字节 `CombatStats` 深拷贝彻底移除
- [ ] **Task 4.3**: 将所有技能 ID、Buff ID、Trigger 监听标签统一采用 `uint32_t`（`entt::hashed_string`），严格禁止核心热循环字符串操作
- [ ] **Task 4.4**: 将业务层散落的粒子发射代码收口为标准 `SkillVfxEvent`，由既有的 `SkillVfxRecipe` 驱动表现

### Phase 5: 全量回归与性能基线验收 (Regression & Verification)
- [ ] **Task 5.1**: 运行现有全量单元与集成测试，验证重构后 12 个基础技能行为与 UI 表现零破坏
- [ ] **Task 5.2**: 运行 `tests/performance/ProjectileSystemBenchmark.cpp`，对比发弹压测下的 P95 耗时与内存分配情况

---

## 4. 测试方法与验证证据 (Testing Methodology)

### 4.1 测试层级与覆盖
1. **单元测试 (Unit Tests)**:
   - `TriggerRuleTests`: 验证既有 `CombatEventType` 的触发响应、ICD 计时、`dt` 冷却倒计时、索敌坐标解析与自适应几率缩放；
   - `RecursionDepthTests`: 构造 A->B->A 循环触发配置，验证在 `depth=2` 时被强行截断，无死锁；
   - `ItemSkillModifierTests`: 验证装备穿脱前后技能 CD、投射物数量、动态标签转化的正确烘焙；
   - `ItemPersistenceCodecTests`: 验证新增 `SectionType::ItemSkillModifiers = 13` 后双向编解码一致性及旧存档向前兼容性；
   - `AreaFieldDeliveryTests`: 验证通用地表领域的脉冲节奏、`owner` 归属与生命周期。
2. **集成测试 (Integration Tests)**:
   - `nmd.tests.skill`: 验证 12 个原有技能在重构后与怪物的实机交互表现保持一致；
   - `nmd.tests.combat`: 验证伤害转化链与异常状态挂载的一致性。
3. **性能基准测试 (Performance Baseline Tests)**:
   - `ProjectileSystemBenchmark`: 连续发射 500 个带穿透投射物，监控堆内存分配是否为 0，单帧耗时是否在预算内 (< 2ms)。

### 4.2 执行命令集
- 构建命令：`.\build.bat RelWithDebInfo`
- 测试运行：`ctest --test-dir build -R "nmd\.tests\.(skill|combat)" --output-on-failure`
- 性能验证：`build/bin/RelWithDebInfo/tests/nmd.tests.benchmark.exe --benchmark_filter="ProjectileSystemBenchmark"`

---

## 5. 验收标准与退出门禁 (Exit Criteria)

1. **编译通过**：工程在 `RelWithDebInfo` 配置下零警告/零错误编译通过；
2. **测试全绿**：原有技能测试集及新增的触发、装备修饰器、SBO 单元测试 100% 绿色通过；
3. **架构合规**：
   - `ItemStats.hpp` 保留枚举数值占位符，`PlusSkillLevelGeneric = 52` 绝不碰撞既有词缀，`NMDS` v1 二进制存档兼容；
   - 所有组件通过 Standard Layout 断言，无任何嵌入堆分配容器；
   - 特例清零与架构解耦：主干彻底消除技能 8、9、124 的特例分支，解耦移交至 ProcEngine、ShadowDuplicationHook、BeamChannel 与 AreaField 系统；以特例彻底剥离为准，避免强拆核心执行状态机；
   - 核心战斗循环中无字符串比较（严格遵守 `code_standard.md` §7.2）；
4. **性能达标**：投射物穿透高频压测下，零额外堆内存分配。
