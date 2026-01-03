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
        
        // Lifetime updated in SkillSystem::Update or here
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
