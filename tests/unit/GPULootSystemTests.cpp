#include "TestCommon.hpp"
#include "engine/render/GPULootSystem.hpp"
#include "game/components/Common.hpp"
#include "game/components/ItemComponent.hpp"

namespace NoMoreDay::render {

TEST_CASE("[Unit] GPULootSystem - SyncDroppedItems grows to fit actual required count") {
    TestSetupScope scope;
    GPULootSystem lootSystem;
    
    // We can't easily Init with real GPU context in unit tests, 
    // but we can manually set m_initialized to true for this test if we had access.
    // Since we don't, we'll call Init(2) and hope the buffer creation failures 
    // don't prevent the logic we want to test.
    lootSystem.Init(2); 

    entt::registry registry;
    
    // Create 5 loot items
    for (int i = 0; i < 5; ++i) {
        auto entity = registry.create();
        registry.emplace<LootTag>(entity);
        registry.emplace<Position>(entity, static_cast<float>(i) * 10.0f, 0.0f);
        registry.emplace<GoldComponent>(entity);
    }

    // This will currently truncate to 2 if Init(2) was called, 
    // because SyncDroppedItems has a break when size >= m_maxInstances.
    lootSystem.SyncDroppedItems(registry);

    // Current implementation will FAIL these checks if truncation happens.
    CHECK(lootSystem.GetSyncedInstanceCount() == 5);
    CHECK(lootSystem.GetMaxInstancesForTest() >= 5);
}

} // namespace NoMoreDay::render
