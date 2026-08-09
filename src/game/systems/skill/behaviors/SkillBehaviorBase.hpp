#pragma once
#include <entt/entt.hpp>
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"

#include <string_view>

namespace NoMoreDay {

struct ElementalConversion {
  Tag source_element = Tag::Physical;
  Tag target_element = Tag::None;
  Color projectile_color = WHITE;
  Color glow_color = WHITE;

  [[nodiscard]] bool IsActive() const noexcept {
    return target_element != Tag::None;
  }
};

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
    
    [[nodiscard]] static ElementalConversion ResolveElementalConversion(
        uint32_t element_node_id, int points) noexcept {
        (void)element_node_id;

        ElementalConversion conv;
        switch (points) {
        case 1: // Fire
            conv.target_element = Tag::Fire;
            conv.projectile_color = {255, 80, 20, 255};
            conv.glow_color = {255, 160, 60, 180};
            break;
        case 2: // Ice / Frost
            conv.target_element = Tag::Cold;
            conv.projectile_color = {100, 200, 255, 255};
            conv.glow_color = {150, 220, 255, 180};
            break;
        case 3: // Lightning
            conv.target_element = Tag::Lightning;
            conv.projectile_color = {200, 180, 255, 255};
            conv.glow_color = {230, 200, 255, 180};
            break;
        default:
            break;
        }
        return conv;
    }

    [[nodiscard]] static float getModifier(uint32_t node_id,
                                           std::string_view key,
                                           float default_val) {
      const auto *node_contract =
          SkillRegistry::Get().GetNodeContract(Derived::kSkillId, node_id);
      if (!node_contract) {
        return default_val;
      }
      if (key == "effectiveness") {
        return node_contract->trigger.effectiveness;
      }
      if (key == "range_mult") {
        return node_contract->trigger.range_mult;
      }
      return default_val;
    }

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
