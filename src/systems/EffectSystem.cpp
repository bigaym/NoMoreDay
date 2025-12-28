#include "EffectSystem.hpp"
#include "../components/EffectComponent.hpp"
#include "../components/Common.hpp"

void EffectSystem::update(entt::registry& registry, float dt) {
    // LOG_TRACE("EffectSystem::update: Processing visual effects");

    // 1. 更新伤害飘字
    auto viewDamage = registry.view<DamagePopup, Position>();
    for(auto entity : viewDamage) {
        auto& popup = viewDamage.get<DamagePopup>(entity);
        auto& pos = viewDamage.get<Position>(entity);

        // 更新计时器
        popup.timer += dt;
        if (popup.timer >= popup.lifeTime) {
            LOG_TRACE("EffectSystem: Destroying damage popup entity {}", (uint32_t)entity);
            registry.destroy(entity);
            continue;
        }

        // 向上浮动
        pos.x += popup.velX * dt;
        pos.y += popup.velY * dt;
        
        // 简单的重力/阻力效果 (可选)
        popup.velY += 50.0f * dt; // 慢慢减缓上升速度
    }

    // 2. 更新攻击特效
    auto viewEffect = registry.view<AttackEffect>();
    for(auto entity : viewEffect) {
        auto& effect = viewEffect.get<AttackEffect>(entity);
        
        effect.timer += dt;
        if (effect.timer >= effect.lifeTime) {
            LOG_TRACE("EffectSystem: Destroying attack effect entity {}", (uint32_t)entity);
            registry.destroy(entity);
        }
    }
}