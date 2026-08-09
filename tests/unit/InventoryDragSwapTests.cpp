#include "TestCommon.hpp"
#include "game/systems/item/InventorySystem.hpp"
#include "game/foundation/components/InventoryComponent.hpp"
#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"

namespace NoMoreDay {

TEST_CASE("[Unit] InventoryDragSwap - full backpack can replace equipped item using dragged source slot") {
    TestSetupScope scope;
    entt::registry registry;
    auto player = registry.create();
    auto& inv = registry.emplace<InventoryComponent>(player);
    auto& equip = registry.emplace<EquipmentComponent>(player);
    registry.emplace<Position>(player, 0.0f, 0.0f);

    // 1. Fill backpack (capacity 40)
    for (int i = 0; i < 40; ++i) {
        auto item = registry.create();
        auto& ic = registry.emplace<ItemComponent>(item);
        ic.id = 1;
        ic.name = "Test Item";
        ic.type = ItemType::Material;
        inv.items[i] = item;
    }

    // 2. Equip an item
    auto equippedItem = registry.create();
    auto& equipComp = registry.emplace<ItemComponent>(equippedItem);
    equipComp.id = 2;
    equipComp.name = "Equipped";
    equipComp.type = ItemType::Weapon;
    equipComp.slot = EquipmentSlot::MainHand;
    equip.set(EquipmentSlot::MainHand, equippedItem);

    // 3. Select one item from backpack to be the "replacement"
    entt::entity newItem = inv.items[5];
    auto& newItemComp = registry.get<ItemComponent>(newItem);
    newItemComp.type = ItemType::Weapon;
    newItemComp.slot = EquipmentSlot::MainHand;

    // Expected: True (it should swap)
    // We use the new specialized API to avoid the "full backpack" check issue
    bool result = InventorySystem::swapInventoryItemIntoEquipment(registry, player, 5, EquipmentSlot::MainHand);

    CHECK(result == true);
    CHECK(equip.get(EquipmentSlot::MainHand) == newItem);
    CHECK(inv.items[5] == equippedItem); 
}

TEST_CASE("[Unit] InventoryDragSwap - equipment drag to inventory uses hovered slot") {
    TestSetupScope scope;
    entt::registry registry;
    auto player = registry.create();
    auto& inv = registry.emplace<InventoryComponent>(player);
    auto& equip = registry.emplace<EquipmentComponent>(player);
    registry.emplace<Position>(player, 0.0f, 0.0f);

    // 1. Equip an item
    auto equippedItem = registry.create();
    auto& eqComp = registry.emplace<ItemComponent>(equippedItem);
    eqComp.id = 3;
    eqComp.name = "Equipped";
    eqComp.type = ItemType::Weapon;
    equip.set(EquipmentSlot::MainHand, equippedItem);

    // 2. Fill slot 0
    auto dummy = registry.create();
    inv.items[0] = dummy;

    // 3. Try to move to slot 7 specifically
    bool result = InventorySystem::moveEquippedItemToInventorySlot(registry, player, EquipmentSlot::MainHand, 7);
    
    CHECK(result == true);
    CHECK(!registry.valid(equip.get(EquipmentSlot::MainHand)));
    CHECK(inv.items[7] == equippedItem);
}

TEST_CASE("[Unit] InventoryDragSwap - bag slot replacement does not fail when source bag is the only reclaimable capacity") {
    TestSetupScope scope;
    entt::registry registry;
    auto player = registry.create();
    auto& inv = registry.emplace<InventoryComponent>(player);
    registry.emplace<Position>(player, 0.0f, 0.0f);

    // 1. Start with BASE_CAPACITY (40)
    // 2. Add a bag that adds 10 slots
    auto bag1 = registry.create();
    auto& bag1Comp = registry.emplace<ItemComponent>(bag1);
    bag1Comp.id = 4;
    bag1Comp.name = "Small Bag";
    bag1Comp.type = ItemType::Bag;
    bag1Comp.bagCapacity = 10;
    InventorySystem::equipBag(registry, player, bag1, 0);
    
    CHECK(inv.capacity == 50);

    // 3. Fill backpack to 49/50 (1 slot left)
    for (int i = 0; i < 49; ++i) {
        if (!registry.valid(inv.items[i])) {
            auto item = registry.create();
            auto& ic = registry.emplace<ItemComponent>(item);
            ic.id = 5;
            ic.name = "Item";
            ic.type = ItemType::Material;
            inv.items[i] = item;
        }
    }

    // 4. Prepare bag2 in the only empty slot (index 49)
    auto bag2 = registry.create();
    auto& bag2Comp = registry.emplace<ItemComponent>(bag2);
    bag2Comp.id = 6;
    bag2Comp.name = "Other Bag";
    bag2Comp.type = ItemType::Bag;
    bag2Comp.bagCapacity = 10;
    inv.items[49] = bag2;

    // 5. Swap bag1 (in slot 0) with bag2 (in inventory index 49)
    bool result = InventorySystem::moveBagItemToInventorySlot(registry, player, 0, 49); 

    CHECK(result == true);
    CHECK(inv.bag_slots[0] == bag2);
    CHECK(inv.items[49] == bag1);
    CHECK(inv.capacity == 50);
}

TEST_CASE("[Unit] InventoryDragSwap - failed two-handed swap rolls back inventory and equipment state") {
    TestSetupScope scope;
    entt::registry registry;
    auto player = registry.create();
    auto& inv = registry.emplace<InventoryComponent>(player);
    auto& equip = registry.emplace<EquipmentComponent>(player);
    registry.emplace<Position>(player, 0.0f, 0.0f);

    for (int i = 0; i < 40; ++i) {
        auto item = registry.create();
        auto& ic = registry.emplace<ItemComponent>(item);
        ic.id = 100 + i;
        ic.name = "Bag Fill";
        ic.type = ItemType::Material;
        inv.items[i] = item;
    }

    auto equippedMainHand = registry.create();
    auto& mainHandComp = registry.emplace<ItemComponent>(equippedMainHand);
    mainHandComp.id = 201;
    mainHandComp.name = "Main Hand";
    mainHandComp.type = ItemType::Weapon;
    mainHandComp.slot = EquipmentSlot::MainHand;
    equip.set(EquipmentSlot::MainHand, equippedMainHand);

    auto equippedOffHand = registry.create();
    auto& offHandComp = registry.emplace<ItemComponent>(equippedOffHand);
    offHandComp.id = 202;
    offHandComp.name = "Off Hand";
    offHandComp.type = ItemType::Shield;
    offHandComp.slot = EquipmentSlot::OffHand;
    equip.set(EquipmentSlot::OffHand, equippedOffHand);

    entt::entity twoHandedWeapon = inv.items[5];
    auto& twoHandedComp = registry.get<ItemComponent>(twoHandedWeapon);
    twoHandedComp.id = 203;
    twoHandedComp.name = "Greatsword";
    twoHandedComp.type = ItemType::Weapon;
    twoHandedComp.slot = EquipmentSlot::MainHand;
    twoHandedComp.isTwoHanded = true;

    bool result = InventorySystem::swapInventoryItemIntoEquipment(
        registry, player, 5, EquipmentSlot::MainHand);

    CHECK(result == false);
    CHECK(equip.get(EquipmentSlot::MainHand) == equippedMainHand);
    CHECK(equip.get(EquipmentSlot::OffHand) == equippedOffHand);
    CHECK(inv.items[5] == twoHandedWeapon);
    CHECK(std::find(inv.items.begin(), inv.items.end(), equippedOffHand) == inv.items.end());
}

} // namespace NoMoreDay
