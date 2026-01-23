# 技能专精天赋节点行为实现 - 实现计划

**Track ID**: `talent_behavior_implementation`
**创建日期**: 2026-01-23
**预估总工时**: 8.5h

---

## 阶段概览

| Phase | 绰号 | 核心目标 | 预估工时 | 状态 |
|-------|------|---------|---------|------|
| 1 | **The ID Alignment** | 修复流云刺 ID 映射不一致 | 1.0h | ⬜ 待开始 |
| 2 | **The Lifecycle Hook** | 实现投射物生命周期回调机制 | 2.5h | ⬜ 待开始 |
| 3 | **The Split & Explode** | 实现碎裂/爆炸类天赋 | 1.5h | ⬜ 待开始 |
| 4 | **The Hover Zone** | 实现回旋悬停类天赋 | 1.5h | ⬜ 待开始 |
| 5 | **The Melee Orbit** | 实现灵剑近战模式 | 1.0h | ⬜ 待开始 |
| 6 | **The Validation** | 编写测试并验证 | 1.0h | ⬜ 待开始 |

---

## Phase 1: The ID Alignment (1.0h)

**目标**: 修复流云刺 (Skill 1) 中 3 个核心天赋的 ID 映射错误。

### 任务清单

| Task ID | 任务描述 | 依赖 | 工时 |
|---------|---------|------|------|
| 1.1 | 创建 `FlowingThrustNodes` 命名空间，定义所有节点 ID 常量 | - | 0.2h |
| 1.2 | 修正 `留影 (Shadow)` 检查：`120 → 130` | 1.1 | 0.1h |
| 1.3 | 修正 `势如破竹 (Momentum)` 检查：`114 → 112` | 1.1 | 0.1h |
| 1.4 | 修正 `风行者 (Windrunner)` 检查：`112 → 113` | 1.1 | 0.1h |
| 1.5 | 修正其他 FlowingThrust 中的硬编码 ID | 1.1 | 0.2h |
| 1.6 | 验证编译通过，运行现有测试 | 1.2-1.5 | 0.3h |

### 详细实现指南

**Task 1.1 - 创建 ID 常量命名空间**

在 `FlowingThrust.cpp` 顶部（namespace NoMoreDay::skills 内）添加：

```cpp
namespace FlowingThrustNodes {
    // 基础分支
    constexpr uint32_t Speed = 100;        // 迅捷之刃
    constexpr uint32_t CritChance = 101;   // 剑心
    
    // 贯穿分支 (左上)
    constexpr uint32_t Pierce = 110;       // 贯日
    constexpr uint32_t Charges = 111;      // 连环
    constexpr uint32_t Momentum = 112;     // 势如破竹
    constexpr uint32_t Windrunner = 113;   // 风行者
    constexpr uint32_t SwordEcho = 114;    // 剑气回响
    
    // 残影分支 (右上)
    constexpr uint32_t Shadow = 130;       // 留影
    constexpr uint32_t Teleport = 131;     // 移形换位
    constexpr uint32_t ShadowDomain = 132; // 影域
    constexpr uint32_t PrisonSlash = 133;  // 瞬狱影杀
    constexpr uint32_t Phantom = 134;      // 虚实相生
    
    // 暴击分支 (左下)
    constexpr uint32_t WeakPoint = 150;    // 要害感知
    constexpr uint32_t Bleed = 151;        // 重创
    constexpr uint32_t FatalBlow = 152;    // 绝命一击
    constexpr uint32_t AllIn = 153;        // 孤注一掷
    constexpr uint32_t ArmorBreak = 154;   // 破甲之志
    
    // 元素分支 (右下)
    constexpr uint32_t ElementShift = 170; // 元素幻化
    constexpr uint32_t ElementBody = 171;  // 元素身法
    constexpr uint32_t QiShield = 172;     // 气劲护体
    constexpr uint32_t Agility = 173;      // 灵动
}
```

**Task 1.2-1.4 - 修正 ID 检查**

搜索并替换 `FlowingThrust.cpp` 中的硬编码 ID：

| 原代码 | 修正后 |
|-------|-------|
| `exec.active_nodes.test(120 % 100)` | `exec.active_nodes.test(FlowingThrustNodes::Shadow % 100)` |
| `exec.active_nodes.test(140 % 100)` | `exec.active_nodes.test(FlowingThrustNodes::ElementShift % 100)` |
| `spec.allocated_points.contains(114)` | `spec.allocated_points.contains(FlowingThrustNodes::Momentum)` |
| `spec.allocated_points.contains(112)` | `spec.allocated_points.contains(FlowingThrustNodes::Windrunner)` |
| `spec.allocated_points.contains(113)` | `spec.allocated_points.contains(FlowingThrustNodes::Speed)` ❌ 需要核实 |

**注意**: Task 1.5 需要逐行审查 `FlowingThrust.cpp` 中所有 `contains()` 和 `test()` 调用。

### 验证目标

- [ ] 编译通过，0 个新 Warning
- [ ] 分配 `留影 (130)` 后，残影正确生成
- [ ] 分配 `势如破竹 (112)` 后，远距离攻击伤害增加

---

## Phase 2: The Lifecycle Hook (2.5h)

**目标**: 为 `Projectile` 组件和 `ProjectileSystem` 添加生命周期回调机制，支持分裂/爆炸/悬停。

### 任务清单

| Task ID | 任务描述 | 依赖 | 工时 |
|---------|---------|------|------|
| 2.1 | 在 `Projectile.hpp` 中添加 `OnDeathBehavior` 枚举和相关字段 | Phase 1 | 0.3h |
| 2.2 | 在 `ProjectileSystem.cpp` 中添加 `OnProjectileDeath()` 回调入口 | 2.1 | 0.4h |
| 2.3 | 实现 `SpawnSplitProjectiles()` 辅助函数骨架 | 2.2 | 0.4h |
| 2.4 | 实现 `SpawnExplosionProjectiles()` 辅助函数骨架 | 2.2 | 0.4h |
| 2.5 | 实现 `ConvertToHoveringHazard()` 辅助函数骨架 | 2.2 | 0.4h |
| 2.6 | 在现有投射物销毁逻辑处调用 `OnProjectileDeath()` | 2.2 | 0.3h |
| 2.7 | 验证编译通过，无功能回归 | 2.1-2.6 | 0.3h |

### 详细实现指南

**Task 2.1 - 扩展 Projectile 组件**

修改 `src/game/components/Projectile.hpp`：

```cpp
struct Projectile {
    // --- 现有字段 ---
    entt::entity owner = entt::null;
    uint64_t cast_id = 0;
    float speed = 300.0f;
    float lifeTime = 1.0f;
    float radius = 10.0f;
    bool pierce = false;
    int pierceCount = 0;
    // ...
    
    // --- 新增: 生命周期回调 (Phase 2) ---
    enum class OnDeathBehavior : uint8_t {
        None = 0,
        Split,      // 分裂成多个子投射物 (扇形)
        Explode,    // 原地爆炸产生径向投射物
        Hover       // 悬停并持续造成伤害
    };
    OnDeathBehavior on_death = OnDeathBehavior::None;
    
    // Split 配置
    uint8_t split_count = 3;         // 子投射物数量
    float split_damage_mult = 0.5f;  // 子投射物伤害倍率
    float split_spread = 0.6f;       // 扇形角度 (弧度)
    
    // Explode 配置
    uint8_t explode_count = 8;       // 爆炸方向数量
    float explode_damage_mult = 0.4f;
    
    // Hover 配置
    float hover_duration = 1.0f;     // 悬停时间
    float hover_tick_rate = 0.2f;    // 伤害间隔
    float hover_damage_mult = 0.3f;  // 每次伤害倍率
};
```

**Task 2.6 - 调用回调**

在 `ProjectileSystem::Update()` 中，找到投射物销毁逻辑（lifeTime <= 0 或碰撞后），添加：

```cpp
// 在销毁前调用生命周期回调
if (proj.on_death != Projectile::OnDeathBehavior::None) {
    OnProjectileDeath(registry, entity, proj, 
        proj.lifeTime <= 0 ? DeathReason::Expired : DeathReason::Collision);
}
```

### 验证目标

- [ ] 编译通过
- [ ] 设置 `on_death = Split` 的投射物在消亡时生成子投射物
- [ ] 现有投射物行为无回归

---

## Phase 3: The Split & Explode (1.5h)

**目标**: 实现裂空斩的 `碎裂之刃 (211)` 和 `万剑归宗 (213)` 天赋。

### 任务清单

| Task ID | 任务描述 | 依赖 | 工时 |
|---------|---------|------|------|
| 3.1 | 实现 `SpawnSplitProjectiles()` 完整逻辑 | Phase 2 | 0.4h |
| 3.2 | 实现 `SpawnExplosionProjectiles()` 完整逻辑 | Phase 2 | 0.4h |
| 3.3 | 在 `RendingWave.cpp` 中检测 211 天赋并设置 `on_death = Split` | 3.1 | 0.2h |
| 3.4 | 在 `RendingWave.cpp` 中检测 213 天赋并设置 `on_death = Explode`, `pierce = false` | 3.2 | 0.2h |
| 3.5 | 验证功能正确 | 3.3-3.4 | 0.3h |

### 详细实现指南

**Task 3.1 - 分裂逻辑**

```cpp
void ProjectileSystem::SpawnSplitProjectiles(entt::registry& registry, 
                                              entt::entity parent_ent, 
                                              const Projectile& parent) {
    auto* pos = registry.try_get<Position>(parent_ent);
    auto* vel = registry.try_get<Velocity>(parent_ent);
    if (!pos || !vel) return;
    
    Vector2 baseDir = Vector2Normalize({vel->vx, vel->vy});
    float startAngle = -parent.split_spread / 2.0f;
    float angleStep = parent.split_count > 1 
        ? parent.split_spread / (parent.split_count - 1) 
        : 0.0f;
    
    for (int i = 0; i < parent.split_count; ++i) {
        float angle = startAngle + i * angleStep;
        Vector2 dir = Vector2Rotate(baseDir, angle);
        
        auto child = registry.create();
        registry.emplace<LocalLevelTag>(child);
        registry.emplace<Position>(child, pos->x, pos->y);
        registry.emplace<Velocity>(child, dir.x * parent.speed * 0.8f, 
                                         dir.y * parent.speed * 0.8f);
        
        auto& childProj = registry.emplace<Projectile>(child);
        childProj.owner = parent.owner;
        childProj.cast_id = parent.cast_id;
        childProj.speed = parent.speed * 0.8f;
        childProj.lifeTime = 0.5f;
        childProj.radius = parent.radius * 0.6f;
        childProj.pierce = parent.pierce;
        childProj.pierceCount = 1;
        childProj.visualType = parent.visualType;
        childProj.on_death = Projectile::OnDeathBehavior::None; // 子弹不再分裂
        
        // 继承伤害并缩放
        childProj.snapshot = parent.snapshot;
        for (auto& mult : childProj.snapshot.damage_multipliers) {
            mult *= parent.split_damage_mult;
        }
        
        registry.emplace<CombatStats>(child, childProj.snapshot);
        
        if (auto* sc = registry.try_get<SkillComponent>(parent_ent)) {
            registry.emplace<SkillComponent>(child, sc->skill_id, parent.owner);
        }
        
        // 视觉颜色继承
        if (auto* color = registry.try_get<ColorComponent>(parent_ent)) {
            registry.emplace<ColorComponent>(child, color->color);
        }
    }
    
    LOG_DEBUG("Projectile split into {} children", parent.split_count);
}
```

**Task 3.3 - RendingWave 天赋检测**

在 `RendingWave.cpp` 的 `DoCast` 中，天赋检测区域添加：

```cpp
// Talent: Sui Lie Zhi Ren (碎裂之刃) - ID 211
if (spec.allocated_points.contains(211) && spec.allocated_points.at(211) > 0) {
    proj.on_death = Projectile::OnDeathBehavior::Split;
    proj.split_count = 3;
    proj.split_damage_mult = 0.5f + 0.1f * spec.allocated_points.at(212); // 连锁反应加成
    LOG_INFO("Rending Wave (211): Split on death enabled");
}

// Talent: Wan Jian Gui Zong (万剑归宗) - ID 213
if (spec.allocated_points.contains(213) && spec.allocated_points.at(213) > 0) {
    proj.on_death = Projectile::OnDeathBehavior::Explode;
    proj.explode_count = 8;
    proj.explode_damage_mult = 0.4f;
    proj.pierce = false; // 不再穿透
    proj.pierceCount = 0;
    LOG_INFO("Rending Wave (213): Explode on hit enabled, pierce disabled");
}
```

### 验证目标

- [ ] 分配 211 后，裂空斩终点分裂成 3 道剑气
- [ ] 分配 213 后，裂空斩命中时爆发 8 道剑气，且不再穿透

---

## Phase 4: The Hover Zone (1.5h)

**目标**: 实现裂空斩的 `回旋劲 (230)` 强化版 `时空锁定 (233)` 天赋。

### 任务清单

| Task ID | 任务描述 | 依赖 | 工时 |
|---------|---------|------|------|
| 4.1 | 设计 `HoveringHazardComponent` 组件 | Phase 2 | 0.2h |
| 4.2 | 实现 `ConvertToHoveringHazard()` 逻辑 | 4.1 | 0.4h |
| 4.3 | 在 `HazardSystem` 中添加悬停伤害 Tick 逻辑 | 4.2 | 0.4h |
| 4.4 | 在 `RendingWave.cpp` 中检测 233 天赋并配置悬停 | 4.3 | 0.2h |
| 4.5 | 验证功能正确 | 4.4 | 0.3h |

### 详细实现指南

**Task 4.1 - 悬停危险区组件**

新增或复用 `HoveringHazardComponent`：

```cpp
// src/game/components/Hazard.hpp (如已存在则扩展)
struct HoveringHazardComponent {
    entt::entity owner = entt::null;
    uint64_t cast_id = 0;
    float duration = 1.0f;
    float tick_rate = 0.2f;
    float tick_timer = 0.0f;
    float radius = 50.0f;
    float damage_per_tick = 10.0f;
    CombatStats snapshot;
    uint32_t skill_id = 0;
    
    // VFX
    bool spin_visual = true;
    float rotation = 0.0f;
};
```

**Task 4.2 - 转换逻辑**

```cpp
void ProjectileSystem::ConvertToHoveringHazard(entt::registry& registry, 
                                                entt::entity proj_ent, 
                                                const Projectile& proj) {
    // 停止移动
    if (auto* vel = registry.try_get<Velocity>(proj_ent)) {
        vel->vx = 0.0f;
        vel->vy = 0.0f;
    }
    
    // 添加悬停组件
    auto& hazard = registry.emplace<HoveringHazardComponent>(proj_ent);
    hazard.owner = proj.owner;
    hazard.cast_id = proj.cast_id;
    hazard.duration = proj.hover_duration;
    hazard.tick_rate = proj.hover_tick_rate;
    hazard.radius = proj.radius * 1.5f;
    hazard.snapshot = proj.snapshot;
    
    // 缩放伤害
    for (auto& mult : hazard.snapshot.damage_multipliers) {
        mult *= proj.hover_damage_mult;
    }
    
    if (auto* sc = registry.try_get<SkillComponent>(proj_ent)) {
        hazard.skill_id = sc->skill_id;
    }
    
    // 移除 Projectile 组件，保留实体用于 Hazard 逻辑
    registry.remove<Projectile>(proj_ent);
    
    LOG_INFO("Projectile converted to Hovering Hazard (duration: {:.1f}s)", 
             proj.hover_duration);
}
```

### 验证目标

- [ ] 分配 233 后，回旋剑气在折返点悬停 1 秒
- [ ] 悬停期间对范围内敌人造成多次伤害

---

## Phase 5: The Melee Orbit (1.0h)

**目标**: 实现灵剑决 `剑影随行 (352)` 近战光环模式。

### 任务清单

| Task ID | 任务描述 | 依赖 | 工时 |
|---------|---------|------|------|
| 5.1 | 在 `SpiritSwordAI` 中添加 `MeleeOrbit` 状态和 `melee_mode` 字段 | - | 0.1h |
| 5.2 | 在 `BladeFormation.cpp` 中检测 352 天赋并设置 `melee_mode` | 5.1 | 0.2h |
| 5.3 | 在 `SummonSystem.cpp` 中实现近战光环伤害逻辑 | 5.2 | 0.4h |
| 5.4 | 验证功能正确 | 5.3 | 0.3h |

### 详细实现指南

**Task 5.3 - 近战光环逻辑**

在 `SummonSystem::UpdateSpiritSwords()` 中：

```cpp
// 检测近战模式
bool isMeleeMode = false;
if (auto* active = registry.try_get<ActiveSkillsComponent>(summon.owner)) {
    for (const auto& spec : active->specialized_slots) {
        if (spec.skill_id == 3 && spec.allocated_points.contains(352) 
            && spec.allocated_points.at(352) > 0) {
            isMeleeMode = true;
            break;
        }
    }
}

if (isMeleeMode) {
    // --- 近战环绕模式 ---
    ai.attack_timer -= dt;
    if (ai.attack_timer <= 0.0f) {
        ai.attack_timer = ai.attack_interval;
        
        float auraRadius = isGiant ? 80.0f : 45.0f;
        
        grid.query({pos.x, pos.y}, auraRadius, 
            [&](entt::entity target, const Position& tPos) {
                if (!registry.all_of<EnemyTag>(target)) return;
                if (registry.all_of<KilledTag>(target)) return;
                
                // 使用 DamagePipeline 计算伤害
                float baseDamage = isGiant ? 30.0f : 15.0f;
                // TODO: 完整 DamagePipeline 调用
                CombatSystem::ApplyDamage(registry, target, baseDamage, 
                                          summon.owner, false, true);
            });
        
        // 视觉效果：旋转斩击
        auto& particleSys = GPUParticleSystem::Get();
        for (int i = 0; i < 8; ++i) {
            float angle = ai.orbit_angle + (float)i * (PI / 4.0f);
            Vector2 offset = {cosf(angle) * auraRadius * 0.5f, 
                              sinf(angle) * auraRadius * 0.5f};
            components::GPUParticle p;
            p.position = {pos.x + offset.x, pos.y + offset.y};
            p.velocity = Vector2Scale(offset, 2.0f);
            p.color = isGiant ? GOLD : SKYBLUE;
            p.lifetime = 0.15f;
            p.maxLifetime = 0.15f;
            p.scale = 2.0f;
            p.flags = 2;
            particleSys.Emit(p);
        }
    }
} else {
    // --- 现有远程投射物逻辑 ---
    // ...
}
```

### 验证目标

- [ ] 分配 352 后，灵剑环绕玩家并对周围敌人造成伤害
- [ ] 灵剑不再发射投射物

---

## Phase 6: The Validation (1.0h)

**目标**: 编写自动化测试，确保修复的正确性。

### 任务清单

| Task ID | 任务描述 | 依赖 | 工时 |
|---------|---------|------|------|
| 6.1 | 创建 `tests/unit/TalentBehaviorTest.cpp` | Phase 1-5 | 0.2h |
| 6.2 | 编写 Test Case: `FlowingThrust_ShadowNode_Spawns` | 6.1 | 0.15h |
| 6.3 | 编写 Test Case: `RendingWave_SplitNode_Creates3Children` | 6.1 | 0.15h |
| 6.4 | 编写 Test Case: `SpiritSword_MeleeMode_EntersState` | 6.1 | 0.15h |
| 6.5 | 运行所有测试 | 6.2-6.4 | 0.15h |
| 6.6 | 运行 `code-risk-analyzer` 审查 | 6.5 | 0.2h |

### 验证目标

- [ ] 所有新增测试用例通过
- [ ] 现有 73+ 测试无回归
- [ ] `code-risk-analyzer` 无新增高风险项

---

## 依赖关系图

```
Phase 1: The ID Alignment
    │
    ▼
Phase 2: The Lifecycle Hook ─────────────────────┐
    │                                            │
    ├──────────────┬──────────────┐              │
    ▼              ▼              ▼              │
Phase 3       Phase 4         Phase 5            │
(Split)       (Hover)        (Melee)             │
    │              │              │              │
    └──────────────┴──────────────┘              │
                   │                             │
                   ▼                             │
             Phase 6: Validation ◄───────────────┘
```

---

## 风险与缓解

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------| 
| 分裂投射物数量过多导致性能问题 | 中 | 帧率下降 | 限制最大分裂层级为 1，子弹不再分裂 |
| ID 常量化导致 JSON 热更新失效 | 低 | 开发效率 | 常量仅用于代码可读性，运行时仍读 JSON |
| 投射物销毁时机与回调竞争 | 中 | 崩溃 | 在销毁前复制必要数据，回调中不访问原实体 |
| 近战光环与远程逻辑并存导致代码复杂 | 中 | 维护困难 | 抽取为独立的 `MeleeAuraSystem` |

---

## 完成定义 (Definition of Done)

- [ ] Phase 1-5 所有任务完成
- [ ] 编译通过，0 新 Warning
- [ ] 6 个新增测试用例全部通过
- [ ] 现有测试无回归
- [ ] `code-risk-analyzer` 审查通过
- [ ] 游戏内手动验证核心天赋功能正常
