#include "game/systems/item/ItemEquipValidationService.hpp"

namespace NoMoreDay {

EquipSlotValidationResult ItemEquipValidationService::ValidateAndResolveSlot(
    const EquipmentComponent &equipment, const ItemComponent &item,
    EquipmentSlot targetSlot, bool hasTitanGrip) {
  EquipmentSlot resolvedSlot =
      (targetSlot != EquipmentSlot::None) ? targetSlot : item.slot;

  bool canEquip = (resolvedSlot == item.slot);
  if (!canEquip) {
    const bool isItemRing =
        (item.slot == EquipmentSlot::Ring || item.slot == EquipmentSlot::Ring1 ||
         item.slot == EquipmentSlot::Ring2);
    const bool isSlotRing =
        (resolvedSlot == EquipmentSlot::Ring1 || resolvedSlot == EquipmentSlot::Ring2);
    if (isItemRing && isSlotRing) {
      canEquip = true;
    }

    if (hasTitanGrip && item.type == ItemType::Weapon) {
      if (resolvedSlot == EquipmentSlot::MainHand ||
          resolvedSlot == EquipmentSlot::OffHand) {
        canEquip = true;
      }
    }
  }

  if (!canEquip) {
    return {false, resolvedSlot};
  }

  if (targetSlot == EquipmentSlot::None && resolvedSlot == EquipmentSlot::Ring) {
    if (equipment.get(EquipmentSlot::Ring1) == entt::null) {
      resolvedSlot = EquipmentSlot::Ring1;
    } else if (equipment.get(EquipmentSlot::Ring2) == entt::null) {
      resolvedSlot = EquipmentSlot::Ring2;
    } else {
      resolvedSlot = EquipmentSlot::Ring1;
    }
  }

  if (resolvedSlot == EquipmentSlot::None) {
    return {false, resolvedSlot};
  }

  return {true, resolvedSlot};
}

} // namespace NoMoreDay
