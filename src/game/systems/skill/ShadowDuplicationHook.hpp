#pragma once
#include <entt/entt.hpp>
#include "raylib.h"

namespace NoMoreDay {
struct SkillData;
struct CombatStats;

struct PreCastShadowDuplicationResult {
  bool duplicate = false;
  float extra_cost = 0.0f;
};

PreCastShadowDuplicationResult CheckPreCastShadowDuplication(
    entt::registry &registry, entt::entity entity, const SkillData *data,
    float base_cost, const CombatStats *stats);

void ExecutePreCastShadowDuplication(entt::registry &registry, entt::entity entity,
                                    uint32_t skill_id, Vector2 target_pos,
                                    const CombatStats *stats);
} // namespace NoMoreDay
