# 天赋星盘节点效果支持修复 - 实现计划

**Track ID**: `talent_constellation_fix`
**创建日期**: 2026-01-23
**预估总工时**: 4.5h

---

## 阶段概览

| Phase | 绰号 | 核心目标 | 预估工时 |
|-------|------|---------|---------|
| 1 | **The Pipeline Patch** | 修复 `AttributePipeline` 对天赋修饰符的支持 | 1.5h | ✅ |
| 2 | **The Behavior Awakening** | 激活 `BehaviorInjectionRegistry` | 1.0h | ✅ |
| 3 | **The Validation** | 编写单元测试并验证修复 | 1.5h | ✅ |
| 4 | **The Cleanup** | 代码审查与性能验证 | 0.5h | ✅ |

---

## Phase 1: The Pipeline Patch (1.5h)

**目标**: 让 `AttributePipeline::Calculate` 正确遍历技能天赋和星盘节点的全局属性修饰符。

### 任务清单

| Task ID | 任务描述 | 依赖 | 工时 |
|---------|---------|------|------|
| 1.1 | 在 `AttributePipeline.cpp` Phase 1 区域添加技能天赋遍历逻辑 | - | 0.5h |
| 1.2 | 验证星盘节点的 `modifiers` 是否已在 AttributePipeline 中处理；若未处理则补充 | 1.1 | 0.3h |
| 1.3 | 添加 `SkillRegistry.hpp` 和 `SkillDefs.hpp` 的 include | 1.1 | 0.1h |
| 1.4 | 验证编译通过，无新增 Warning | 1.1-1.3 | 0.2h |
| 1.5 | 手动冒烟测试：分配天赋点后 `CombatStats` 变化正确 | 1.4 | 0.4h |

### 详细实现指南

**Task 1.1 - 核心修改**

在 `AttributePipeline.cpp` 的 `Calculate` 函数中，找到 `// Phase 1: Gather Modifiers` 区域（约 Line 397-420），在 `processAffixes` lambda 调用之后、`// Phase 2: Resolve Primary` 之前插入：

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
            
            // 仅处理无条件修饰符 (条件修饰符由 GetStatWithTags 动态处理)
            for (const StatModifier& mod : node.stat_modifiers) {
                if (mod.required_tags == Tag::None) {
                    ApplyStatModifier(calcs, mod.type, mod.mode, 
                                      mod.value * static_cast<float>(points));
                }
            }
        }
    }
}
```

**Task 1.2 - 星盘验证**

检查现有 `AstrolabeComponent` 处理逻辑。根据当前代码审计，`AttributePipeline` **未**遍历星盘节点的 `modifiers`，仅 `StatsSystem::GetStatWithTags` 处理了动态属性。需添加类似逻辑：

```cpp
// Phase 1.6: 处理星盘节点的全局属性修饰符
if (auto* astrolabe = registry.try_get<AstrolabeComponent>(entity)) {
    const auto& astroReg = AstrolabeRegistry::Get();
    for (uint32_t node_id : astrolabe->activated_nodes) {
        if (const auto* node = astroReg.GetNode(node_id)) {
            for (const StatModifier& mod : node->modifiers) {
                if (mod.required_tags == Tag::None) {
                    ApplyStatModifier(calcs, mod.type, mod.mode, mod.value);
                }
            }
        }
    }
}
```

### 验证目标

- [ ] 编译通过，0 个新 Warning
- [ ] 分配 +100 MaxHealth 天赋后，`CombatStats.max_health` 增加 100

---

## Phase 2: The Behavior Awakening (1.0h)

**目标**: 激活 `BehaviorInjectionRegistry`，使 `behavior_id` 字段生效。

### 任务清单

| Task ID | 任务描述 | 依赖 | 工时 |
|---------|---------|------|------|
| 2.1 | 在 `BehaviorInjectionRegistry.cpp` 的 `Init()` 中注册 `"shadow_caster"` 行为 | Phase 1 | 0.3h |
| 2.2 | 添加必要的 include (`SkillDefs.hpp`) | 2.1 | 0.1h |
| 2.3 | 创建 `BehaviorID` 命名空间，集中管理行为 ID 常量 | 2.1 | 0.2h |
| 2.4 | 验证编译通过 | 2.1-2.3 | 0.1h |
| 2.5 | 手动冒烟测试：分配含 `behavior_id` 的天赋后，对应组件被正确添加 | 2.4 | 0.3h |

### 详细实现指南

**Task 2.1 & 2.3 - 注册行为**

修改 `BehaviorInjectionRegistry.cpp`:

```cpp
#include "game/systems/skill/BehaviorInjectionRegistry.hpp"
#include "game/components/SkillDefs.hpp" // For ShadowKillArrayReady
#include <spdlog/spdlog.h>

namespace NoMoreDay {

// 行为 ID 常量定义
namespace BehaviorID {
    constexpr const char* ShadowCaster = "shadow_caster";
    // 未来扩展...
}

std::unordered_map<std::string, BehaviorInjectionRegistry::BehaviorInjector> 
    BehaviorInjectionRegistry::injectors;

void BehaviorInjectionRegistry::Register(const std::string& id, BehaviorInjector injector) {
    if (injectors.contains(id)) {
        spdlog::warn("BehaviorInjectionRegistry: Overwriting injector for ID '{}'", id);
    }
    injectors[id] = std::move(injector);
}

void BehaviorInjectionRegistry::Apply(const std::string& id, entt::registry& registry, entt::entity entity) {
    if (id.empty()) return;

    auto it = injectors.find(id);
    if (it != injectors.end()) {
        it->second(registry, entity);
        spdlog::debug("BehaviorInjectionRegistry: Applied behavior '{}' to entity {}", 
                      id, static_cast<uint32_t>(entity));
    } else {
        spdlog::warn("BehaviorInjectionRegistry: Unknown behavior ID '{}'", id);
    }
}

void BehaviorInjectionRegistry::Init() {
    if (!injectors.empty()) return;

    // --- 核心行为注册 ---
    
    // 影分身施法: 标记实体，使其下次技能释放触发分身复制
    Register(BehaviorID::ShadowCaster, [](entt::registry& r, entt::entity e) {
        r.get_or_emplace<ShadowKillArrayReady>(e);
    });

    spdlog::info("BehaviorInjectionRegistry: Initialized with {} behaviors", injectors.size());
}

} // namespace NoMoreDay
```

### 验证目标

- [ ] 编译通过
- [ ] 日志输出 "Initialized with 1 behaviors"
- [ ] 分配 `shadow_caster` 天赋后，实体拥有 `ShadowKillArrayReady` 组件

---

## Phase 3: The Validation (1.5h)

**目标**: 编写自动化测试，确保修复的正确性和回归安全性。

### 任务清单

| Task ID | 任务描述 | 依赖 | 工时 |
|---------|---------|------|------|
| 3.1 | 创建 `tests/unit/TalentModifierTest.cpp` | Phase 1-2 | 0.5h |
| 3.2 | 编写 Test Case: `TalentStatModifier_MaxHealth_Applied` | 3.1 | 0.2h |
| 3.3 | 编写 Test Case: `TalentStatModifier_Armor_Scaled` | 3.1 | 0.2h |
| 3.4 | 编写 Test Case: `BehaviorInjection_ShadowCaster_Applied` | 3.1 | 0.2h |
| 3.5 | 编写 Test Case: `AstrolabeModifier_MoveSpeed_Applied` | 3.1 | 0.2h |
| 3.6 | 运行所有测试，确保通过 | 3.1-3.5 | 0.2h |

### 测试骨架

```cpp
// tests/unit/TalentModifierTest.cpp
#include <doctest/doctest.h>
#include <entt/entt.hpp>
#include "game/systems/stats/AttributePipeline.hpp"
#include "game/systems/skill/BehaviorInjectionRegistry.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include "game/data/SkillRegistry.hpp"

using namespace NoMoreDay;

TEST_SUITE("TalentModifierTest") {

    TEST_CASE("TalentStatModifier_MaxHealth_Applied") {
        entt::registry registry;
        auto player = registry.create();
        
        // Setup: 创建带有 MaxHealth 修饰符的天赋节点
        SkillData skill;
        skill.id = 999;
        skill.name_key = "TestSkill";
        SkillRegistry::Get().RegisterSkill(skill);
        
        SkillTreeDefinition tree;
        tree.skill_id = 999;
        TalentNode node;
        node.id = 99901;
        node.name_key = "Test Node";
        node.max_points = 3;
        node.stat_modifiers.push_back({
            .value = 50.0f,
            .type = StatType::MaxHealth,
            .mode = ModifierMode::Flat
        });
        tree.nodes[99901] = node;
        // TODO: Register tree to SkillRegistry
        
        // Setup player
        registry.emplace<ActiveSkillsComponent>(player);
        auto& active = registry.get<ActiveSkillsComponent>(player);
        active.specialized_slots[0].skill_id = 999;
        active.specialized_slots[0].allocated_points[99901] = 2; // 2 points
        
        // Act
        AttributePipeline::Calculate(registry, player);
        
        // Assert
        auto& stats = registry.get<CombatStats>(player);
        // Base 100 + 50 * 2 = 200
        CHECK(stats.max_health >= 200.0f);
    }
    
    TEST_CASE("BehaviorInjection_ShadowCaster_Applied") {
        entt::registry registry;
        auto player = registry.create();
        
        BehaviorInjectionRegistry::Init();
        BehaviorInjectionRegistry::Apply("shadow_caster", registry, player);
        
        CHECK(registry.all_of<ShadowKillArrayReady>(player));
    }
}
```

### 验证目标

- [ ] 所有 4 个测试用例通过
- [ ] 测试覆盖率：核心修复路径 100%

---

## Phase 4: The Cleanup (0.5h)

**目标**: 代码审查、性能验证与文档更新。

### 任务清单

| Task ID | 任务描述 | 依赖 | 工时 |
|---------|---------|------|------|
| 4.1 | 使用 `code-risk-analyzer` 审查修改 | Phase 3 | 0.2h |
| 4.2 | 运行 `RenderingBenchmark` 验证无性能回归 | 4.1 | 0.2h |
| 4.3 | 更新 Track 状态，关闭任务 | 4.2 | 0.1h |

### 验证目标

- [ ] `code-risk-analyzer` 无新增高风险项
- [ ] `AttributePipeline::Calculate` 耗时增加 < 5%
- [ ] 所有 73+ 测试用例通过

---

## 依赖关系图

```
Phase 1 ─────────────────────────────────────────┐
  │                                              │
  ├─ Task 1.1 ─► Task 1.3 ─► Task 1.4           │
  │      │                      │                │
  │      └─► Task 1.2 ──────────┘                │
  │                             │                │
  │                             ▼                │
  │                        Task 1.5              │
  │                                              │
Phase 2 ◄────────────────────────────────────────┘
  │
  ├─ Task 2.1 ─► Task 2.2 ─► Task 2.4
  │      │                      │
  │      └─► Task 2.3 ──────────┘
  │                             │
  │                             ▼
  │                        Task 2.5
  │
Phase 3 ◄────────────────────────────────────────
  │
  └─ Task 3.1 ─► Tasks 3.2-3.5 ─► Task 3.6
                                    │
Phase 4 ◄───────────────────────────┘
  │
  └─ Task 4.1 ─► Task 4.2 ─► Task 4.3
```

---

## 风险与缓解

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| SkillRegistry 无法在测试中动态注册 Tree | 中 | 测试无法覆盖 | 添加 `RegisterSkillTree()` 方法或使用 Mock |
| 条件修饰符被双重应用 | 低 | 数值偏高 | 仅处理 `required_tags == Tag::None` |
| 星盘与天赋修饰符冲突 | 低 | 数值异常 | 统一使用相同的 `ApplyStatModifier` 逻辑 |
