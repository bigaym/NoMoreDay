#pragma once
#include "engine/physics/SpatialGrid.hpp"
#include "engine/physics/SIMDSpatialGrid.hpp"
#include <entt/entt.hpp>
#include "game/components/Projectile.hpp"
#include "game/components/Common.hpp"
#include <taskflow/taskflow.hpp>


namespace NoMoreDay {

class ProjectileSystem {
public:
  enum class DeathReason { Expired, Collision };

  static void Update(entt::registry &registry, systems::SpatialHashGrid &grid,
                     float dt, tf::Executor *executor = nullptr);

  // Lifecycle Callbacks (Phase 2)
  static void OnProjectileDeath(entt::registry &registry, entt::entity entity,
                                const struct Projectile &proj,
                                DeathReason reason);

  // Behavior Implementations
  static void SpawnSplitProjectiles(entt::registry &registry,
                                    entt::entity parent_ent,
                                    const struct Projectile &parent);
  static void SpawnExplosionProjectiles(entt::registry &registry,
                                        entt::entity parent_ent,
                                        const struct Projectile &parent);
  static void ConvertToHoveringHazard(entt::registry &registry,
                                      entt::entity proj_ent,
                                      const struct Projectile &proj);

private:
  static inline systems::SIMDSpatialGrid s_enemyGrid{Constants::World::GRID_COLS,
                                                     Constants::World::GRID_ROWS,
                                                     Constants::World::GRID_CELL_SIZE};
};

} // namespace NoMoreDay
