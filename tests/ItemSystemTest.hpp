#pragma once

#include "../src/components/ItemComponent.hpp"
#include "../src/core/ItemFactory.hpp"
#include <nlohmann/json.hpp>

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
    item.textureId = 12345;

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
    CHECK(deserialized.textureId == 12345);
}

TEST_CASE("ItemFactory Texture Assignment") {
    entt::registry registry;
    // Note: RNG might produce different results, but textureId should always be non-zero
    // because we have assets for weapons and armor.
    
    // Weapon
    auto weaponEntity = NoMoreDay::ItemFactory::createWeapon(registry, 10, NoMoreDay::Rarity::Common);
    const auto& weapon = registry.get<NoMoreDay::ItemComponent>(weaponEntity);
    CHECK(weapon.textureId != 0);
    
    // Armor (Chest)
    auto armorEntity = NoMoreDay::ItemFactory::createArmor(registry, 10, NoMoreDay::Rarity::Common, NoMoreDay::EquipmentSlot::Chest);
    const auto& armor = registry.get<NoMoreDay::ItemComponent>(armorEntity);
    CHECK(armor.textureId != 0);
}

TEST_CASE("EquipmentSlot Enum") {
    CHECK(static_cast<int>(EquipmentSlot::None) == 0);
    CHECK(static_cast<int>(EquipmentSlot::Ring2) == 11);
    CHECK(static_cast<int>(EquipmentSlot::Count) == 12);
}
