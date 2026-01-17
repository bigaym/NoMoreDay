#include "TestCommon.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "doctest.h"

using namespace NoMoreDay;

TEST_SUITE("Legendary Affix Infrastructure") {
    TEST_CASE("Legendary Affixes are loaded but not randomly rolled") {
        entt::registry registry;
        ItemFactory::initialize();

        // 1. Verify that legendary affixes are in the definition pool
        // Since s_affixDefinitions is private, we can only verify indirectly 
        // or we need to add a getter. 
        // But we can check if createAffix can create one if we know the ID.
        
        Affix legAffix = ItemFactory::createAffix(static_cast<AffixType>(1001), 7);
        CHECK(static_cast<uint16_t>(legAffix.type) == 1001);
        
        // 2. Perform multiple random rolls and ensure none have ID > 1000
        bool foundLegendary = false;
        for (int i = 0; i < 100; ++i) {
            // Generate a legendary item to get maximum affixes
            entt::entity itemEntity = ItemFactory::createWeapon(registry, 100, Rarity::Legendary);
            const auto& item = registry.get<ItemComponent>(itemEntity);
            
            for (const auto& aff : item.affixes) {
                if (static_cast<uint16_t>(aff.type) >= 1000) {
                    foundLegendary = true;
                    break;
                }
            }
            if (foundLegendary) break;
            registry.destroy(itemEntity);
        }
        
        CHECK_MESSAGE(!foundLegendary, "Legendary affixes should not be rolled randomly");
    }

    TEST_CASE("Legendary Affix Name and Value Verification") {
        ItemFactory::initialize();
        
        // 1001 is leg_infernal_touch (炼狱之触) in assets/data/legendary_affixes.json
        AffixType type = static_cast<AffixType>(1001);
        Affix aff = ItemFactory::createAffix(type, 7);
        
        // Verify value (JSON says 1.0)
        CHECK(aff.value == 1.0f);
        
        // Verify Name Display
        std::string desc = GetAffixDescription(aff, false);
        CHECK(desc == "炼狱之触");
        
        // Verify with tier prefix
        std::string descWithTier = GetAffixDescription(aff, true);
        CHECK(descWithTier == "[T7] 炼狱之触");
    }

    TEST_CASE("AffixType uint16_t capacity test") {
        // Ensure static_cast and storage works for > 255
        AffixType type = static_cast<AffixType>(1234);
        uint16_t raw = static_cast<uint16_t>(type);
        CHECK(raw == 1234);
    }
}