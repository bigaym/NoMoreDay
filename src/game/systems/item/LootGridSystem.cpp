#include "game/systems/item/LootGridSystem.hpp"
#include "game/render/GameplayRenderAdapter.hpp"
#include "engine/physics/SIMDSpatialGrid.hpp"
#include "game/components/Common.hpp"

namespace NoMoreDay::systems {

void LootGridSystem::update(entt::registry& registry) {
    if (GameplayRenderAdapter::s_itemGridDirty && GameplayRenderAdapter::s_itemGrid) {
        NoMoreDay::utils::ScopedTimer timer("Loot Grid Rebuild", 100);
        // Use the newly added LootTag for efficient spatial grid rebuilds
        auto lootView = registry.view<LootTag, Position>();
        GameplayRenderAdapter::s_itemGrid->rebuild<Position>(lootView, registry);
        
        GameplayRenderAdapter::s_itemGridDirty = false;
    }
}

} // namespace NoMoreDay::systems
