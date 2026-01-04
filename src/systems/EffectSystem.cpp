#include "EffectSystem.hpp"
#include "GPUParticleSystem.hpp"
#include "../components/EffectComponent.hpp"
#include "../components/Common.hpp"
#include "../components/GPUData.hpp"
#include "raymath.h"

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
        
        // --- Animation Logic ---
        
        // Scale animation (Pop effect)
        if (popup.isCrit) {
            // Crit: Pop big, then settle (弹性效果)
            if (popup.timer < 0.15f) {
                // 0 -> 0.15s: 0.5 -> 1.8
                popup.currentScale = 0.5f + (popup.timer / 0.15f) * 1.3f;
            } else if (popup.timer < 0.3f) {
                // 0.15 -> 0.3s: 1.8 -> 1.2
                float t = (popup.timer - 0.15f) / 0.15f;
                popup.currentScale = 1.8f - t * 0.6f;
            } else {
                popup.currentScale = 1.2f;
            }
        } else {
            // Normal: Slight pop
            if (popup.timer < 0.1f) {
                popup.currentScale = 0.5f + (popup.timer / 0.1f) * 0.5f;
            } else {
                popup.currentScale = 1.0f;
            }
        }

        // Movement Physics
        pos.x += popup.velX * dt;
        pos.y += popup.velY * dt;
        
        // Gravity (slow down upward movement, eventually fall)
        // 假设 velY 初始为负值 (向上)
        popup.velY += 200.0f * dt; 
        
        // Drag on X
        popup.velX *= (1.0f - 3.0f * dt);
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

    // 3. 更新通用视觉特效
    auto viewVisual = registry.view<VisualEffect, Position>();
    for(auto entity : viewVisual) {
        auto& effect = viewVisual.get<VisualEffect>(entity);
        auto& pos = viewVisual.get<Position>(entity);

        // 如果特效刚开始，发射一些粒子
        if (effect.timer == 0.0f) {
            for (int i = 0; i < 30; i++) {
                NoMoreDay::components::GPUParticle p;
                p.position = { pos.x, pos.y };
                float angle = (float)(rand() % 360) * DEG2RAD;
                float speed = (float)(rand() % 150 + 50);
                p.velocity = { cosf(angle) * speed, sinf(angle) * speed };
                
                p.color = effect.color; 
                
                p.lifetime = 0.5f + (rand() % 100) / 100.0f;
                p.maxLifetime = p.lifetime;
                p.scale = 2.0f + (rand() % 40) / 10.0f;
                
                NoMoreDay::systems::GPUParticleSystem::Get().Emit(p);
            }
        }
        
        effect.timer += dt;
        if (effect.timer >= effect.lifeTime) {
             // 简单的淡出或结束逻辑，这里直接销毁
             registry.destroy(entity);
        }
    }
}