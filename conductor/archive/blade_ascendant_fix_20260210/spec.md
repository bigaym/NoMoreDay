# 剑修技能行为修复与补全 - 技术规格

## 1. 概述

基于 `conductor/reviews/blade_ascendant_review_20260210.md` 审查报告中发现的三类核心问题，对 `src/game/systems/skill/behaviors/` 下所有剑修技能行为文件进行系统性修复。

### 1.1 设计目标

| 目标 | 描述 | 优先级 |
|------|------|--------|
| **ID 常量化重构** | 消除全部 Behavior 文件中的魔法数字，统一使用 `constexpr` 命名空间 | **P0 URGENT** |
| **BladeFormation ID 修正** | 修正 `311`(应为震波or无尽剑匣) 和 `321`(应为351) 的逻辑错位 | **P0 URGENT** |
| **InfiniteBlades ID 清理** | 消除 551/520 混淆与冗余检查 | **P0 HIGH** |
| **元素系统补全** | 实现通用元素转换辅助函数，补全各技能缺失的 Fire/Lightning 分支 | **P1 MEDIUM** |
| **缺失机制实现** | 补全风行者碰撞穿透、要害感知、分裂弹道数据 | **P1 MEDIUM** |
| **自动化 ID 校验** | 编写测试用例，确保 C++ 中引用的每个天赋 ID 在 `skills.json` 中存在 | **P2 LOW** |

### 1.2 影响范围 (Blast Radius)

| 文件 | 操作类型 | 风险 |
|------|----------|------|
| `behaviors/BladeFormation.cpp` | **修改** - 添加命名空间 + 修正 ID 映射 | 🔴 高 (逻辑错位) |
| `behaviors/RendingWave.cpp` | **修改** - 添加命名空间 + 补全 270 元素分支 | 🟡 中 |
| `behaviors/InfiniteBlades.cpp` | **修改** - 添加命名空间 + 清理冗余 | 🟡 中 |
| `behaviors/FlowingThrust.cpp` | **修改** - 补全 113/150/170 实现 | 🟡 中 |
| `behaviors/BladeWard.cpp` | **修改** - 添加命名空间 | 🟢 低 |
| `behaviors/SwordArray.cpp` | **修改** - 添加命名空间 | 🟢 低 |
| `behaviors/PhantomFlash.cpp` | **修改** - 添加命名空间 | 🟢 低 |
| `behaviors/MindBlade.cpp` | **修改** - 添加命名空间 | 🟢 低 |
| `behaviors/BladeBoomerang.cpp` | **修改** - 添加命名空间 | 🟢 低 |
| `behaviors/SkillBehaviorBase.hpp` | **修改** - 添加 `ApplyElementalConversion` 辅助 | 🟡 中 (基类变更) |
| `tests/integration/GameplaySystems.cpp` | **修改** - 添加 ID 校验测试 | 🟢 低 |

### 1.3 不可触碰文件 (Do NOT Modify)

- `assets/data/skills.json` — JSON 是 **Source of Truth**，代码必须适配 JSON，而非反向修改。
- `game/systems/skill/SkillSystem.hpp` — 核心调度系统，不在本 Track 范围内。
- `game/systems/combat/CombatSystem.hpp` — 战斗管线，不在范围内。
- `game/data/SkillRegistry.hpp` — 数据注册表，仅做只读引用。

---

## 2. 风险 1: ID 魔法数字与逻辑错位

### 2.1 问题现状

**BladeFormation.cpp (Skill 3)** 中直接使用数字进行天赋判断：

```cpp
// ❌ 当前代码 (错误)
if (exec.active_nodes.test(311 % 100))   // 代码认为是 "Shockwave on Crit"
    formation.shockwave_on_crit = true;  // 但 JSON 311 = "无尽剑匣" (剑数翻倍)

if (exec.active_nodes.test(321 % 100))   // 代码认为是 "Mana on Hit"
    formation.mana_on_hit = true;        // 但 JSON 中 Skill 3 无 321，应为 351 "气劲回流"
```

**JSON 真实映射 (Skill 3 talent_tree)**:

| JSON ID | JSON Name     | 代码中误用 ID | 代码功能        | 实际冲突              |
|---------|---------------|---------------|-----------------|----------------------|
| 311     | 无尽剑匣       | 311           | Shockwave       | ❌ 功能完全不匹配     |
| 351     | 气劲回流       | 321           | Mana on Hit     | ❌ ID 不存在于 JSON   |
| 330     | 巨剑降临       | 330           | Giant Sword     | ✅ 正确              |
| 352     | 剑影随行       | 352           | Melee Orbit     | ✅ 正确              |
| 353     | 不灭剑魂       | 353           | Immortality     | ✅ 正确              |

### 2.2 技术方案

#### 2.2.1 constexpr 命名空间模式 (参考 FlowingThrust.cpp)

所有 Behavior 文件必须在类定义前声明 `constexpr` ID 命名空间：

```cpp
// ✅ 标准模式 (每个文件必须遵循)
namespace BladeFormationNodes {
// === 基础 ===
constexpr uint32_t SwordPool        = 300; // 剑池
constexpr uint32_t SwiftIntent      = 301; // 疾风意
// === 索敌分支 (左上) ===
constexpr uint32_t SearchRange      = 310; // 索敌范围
constexpr uint32_t InfiniteSheath   = 311; // 无尽剑匣 (剑数翻倍, 伤害-40%)
constexpr uint32_t SpiritInfusion   = 312; // 灵力灌注
constexpr uint32_t Godspeed         = 313; // 神速
// === 巨剑分支 (右上) ===
constexpr uint32_t GiantSword       = 330; // 巨剑降临
constexpr uint32_t WeakpointLock    = 331; // 弱点锁定
constexpr uint32_t DeadlyEdge       = 332; // 致命锋芒
constexpr uint32_t SwordPressure    = 333; // 剑压
// === 防御分支 (左下) ===
constexpr uint32_t SpiritGuard      = 350; // 灵剑护体
constexpr uint32_t QiReflow         = 351; // 气劲回流 (命中回蓝)
constexpr uint32_t ShadowTrack      = 352; // 剑影随行 (近战环绕)
constexpr uint32_t Immortality      = 353; // 不灭剑魂
// === 元素分支 (右下) ===
constexpr uint32_t ElementEnchant   = 370; // 元素附魔
constexpr uint32_t SpiritCharge     = 371; // 灵剑充能
constexpr uint32_t SpellResonance   = 372; // 法术共鸣
constexpr uint32_t AllBladesReturn  = 373; // 万剑归宗
} // namespace BladeFormationNodes
```

#### 2.2.2 BladeFormation.cpp 修正映射

修正后的代码：
```cpp
// ✅ 修正后
// Node 311 在 JSON 中是 "无尽剑匣" (Infinite Sheath): 剑数翻倍, 伤害-40%
if (exec.active_nodes.test(BladeFormationNodes::InfiniteSheath % 100)) {
    formation.max_swords *= 2;
    formation.damage_penalty = 0.6f; // -40% 伤害
}

// Node 351 在 JSON 中是 "气劲回流" (Qi Reflow): 命中回蓝
if (exec.active_nodes.test(BladeFormationNodes::QiReflow % 100))
    formation.mana_on_hit = true;
```

**注意**: 原 `shockwave_on_crit` 功能在 JSON 中**没有对应的天赋节点**。应当移除，或在未来版本中重新设计节点承载此效果。

---

## 3. 风险 2: 元素分支缺失

### 3.1 问题现状

设计文档每个技能都有"元素转化"分支，但实现情况：

| 技能 | 元素节点 ID | JSON 定义 | 代码实现 |
|------|------------|-----------|---------|
| Skill 1 流云刺 | 170 | 火/冰/雷 | ❌ 仅 Frost |
| Skill 2 裂空斩 | 270 | 元素形态 | ❌ 完全缺失 |
| Skill 3 灵剑决 | 370 | 元素附魔 | ❌ 完全缺失 |
| Skill 4 剑气护体 | 470 | 反制剑气 | 需验证 |
| Skill 5 万剑归宗 | 570 | 需验证 | 需验证 |

### 3.2 技术方案：通用元素转换辅助函数

在 `SkillBehaviorBase.hpp` 中添加静态辅助，所有 Behavior 共用：

```cpp
// SkillBehaviorBase.hpp - 新增
struct ElementalConversion {
    Tag source_element = Tag::Physical;
    Tag target_element = Tag::None;
    Color projectile_color = WHITE;
    Color glow_color = WHITE;
    
    [[nodiscard]] bool IsActive() const { return target_element != Tag::None; }
};

// 根据天赋点数和元素节点 ID 解析转化类型
// 约定: X70 = 元素转化根节点, points 1=Fire, 2=Ice, 3=Lightning
static ElementalConversion ResolveElementalConversion(
    uint32_t element_node_id, int points) 
{
    ElementalConversion conv;
    conv.source_element = Tag::Physical;
    switch (points) {
        case 1: // Fire
            conv.target_element = Tag::Fire;
            conv.projectile_color = {255, 80, 20, 255};
            conv.glow_color = {255, 160, 60, 180};
            break;
        case 2: // Ice / Frost
            conv.target_element = Tag::Cold;
            conv.projectile_color = {100, 200, 255, 255};
            conv.glow_color = {150, 220, 255, 180};
            break;
        case 3: // Lightning
            conv.target_element = Tag::Lightning;
            conv.projectile_color = {200, 180, 255, 255};
            conv.glow_color = {230, 200, 255, 180};
            break;
        default:
            break;
    }
    return conv;
}
```

### 3.3 使用方式 (以 RendingWave 为例)

```cpp
// RendingWave.cpp - 在天赋分支逻辑中
ElementalConversion elemConv;
if (spec.allocated_points.contains(RendingWaveNodes::ElementForm)) {
    int pts = spec.allocated_points.at(RendingWaveNodes::ElementForm);
    elemConv = ResolveElementalConversion(RendingWaveNodes::ElementForm, pts);
}

// 创建投射物时
if (elemConv.IsActive()) {
    auto &mods = registry.emplace<SkillModifierComponent>(proj_ent);
    mods.damage_modifiers.push_back(
        DamageModifier{elemConv.source_element, elemConv.target_element, 
                       1.0f, ModifierType::Convert});
    registry.emplace<ColorComponent>(proj_ent, elemConv.projectile_color);
}
```

---

## 4. 风险 3: 缺失机制

### 4.1 FlowingThrust [113] 风行者 - 无视体积碰撞

当前仅实现移速加成。需要在突刺期间暂时添加 `PhaseTag` 组件，使物理系统跳过此实体的碰撞检测。

```cpp
// FlowingThrust.cpp DoCast 中
if (exec.active_nodes.test(FlowingThrustNodes::Windrunner % 100)) {
    // 移速加成 (已有)
    // 追加：突刺期间无视碰撞
    registry.emplace_or_replace<PhaseTag>(owner);
    // 需要在突刺结束后移除 PhaseTag (通过 DoEnd 或定时器)
}
```

**前提**: 需确认 `PhaseTag` 在物理系统中是否已被检查。若不存在，需在 `TilemapCollisionSystem` 中添加对此 Tag 的跳过逻辑。

### 4.2 FlowingThrust [150] 要害感知 - 暴击率翻倍

```cpp
// FlowingThrust.cpp DoHit 中
if (exec.active_nodes.test(FlowingThrustNodes::VitalSense % 100)) {
    auto* targetStats = registry.try_get<CombatStats>(target);
    auto* targetHP = registry.try_get<Health>(target);
    if (targetHP) {
        bool isFullHP = (targetHP->current >= targetHP->max * 0.99f);
        bool isCC = registry.any_of<StunnedTag, FrozenTag, RootedTag>(target);
        if (isFullHP || isCC) {
            // 暴击率翻倍 - 通过临时修改 snapshot
            // 或直接在 hit 计算时传入 flag
        }
    }
}
```

### 4.3 RendingWave [211] 碎裂之刃 - 分裂子弹道

当前仅设置 `OnDeathBehavior::Split` 和 `split_count = 3`，但未定义子弹道的伤害倍率、速度、半径等参数。

```cpp
// 需要在 Projectile 组件中确保 Split 行为有默认的子弹道参数
// 或在 ProjectileSystem 的 Split 处理逻辑中使用合理默认值
proj.split_count = 3;
proj.split_damage_mult = 0.5f;   // 新增字段
proj.split_speed_mult = 0.8f;    // 新增字段
proj.split_radius_mult = 0.6f;   // 新增字段
```

---

## 5. 验收标准

- [ ] **AC-ID-1**: 所有 9 个 Behavior 文件使用 `constexpr` 命名空间定义 ID，零魔法数字。
- [ ] **AC-ID-2**: `BladeFormation.cpp` 中 311 映射到"无尽剑匣"效果，351 映射到"气劲回流"效果。
- [ ] **AC-ID-3**: `InfiniteBlades.cpp` 中消除 551/520 混淆，无冗余检查。
- [ ] **AC-ELEM-1**: `SkillBehaviorBase.hpp` 提供 `ResolveElementalConversion()` 辅助函数。
- [ ] **AC-ELEM-2**: Skill 1 (流云刺), Skill 2 (裂空斩), Skill 3 (灵剑决) 元素分支完整 (Fire/Ice/Lightning)。
- [ ] **AC-MECH-1**: FlowingThrust [113] 风行者在突刺期间无视体积碰撞。
- [ ] **AC-MECH-2**: FlowingThrust [150] 要害感知对满血/受控敌人暴击率翻倍。
- [ ] **AC-TEST-1**: 集成测试用例加载 `skills.json`，验证所有 C++ 引用 ID 存在于 JSON。
- [ ] **AC-BUILD**: 全量编译通过，零 Warning（与本 Track 相关）。
