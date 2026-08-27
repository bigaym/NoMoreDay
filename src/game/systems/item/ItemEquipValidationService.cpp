#include "game/systems/item/ItemEquipValidationService.hpp"
#include "game/systems/item/storage/ItemTemplateRegistry.hpp"

namespace NoMoreDay {

EquipSlotValidationResult ItemEquipValidationService::ValidateAndResolveSlot(
    const EquipmentComponent &equipment, const ItemComponent &item,
    EquipmentSlot targetSlot, bool hasTitanGrip) {
  EquipmentSlot itemSlot = item.slot;
  ItemType itemType = item.type;
  if (itemSlot == EquipmentSlot::None && item.baseId > 0) {
    if (const auto *tmpl = ItemTemplateRegistry::Instance().find(item.baseId)) {
      itemSlot = tmpl->slot;
      itemType = tmpl->type;
    }
  }

  EquipmentSlot resolvedSlot =
      (targetSlot != EquipmentSlot::None) ? targetSlot : itemSlot;

  bool canEquip = (resolvedSlot == itemSlot);
  if (!canEquip) {
    const bool isItemRing =
        (itemSlot == EquipmentSlot::Ring || itemSlot == EquipmentSlot::Ring1 ||
         itemSlot == EquipmentSlot::Ring2);
    const bool isSlotRing =
        (resolvedSlot == EquipmentSlot::Ring1 || resolvedSlot == EquipmentSlot::Ring2);
    if (isItemRing && isSlotRing) {
      canEquip = true;
    }

    if (hasTitanGrip && itemType == ItemType::Weapon) {
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
