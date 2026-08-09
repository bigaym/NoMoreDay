#include "TestCommon.hpp"
#include "engine/render/GPULootSystem.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/application/render/GPULootAdapter.hpp"

namespace NoMoreDay::render {

TEST_CASE("[Unit] GPULootSystem - UploadInstances grows to fit actual required count") {
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

    // Game-side projection produces the exact instance set; the Engine sizes to
    // the span, so the prior truncation-to-capacity bug cannot resurface.
    NoMoreDay::LootProjection projection =
        NoMoreDay::GPULootAdapter::BuildLoot(registry);
    lootSystem.UploadInstances(projection.instances);

    CHECK(lootSystem.GetSyncedInstanceCount() == 5);
    CHECK(lootSystem.GetMaxInstancesForTest() >= 5);
}

} // namespace NoMoreDay::render
