#include "TestCommon.hpp"
#include "game/systems/item/ItemEquipValidationService.hpp"

using namespace NoMoreDay;

TEST_CASE("[Unit] ItemEquipValidationService - Slot validation and ring routing") {
  TestSetupScope scope;

  SUBCASE("Generic ring auto-assigns to free slot") {
    EquipmentComponent equipment;
    ItemComponent ring;
    ring.slot = EquipmentSlot::Ring;

    auto first = ItemEquipValidationService::ValidateAndResolveSlot(
        equipment, ring, EquipmentSlot::None, false);
    CHECK(first.canEquip == true);
    CHECK(first.resolvedSlot == EquipmentSlot::Ring1);

    equipment.set(EquipmentSlot::Ring1, entt::entity{1});
    auto second = ItemEquipValidationService::ValidateAndResolveSlot(
        equipment, ring, EquipmentSlot::None, false);
    CHECK(second.canEquip == true);
    CHECK(second.resolvedSlot == EquipmentSlot::Ring2);

    equipment.set(EquipmentSlot::Ring2, entt::entity{2});
    auto fallback = ItemEquipValidationService::ValidateAndResolveSlot(
        equipment, ring, EquipmentSlot::None, false);
    CHECK(fallback.canEquip == true);
    CHECK(fallback.resolvedSlot == EquipmentSlot::Ring1);
  }

  SUBCASE("Weapon offhand is rejected without Titan Grip") {
    EquipmentComponent equipment;
    ItemComponent weapon;
    weapon.type = ItemType::Weapon;
    weapon.slot = EquipmentSlot::MainHand;

    auto result = ItemEquipValidationService::ValidateAndResolveSlot(
        equipment, weapon, EquipmentSlot::OffHand, false);
    CHECK(result.canEquip == false);
    CHECK(result.resolvedSlot == EquipmentSlot::OffHand);
  }

  SUBCASE("Weapon offhand is allowed with Titan Grip") {
    EquipmentComponent equipment;
    ItemComponent weapon;
    weapon.type = ItemType::Weapon;
    weapon.slot = EquipmentSlot::MainHand;

    auto result = ItemEquipValidationService::ValidateAndResolveSlot(
        equipment, weapon, EquipmentSlot::OffHand, true);
    CHECK(result.canEquip == true);
    CHECK(result.resolvedSlot == EquipmentSlot::OffHand);
  }

  SUBCASE("Missing slot remains invalid") {
    EquipmentComponent equipment;
    ItemComponent item;
    item.slot = EquipmentSlot::None;

    auto result = ItemEquipValidationService::ValidateAndResolveSlot(
        equipment, item, EquipmentSlot::None, false);
    CHECK(result.canEquip == false);
    CHECK(result.resolvedSlot == EquipmentSlot::None);
  }
}
