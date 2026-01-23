# 技能专精天赋节点行为实现 - 技术规格书

**Track ID**: `talent_behavior_implementation`
**创建日期**: 2026-01-23
**状态**: Phase 1 - 设计中
**前置依赖**: `talent_constellation_fix` (已完成)

---

## 1. 问题陈述 (Problem Statement)

### 1.1 审计发现

通过 `code-risk-analyzer` 对 `skills.json` 与 `behaviors/*.cpp` 的对比审计，发现以下两类严重问题：

| 问题类型 | 影响范围 | 严重程度 |
|---------|---------|---------|
| **ID 映射不一致** | 流云刺 (Skill 1) 的 3 个核心天赋 | 🔴 **致命** - 天赋完全失效 |
| **逻辑缺失** | 约 15 个高级天赋节点 | 🟡 **严重** - 功能未实现 |

### 1.2 ID 映射不一致详情

| 技能 | 节点名称 | Json 定义 ID | C++ 硬编码 ID | 状态 |
|:---:|:---:|:---:|:---:|:---:|
| 流云刺 (Skill 1) | 留影 (Shadow) | **130** | **120** | ❌ Broken |
| 流云刺 (Skill 1) | 势如破竹 (Momentum) | **112** | **114** | ❌ Broken |
| 流云刺 (Skill 1) | 风行者 (Windrunner) | **113** | **112** | ❌ Broken |

**根因**: C++ 代码使用 `exec.active_nodes.test(ID % 100)` 进行位掩码检查，但 JSON 定义的 ID 与代码硬编码的 ID 不匹配。

### 1.3 逻辑缺失详情

#### A. 裂空斩 (Rending Wave - Skill 2)

| Node ID | 名称 | 描述 | 当前状态 |
|---------|------|------|---------|
| 211 | 碎裂之刃 | 剑气命中或达最大距离时，分裂成 3 道小剑气 | ❌ 无分裂逻辑 |
| 213 | 万剑归宗 | 不再穿透，命中时爆发 8 道微型剑气 | ❌ 强制穿透，无爆发 |
| 231 | 引力陷阱 | 折返点生成引力场，减速并牵引敌人 | ❌ 仅基础回旋镖 |
| 233 | 时空锁定 | 剑气在折返点停留 1s 高速旋转切割 | ❌ 无悬停逻辑 |
| 271 | 异常扩散 | 击中异常状态敌人时，将状态扩散至周围 | ❌ 无扩散逻辑 |

#### B. 灵剑决 (Spirit Sword - Skill 3)

| Node ID | 名称 | 描述 | 当前状态 |
|---------|------|------|---------|
| 352 | 剑影随行 | 灵剑不再飞出，环绕自身造成近战伤害 | ❌ 总是远程投射物 |
| 372 | 法术共鸣 | 模仿上一次施放的法术 (25% 效力) | ❌ 硬编码 Skill 2 |

#### C. 流云刺 (Flowing Thrust - Skill 1)

| Node ID | 名称 | 描述 | 当前状态 |
|---------|------|------|---------|
| 113 | 风行者 | 施放后获得疾风状态，下一次移动无视体积碰撞 | ⚠️ 移速已实现，碰撞未实现 |
| 131 | 移形换位 | 再次施放时瞬移至残影位置并引爆 | ❌ 无引爆逻辑 |
| 133 | 瞬狱影杀 | 消耗剑意，瞬间 5 次斩击，期间无敌 | ❌ 未实现 |
| 154 | 破甲之志 | 牺牲 50% 护甲，获得等量物理穿透 | ❌ 属性动态转换未实现 |

---

## 2. 技术方案 (Technical Design)

### 2.1 Phase 1: ID 同步修复

**策略**: 以 JSON 数据为唯一真相源 (Single Source of Truth)，修改 C++ 代码以匹配。

**修改文件**: `src/game/systems/skill/behaviors/FlowingThrust.cpp`

**当前代码 (Line 63-67)**:
```cpp
// Talent 140: Frost Thrust (Phys -> Cold)
bool is_cold = exec.active_nodes.test(140 % 100);  // ✅ 正确

// Talent 120: Shadow (Spawn Shadow Echo)  
if (exec.active_nodes.test(120 % 100) && ...  // ❌ 错误，应为 130
```

**修复后**:
```cpp
// --- ID 常量定义 (避免魔法数字) ---
namespace FlowingThrustNodes {
    constexpr uint32_t Pierce = 110;       // 贯日
    constexpr uint32_t Charges = 111;      // 连环
    constexpr uint32_t Momentum = 112;     // 势如破竹
    constexpr uint32_t Windrunner = 113;   // 风行者
    constexpr uint32_t SwordEcho = 114;    // 剑气回响
    constexpr uint32_t Shadow = 130;       // 留影
    constexpr uint32_t Teleport = 131;     // 移形换位
    constexpr uint32_t ShadowDomain = 132; // 影域
    constexpr uint32_t PrisonSlash = 133;  // 瞬狱影杀
    constexpr uint32_t ArmorBreak = 154;   // 破甲之志
    constexpr uint32_t FrostThrust = 170;  // 元素幻化 (冰)
}

// Talent 130: Shadow (Spawn Shadow Echo)
if (exec.active_nodes.test(FlowingThrustNodes::Shadow % 100) && ...
```

### 2.2 Phase 2: 投射物生命周期回调机制

为支持 **分裂/爆炸/悬停** 类天赋，需要扩展 `Projectile` 组件和 `ProjectileSystem` 逻辑。

**新增组件字段** (`src/game/components/Projectile.hpp`):
```cpp
struct Projectile {
    // ... 现有字段 ...
    
    // --- 生命周期回调 (Talent Support) ---
    enum class OnDeathBehavior : uint8_t {
        None = 0,
        Split,      // 分裂成多个子投射物
        Explode,    // 原地爆炸产生新投射物
        Hover       // 悬停并持续造成伤害
    };
    OnDeathBehavior on_death = OnDeathBehavior::None;
    
    uint8_t split_count = 0;        // Split: 分裂数量
    float split_damage_mult = 1.0f; // Split: 子投射物伤害倍率
    float hover_duration = 0.0f;    // Hover: 悬停时间
    float hover_tick_rate = 0.0f;   // Hover: 伤害间隔
};
```

**ProjectileSystem 扩展** (`src/game/systems/projectile/ProjectileSystem.cpp`):
```cpp
void ProjectileSystem::OnProjectileDeath(entt::registry& registry, 
                                         entt::entity proj_ent, 
                                         const Projectile& proj,
                                         DeathReason reason) {
    switch (proj.on_death) {
        case Projectile::OnDeathBehavior::Split:
            SpawnSplitProjectiles(registry, proj_ent, proj);
            break;
        case Projectile::OnDeathBehavior::Explode:
            SpawnExplosionProjectiles(registry, proj_ent, proj);
            break;
        case Projectile::OnDeathBehavior::Hover:
            ConvertToHoveringHazard(registry, proj_ent, proj);
            break;
        default:
            break;
    }
}
```

### 2.3 Phase 3: 灵剑模式扩展

为支持 **剑影随行 (352)** 近战光环模式，扩展 `SpiritSwordAI` 状态机。

**修改组件** (`src/game/components/SkillDefs.hpp`):
```cpp
struct SpiritSwordAI {
    // ... 现有字段 ...
    
    enum class State : uint8_t {
        Idle,       // Orbiting
        Chasing,    // Flying to target (Sword Rain)
        Attacking,  // Striking (Heavy Sword)
        Returning,  // Returning to orbit
        MeleeOrbit  // NEW: 近战环绕模式
    } state = State::Idle;
    
    bool melee_mode = false; // 352 天赋激活
};
```

**SummonSystem 修改**:
```cpp
if (ai.melee_mode) {
    // 近战光环模式: 不发射投射物，而是创建环绕伤害区域
    float auraRadius = 50.0f * (isGiant ? 2.0f : 1.0f);
    
    grid.query({pos.x, pos.y}, auraRadius, 
        [&](entt::entity target, const Position& tPos) {
            if (registry.all_of<EnemyTag>(target) && !registry.all_of<KilledTag>(target)) {
                // 应用近战伤害 (每 0.3s 一次)
                // ...
            }
        });
} else {
    // 现有远程投射物逻辑
    SkillSystem::ShadowCast(registry, proxy, 2, ...);
}
```

### 2.4 Phase 4: 异常状态扩散

为支持 **异常扩散 (271)**，需要创建通用的状态扩散工具函数。

**新增工具函数** (`src/game/systems/combat/StatusEffectUtils.hpp`):
```cpp
namespace StatusEffectUtils {
    /**
     * @brief 扩散目标身上的所有异常状态到周围敌人
     * @param registry ECS 注册表
     * @param source 状态来源实体
     * @param center 扩散中心位置
     * @param radius 扩散半径
     * @param grid 空间网格
     */
    void SpreadAilmentsFrom(entt::registry& registry, 
                            entt::entity source, 
                            Vector2 center, 
                            float radius,
                            const SpatialHashGrid& grid);
}
```

**实现逻辑**:
```cpp
void StatusEffectUtils::SpreadAilmentsFrom(...) {
    auto* effects = registry.try_get<ActiveEffectsComponent>(source);
    if (!effects) return;
    
    // 收集可扩散的异常状态
    std::vector<BuffEffect> ailments;
    for (const auto& buff : effects->buffs) {
        if (buff.type == BuffType::Ailment) {
            ailments.push_back(buff);
        }
    }
    if (ailments.empty()) return;
    
    // 扩散到周围敌人
    grid.query(center, radius, [&](entt::entity target, const Position& tPos) {
        if (target == source) return;
        if (!registry.all_of<EnemyTag>(target)) return;
        
        auto& targetEffects = registry.get_or_emplace<ActiveEffectsComponent>(target);
        for (const auto& ailment : ailments) {
            BuffEffect copy = ailment;
            copy.remaining = ailment.duration * 0.5f; // 扩散后持续时间减半
            targetEffects.AddOrRefresh(copy);
        }
    });
}
```

---

## 3. 优先级排序 (Priority Matrix)

| 优先级 | 问题 | 理由 |
|-------|------|------|
| P0 | ID 同步修复 | 新手最先接触的技能，完全失效 |
| P1 | 投射物生命周期回调 | 解锁多个高级天赋的基础设施 |
| P2 | 灵剑模式扩展 | 影响单一技能，但机制较简单 |
| P3 | 异常状态扩散 | 依赖异常状态系统完善程度 |

---

## 4. 验收清单 (Acceptance Criteria)

| ID | 验收项 | 验证方法 |
|----|-------|---------| 
| AC-1 | 流云刺 `留影 (130)` 天赋能正确生成残影 | 单元测试 + 游戏内验证 |
| AC-2 | 流云刺 `势如破竹 (112)` 距离增伤正确触发 | 单元测试：远距离攻击伤害 > 近距离 |
| AC-3 | 裂空斩 `碎裂之刃 (211)` 能正确分裂成 3 道剑气 | 单元测试：验证子投射物数量 |
| AC-4 | 裂空斩 `时空锁定 (233)` 能正确悬停切割 | 游戏内验证：观察悬停行为 |
| AC-5 | 灵剑决 `剑影随行 (352)` 能切换为近战模式 | 单元测试：验证 AI 状态为 MeleeOrbit |
| AC-6 | 无性能回归 | Benchmark 耗时增加 < 10% |

---

## 5. 风险评估

| 风险 | 等级 | 缓解措施 |
|-----|------|---------| 
| 投射物生命周期回调导致大量瞬时实体创建 | 🟡 中 | 使用对象池 (EntityPool) |
| ID 常量化后与热更新冲突 | 🟢 低 | 保持 JSON 为数据源，常量仅用于代码可读性 |
| 异常扩散导致性能问题 (大群怪) | 🟡 中 | 添加扩散次数上限 (max 5 targets) |
| 近战光环模式伤害计算与现有管线不兼容 | 🟡 中 | 复用 DamagePipeline 而非直接 ApplyDamage |

---

## 6. 不在本次范围内 (Out of Scope)

以下问题复杂度较高，建议后续 Track 处理：

| 问题 | 原因 |
|------|------|
| `破甲之志 (154)` 护甲转物穿 | 需要 AttributePipeline 支持"基于面板值的动态转换" |
| `瞬狱影杀 (133)` 无敌斩击 | 需要完整的无敌帧系统 |
| `法术共鸣 (372)` 动态模仿 | 需要技能历史记录系统 |
