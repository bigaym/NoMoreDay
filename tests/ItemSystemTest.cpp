#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "components/ItemComponent.hpp"
#include <nlohmann/json.hpp>

using namespace NoMoreDay;

TEST_CASE("ItemComponent Serialization") {
    ItemComponent item;
    item.id = 100;
    item.name = "Test Sword";
    item.type = ItemType::Weapon;
    item.slot = EquipmentSlot::MainHand;
    item.rarity = Rarity::Rare;
    item.value = 500.0f;
    item.attack = 10.0f;
    item.isTwoHanded = true;

    nlohmann::json j = item;
    ItemComponent deserialized = j.get<ItemComponent>();

    CHECK(deserialized.id == 100);
    CHECK(deserialized.name == "Test Sword");
    CHECK(deserialized.type == ItemType::Weapon);
    CHECK(deserialized.slot == EquipmentSlot::MainHand);
    CHECK(deserialized.rarity == Rarity::Rare);
    CHECK(deserialized.value == 500.0f);
    CHECK(deserialized.attack == 10.0f);
    CHECK(deserialized.isTwoHanded == true);
}

TEST_CASE("EquipmentSlot Enum") {
    CHECK(static_cast<int>(EquipmentSlot::None) == 0);
    CHECK(static_cast<int>(EquipmentSlot::Ring2) == 11);
    CHECK(static_cast<int>(EquipmentSlot::Count) == 12);
}
