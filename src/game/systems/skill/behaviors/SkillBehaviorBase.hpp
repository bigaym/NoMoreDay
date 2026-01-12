#pragma once
#include <entt/entt.hpp>
#include "game/components/SkillDefs.hpp"
#include "game/systems/skill/SkillSystem.hpp"

namespace NoMoreDay {

/**
 * @brief CRTP base class for skill behaviors.
 * 
 * Provides zero-overhead static polymorphism for skill logic.
 * Derived classes should implement DoCast() and optionally DoTick(), DoEnd().
 * 
 * Usage:
 *   struct FlowingThrust : SkillBehaviorBase<FlowingThrust> {
 *       static constexpr uint32_t kSkillId = 1;
 *       static void DoCast(entt::registry&, entt::entity, SkillExecution&);
 *   };
 */
template<typename Derived>
struct SkillBehaviorBase {
    // MSVC fix: Derived is incomplete during base class instantiation in CRTP.
    // static constexpr uint32_t SkillId = Derived::kSkillId;
    
    /**
     * @brief Called when the skill effect should be executed.
     * Derived class MUST implement DoCast().
     */
    static void OnCast(entt::registry& reg, entt::entity owner, SkillExecution& exec) {
        Derived::DoCast(reg, owner, exec);
    }
    
    /**
     * @brief Called each tick for channeled skills.
     * Optional - only implement if the skill is channeled.
     */
    static void OnTick(entt::registry& reg, entt::entity owner, ChannelingComponent& chan, float dt) {
        if constexpr (requires { Derived::DoTick(reg, owner, chan, dt); }) {
            Derived::DoTick(reg, owner, chan, dt);
        }
    }
    
    /**
     * @brief Called when the skill ends (channeling finishes, buff expires, etc).
     * Optional - only implement for special cleanup.
     */
    static void OnEnd(entt::registry& reg, entt::entity owner, uint32_t skill_id) {
        if constexpr (requires { Derived::DoEnd(reg, owner, skill_id); }) {
            Derived::DoEnd(reg, owner, skill_id);
        }
    }
    
    /**
     * @brief Called when skill hits a target.
     * Optional - for skills with special on-hit effects.
     */
    static void OnHit(entt::registry& reg, entt::entity attacker, entt::entity target, 
                      Tag hit_tags, bool is_crit) {
        if constexpr (requires { Derived::DoHit(reg, attacker, target, hit_tags, is_crit); }) {
            Derived::DoHit(reg, attacker, target, hit_tags, is_crit);
        }
    }
};

} // namespace NoMoreDay
