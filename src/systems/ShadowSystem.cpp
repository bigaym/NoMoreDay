#include "ShadowSystem.hpp"
#include "../components/SkillSystem.hpp"
#include "../components/Common.hpp"
#include "SkillSystem.hpp"
#include "../tools/Logger.hpp"

namespace NoMoreDay {

void ShadowSystem::Update(entt::registry& registry, float dt) {
    auto view = registry.view<ShadowComponent>();
    for (auto entity : view) {
        auto& shadow = view.get<ShadowComponent>(entity);
        
        // 1. Trigger skill if delay is up
        if (!shadow.triggered) {
            shadow.delay -= dt;
            if (shadow.delay <= 0.0f) {
                // Note: We use the snapshot stats for damage calculation if needed.
                // For now, SkillSystem::ShadowCast handles the execution logic.
                SkillSystem::ShadowCast(registry, entity, shadow.snapshot.skill_id, shadow.snapshot.position, shadow.snapshot.target_pos);
                shadow.triggered = true;
            }
        }

        // 2. Update lifetime
        shadow.lifetime -= dt;
        if (shadow.lifetime <= 0.0f) {
            // Check if shadow is still casting anything
            bool still_casting = false;
            auto exec_view = registry.view<SkillExecution>();
            for(auto exec_ent : exec_view) {
                if(exec_view.get<SkillExecution>(exec_ent).owner == entity) {
                    still_casting = true;
                    break;
                }
            }

            if (!still_casting) {
                registry.destroy(entity);
            }
        }
    }

    // Process legacy ShadowLifetime if any (compatibility)
    auto legacy_view = registry.view<ShadowLifetime>(entt::exclude<ShadowComponent>);
    for (auto entity : legacy_view) {
        auto& lifetime = legacy_view.get<ShadowLifetime>(entity);
        lifetime.remaining -= dt;
        if (lifetime.remaining <= 0.0f) {
            bool still_casting = false;
            auto exec_view = registry.view<SkillExecution>();
            for(auto exec_ent : exec_view) {
                if(exec_view.get<SkillExecution>(exec_ent).owner == entity) {
                    still_casting = true;
                    break;
                }
            }
            if (!still_casting) {
                registry.destroy(entity);
            }
        }
    }
}

}
