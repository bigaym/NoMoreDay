#include "game/systems/combat/VisualFXSystem.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "game/components/Combat.hpp"
#include "game/components/Stats.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Common.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/RenderSystem.hpp"
#include "core/utils/FrameRateUtils.hpp"
#include "raymath.h"

namespace NoMoreDay::systems {

void VisualFXSystem::Initialize(entt::registry& registry) {
    // 1. On Hit VFX
    CombatEventDispatcher::Register(CombatEventType::OnSkillHit, [](entt::registry& r, const CombatEvent& evt) {
        if (!r.valid(evt.target) || !r.all_of<Position>(evt.target)) return;
        const auto& pos = r.get<Position>(evt.target);
        Vector2 p = {pos.x, pos.y};
        
        auto& particleSys = GPUParticleSystem::Get();

        // Skill Specific
        if (evt.skill_id == 2) { // Rending Wave
             auto splash = InkEffectHelper::CreateInkSplash(p, 8, 15.0f, 150.0f);
             particleSys.EmitBatch(splash);
        }
        else if (evt.skill_id == 7) { // Mind Blade
             auto splash = InkEffectHelper::CreateInkSplash(p, 8, 15.0f, 150.0f);
             for(auto& part : splash) {
                 part.color = GOLD;
                 particleSys.Emit(part);
             }
        }
        // General Hit (if no specific logic, or always?)
        else if (evt.value > 0.0f) {
            auto splash = InkEffectHelper::CreateInkSplash(p, 3, 10.0f, 80.0f);
            for(auto& part : splash) particleSys.Emit(part);
        }
    }, 100);

    // 2. On Crit VFX
    CombatEventDispatcher::Register(CombatEventType::OnCrit, [](entt::registry& r, const CombatEvent& evt) {
        if (!r.valid(evt.target) || !r.all_of<Position>(evt.target)) return;
        const auto& pos = r.get<Position>(evt.target);
        
        auto& particleSys = GPUParticleSystem::Get();
        
        // Gold Spark
        particleSys.Emit(InkEffectHelper::CreateSpark({pos.x, pos.y}, {0, -100.0f}, GOLD, 2.0f));
        
        // Screen Shake (Small)
        RenderSystem::AddScreenShake(0.15f);
    }, 100);
}

void VisualFXSystem::Update(entt::registry& registry, float dt) {
    // Sword Intent Aura
    auto view = registry.view<SwordIntentComponent, Position>();
    auto& particleSys = GPUParticleSystem::Get();

    for (auto entity : view) {
        auto& intent = view.get<SwordIntentComponent>(entity);
        const auto& pos = view.get<Position>(entity);
        
        if (intent.stacks > 0) {
            // Density based on stacks
            float density = intent.stacks * 5.0f; // Particles per second
            if (utils::FrameRateUtils::ShouldTrigger(density, dt)) {
                components::GPUParticle p;
                p.position = { pos.x + (float)GetRandomValue(-20, 20), pos.y + (float)GetRandomValue(-40, 0) };
                p.velocity = { 0, -50.0f };
                p.acceleration = {0,0};
                p.color = ColorAlpha(WHITE, 0.3f + (intent.stacks * 0.05f));
                p.lifetime = 0.6f;
                p.maxLifetime = 0.6f;
                p.scale = 1.5f;
                p.flags = 2; // Spark
                particleSys.Emit(p);
            }
            
            // Max Stacks Burst (Gold Aura)
            if (intent.stacks >= intent.max_stacks) {
                 if (utils::FrameRateUtils::ShouldTrigger(10.0f, dt)) {
                     components::GPUParticle p;
                     p.position = { pos.x + (float)GetRandomValue(-15,15), pos.y - 20.0f + (float)GetRandomValue(-10,10)};
                     p.velocity = { (float)GetRandomValue(-20,20), (float)GetRandomValue(-50,-20) };
                     p.acceleration = {0,0};
                     p.color = GOLD;
                     p.lifetime = 0.4f;
                     p.maxLifetime = 0.4f;
                     p.scale = 2.0f;
                     p.flags = 2; 
                     particleSys.Emit(p);
                 }
            }
        }
    }
}

} // namespace NoMoreDay::systems
