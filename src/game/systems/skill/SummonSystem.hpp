#pragma once
#include <entt/entt.hpp>
#include "engine/physics/SpatialGrid.hpp"

namespace NoMoreDay::systems {

class SummonSystem {
public:
    static void Update(entt::registry& registry, float dt, const SpatialHashGrid& grid);
    static void UpdateSpiritSwords(entt::registry& registry, float dt, const SpatialHashGrid& grid);

private:
};

} // namespace NoMoreDay::systems
