#include "game/application/ui/GameUiCommandHandler.hpp"
#include "game/application/ui/GameUiCommandHandlerInternal.hpp"

#include "game/systems/item/StashSystem.hpp"

using namespace NoMoreDay::ui::detail;

namespace NoMoreDay::ui {

// --- Stash -----------------------------------------------------------------

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteStashTransfer(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  (void)player;
  if (payload.sourceTab < 0 || payload.targetTab < 0 ||
      payload.sourceSlot < 0 || payload.targetSlot < 0) {
    return {false, GameUiResultCode::InvalidIndex,
            "Invalid stash tab or slot", {}};
  }
  const NoMoreDay::StashType type =
      static_cast<NoMoreDay::StashType>(payload.stashTarget);
  if (NoMoreDay::StashSystem::transferItem(
          registry, type, payload.sourceTab, payload.sourceSlot, type,
          payload.targetTab, payload.targetSlot)) {
    return {true, GameUiResultCode::Success, "", {}};
  }
  return {false, GameUiResultCode::InvalidSlot, "Cannot transfer stash item",
          {}};
}

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteStashDeposit(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  const entt::entity item = ToEntity(payload.sourceDomainId);
  if (!IsValidItem(registry, item)) {
    return {false, GameUiResultCode::InvalidTarget, "Invalid deposit target",
            {}};
  }
  if (!IsItemOwnedByPlayer(registry, player, item)) {
    return {false, GameUiResultCode::NotInInventory,
            "Item is not in the player's inventory", {}};
  }
  if (payload.targetTab < 0 || payload.targetSlot < 0) {
    return {false, GameUiResultCode::InvalidIndex,
            "Invalid stash tab or slot", {}};
  }
  if (!NoMoreDay::StashSystem::canStoreItem(registry, item)) {
    return {false, GameUiResultCode::DomainPrecondition,
            "Item cannot be stored in the stash", {}};
  }
  const NoMoreDay::StashType type =
      static_cast<NoMoreDay::StashType>(payload.stashTarget);

  if (static_cast<GameUiItemSource>(payload.itemSource) ==
      GameUiItemSource::Equipment) {
    auto* equipment =
        registry.template try_get<NoMoreDay::EquipmentComponent>(player);
    if (equipment == nullptr) {
      return {false, GameUiResultCode::MissingComponent,
              "Player has no equipment", {}};
    }
    NoMoreDay::EquipmentSlot slot = NoMoreDay::EquipmentSlot::None;
    if (payload.sourceSlot >= 0 &&
        payload.sourceSlot <
            static_cast<int>(NoMoreDay::EquipmentSlot::Count)) {
      slot = static_cast<NoMoreDay::EquipmentSlot>(payload.sourceSlot);
    } else {
      for (std::size_t i = 0; i < equipment->slots.size(); ++i) {
        if (equipment->slots[i] == item) {
          slot = static_cast<NoMoreDay::EquipmentSlot>(i);
          break;
        }
      }
    }
    if (slot == NoMoreDay::EquipmentSlot::None ||
        equipment->slots[static_cast<std::size_t>(slot)] != item) {
      return {false, GameUiResultCode::NotEquipped,
              "Item is not equipped in the specified slot", {}};
    }

    NoMoreDay::StashTab* tab =
        NoMoreDay::StashSystem::getTab(registry, type, payload.targetTab);
    if (!tab || payload.targetSlot >= NoMoreDay::StashTab::CAPACITY) {
      return {false, GameUiResultCode::InvalidIndex,
              "Invalid stash tab or slot", {}};
    }
    if (tab->items[payload.targetSlot] != entt::null) {
      return {false, GameUiResultCode::CapacityFull,
              "Target stash slot is occupied", {}};
    }

    equipment->set(slot, entt::null);
    tab->items[payload.targetSlot] = item;
    registry.template get_or_emplace<NoMoreDay::StatsDirty>(player);

    std::vector<std::uint64_t> cleared{entt::to_integral(item)};
    return {true, GameUiResultCode::Success, "", std::move(cleared)};
  }

  const int invSlot = payload.sourceSlot >= 0 ? payload.sourceSlot : 0;
  if (NoMoreDay::StashSystem::depositFromInventory(
          registry, item, invSlot, type, payload.targetTab,
          payload.targetSlot)) {
    // The item relocated into the stash; drop it from inventory drag sources.
    std::vector<std::uint64_t> cleared{entt::to_integral(item)};
    return {true, GameUiResultCode::Success, "", std::move(cleared)};
  }
  return {false, GameUiResultCode::CapacityFull, "Cannot deposit item", {}};
}

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteStashWithdraw(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  if (payload.sourceTab < 0 || payload.sourceSlot < 0 || payload.targetSlot < 0) {
    return {false, GameUiResultCode::InvalidIndex,
            "Invalid stash tab or slot", {}};
  }
  const NoMoreDay::StashType type =
      static_cast<NoMoreDay::StashType>(payload.stashTarget);
  if (NoMoreDay::StashSystem::withdrawToSpecificSlot(
          registry, type, payload.sourceTab, payload.sourceSlot, player,
          payload.targetSlot)) {
    return {true, GameUiResultCode::Success, "", {}};
  }
  return {false, GameUiResultCode::CapacityFull, "Inventory is full", {}};
}

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteStashUnlockTab(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  (void)player;
  const NoMoreDay::StashType type =
      static_cast<NoMoreDay::StashType>(payload.stashTarget);
  if (NoMoreDay::StashSystem::unlockTab(registry, type)) {
    return {true, GameUiResultCode::Success, "Stash tab unlocked", {}};
  }
  return {false, GameUiResultCode::DomainPrecondition,
          "Cannot unlock stash tab", {}};
}

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteStashSort(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  (void)player;
  if (payload.targetTab < 0) {
    return {false, GameUiResultCode::InvalidIndex, "Invalid stash tab", {}};
  }
  const NoMoreDay::StashType type =
      static_cast<NoMoreDay::StashType>(payload.stashTarget);
  const auto mode = static_cast<NoMoreDay::StashSortMode>(payload.sortMode);
  NoMoreDay::StashSystem::sortTab(registry, type, payload.targetTab, mode);
  return {true, GameUiResultCode::Success, "", {}};
}

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteStashAutoDeposit(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  (void)player;
  const NoMoreDay::StashType type =
      static_cast<NoMoreDay::StashType>(payload.stashTarget);
  const int deposited = NoMoreDay::StashSystem::autoDeposit(registry, type);
  std::string notification =
      deposited > 0 ? "Auto-deposited " + std::to_string(deposited) + " items"
                    : "No items to auto-deposit";
  return {true, GameUiResultCode::Success, std::move(notification), {}};
}


// Explicit instantiations for the concrete registry type used in production.
template GameUiResult GameUiCommandHandler::ExecuteStashTransfer<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteStashDeposit<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteStashWithdraw<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteStashUnlockTab<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteStashSort<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteStashAutoDeposit<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;

} // namespace NoMoreDay::ui
