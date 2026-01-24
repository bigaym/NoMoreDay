#include "TestCommon.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/components/EquipmentComponent.hpp"
#include "game/components/Common.hpp"
#include "game/systems/item/InventorySystem.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/Stats.hpp"
#include "game/components/InventoryComponent.hpp"
#include "game/systems/ui/UISystem.hpp"

TEST_CASE("Item Level Scaling and Requirements") {
    TestSetupScope scope;
    entt::registry registry;

    SUBCASE("Scaling Formula Verification") {
        float mult1 = Constants::Items::GetLevelMultiplier(1);
        float mult50 = Constants::Items::GetLevelMultiplier(50);
        float mult100 = Constants::Items::GetLevelMultiplier(100);

        CHECK(mult1 == doctest::Approx(1.0f));
        CHECK(mult50 == doctest::Approx(1.0f + 49.0f * (1.5f / 99.0f)));
        CHECK(mult100 == doctest::Approx(2.5f));
    }

    SUBCASE("Item Creation with Level") {
        auto item1 = ItemFactory::createWeapon(registry, 1, Rarity::Common);
        auto item100 = ItemFactory::createWeapon(registry, 100, Rarity::Common);

        auto& comp1 = registry.get<ItemComponent>(item1);
        auto& comp100 = registry.get<ItemComponent>(item100);

        CHECK(comp1.itemLevel == 1);
        CHECK(comp100.itemLevel == 100);
        
        // Stats should be approx 2.5x
        // Note: Base stats have some randomness, but multiplier should dominate.
        CHECK(comp100.attack >= comp1.attack * 2.0f); 
    }

    SUBCASE("Level Requirement Enforcement") {
        auto player = registry.create();
        registry.emplace<PlayerTag>(player);
        auto& stats = registry.emplace<PlayerStats>(player);
        stats.level = 10;
        
        registry.emplace<InventoryComponent>(player);
        registry.emplace<EquipmentComponent>(player);

        auto lowLvlItem = ItemFactory::createWeapon(registry, 5, Rarity::Common);
        auto highLvlItem = ItemFactory::createWeapon(registry, 20, Rarity::Common);

        // Equip low level item should succeed
        CHECK(InventorySystem::equipItem(registry, player, lowLvlItem) == true);

        // Equip high level item should fail
        CHECK(InventorySystem::equipItem(registry, player, highLvlItem) == false);

        // Level up player
        stats.level = 20;
        CHECK(InventorySystem::equipItem(registry, player, highLvlItem) == true);
    }
}
