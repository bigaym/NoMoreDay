#include "game/systems/item/LootGridSystem.hpp"
#include "engine/render/RenderSystem.hpp"
#include "engine/physics/SIMDSpatialGrid.hpp"
#include "game/components/Common.hpp"

namespace NoMoreDay::systems {

void LootGridSystem::update(entt::registry& registry) {
    if (RenderSystem::s_itemGridDirty && RenderSystem::s_itemGrid) {
        NoMoreDay::utils::ScopedTimer timer("Loot Grid Rebuild", 100);
        // Use the newly added LootTag for efficient spatial grid rebuilds
        auto lootView = registry.view<LootTag, Position>();
        RenderSystem::s_itemGrid->rebuild<Position>(lootView, registry);
        
        RenderSystem::s_itemGridDirty = false;
    }
}

} // namespace NoMoreDay::systems
