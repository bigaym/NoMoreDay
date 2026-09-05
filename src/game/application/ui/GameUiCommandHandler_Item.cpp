#include "game/application/ui/GameUiCommandHandler.hpp"
#include "game/application/ui/GameUiCommandHandlerInternal.hpp"

#include "game/systems/item/InventorySystem.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "game/systems/item/CraftingSystem.hpp"
#include "game/contracts/impl/StatsSystem.hpp"

using namespace NoMoreDay::ui::detail;

namespace NoMoreDay::ui {

// --- Ground pickup ---------------------------------------------------------

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecutePickup(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  const entt::entity item = ToEntity(payload.sourceDomainId);
  if (!IsValidItem(registry, item)) {
    return {false, GameUiResultCode::InvalidTarget, "Invalid pickup target",
            {}};
  }
  if (registry.template try_get<NoMoreDay::InventoryComponent>(player) ==
      nullptr) {
    return {false, GameUiResultCode::MissingComponent, "Player has no inventory",
            {}};
  }
  if (!PickupWithinRange(registry, player, item)) {
    return {false, GameUiResultCode::TooFarAway, "Too far away", {}};
  }
  // The inventory system is the single owner of item mutations; it re-checks
  // entity validity and capacity before stacking/placing the item.
  if (InventorySystem::pickUpItem(registry, player, item)) {
    return {true, GameUiResultCode::Success, "", {}};
  }
  return {false, GameUiResultCode::CapacityFull, "Inventory is full", {}};
}

// --- Inventory / equipment / bag ------------------------------------------

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteEquip(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  const entt::entity item = ToEntity(payload.sourceDomainId);
  if (!IsValidItem(registry, item)) {
    return {false, GameUiResultCode::InvalidTarget, "Invalid equip target",
            {}};
  }
  if (!IsItemOwnedByPlayer(registry, player, item)) {
    return {false, GameUiResultCode::NotInInventory,
            "Item is not in the player's inventory", {}};
  }

  const auto& itemComp = registry.get<NoMoreDay::ItemComponent>(item);
  if (itemComp.type == NoMoreDay::ItemType::Bag) {
    // Bags go into the bag slots (mirror the legacy context-menu path).
    const auto* inventory =
        registry.template try_get<NoMoreDay::InventoryComponent>(player);
    if (inventory == nullptr) {
      return {false, GameUiResultCode::MissingComponent,
              "Player has no inventory", {}};
    }
    int emptySlot = -1;
    for (int i = 0; i < NoMoreDay::InventoryComponent::MAX_BAG_SLOTS; ++i) {
      if (!registry.valid(inventory->bag_slots[i])) {
        emptySlot = i;
        break;
      }
    }
    if (emptySlot == -1) {
      emptySlot = 0; // Mirrors the legacy fallback slot.
    }
    if (InventorySystem::equipBag(registry, player, item, emptySlot)) {
      return {true, GameUiResultCode::Success, "", {}};
    }
    return {false, GameUiResultCode::CapacityFull, "Cannot equip bag", {}};
  }

  if (registry.template try_get<NoMoreDay::EquipmentComponent>(player) ==
      nullptr) {
    return {false, GameUiResultCode::MissingComponent,
            "Player has no equipment", {}};
  }
  // The equip validation service resolves the slot from the item; a payload
  // slot override is honored when it names a real slot.
  NoMoreDay::EquipmentSlot targetSlot = NoMoreDay::EquipmentSlot::None;
  if (payload.equipmentSlot <
      static_cast<std::uint8_t>(NoMoreDay::EquipmentSlot::Count)) {
    targetSlot = static_cast<NoMoreDay::EquipmentSlot>(payload.equipmentSlot);
  }

  // R6: equipment-zone drag and drop. The UI session only carries the item
  // domain id plus the source location metadata; the handler re-validates the
  // claim and routes through the transactional system calls.
  if (static_cast<GameUiItemSource>(payload.itemSource) ==
          GameUiItemSource::Inventory &&
      payload.sourceSlot >= 0) {
    // Inventory -> equipment: transactional swap (replaces the worn item and
    // parks the displaced piece in the vacated inventory slot).
    const auto* inv = registry.template try_get<NoMoreDay::InventoryComponent>(
        player);
    if (inv == nullptr) {
      return {false, GameUiResultCode::MissingComponent,
              "Player has no inventory", {}};
    }
    if (payload.sourceSlot >= static_cast<int>(inv->items.size()) ||
        !registry.valid(inv->items[payload.sourceSlot]) ||
        inv->items[payload.sourceSlot] != item) {
      return {false, GameUiResultCode::InvalidSlot, "Invalid source slot", {}};
    }
    if (InventorySystem::swapInventoryItemIntoEquipment(
            registry, player, payload.sourceSlot, targetSlot)) {
      std::vector<std::uint64_t> cleared;
      cleared.push_back(entt::to_integral(item));
      return {true, GameUiResultCode::Success, "", std::move(cleared)};
    }
    return {false, GameUiResultCode::DomainPrecondition,
            "Cannot equip item into that slot", {}};
  }
  if (static_cast<GameUiItemSource>(payload.itemSource) ==
          GameUiItemSource::Equipment &&
      payload.sourceSlot >= 0) {
    // Equipment -> equipment: equip first, then vacate the source slot.
    const auto* equip = registry.template try_get<NoMoreDay::EquipmentComponent>(
        player);
    if (equip == nullptr) {
      return {false, GameUiResultCode::MissingComponent,
              "Player has no equipment", {}};
    }
    if (payload.sourceSlot >= static_cast<int>(equip->slots.size()) ||
        !registry.valid(equip->slots[payload.sourceSlot]) ||
        equip->slots[payload.sourceSlot] != item) {
      return {false, GameUiResultCode::InvalidSlot, "Invalid source slot", {}};
    }
    if (!InventorySystem::equipItem(registry, player, item, targetSlot)) {
      return {false, GameUiResultCode::DomainPrecondition,
              "Cannot equip item into that slot", {}};
    }
    auto* equipPtr = registry.template try_get<NoMoreDay::EquipmentComponent>(
        player);
    if (equipPtr != nullptr &&
        payload.sourceSlot < static_cast<int>(equipPtr->slots.size())) {
      equipPtr->slots[payload.sourceSlot] = entt::null;
    }
    return {true, GameUiResultCode::Success, "", {}};
  }

  if (InventorySystem::equipItem(registry, player, item, targetSlot)) {
    return {true, GameUiResultCode::Success, "", {}};
  }
  return {false, GameUiResultCode::DomainPrecondition, "Cannot equip item",
          {}};
}

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteUnequip(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  const auto* equipment =
      registry.template try_get<NoMoreDay::EquipmentComponent>(player);
  if (equipment == nullptr) {
    return {false, GameUiResultCode::MissingComponent, "Player has no equipment",
            {}};
  }

  NoMoreDay::EquipmentSlot slot = NoMoreDay::EquipmentSlot::None;
  if (payload.sourceDomainId != 0) {
    // Resolve the slot that holds the given item.
    const entt::entity item = ToEntity(payload.sourceDomainId);
    for (std::size_t i = 0; i < equipment->slots.size(); ++i) {
      if (equipment->slots[i] == item) {
        slot = static_cast<NoMoreDay::EquipmentSlot>(i);
        break;
      }
    }
    if (slot == NoMoreDay::EquipmentSlot::None) {
      return {false, GameUiResultCode::NotEquipped, "Item is not equipped",
              {}};
    }
  } else {
    if (payload.equipmentSlot == 0 ||
        payload.equipmentSlot >=
            static_cast<std::uint8_t>(NoMoreDay::EquipmentSlot::Count)) {
      return {false, GameUiResultCode::InvalidSlot, "Invalid equipment slot",
              {}};
    }
    slot = static_cast<NoMoreDay::EquipmentSlot>(payload.equipmentSlot);
  }

  // R6: equipment-zone drag and drop onto a specific inventory slot.
  if (payload.targetSlot >= 0) {
    if (InventorySystem::moveEquippedItemToInventorySlot(
            registry, player, slot, payload.targetSlot)) {
      std::vector<std::uint64_t> cleared;
      cleared.push_back(entt::to_integral(equipment->slots[static_cast<std::size_t>(slot)]));
      return {true, GameUiResultCode::Success, "", std::move(cleared)};
    }
    return {false, GameUiResultCode::CapacityFull, "Inventory is full", {}};
  }

  if (InventorySystem::unequipItem(registry, player, slot)) {
    return {true, GameUiResultCode::Success, "", {}};
  }
  return {false, GameUiResultCode::CapacityFull, "Inventory is full", {}};
}

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteUse(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  const entt::entity item = ToEntity(payload.sourceDomainId);
  if (!IsValidItem(registry, item)) {
    return {false, GameUiResultCode::InvalidTarget, "Invalid use target", {}};
  }
  if (!IsItemOwnedByPlayer(registry, player, item)) {
    return {false, GameUiResultCode::NotInInventory,
            "Item is not in the player's inventory", {}};
  }
  if (InventorySystem::useItem(registry, player, item)) {
    // Consumption semantics are owned by the inventory system; the entity may
    // be destroyed for single-charge consumables, so the UI session drops it.
    const auto* itemComp = registry.template try_get<NoMoreDay::ItemComponent>(item);
    const bool consumed = (itemComp == nullptr || itemComp->quantity <= 0);
    std::vector<std::uint64_t> cleared;
    if (consumed) {
      cleared.push_back(entt::to_integral(item));
    }
    return {true, GameUiResultCode::Success, "", std::move(cleared)};
  }
  return {false, GameUiResultCode::DomainPrecondition, "Cannot use item", {}};
}

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteDrop(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  const entt::entity item = ToEntity(payload.sourceDomainId);
  if (!IsValidItem(registry, item)) {
    return {false, GameUiResultCode::InvalidTarget, "Invalid drop target", {}};
  }
  if (!IsItemOwnedByPlayer(registry, player, item)) {
    return {false, GameUiResultCode::NotInInventory,
            "Item is not in the player's inventory", {}};
  }
  if (payload.quantity <= 0) {
    return {false, GameUiResultCode::InvalidIndex, "Invalid drop quantity", {}};
  }

  // Copy the stack size before the mutator; dropItem may split or move the
  // whole entity.
  const int currentQuantity =
      registry.get<NoMoreDay::ItemComponent>(item).quantity;
  const bool dropWholeStack = payload.quantity >= currentQuantity;
  if (InventorySystem::dropItem(registry, player, item, payload.quantity)) {
    std::vector<std::uint64_t> cleared;
    if (dropWholeStack) {
      cleared.push_back(entt::to_integral(item));
    }
    return {true, GameUiResultCode::Success, "", std::move(cleared)};
  }
  return {false, GameUiResultCode::DomainPrecondition, "Cannot drop item", {}};
}

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteDestroy(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  const entt::entity item = ToEntity(payload.sourceDomainId);
  if (!IsValidItem(registry, item)) {
    return {false, GameUiResultCode::InvalidTarget, "Invalid destroy target",
            {}};
  }
  if (!IsItemOwnedByPlayer(registry, player, item)) {
    return {false, GameUiResultCode::NotInInventory,
            "Item is not in the player's inventory", {}};
  }
  // Locked items cannot be destroyed (lock is the anti-mistake contract).
  if (registry.get<NoMoreDay::ItemComponent>(item).isLocked) {
    return {false, GameUiResultCode::Locked, "Item is locked", {}};
  }
  if (payload.quantity <= 0) {
    return {false, GameUiResultCode::InvalidIndex, "Invalid destroy quantity",
            {}};
  }

  const int currentQuantity =
      registry.get<NoMoreDay::ItemComponent>(item).quantity;
  const bool destroyWholeStack = payload.quantity >= currentQuantity;
  if (InventorySystem::destroyItem(registry, player, item, payload.quantity)) {
    std::vector<std::uint64_t> cleared;
    if (destroyWholeStack) {
      cleared.push_back(entt::to_integral(item));
    }
    return {true, GameUiResultCode::Success, "", std::move(cleared)};
  }
  return {false, GameUiResultCode::DomainPrecondition, "Cannot destroy item",
          {}};
}

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteLock(
    Registry& registry, const GameUiIntentPayload& payload,
    bool locked) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  const entt::entity item = ToEntity(payload.sourceDomainId);
  if (!IsValidItem(registry, item)) {
    return {false, GameUiResultCode::InvalidTarget, "Invalid lock target", {}};
  }
  if (!IsItemOwnedByPlayer(registry, player, item)) {
    return {false, GameUiResultCode::NotOwned,
            "Item is not owned by the player", {}};
  }
  if (InventorySystem::setItemLocked(registry, player, item, locked)) {
    return {true, GameUiResultCode::Success, "", {}};
  }
  return {false, GameUiResultCode::DomainPrecondition, "Cannot change lock",
          {}};
}

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteOrganize(
    Registry& registry, const GameUiIntentPayload& payload) const {
  (void)payload;
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  if (registry.template try_get<NoMoreDay::InventoryComponent>(player) ==
      nullptr) {
    return {false, GameUiResultCode::MissingComponent, "Player has no inventory",
            {}};
  }
  InventorySystem::organize(registry, player);
  return {true, GameUiResultCode::Success, "", {}};
}

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteMove(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  if (payload.sourceSlot < 0 || payload.targetSlot < 0) {
    return {false, GameUiResultCode::InvalidSlot, "Invalid move slot", {}};
  }
  if (InventorySystem::moveItem(registry, player, payload.sourceSlot,
                                payload.targetSlot)) {
    return {true, GameUiResultCode::Success, "", {}};
  }
  return {false, GameUiResultCode::InvalidSlot,
          "Cannot move item to that slot", {}};
}

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteSwap(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  if (payload.sourceSlot < 0 || payload.targetSlot < 0) {
    return {false, GameUiResultCode::InvalidSlot, "Invalid swap slot", {}};
  }
  if (InventorySystem::swapItems(registry, player, payload.sourceSlot,
                                 payload.targetSlot)) {
    return {true, GameUiResultCode::Success, "", {}};
  }
  return {false, GameUiResultCode::InvalidSlot, "Cannot swap those slots", {}};
}

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteBag(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }

  const auto action = static_cast<GameUiBagAction>(payload.bagAction);
  if (action == GameUiBagAction::Unequip) {
    if (payload.sourceSlot < 0 ||
        payload.sourceSlot >= NoMoreDay::InventoryComponent::MAX_BAG_SLOTS) {
      return {false, GameUiResultCode::InvalidSlot, "Invalid bag slot", {}};
    }
    // R6: bag -> inventory drag and drop onto a specific slot.
    if (payload.targetSlot >= 0) {
      if (InventorySystem::moveBagItemToInventorySlot(
              registry, player, payload.sourceSlot, payload.targetSlot)) {
        std::vector<std::uint64_t> cleared;
        cleared.push_back(entt::to_integral(
            registry.get<NoMoreDay::InventoryComponent>(player)
                .bag_slots[payload.sourceSlot]));
        return {true, GameUiResultCode::Success, "", std::move(cleared)};
      }
      return {false, GameUiResultCode::CapacityFull, "Inventory is full", {}};
    }
    if (InventorySystem::unequipBag(registry, player, payload.sourceSlot,
                                    /*putBackInInventory=*/true)) {
      return {true, GameUiResultCode::Success, "", {}};
    }
    return {false, GameUiResultCode::CapacityFull, "Inventory is full", {}};
  }

  // Equip a bag from the inventory.
  const entt::entity bagItem = ToEntity(payload.sourceDomainId);
  if (!IsValidItem(registry, bagItem)) {
    return {false, GameUiResultCode::InvalidTarget, "Invalid bag item", {}};
  }
  if (!IsItemOwnedByPlayer(registry, player, bagItem)) {
    return {false, GameUiResultCode::NotInInventory,
            "Bag is not in the player's inventory", {}};
  }
  const auto* inventory =
      registry.template try_get<NoMoreDay::InventoryComponent>(player);
  if (inventory == nullptr) {
    return {false, GameUiResultCode::MissingComponent, "Player has no inventory",
            {}};
  }
  int slotIndex = payload.targetSlot >= 0 ? payload.targetSlot : -1;
  if (slotIndex < 0) {
    for (int i = 0; i < NoMoreDay::InventoryComponent::MAX_BAG_SLOTS; ++i) {
      if (!registry.valid(inventory->bag_slots[i])) {
        slotIndex = i;
        break;
      }
    }
  }
  if (slotIndex < 0 ||
      slotIndex >= NoMoreDay::InventoryComponent::MAX_BAG_SLOTS) {
    return {false, GameUiResultCode::CapacityFull, "No free bag slot", {}};
  }
  // R6: bag -> bag drag and drop swaps the source bag out of its slot first
  // (mirror of the legacy UI inventory path; the source slot is vacated
  // without putting the bag back into the inventory grid).
  if (static_cast<GameUiItemSource>(payload.itemSource) ==
          GameUiItemSource::Bag &&
      payload.sourceSlot >= 0 && payload.sourceSlot != slotIndex) {
    if (!InventorySystem::unequipBag(registry, player, payload.sourceSlot,
                                     /*putBackInInventory=*/false)) {
      return {false, GameUiResultCode::CapacityFull, "Cannot swap bags", {}};
    }
  }
  if (InventorySystem::equipBag(registry, player, bagItem, slotIndex)) {
    return {true, GameUiResultCode::Success, "", {}};
  }
  return {false, GameUiResultCode::DomainPrecondition, "Cannot equip bag", {}};
}

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteSocket(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  const entt::entity item = ToEntity(payload.targetDomainId);
  const entt::entity rune = ToEntity(payload.sourceDomainId);
  if (!IsValidItem(registry, item) || !IsValidItem(registry, rune)) {
    return {false, GameUiResultCode::InvalidTarget, "Invalid socket target",
            {}};
  }
  if (!IsItemOwnedByPlayer(registry, player, item) ||
      !IsItemOwnedByPlayer(registry, player, rune)) {
    return {false, GameUiResultCode::NotOwned,
            "Item and rune must belong to the player", {}};
  }

  // R6: a stacked rune is split into a single-rune entity before socketing
  // (legacy UI inventory semantics migrated into the handler). The split copy
  // is destroyed when the socket attempt fails.
  entt::entity runeToSocket = rune;
  bool wasSplit = false;
  auto& runeComp = registry.get<NoMoreDay::ItemComponent>(rune);
  if (runeComp.quantity > 1) {
    runeComp.quantity -= 1;
    runeToSocket = NoMoreDay::ItemFactory::createMaterial(registry, runeComp.id,
                                                          1);
    wasSplit = true;
  }

  const NoMoreDay::CraftingResult result =
      NoMoreDay::CraftingSystem::socketRune(registry, item, runeToSocket,
                                            payload.socketIndex);
  GameUiResult uiResult = ToCraftingResult(result);
  if (uiResult.success) {
    // The rune now lives inside the item's sockets; drop it from the UI
    // drag/session sources.
    uiResult.clearedDomainIds.push_back(entt::to_integral(runeToSocket));
    if (!wasSplit) {
      // Vacate the source slot the rune was dragged from (R6: the UI only
      // carries the source metadata; the handler clears the container).
      const auto* inv = registry.template try_get<NoMoreDay::InventoryComponent>(
          player);
  if (static_cast<GameUiItemSource>(payload.itemSource) ==
          GameUiItemSource::Inventory &&
      inv != nullptr && payload.sourceSlot >= 0 &&
      payload.sourceSlot < static_cast<int>(inv->items.size()) &&
      inv->items[payload.sourceSlot] == rune) {
    registry.template get<NoMoreDay::InventoryComponent>(player)
        .items[payload.sourceSlot] = entt::null;
  } else if (static_cast<GameUiItemSource>(payload.itemSource) ==
                 GameUiItemSource::Equipment &&
             payload.sourceSlot >= 0) {
    auto* equip = registry.template try_get<NoMoreDay::EquipmentComponent>(
        player);
    if (equip != nullptr &&
        payload.sourceSlot < static_cast<int>(equip->slots.size()) &&
        equip->slots[payload.sourceSlot] == rune) {
      equip->slots[payload.sourceSlot] = entt::null;
    }
  } else if (static_cast<GameUiItemSource>(payload.itemSource) ==
                 GameUiItemSource::Bag &&
             payload.sourceSlot >= 0 &&
             payload.sourceSlot <
                 NoMoreDay::InventoryComponent::MAX_BAG_SLOTS) {
        InventorySystem::unequipBag(registry, player, payload.sourceSlot,
                                    /*putBackInInventory=*/true);
      }
    }
  } else if (wasSplit && registry.valid(runeToSocket)) {
    registry.destroy(runeToSocket); // Roll back the split copy.
  }
  return uiResult;
}

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteUnsocket(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  const entt::entity item = ToEntity(payload.targetDomainId);
  if (!IsValidItem(registry, item)) {
    return {false, GameUiResultCode::InvalidTarget, "Invalid unsocket target",
            {}};
  }
  if (!IsItemOwnedByPlayer(registry, player, item)) {
    return {false, GameUiResultCode::NotOwned,
            "Item is not owned by the player", {}};
  }
  const NoMoreDay::CraftingResult result =
      NoMoreDay::CraftingSystem::unsocketRune(registry, item,
                                              payload.socketIndex);
  return ToCraftingResult(result);
}


// Explicit instantiations for the concrete registry type used in production.
template GameUiResult GameUiCommandHandler::ExecutePickup<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteEquip<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteUnequip<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteUse<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteDrop<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteDestroy<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteLock<entt::registry>(
    entt::registry&, const GameUiIntentPayload&, bool) const;
template GameUiResult GameUiCommandHandler::ExecuteOrganize<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteMove<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteSwap<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteBag<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteSocket<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteUnsocket<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;

} // namespace NoMoreDay::ui
