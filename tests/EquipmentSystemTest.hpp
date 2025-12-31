#pragma once

#include "../src/components/EquipmentComponent.hpp"
#include "../src/components/ItemComponent.hpp"
#include "../src/components/InventoryComponent.hpp"
#include "../src/components/Stats.hpp"
#include "../src/systems/InventorySystem.hpp"
#include <entt/entt.hpp>
#include "TestCommon.hpp"

TEST_CASE("EquipmentComponent Initialization") {

    EquipmentComponent eq;
    for (int i = 1; i < static_cast<int>(EquipmentSlot::Count); ++i) {
        CHECK(eq.Get(static_cast<EquipmentSlot>(i)) == entt::entity{entt::null});
    }
}

TEST_CASE("EquipmentComponent Direct Set/Get") {

    entt::registry registry;
    EquipmentComponent eq;
    
    entt::entity itemEntity = registry.create();
    ItemComponent& item = registry.emplace<ItemComponent>(itemEntity);
    item.name = "Sword";
    item.slot = EquipmentSlot::MainHand;

    // Equip
    eq.Set(EquipmentSlot::MainHand, itemEntity);
    CHECK(eq.Get(EquipmentSlot::MainHand) == itemEntity);

    // Unequip
    entt::entity removed = eq.Unequip(EquipmentSlot::MainHand);
    CHECK(removed == itemEntity);
    CHECK(eq.Get(EquipmentSlot::MainHand) == entt::entity{entt::null});
}

TEST_CASE("InventorySystem Equip Logic") {

    entt::registry registry;
    entt::entity player = registry.create();
    
    registry.emplace<InventoryComponent>(player);
    registry.emplace<EquipmentComponent>(player);
    registry.emplace<StatsDirty>(player);
    registry.emplace<CombatStats>(player); // Needed for equip logic (StatsDirty handling)

    // Create Item in Inventory
    entt::entity sword = registry.create();
    ItemComponent& swordComp = registry.emplace<ItemComponent>(sword);
    swordComp.name = "Test Sword";
    swordComp.slot = EquipmentSlot::MainHand;
    swordComp.type = ItemType::Weapon;
    
    InventorySystem::pickUpItem(registry, player, sword);
    CHECK(InventorySystem::hasItem(registry, player, swordComp.id));

    // Equip via System
    bool success = InventorySystem::equipItem(registry, player, sword);
    CHECK(success);
    
    // Verify it's in slot
    const auto& equip = registry.get<EquipmentComponent>(player);
    CHECK(equip.Get(EquipmentSlot::MainHand) == sword);
    
    // Verify it's NOT in inventory (InventorySystem::equipItem removes from inv)
    // Actually InventorySystem::equipItem sets slot to null in items vector, doesn't remove from vector?
    // Let's check implementation: "*it = entt::null;"
    auto& inv = registry.get<InventoryComponent>(player);
    bool foundInInv = false;
    for(auto item : inv.items) {
        if (item == sword) foundInInv = true;
    }
    CHECK_FALSE(foundInInv);

    // Unequip via System
    success = InventorySystem::unequipItem(registry, player, EquipmentSlot::MainHand);
    CHECK(success);
    
    // Verify back in inventory
    foundInInv = false;
    for(auto item : inv.items) {
        if (item == sword) foundInInv = true;
    }
    CHECK(foundInInv);
    CHECK(equip.Get(EquipmentSlot::MainHand) == entt::entity{entt::null});
}
