#pragma once

#include "game/data/MonsterAffixRegistry.hpp"
#include "game/components/Common.hpp"
#include "game/components/Stats.hpp"
#include "game/components/Buff.hpp"
#include "game/components/AIComponent.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "core/logging/Logger.hpp"
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
    
    static constexpr float FROZEN_AURA_RADIUS = 150.0f;     // 冰冻光环半径
    static constexpr float FROZEN_SLOW_AMOUNT = 0.3f;       // 减速量 (30%)
    static constexpr float FROZEN_AURA_TICK = 0.5f;         // 光环刷新间隔
    
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
                        ProcessFrozen(registry, entity, pos, playerEntity, affix, dt);
                        break;
                    default:
                        break;
                }
            }
        }
    }
    
private:
    /**
     * @brief 熔火词缀 - 生成火焰路径
     */
    static void ProcessMolten(entt::registry& registry, entt::entity enemy, 
                              const Position& pos, MonsterAffixComponent& affix, float dt) {
        if (affix.timer1 >= MOLTEN_TICK_INTERVAL) {
            affix.timer1 = 0.0f;
            
            // Create fire trail entity
            auto fireEntity = registry.create();
            registry.emplace<Position>(fireEntity, pos.x, pos.y);
            registry.emplace<LocalLevelTag>(fireEntity);
            
            // Use DelayedDestroyComponent for auto-cleanup
            registry.emplace<DelayedDestroyComponent>(fireEntity, MOLTEN_TRAIL_DURATION);
            
            // Add a simple visual marker (will be rendered by EffectSystem)
            registry.emplace<ColorComponent>(fireEntity, Color{255, 80, 0, 200}); // Orange
            registry.emplace<Radius>(fireEntity, MOLTEN_TRAIL_RADIUS);
            
            // Tag for damage zone processing
            registry.emplace<MoltenTrailTag>(fireEntity);
            
            LOG_TRACE("Molten trail spawned at ({:.1f}, {:.1f})", pos.x, pos.y);
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
     * @brief 极寒词缀 - 周围减速光环
     */
    static void ProcessFrozen(entt::registry& registry, entt::entity enemy,
                              const Position& enemyPos, entt::entity player,
                              MonsterAffixComponent& affix, float dt) {
        // Check every 0.5s
        static float frozenTimer = 0.0f;
        frozenTimer += dt;
        if (frozenTimer < FROZEN_AURA_TICK) return;
        frozenTimer = 0.0f;
        
        if (!registry.valid(player)) return;
        
        const auto* playerPos = registry.try_get<Position>(player);
        if (!playerPos) return;
        
        float dx = playerPos->x - enemyPos.x;
        float dy = playerPos->y - enemyPos.y;
        float distSq = dx * dx + dy * dy;
        
        if (distSq < FROZEN_AURA_RADIUS * FROZEN_AURA_RADIUS) {
            // Apply slow debuff to player
            auto& effects = registry.get_or_emplace<ActiveEffectsComponent>(player);
            
            BuffEffect slow;
            slow.id = "frozen_aura_slow";
            slow.name = "冰寒减速";
            slow.description = "被冰寒光环减速";
            slow.type = BuffType::SpeedDown;
            slow.is_debuff = true;
            slow.duration = 1.0f;
            slow.remaining = 1.0f;
            slow.modifiers.push_back({StatType::MoveSpeed, ModifierMode::PercentAdd, -FROZEN_SLOW_AMOUNT * 100.0f});
            
            effects.AddOrRefresh(slow);
        }
    }
    
    /**
     * @brief OnDealDamage 回调 - 处理 Nullifier 词缀
     */
    static void OnEnemyDealDamage(entt::registry& registry, const CombatEvent& evt) {
        // Check if attacker has Nullifier affix
        auto* affix = registry.try_get<MonsterAffixComponent>(evt.source);
        if (!affix || !affix->hasOnHit) return;
        
        if (!affix->HasAffix(MonsterAffixType::Nullifier)) return;
        
        // Dispel player buffs
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
};

} // namespace NoMoreDay
