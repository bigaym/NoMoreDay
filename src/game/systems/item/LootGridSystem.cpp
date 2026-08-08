#include "game/systems/item/LootGridSystem.hpp"
#include "game/ui_shared/UiShared.hpp"
#include "engine/physics/SIMDSpatialGrid.hpp"
#include "game/components/Common.hpp"

namespace NoMoreDay::systems {

void LootGridSystem::update(entt::registry& registry) {
    if (UiShared::s_itemGridDirty && UiShared::s_itemGrid) {
        NoMoreDay::utils::ScopedTimer timer("Loot Grid Rebuild", 100);
        // Use the newly added LootTag for efficient spatial grid rebuilds
        auto lootView = registry.view<LootTag, Position>();
        UiShared::s_itemGrid->rebuild<Position>(lootView, registry);
        
        UiShared::s_itemGridDirty = false;
    }
}

} // namespace NoMoreDay::systems
