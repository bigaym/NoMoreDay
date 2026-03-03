#include "game/systems/combat/DamageMitigationService.hpp"
#include "game/systems/combat/CombatFormula.hpp"
#include "game/systems/combat/StatsSystem.hpp"
#include <algorithm>
#include <bit>

namespace NoMoreDay {
namespace {

float ClampMoreToMultiplier(float more) {
  return std::max(0.0f, 1.0f + more);
}

} // namespace

float DamageMitigationService::Apply(
    entt::registry &registry, entt::entity attacker, uint32_t skill_id,
    Tag instance_tags, Tag final_type, float damage,
    const CombatStats *defender_stats,
    const systems::EndgameModifierAggregate &endgame, bool skip_mitigation,
    bool was_blocked, float block_multiplier, entt::entity source_entity) {
  using namespace NoMoreDay::Constants::Combat::Pipeline;

  const int type_idx = std::countr_zero(static_cast<uint64_t>(final_type));
  float damage_after_res = damage;
  if (skip_mitigation) {
    return damage_after_res;
  }

  if (was_blocked) {
    damage_after_res *= block_multiplier;
  }

  float res = 0.0f;
  if (type_idx < ELEMENTAL_TYPE_COUNT) {
    res = defender_stats ? defender_stats->resistances[type_idx] : 0.0f;
    res += endgame.incoming_resistance_bonus;
    res -= endgame.outgoing_resistance_reduction;
    res = std::clamp(res, RESISTANCE_MIN, RESISTANCE_MAX);
  }

  damage_after_res *= (1.0f - res);

  if (final_type == Tag::Physical && defender_stats) {
    float armor = defender_stats->armor + endgame.incoming_armor_bonus;
    const float pen = StatsSystem::GetStatWithTags(
        registry, attacker, StatType::ArmorPenetration, instance_tags, skill_id,
        source_entity);
    const float effective_armor =
        armor - pen - endgame.outgoing_armor_reduction;
    const int area_level = defender_stats->cached_area_level;
    const float armor_multiplier =
        NoMoreDay::CombatFormula::CalculateArmorMultiplier(effective_armor,
                                                           area_level);

    damage_after_res *= armor_multiplier;
  }

  if (defender_stats && defender_stats->damage_reduction > 0.0f) {
    const float effective_dr = std::clamp(
        defender_stats->damage_reduction +
            endgame.incoming_global_damage_reduction_bonus -
            endgame.outgoing_global_damage_reduction_reduction,
        0.0f, DR_MAX);
    damage_after_res *= (1.0f - effective_dr);
  } else {
    const float effective_dr = std::clamp(
        endgame.incoming_global_damage_reduction_bonus -
            endgame.outgoing_global_damage_reduction_reduction,
        0.0f, DR_MAX);
    damage_after_res *= (1.0f - effective_dr);
  }

  damage_after_res *= ClampMoreToMultiplier(endgame.incoming_damage_taken_more);

  return damage_after_res;
}

} // namespace NoMoreDay
