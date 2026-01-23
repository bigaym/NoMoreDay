# 天赋星盘节点效果支持修复 - 技术规格书

**Track ID**: `talent_constellation_fix`
**创建日期**: 2026-01-23
**状态**: Phase 4 - 完成

---

## 1. 问题陈述 (Problem Statement)

### 1.1 根因分析

通过 `code-risk-analyzer` 审计发现，天赋星盘系统中存在两类严重的功能缺失：

| 问题类型 | 影响范围 | 根因 |
|---------|---------|------|
| **防御/全局属性节点失效** | 所有非伤害类 `stat_modifiers` | `AttributePipeline::Calculate` 未遍历 `ActiveSkillsComponent::specialized_slots` |
| **行为注入节点失效** | 所有 `behavior_id` 字段 | `BehaviorInjectionRegistry::Init()` 为空，无注册逻辑 |

### 1.2 影响的数据流

当前架构下，天赋节点的 `stat_modifiers` 数据有两条消费路径：

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        天赋节点数据消费路径                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  TalentNode.stat_modifiers                                                  │
│       │                                                                     │
│       ├──► [路径A] StatsSystem::GetStatWithTags (动态查询)                   │
│       │         ✅ 支持: CritChance, PhysicalDamage, FireDamage, etc.       │
│       │         ✅ 原因: DamagePipeline 显式调用此函数                        │
│       │                                                                     │
│       └──► [路径B] AttributePipeline::Calculate (静态烘焙)                   │
│                 ❌ 不支持: MaxHealth, Armor, MoveSpeed, Resistances         │
│                 ❌ 原因: 该函数从未遍历 specialized_slots                    │
│                                                                             │
│  TalentNode.behavior_id                                                     │
│       │                                                                     │
│       └──► [路径C] BehaviorInjectionRegistry::Apply                         │
│                 ❌ 不支持: 所有行为 ID                                        │
│                 ❌ 原因: 注册表为空                                           │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. 技术方案 (Technical Design)

### 2.1 修复方案 A: AttributePipeline 集成

在 `AttributePipeline::Calculate` 的 Phase 1 (Gather Modifiers) 阶段，新增对技能专精天赋的遍历逻辑。

**修改位置**: `src/game/systems/stats/AttributePipeline.cpp` (约 Line 400-420)

**新增代码逻辑**:
```cpp
// Phase 1.5: 处理技能专精天赋的全局属性修饰符
if (auto* active = registry.try_get<ActiveSkillsComponent>(entity)) {
    for (const auto& specialized : active->specialized_slots) {
        if (specialized.skill_id == 0) continue;
        
        const auto* tree = SkillRegistry::Get().GetSkillTree(specialized.skill_id);
        if (!tree) continue;
        
        for (const auto& [node_id, points] : specialized.allocated_points) {
            if (points <= 0) continue;
            
            auto node_it = tree->nodes.find(node_id);
            if (node_it == tree->nodes.end()) continue;
            
            const TalentNode& node = node_it->second;
            
            // 应用 stat_modifiers (仅处理无条件修饰符)
            for (const StatModifier& mod : node.stat_modifiers) {
                if (mod.required_tags == Tag::None) {
                    // 按点数缩放
                    ApplyStatModifier(calcs, mod.type, mod.mode, 
                                      mod.value * static_cast<float>(points));
                }
            }
        }
    }
}
```

**设计决策**:
- 仅处理 `required_tags == Tag::None` 的修饰符，条件修饰符已由 `StatsSystem::GetStatWithTags` 处理
- 修饰符值按分配点数 (`points`) 线性缩放，与现有天赋点逻辑一致
- 插入位置在装备词缀处理之后、主属性转换之前

### 2.2 修复方案 B: BehaviorInjectionRegistry 激活

为通用天赋行为建立注册机制，并预注册常见的行为 ID。

**修改位置**: `src/game/systems/skill/BehaviorInjectionRegistry.cpp`

**新增 Behavior ID 定义**:
```cpp
namespace BehaviorID {
    constexpr std::string_view ShadowCaster = "shadow_caster";
    constexpr std::string_view ChainLightning = "chain_lightning";
    constexpr std::string_view BleedOnHit = "bleed_on_hit";
    constexpr std::string_view LifeLeechOnCrit = "life_leech_on_crit";
}
```

**Init() 实现**:
```cpp
void BehaviorInjectionRegistry::Init() {
    if (!injectors.empty()) return;
    
    // 影分身施法 - 技能命中时有概率触发分身
    Register("shadow_caster", [](entt::registry& r, entt::entity e) {
        r.get_or_emplace<ShadowKillArrayReady>(e);
    });
    
    // 未来扩展点...
}
```

### 2.3 星盘节点 (AstrolabeNode) 修饰符处理

当前 `AstrolabeNode` 的 `modifiers` 字段已被 `StatsSystem::GetStatWithTags` 正确处理（Line 280-287），但 `AttributePipeline` 同样需要遍历以支持全局属性。

**与技能天赋共用同一模式**，在 `AttributePipeline::Calculate` 中复用现有的星盘遍历逻辑：
```cpp
// 已存在于 StatsSystem, 需要在 AttributePipeline 中镜像
if (auto* astrolabe = registry.try_get<AstrolabeComponent>(entity)) {
    for (uint32_t node_id : astrolabe->activated_nodes) {
        if (const auto* node = AstrolabeRegistry::Get().GetNode(node_id)) {
            for (const StatModifier& mod : node->modifiers) {
                if (mod.required_tags == Tag::None) {
                    ApplyStatModifier(calcs, mod.type, mod.mode, mod.value);
                }
            }
        }
    }
}
```

---

## 3. 受影响的 StatType 清单

以下 `StatType` 在 `AttributePipeline` 中静态烘焙，**必须**通过本次修复才能支持：

| StatType | 当前状态 | 修复后状态 |
|----------|---------|-----------|
| `MaxHealth` | ❌ 仅装备/星盘 | ✅ 完整支持 |
| `MaxMana` | ❌ 仅装备/星盘 | ✅ 完整支持 |
| `Armor` | ❌ 仅装备/星盘 | ✅ 完整支持 |
| `MoveSpeed` | ❌ 仅装备/星盘 | ✅ 完整支持 |
| `ResistFire/Cold/...` | ❌ 仅装备/星盘 | ✅ 完整支持 |
| `HealthRegen` | ❌ 仅装备/星盘 | ✅ 完整支持 |
| `ManaRegen` | ❌ 仅装备/星盘 | ✅ 完整支持 |
| `BlockChance` | ❌ 仅装备/星盘 | ✅ 完整支持 |
| `DodgeChance` | ❌ 仅装备/星盘 | ✅ 完整支持 |

---

## 4. 验收清单 (Acceptance Criteria)

| ID | 验收项 | 验证方法 |
|----|-------|---------|
| AC-1 | 技能天赋树中的 `MaxHealth` 修饰符能正确增加玩家生命上限 | 单元测试：分配 +100 MaxHealth 节点后验证 `CombatStats.max_health` 增加 |
| AC-2 | 技能天赋树中的 `Armor` 修饰符能正确增加护甲值 | 单元测试：分配 +50 Armor 节点后验证 `CombatStats.armor` 增加 |
| AC-3 | 星盘节点中的 `MoveSpeed` 修饰符能正确增加移速 | 单元测试：激活移速节点后验证 `CombatStats.move_speed` 增加 |
| AC-4 | `behavior_id: "shadow_caster"` 节点能正确添加 `ShadowKillArrayReady` 组件 | 单元测试：施放技能后验证组件存在 |
| AC-5 | 修饰符按分配点数正确缩放 | 单元测试：分配 3 点 +10% 节点后验证增加 30% |
| AC-6 | 无性能回归：`AttributePipeline::Calculate` 耗时增加 < 5% | 性能测试：Benchmark 对比 |

---

## 5. 风险评估

| 风险 | 等级 | 缓解措施 |
|-----|------|---------|
| 重复计算：条件修饰符可能在两个管道中被双重应用 | 🟡 中 | 仅在 AttributePipeline 处理 `required_tags == Tag::None` 的情况 |
| 性能：额外遍历可能影响大量实体 | 🟢 低 | 仅 `StatsDirty` 实体触发，且天赋数据结构紧凑 |
| 兼容性：已分配的天赋点可能导致"数值跳变" | 🟢 低 | 属于 Bug 修复，数值变化符合预期 |
