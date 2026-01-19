#pragma once

#include "game/components/Common.hpp"
#include "game/components/Stats.hpp"
#include "game/components/AIComponent.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/AdvancedAffixComponents.hpp"
#include "game/data/MonsterAffixRegistry.hpp"
#include "core/logging/Logger.hpp"
#include <entt/entt.hpp>
#include <cstdlib>

namespace NoMoreDay {
namespace EntityUtils {

/**
 * @brief 克隆一个实体 (用于 Mirror Image 词缀)
 * 
 * 复制外观、位置和基础属性,但重置状态组件
 * 克隆体拥有降低的生命和伤害
 * 
 * @param registry ECS注册表
 * @param source 源实体
 * @param damageMultiplier 伤害倍率 (默认 0.5)
 * @param healthMultiplier 生命倍率 (默认 0.1)
 * @param lifetime 克隆体存活时间 (默认 10秒)
 * @return 克隆体实体ID
 */
inline entt::entity CloneEntity(entt::registry& registry, entt::entity source,
                                float damageMultiplier = 0.5f,
                                float healthMultiplier = 0.1f,
                                float lifetime = 10.0f) {
    if (!registry.valid(source)) {
        LOG_ERROR("CloneEntity: Invalid source entity");
        return entt::null;
    }
    
    // 创建新实体
    auto clone = registry.create();
    
    // === 复制外观组件 ===
    if (auto* pos = registry.try_get<Position>(source)) {
        // 在源实体附近随机位置生成
        float offsetX = (rand() % 100 - 50) * 0.5f;
        float offsetY = (rand() % 100 - 50) * 0.5f;
        registry.emplace<Position>(clone, pos->x + offsetX, pos->y + offsetY);
    }
    
    if (auto* vel = registry.try_get<Velocity>(source)) {
        registry.emplace<Velocity>(clone, 0.0f, 0.0f);  // 初始速度为0
    }
    
    if (auto* radius = registry.try_get<Radius>(source)) {
        registry.emplace<Radius>(clone, radius->value);
    }
    
    if (auto* tex = registry.try_get<TextureIDComponent>(source)) {
        registry.emplace<TextureIDComponent>(clone, tex->id);
    }
    
    if (auto* color = registry.try_get<ColorComponent>(source)) {
        // 克隆体稍微透明
        Color cloneColor = color->color;
        cloneColor.a = 180;
        registry.emplace<ColorComponent>(clone, cloneColor);
    }
    
    // === 复制基础组件 ===
    registry.emplace<GPUIndex>(clone, -1);
    registry.emplace<IDComponent>(clone);  // UUID 会自动初始化为 0
    registry.emplace<LocalLevelTag>(clone);
    registry.emplace<EnemyTag>(clone);
    
    // === 复制状态组件 (但修改数值) ===
    if (auto* enemyState = registry.try_get<EnemyStateComponent>(source)) {
        registry.emplace<EnemyStateComponent>(clone, enemyState->raceType, enemyState->archetypeType);
    }
    
    // 复制战斗属性 (降低伤害)
    if (auto* srcStats = registry.try_get<CombatStats>(source)) {
        auto& cloneStats = registry.emplace<CombatStats>(clone);
        cloneStats = *srcStats;  // 复制所有属性
        
        // 降低伤害 (直接修改武器伤害)
        cloneStats.min_weapon_damage *= damageMultiplier;
        cloneStats.max_weapon_damage *= damageMultiplier;
    }
    
    // 复制生命 (大幅降低)
    if (auto* srcHealth = registry.try_get<HealthComponent>(source)) {
        float cloneMaxHP = srcHealth->max * healthMultiplier;
        registry.emplace<HealthComponent>(clone, cloneMaxHP, cloneMaxHP);
    }
    
    // 复制 AI 组件
    if (auto* srcAI = registry.try_get<AIComponent>(source)) {
        registry.emplace<AIComponent>(clone, 
            AIType::CHASE,  // 克隆体直接进入追击模式
            srcAI->detectionRange,
            srcAI->attackRange,
            srcAI->speed
        );
    }
    
    // 复制攻击状态
    if (auto* srcAttack = registry.try_get<AttackState>(source)) {
        registry.emplace<AttackState>(clone, *srcAttack);
    }
    
    // === 添加克隆体标记 ===
    registry.emplace<CloneComponent>(clone, 
        source,              // parent
        damageMultiplier,
        healthMultiplier,
        lifetime,
        0.0f,                // elapsed
        true,                // hasInvulnerableFrame
        1.0f                 // invulnerableDuration
    );
    
    // 添加短暂无敌帧
    registry.emplace<InvulnerableComponent>(clone, 
        1.0f,                // duration
        0.0f,                // elapsed
        source,              // source
        Color{180, 180, 255, 150},  // shieldColor
        0.0f                 // shieldRadius (使用实体半径)
    );
    
    // 触发属性重算
    registry.emplace<StatsDirty>(clone);
    
    LOG_INFO("CloneEntity: Created clone {} from source {}", 
             static_cast<uint32_t>(clone), static_cast<uint32_t>(source));
    
    return clone;
}

/**
 * @brief 更新克隆体组件 - 处理生命周期和无敌帧
 * 
 * @param registry ECS注册表
 * @param dt 时间增量
 */
inline void UpdateClones(entt::registry& registry, float dt) {
    auto view = registry.view<CloneComponent, HealthComponent>();
    
    for (auto entity : view) {
        auto& clone = view.get<CloneComponent>(entity);
        
        clone.elapsed += dt;
        
        // 检查是否超时
        if (clone.elapsed >= clone.lifetime) {
            registry.destroy(entity);
            continue;
        }
        
        // 更新无敌帧
        if (clone.hasInvulnerableFrame && clone.elapsed < clone.invulnerableDuration) {
            // 无敌帧期间确保有 InvulnerableComponent
            if (!registry.all_of<InvulnerableComponent>(entity)) {
                registry.emplace<InvulnerableComponent>(entity, 
                    clone.invulnerableDuration - clone.elapsed,
                    0.0f,
                    clone.parent,
                    Color{180, 180, 255, 150},
                    0.0f
                );
            }
        } else if (clone.hasInvulnerableFrame) {
            // 无敌帧结束,移除组件
            clone.hasInvulnerableFrame = false;
            registry.remove<InvulnerableComponent>(entity);
        }
    }
}

/**
 * @brief 更新连接组件 - 处理生命周期
 * 
 * @param registry ECS注册表
 * @param dt 时间增量
 */
inline void UpdateLinks(entt::registry& registry, float dt) {
    auto view = registry.view<LinkComponent>();
    
    for (auto entity : view) {
        auto& link = view.get<LinkComponent>(entity);
        
        // 检查目标是否依然有效
        if (!registry.valid(link.target) || registry.all_of<KilledTag>(link.target)) {
            link.isActive = false;
            link.lifetime = -1.0f; // 标记为待移除
        }
        
        if (link.lifetime > 0.0f) {
            link.lifetime -= dt;
            if (link.lifetime <= 0.0f) {
                link.isActive = false;
            }
        }
    }
    
    // 移除失效或超时的连接
    auto view2 = registry.view<LinkComponent>();
    std::vector<entt::entity> toRemove;
    for (auto entity : view2) {
        if (!view2.get<LinkComponent>(entity).isActive) {
            toRemove.push_back(entity);
        }
    }
    
    for (auto entity : toRemove) {
        registry.remove<LinkComponent>(entity);
    }
}

/**
 * @brief 更新无敌组件 - 处理持续时间
 * 
 * @param registry ECS注册表
 * @param dt 时间增量
 */
inline void UpdateInvulnerable(entt::registry& registry, float dt) {
    auto view = registry.view<InvulnerableComponent>();
    
    for (auto entity : view) {
        auto& invuln = view.get<InvulnerableComponent>(entity);
        
        // 永久无敌 (duration == 0) 不处理
        if (invuln.duration == 0.0f) continue;
        
        invuln.elapsed += dt;
        
        // 检查是否超时
        if (invuln.elapsed >= invuln.duration) {
            registry.remove<InvulnerableComponent>(entity);
        }
    }
}

} // namespace EntityUtils
} // namespace NoMoreDay
