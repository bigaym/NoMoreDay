#pragma once

#include "game/components/Stats.hpp"
#include "game/systems/combat/EndgameModifierContract.hpp"
#include <cstdint>
#include <entt/entt.hpp>

namespace NoMoreDay {

class DamageMitigationService {
public:
  [[nodiscard]] static float Apply(
      entt::registry &registry, entt::entity attacker, entt::entity defender,
      uint32_t skill_id,
      Tag instance_tags, Tag final_type, float damage,
      const CombatStats *defender_stats,
      const systems::EndgameModifierAggregate &endgame, bool skip_mitigation,
      bool was_blocked, float block_multiplier, entt::entity source_entity);
};

} // namespace NoMoreDay
