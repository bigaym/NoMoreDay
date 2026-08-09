#pragma once

#include "game/foundation/components/EquipmentComponent.hpp"

namespace NoMoreDay {

struct EquipSlotValidationResult {
  bool canEquip = false;
  EquipmentSlot resolvedSlot = EquipmentSlot::None;
};

class ItemEquipValidationService {
public:
  static EquipSlotValidationResult ValidateAndResolveSlot(
      const EquipmentComponent &equipment, const ItemComponent &item,
      EquipmentSlot targetSlot, bool hasTitanGrip);
};

} // namespace NoMoreDay
