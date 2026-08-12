#include "game/systems/item/LootGridSystem.hpp"

#include "core/utils/ScopedTimer.hpp"
#include "engine/render/SIMDSpatialGrid.hpp"
#include "game/foundation/components/Common.hpp"

namespace NoMoreDay::systems {

// --- 战利品空间网格（item 域写 dirty / render 域读查询；U8 收尾自 UiShared 迁入）---
std::unique_ptr<SIMDSpatialGrid> LootGridSystem::s_grid;
bool LootGridSystem::s_dirty = true;

void LootGridSystem::Init() {
  s_grid = std::make_unique<SIMDSpatialGrid>(256, 256, 128.0f);
  s_dirty = true;
}

void LootGridSystem::Shutdown() { s_grid = nullptr; }

void LootGridSystem::update(entt::registry& registry) {
    if (s_dirty && s_grid) {
        NoMoreDay::utils::ScopedTimer timer("Loot Grid Rebuild", 100);
        // Use the newly added LootTag for efficient spatial grid rebuilds
        auto lootView = registry.view<LootTag, Position>();
        s_grid->rebuild<Position>(lootView, registry);
        
        s_dirty = false;
    }
}

} // namespace NoMoreDay::systems
