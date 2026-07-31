#pragma once

#include "game/systems/physics/SpatialGrid.hpp"
#include "game/components/Common.hpp"
#include "game/components/Stats.hpp"
#include <entt/entt.hpp>
#include <raylib.h>

namespace NoMoreDay::systems {

class SummonCombatBridge {
public:
  static CombatStats ResolveInheritedStats(entt::registry &registry,
                                           entt::entity summon);
  static bool CastSpiritSwordShadow(entt::registry &registry, entt::entity summon,
                                    entt::entity target, const Vector2 &origin,
                                    bool is_giant);
  static void ApplyMeleeOrbitContact(entt::registry &registry,
                                     entt::entity summon,
                                     const SpatialHashGrid &grid,
                                     const Position &origin);

private:
  static bool ConsumeProcBudget(entt::registry &registry, entt::entity summon,
                                float cost);
  static void EnsureSummonAttribution(entt::registry &registry,
                                      entt::entity summon);
};

} // namespace NoMoreDay::systems
