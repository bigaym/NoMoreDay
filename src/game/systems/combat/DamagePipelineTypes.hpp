#pragma once
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include <entt/entt.hpp>
#include <vector>

namespace NoMoreDay {

struct DamageResult {
  float total_damage = 0.0f;
  bool is_crit = false;
  bool was_dodged = false;
  bool was_blocked = false;
  float block_multiplier = 1.0f;
  DamagePool final_pool; // Damage broken down by type
};

struct DamageRequest {
  entt::entity attacker = entt::null;
  entt::entity defender = entt::null;
  uint32_t skill_id = 0;
  DamagePool base_pool;
  float added_effectiveness = 1.0f;
  float trigger_effectiveness = 1.0f;
  Tag additional_tags = Tag::None;
  entt::entity source_entity = entt::null;
  bool is_simulation = false;
  bool dispatch_damage_events = true;
  bool skip_mitigation = false;
  bool thorns_like_damage = false;
};

struct DamageExecutionResult {
  DamageResult damage;
  bool target_killed = false;
  float final_applied_damage = 0.0f;
  float barrier_absorbed = 0.0f;
  bool was_prevented = false;
};

} // namespace NoMoreDay
