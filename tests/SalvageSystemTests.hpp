#pragma once
#include "doctest.h"
#include "game/systems/item/SalvageSystem.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/components/InventoryComponent.hpp"
#include "game/components/MaterialBankComponent.hpp"
#include <entt/entt.hpp>

namespace NoMoreDay::Test {

TEST_CASE("SalvageSystem: Basic Functionality") {
    entt::registry registry;
    auto player = registry.create();
    registry.emplace<MaterialBankComponent>(player);

    SUBCASE("CanSalvage logic") {
        ItemComponent magicItem;
        magicItem.rarity = Rarity::Magic;
        magicItem.type = ItemType::Weapon;
        CHECK(SalvageSystem::CanSalvage(magicItem) == true);

        ItemComponent commonItem;
        commonItem.rarity = Rarity::Common;
        commonItem.type = ItemType::Weapon;
        CHECK(SalvageSystem::CanSalvage(commonItem) == false);

        ItemComponent uniqueItem;
        uniqueItem.rarity = Rarity::Legendary;
        uniqueItem.type = ItemType::Weapon;
        uniqueItem.legendaryPotential = 1;
        CHECK(SalvageSystem::CanSalvage(uniqueItem) == false);
        
        ItemComponent materialItem;
        materialItem.rarity = Rarity::Magic;
        materialItem.type = ItemType::Material;
        CHECK(SalvageSystem::CanSalvage(materialItem) == false);

        ItemComponent lockedItem;
        lockedItem.rarity = Rarity::Magic;
        lockedItem.type = ItemType::Weapon;
        lockedItem.isLocked = true;
        CHECK(SalvageSystem::CanSalvage(lockedItem) == false);
    }

    SUBCASE("CalculateYield mapping") {
        ItemComponent item;
        item.rarity = Rarity::Magic;
        item.type = ItemType::Weapon;
        
        // Add a strength affix T4
        Affix aff;
        aff.type = AffixType::Strength;
        aff.tier = 4;
        item.affixes.push_back(aff);
        
        auto yield = SalvageSystem::CalculateYield(item);
        REQUIRE(yield.size() == 1);
        CHECK(yield[0].materialId == 4000 + static_cast<uint32_t>(AffixType::Strength));
        // T4 yield is rand(1, 4)
        CHECK(yield[0].count >= 1);
        CHECK(yield[0].count <= 4);
    }

    SUBCASE("Execute salvage") {
        auto itemEnt = registry.create();
        auto& item = registry.emplace<ItemComponent>(itemEnt);
        item.name = "Test Sword";
        item.rarity = Rarity::Magic;
        item.type = ItemType::Weapon;
        
        Affix aff;
        aff.type = AffixType::Dexterity;
        aff.tier = 5;
        item.affixes.push_back(aff);

        SalvageSystem::Execute(registry, itemEnt, player);

        // Item should be destroyed
        CHECK(registry.valid(itemEnt) == false);

        // Player should have some dexterity shards
        auto& bank = registry.get<MaterialBankComponent>(player);
        uint32_t dexShardId = 4000 + static_cast<uint32_t>(AffixType::Dexterity);
        CHECK(bank.GetCount(dexShardId) > 0);
    }

    SUBCASE("Inventory cleanup") {
        auto playerEnt = registry.create();
        registry.emplace<MaterialBankComponent>(playerEnt);
        auto& inv = registry.emplace<InventoryComponent>(playerEnt);
        
        auto itemEnt = registry.create();
        auto& item = registry.emplace<ItemComponent>(itemEnt);
        item.name = "Test";
        item.type = ItemType::Weapon;
        item.rarity = Rarity::Magic;
        
        inv.items[0] = itemEnt;

        SalvageSystem::Execute(registry, itemEnt, playerEnt);

        CHECK(registry.valid(itemEnt) == false);
        CHECK((inv.items[0] == entt::null));
    }

    SUBCASE("Legendary Essence") {
        ItemComponent item;
        item.rarity = Rarity::Legendary;
        item.type = ItemType::Armor;
        
        Affix aff;
        aff.type = static_cast<AffixType>(1001); // Some legendary affix
        aff.tier = 4;
        item.affixes.push_back(aff);
        
        auto yield = SalvageSystem::CalculateYield(item);
        REQUIRE(yield.size() == 1);
        CHECK(yield[0].materialId == 4999);
        CHECK(yield[0].count >= 1);
    }
}

}
