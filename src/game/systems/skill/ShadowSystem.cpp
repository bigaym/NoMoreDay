#include "game/systems/skill/ShadowSystem.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/foundation/components/Combat.hpp"

namespace NoMoreDay {

    void ShadowSystem::Update(entt::registry& registry, float dt) {
        auto shadow_view = registry.view<ShadowComponent>();
        std::vector<entt::entity> expired_shadows;
        
        for (auto entity : shadow_view) {
            auto& shadow = shadow_view.get<ShadowComponent>(entity);
            
            if (!shadow.triggered) {
                shadow.delay -= dt;
                if (shadow.delay <= 0.0f) {
                    // Apply snapshot stats to the entity before casting
                    registry.emplace_or_replace<CombatStats>(entity, shadow.snapshot.stats);
                    
                    // Execute the skill
                    SkillSystem::ShadowCast(registry, entity, shadow.snapshot.skill_id, shadow.snapshot.position, shadow.snapshot.target_pos);
                    shadow.triggered = true;
                }
            }

            bool is_expired = false;
            
            // Logic: If triggered, wait for cast to finish.
            // If lifetime ends, destroy anyway (hard limit).
            // But we should probably allow animation to finish if lifetime > 0?
            // The logic below says: if triggered AND not casting, expire.
            
            if (shadow.triggered) {
                bool still_casting = false;
                auto exec_view = registry.view<SkillExecution>();
                for(auto exec_ent : exec_view) {
                    if(exec_view.get<SkillExecution>(exec_ent).owner == entity) {
                        still_casting = true;
                        break;
                    }
                }
                if (!still_casting) is_expired = true;
            }

            shadow.lifetime -= dt;
            if (shadow.lifetime <= 0.0f) is_expired = true;

            if (is_expired) {
                expired_shadows.push_back(entity);
            }
        }
        
        for (auto e : expired_shadows) {
            if (registry.valid(e)) {
                registry.destroy(e);
            }
        }
    }

}
