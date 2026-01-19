#pragma once

#include "game/data/MonsterAffixRegistry.hpp"
#include "game/components/Common.hpp"
#include "game/components/Stats.hpp"
#include "game/components/Buff.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/HazardComponents.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "core/logging/Logger.hpp"
#include "core/math/ThreadSafeRandom.hpp"
#include <entt/entt.hpp>
#include <cmath>

namespace NoMoreDay {

/**
 * @brief 熔火路径标签 - 用于伤害区域处理
 */
struct MoltenTrailTag {};

/**
 * @brief 怪物词缀系统 - 处理机制型词缀的运行时逻辑
 * 
 * 处理需要 Update/OnHit/OnDeath 回调的词缀：
 * - Molten: 火焰路径
 * - Teleporter: 闪烁
 * - Berserker: 狂暴
 * - Frozen: 减速光环
 * - Nullifier: 驱散 (via CombatEventDispatcher)
 */
class MonsterAffixSystem {
public:
    // 常量定义
    static constexpr float MOLTEN_TICK_INTERVAL = 0.5f;     // 火焰路径生成间隔
    static constexpr float MOLTEN_TRAIL_DURATION = 3.0f;    // 火焰持续时间
    static constexpr float MOLTEN_TRAIL_DAMAGE = 10.0f;     // 每秒火焰伤害
    static constexpr float MOLTEN_TRAIL_RADIUS = 20.0f;     // 火焰半径
    
    static constexpr float TELEPORT_COOLDOWN = 5.0f;        // 闪烁冷却时间
    static constexpr float TELEPORT_TRIGGER_DISTANCE = 300.0f;  // 触发闪烁的距离
    static constexpr float TELEPORT_TARGET_DISTANCE = 50.0f;    // 闪烁到玩家的距离
    
    static constexpr float BERSERKER_HP_THRESHOLD = 0.5f;   // 狂暴触发血量阈值
    static constexpr float BERSERKER_DAMAGE_MULT = 2.0f;    // 狂暴伤害倍率
    static constexpr float BERSERKER_SCALE_MULT = 1.5f;     // 狂暴体型倍率
    
    static constexpr float FROZEN_ORB_INTERVAL = 4.0f;      // 冰球生成间隔
    static constexpr float FROZEN_ORB_SPEED = 120.0f;       // 冰球飞行速度
    static constexpr float FROZEN_ORB_DAMAGE = 60.0f;       // 冰球爆炸伤害
    
    static constexpr float VOIDZONE_SPAWN_INTERVAL_MIN = 5.0f;  // 虚空区域生成最小间隔
    static constexpr float VOIDZONE_SPAWN_INTERVAL_MAX = 8.0f;  // 虚空区域生成最大间隔
    static constexpr float VOIDZONE_WARNING_DURATION = 1.0f;    // 虚空区域预警时间
    static constexpr float VOIDZONE_ACTIVE_DURATION = 4.0f;     // 虚空区域激活时间
    static constexpr float VOIDZONE_DAMAGE_PER_TICK = 50.0f;    // 虚空区域每次伤害
    static constexpr float VOIDZONE_TICK_INTERVAL = 0.2f;       // 虚空区域伤害间隔
    
    static constexpr float STORMSTRIDER_TRIGGER_CHANCE = 0.25f; // 雷行触发概率 (25%)
    static constexpr float STORMSTRIDER_GHOST_DELAY = 1.5f;     // 雷电残影爆炸延迟
    static constexpr float STORMSTRIDER_DAMAGE = 80.0f;         // 雷电残影伤害
    
    /**
     * @brief 初始化系统 - 注册 CombatEvent 处理器
     */
    static void Init() {
        // Register OnDealDamage handler for Nullifier affix
        CombatEventDispatcher::Register(
            CombatEventType::OnDealDamage,
            [](entt::registry& registry, const CombatEvent& evt) {
                OnEnemyDealDamage(registry, evt);
            },
            -10 // Lower priority to run after damage calculation
        );
        
        // Register OnTakeDamage handler for StormStrider affix
        CombatEventDispatcher::Register(
            CombatEventType::OnTakeDamage,
            [](entt::registry& registry, const CombatEvent& evt) {
                OnEnemyTakeDamage(registry, evt);
            },
            -10
        );
        
        LOG_INFO("MonsterAffixSystem: Initialized");
    }
    
    /**
     * @brief 主更新循环 - 处理需要 Update 的词缀
     */
    static void Update(entt::registry& registry, float dt) {
        auto view = registry.view<MonsterAffixComponent, Position>(entt::exclude<KilledTag>);
        
        // Get player position for distance checks
        Position playerPos = {0, 0};
        auto playerView = registry.view<PlayerTag, Position>();
        entt::entity playerEntity = entt::null;
        if (playerView.begin() != playerView.end()) {
            playerEntity = playerView.front();
            playerPos = playerView.get<Position>(playerEntity);
        }
        
        for (auto entity : view) {
            auto& affix = view.get<MonsterAffixComponent>(entity);
            const auto& pos = view.get<Position>(entity);
            
            // Skip if no Update-type affixes
            if (!affix.hasUpdate) continue;
            
            // Update timers
            affix.timer1 += dt;
            affix.timer2 += dt;
            
            // Process each affix
            for (auto affixType : affix.affixes) {
                switch (affixType) {
                    case MonsterAffixType::Molten:
                        ProcessMolten(registry, entity, pos, affix, dt);
                        break;
                    case MonsterAffixType::Teleporter:
                        ProcessTeleporter(registry, entity, pos, playerPos, affix, dt);
                        break;
                    case MonsterAffixType::Berserker:
                        ProcessBerserker(registry, entity, affix);
                        break;
                    case MonsterAffixType::Frozen:
                        ProcessFrozen(registry, entity, pos, playerPos, affix, dt);
                        break;
                    case MonsterAffixType::VoidZone:
                        ProcessVoidZone(registry, entity, pos, playerPos, affix, dt);
                        break;
                    default:
                        break;
                }
            }
        }
    }
    
private:
    /**
     * @brief 熔火词缀 - 生成火焰路径 (已配置化为 HazardComponent)
     */
    static void ProcessMolten(entt::registry& registry, entt::entity enemy, 
                              const Position& pos, MonsterAffixComponent& affix, float dt) {
        if (affix.timer1 >= MOLTEN_TICK_INTERVAL) {
            affix.timer1 = 0.0f;
            
            // Create fire trail entity
            auto fireEntity = registry.create();
            registry.emplace<Position>(fireEntity, pos.x, pos.y);
            registry.emplace<LocalLevelTag>(fireEntity);
            registry.emplace<Radius>(fireEntity, MOLTEN_TRAIL_RADIUS);
            
            // Hazard 配置 (取代旧的 MoltenTrailTag)
            HazardComponent hazard;
            hazard.damagePerTick = MOLTEN_TRAIL_DAMAGE * MOLTEN_TICK_INTERVAL;
            hazard.tickInterval = MOLTEN_TICK_INTERVAL;
            hazard.duration = MOLTEN_TRAIL_DURATION;
            hazard.radius = MOLTEN_TRAIL_RADIUS;
            hazard.damageType = DamageType::Fire;
            hazard.isDelayedExplosion = false;
            hazard.hitsPlayers = true;
            hazard.hitsEnemies = false;
            hazard.owner = enemy;
            registry.emplace<HazardComponent>(fireEntity, hazard);
            
            // 视觉效果
            HazardVisualComponent visual;
            visual.tintColor = Color{255, 80, 0, 180}; // 橙色
            visual.particleEmitInterval = 0.2f;
            visual.particlesPerEmit = 2;
            registry.emplace<HazardVisualComponent>(fireEntity, visual);
            
            LOG_TRACE("Molten trail (Hazard) spawned at ({:.1f}, {:.1f})", pos.x, pos.y);
        }
    }
    
    /**
     * @brief 闪烁词缀 - 瞬移到玩家附近
     */
    static void ProcessTeleporter(entt::registry& registry, entt::entity enemy,
                                  const Position& enemyPos, const Position& playerPos,
                                  MonsterAffixComponent& affix, float dt) {
        if (affix.timer2 >= TELEPORT_COOLDOWN) {
            float dx = playerPos.x - enemyPos.x;
            float dy = playerPos.y - enemyPos.y;
            float distSq = dx * dx + dy * dy;
            
            // Only teleport if far from player
            if (distSq > TELEPORT_TRIGGER_DISTANCE * TELEPORT_TRIGGER_DISTANCE) {
                affix.timer2 = 0.0f;
                
                // Calculate new position behind player
                float dist = std::sqrt(distSq);
                float nx = dx / dist;
                float ny = dy / dist;
                
                float newX = playerPos.x - nx * TELEPORT_TARGET_DISTANCE;
                float newY = playerPos.y - ny * TELEPORT_TARGET_DISTANCE;
                
                // Update position
                auto& pos = registry.get<Position>(enemy);
                pos.x = newX;
                pos.y = newY;
                
                // TODO: Add teleport VFX (fade out/in)
                
                LOG_TRACE("Teleporter enemy blinked to ({:.1f}, {:.1f})", newX, newY);
            }
        }
    }
    
    /**
     * @brief 狂暴词缀 - 低血量时激活
     */
    static void ProcessBerserker(entt::registry& registry, entt::entity enemy,
                                 MonsterAffixComponent& affix) {
        if (affix.isBerserk) return; // Already berserk
        
        auto* hp = registry.try_get<HealthComponent>(enemy);
        if (!hp) return;
        
        float hpRatio = hp->current / hp->max;
        if (hpRatio <= BERSERKER_HP_THRESHOLD) {
            affix.isBerserk = true;
            
            // Trigger recalculation to apply multipliers via StatsSystem
            registry.get_or_emplace<StatsDirty>(enemy);
            
            // Apply scale multiplier
            if (auto* sprite = registry.try_get<SpriteComponent>(enemy)) {
                sprite->scale *= BERSERKER_SCALE_MULT;
            }
            
            // Add red tint
            if (auto* color = registry.try_get<ColorComponent>(enemy)) {
                color->color = Color{255, 50, 50, 255};
            }
            
            LOG_INFO("Berserker activated for entity {}", (uint32_t)enemy);
        }
    }
    
    /**
     * @brief 极寒词缀 - 生成追踪冰球
     */
    static void ProcessFrozen(entt::registry& registry, entt::entity enemy,
                              const Position& enemyPos, const Position& playerPos,
                              MonsterAffixComponent& affix, float dt) {
        if (affix.timer1 >= FROZEN_ORB_INTERVAL) {
            affix.timer1 = 0.0f;
            
            // 生成冰球实体
            auto orbEntity = registry.create();
            registry.emplace<Position>(orbEntity, enemyPos.x, enemyPos.y);
            registry.emplace<LocalLevelTag>(orbEntity);
            registry.emplace<FrozenOrbTag>(orbEntity);
            registry.emplace<Radius>(orbEntity, 15.0f);
            
            // 计算朝向玩家的初速度
            float dx = playerPos.x - enemyPos.x;
            float dy = playerPos.y - enemyPos.y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > 0.1f) {
                float nx = dx / dist;
                float ny = dy / dist;
                registry.emplace<Velocity>(orbEntity, nx * FROZEN_ORB_SPEED, ny * FROZEN_ORB_SPEED);
            } else {
                registry.emplace<Velocity>(orbEntity, 0.0f, 0.0f);
            }
            
            // 冰球组件
            FrozenOrbComponent orb;
            orb.travelDuration = 2.0f;
            orb.stopDuration = 1.0f;
            registry.emplace<FrozenOrbComponent>(orbEntity, orb);
            
            // Hazard 组件（用于爆炸）
            HazardComponent hazard;
            hazard.explosionDamage = FROZEN_ORB_DAMAGE;
            hazard.isDelayedExplosion = true;
            hazard.damageType = DamageType::Cold;
            hazard.duration = 3.0f; // 总生命周期
            hazard.owner = enemy;
            registry.emplace<HazardComponent>(orbEntity, hazard);
            
            // 视觉效果
            registry.emplace<ColorComponent>(orbEntity, Color{150, 220, 255, 220});
            
            LOG_TRACE("Frozen orb spawned at ({:.1f}, {:.1f})", enemyPos.x, enemyPos.y);
        }
    }
    
    /**
     * @brief 虚空区域词缀 - 在玩家脚下生成虚空区域
     */
    static void ProcessVoidZone(entt::registry& registry, entt::entity enemy,
                                const Position& enemyPos, const Position& playerPos,
                                MonsterAffixComponent& affix, float dt) {
        // 使用怪物独立的冷却计时器
        if (affix.voidZoneNextSpawnTime <= 0.0f) {
            // 初始化第一次触发时间
            affix.voidZoneNextSpawnTime = NoMoreDay::utils::ThreadSafeRandom::GetFloat(VOIDZONE_SPAWN_INTERVAL_MIN, VOIDZONE_SPAWN_INTERVAL_MAX);
        }

        if (affix.timer2 >= affix.voidZoneNextSpawnTime) {
            affix.timer2 = 0.0f;
            affix.voidZoneNextSpawnTime = NoMoreDay::utils::ThreadSafeRandom::GetFloat(VOIDZONE_SPAWN_INTERVAL_MIN, VOIDZONE_SPAWN_INTERVAL_MAX);
            
            // 在玩家当前位置生成虚空区域
            auto zoneEntity = registry.create();
            registry.emplace<Position>(zoneEntity, playerPos.x, playerPos.y);
            registry.emplace<LocalLevelTag>(zoneEntity);
            registry.emplace<VoidZoneTag>(zoneEntity);
            registry.emplace<Radius>(zoneEntity, 80.0f);
            
            // Hazard 组件
            HazardComponent hazard;
            hazard.damagePerTick = VOIDZONE_DAMAGE_PER_TICK;
            hazard.tickInterval = VOIDZONE_TICK_INTERVAL;
            hazard.duration = VOIDZONE_WARNING_DURATION + VOIDZONE_ACTIVE_DURATION;
            hazard.radius = 80.0f;
            hazard.damageType = DamageType::Shadow;
            hazard.isDelayedExplosion = false;
            hazard.hitsPlayers = true;
            hazard.hitsEnemies = false;
            hazard.hasWarningPhase = true;
            hazard.warningDuration = VOIDZONE_WARNING_DURATION;
            hazard.isWarningActive = true;
            hazard.owner = enemy;
            registry.emplace<HazardComponent>(zoneEntity, hazard);
            
            // 视觉效果
            HazardVisualComponent visual;
            visual.tintColor = Color{80, 0, 120, 150}; // 预警阶段暗紫色
            visual.particleEmitInterval = 0.1f;
            visual.particlesPerEmit = 5;
            registry.emplace<HazardVisualComponent>(zoneEntity, visual);
            
            LOG_TRACE("Void zone spawned at ({:.1f}, {:.1f})", playerPos.x, playerPos.y);
        }
    }
    
public:
    /**
     * @brief OnDealDamage 回调 - 处理 Nullifier 和 StormStrider 词缀
     */
    static void OnEnemyDealDamage(entt::registry& registry, const CombatEvent& evt) {
        // Check if attacker has OnHit affixes
        auto* affix = registry.try_get<MonsterAffixComponent>(evt.source);
        if (!affix || !affix->hasOnHit) return;
        
        // Nullifier: Dispel buffs
        if (affix->HasAffix(MonsterAffixType::Nullifier)) {
            if (registry.valid(evt.target)) {
                if (auto* effects = registry.try_get<ActiveEffectsComponent>(evt.target)) {
                    // Remove all non-debuff effects
                    effects->effects.erase(
                        std::remove_if(effects->effects.begin(), effects->effects.end(),
                            [](const BuffEffect& e) { return !e.is_debuff; }),
                        effects->effects.end()
                    );
                    
                    LOG_INFO("Nullifier dispelled buffs from entity {}", (uint32_t)evt.target);
                }
            }
        }
    }
    
    /**
     * @brief OnTakeDamage 回调 - 处理 StormStrider 词缀
     */
    static void OnEnemyTakeDamage(entt::registry& registry, const CombatEvent& evt) {
        // Check if defender has StormStrider affix
        auto* affix = registry.try_get<MonsterAffixComponent>(evt.target);
        if (!affix || !affix->hasOnHit) return;
        
        if (!affix->HasAffix(MonsterAffixType::StormStrider)) return;
        
        // 概率触发
        if (NoMoreDay::utils::ThreadSafeRandom::GetFloat(0.0f, 1.0f) > STORMSTRIDER_TRIGGER_CHANCE) return;
        
        // 获取怪物位置
        auto* pos = registry.try_get<Position>(evt.target);
        if (!pos) return;
        
        // 生成雷电残影
        auto ghostEntity = registry.create();
        registry.emplace<Position>(ghostEntity, pos->x, pos->y);
        registry.emplace<LocalLevelTag>(ghostEntity);
        registry.emplace<LightningGhostTag>(ghostEntity);
        registry.emplace<Radius>(ghostEntity, 15.0f);
        
        // 雷电残影组件
        LightningGhostComponent ghost;
        ghost.explosionDelay = STORMSTRIDER_GHOST_DELAY;
        registry.emplace<LightningGhostComponent>(ghostEntity, ghost);
        
        // Hazard 组件 (用于 owner 追踪)
        HazardComponent hazard;
        hazard.owner = evt.target;
        registry.emplace<HazardComponent>(ghostEntity, hazard);
        
        // 视觉效果（半透明黄色）
        registry.emplace<ColorComponent>(ghostEntity, Color{255, 255, 100, 150});
        
        LOG_TRACE("Lightning ghost spawned at ({:.1f}, {:.1f})", pos->x, pos->y);
    }
    
    /**
     * @brief OnDeath 回调 - 处理 Toxic 词缀
     */
    static void OnEnemyDeath(entt::registry& registry, entt::entity enemy) {
        auto* affix = registry.try_get<MonsterAffixComponent>(enemy);
        if (!affix || !affix->hasOnDeath) return;
        
        if (!affix->HasAffix(MonsterAffixType::Toxic)) return;
        
        // 获取怪物位置
        auto* pos = registry.try_get<Position>(enemy);
        if (!pos) return;
        
        // 获取玩家位置
        Position playerPos = {0, 0};
        auto playerView = registry.view<PlayerTag, Position>();
        if (playerView.begin() != playerView.end()) {
            playerPos = playerView.get<Position>(playerView.front());
        }
        
        // 生成 3 个挥发性球体
        for (int i = 0; i < 3; i++) {
            auto orbEntity = registry.create();
            
            // 随机偏移位置
            float offsetX = NoMoreDay::utils::ThreadSafeRandom::GetFloat(-20.0f, 20.0f);
            float offsetY = NoMoreDay::utils::ThreadSafeRandom::GetFloat(-20.0f, 20.0f);
            registry.emplace<Position>(orbEntity, pos->x + offsetX, pos->y + offsetY);
            registry.emplace<LocalLevelTag>(orbEntity);
            registry.emplace<VolatileOrbTag>(orbEntity);
            registry.emplace<Radius>(orbEntity, 12.0f);
            
            // 初始速度（朝向玩家）
            float dx = playerPos.x - pos->x;
            float dy = playerPos.y - pos->y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > 0.1f) {
                float nx = dx / dist;
                float ny = dy / dist;
                registry.emplace<Velocity>(orbEntity, nx * 80.0f, ny * 80.0f);
            } else {
                registry.emplace<Velocity>(orbEntity, 0.0f, 0.0f);
            }
            
            // 挥发性球体组件
            VolatileOrbComponent orb;
            orb.maxLifetime = 3.0f;
            orb.homingStrength = 200.0f;
            orb.speed = 150.0f;
            orb.owner = enemy;
            registry.emplace<VolatileOrbComponent>(orbEntity, orb);
            
            // 视觉效果
            registry.emplace<ColorComponent>(orbEntity, Color{100, 255, 100, 200});
        }
        
        LOG_TRACE("Toxic volatile orbs spawned at ({:.1f}, {:.1f})", pos->x, pos->y);
    }
};

} // namespace NoMoreDay
