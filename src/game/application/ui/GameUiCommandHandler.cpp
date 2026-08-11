#include "game/application/ui/GameUiCommandHandler.hpp"

#include <entt/entt.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/components/InventoryComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/systems/item/InventorySystem.hpp"

namespace NoMoreDay::ui {
namespace {

// Same world-space pickup threshold used by the snapshot builder and the
// legacy mouse click pickup path (UISystem.cpp: distSq <= 180.0f * 180.0f).
inline constexpr float kPickupRange = 180.0f;

bool PickupWithinRange(const entt::registry& registry, entt::entity player,
                       entt::entity item) {
  const auto* playerPos = registry.try_get<const Position>(player);
  const auto* itemPos = registry.try_get<const Position>(item);
  if (playerPos == nullptr || itemPos == nullptr) {
    return false; // No position pair: cannot prove the pickup is in range.
  }
  const float dx = itemPos->x - playerPos->x;
  const float dy = itemPos->y - playerPos->y;
  const float distSq = dx * dx + dy * dy;
  return distSq <= kPickupRange * kPickupRange;
}

} // namespace

template <typename Registry>
GameUiResult GameUiCommandHandler::Execute(Registry& registry,
                                           const GameUiIntent& intent) const {
  switch (intent.kind) {
  case GameUiIntentKind::PickupItem:
    break; // Handled below.
  case GameUiIntentKind::EquipItem:
  case GameUiIntentKind::UseItem: {
    const auto playerView = registry.template view<const PlayerTag>();
    if (playerView.begin() == playerView.end()) {
      return {false, "No player"};
    }
    const entt::entity player = playerView.front();

    const entt::entity item =
        static_cast<entt::entity>(static_cast<entt::id_type>(intent.domainId));
    const bool equipping = intent.kind == GameUiIntentKind::EquipItem;
    if (!registry.valid(item) ||
        !registry.template all_of<NoMoreDay::ItemComponent>(item)) {
      return {false, equipping ? "Invalid equip target" : "Invalid use target"};
    }

    auto* inventory =
        registry.template try_get<NoMoreDay::InventoryComponent>(player);
    if (inventory == nullptr) {
      return {false, "Player has no inventory"};
    }

    // The domain id names an item entity stored in the player's bag; bag
    // items carry no world position, so no distance check applies here.
    if (std::find(inventory->items.begin(), inventory->items.end(), item) ==
        inventory->items.end()) {
      return {false, "Item is not in the player's inventory"};
    }

    if (!equipping) {
      // The inventory system owns consumption semantics (consumable check,
      // potion cooldown, quantity decrement / destroy).
      if (InventorySystem::useItem(registry, player, item)) {
        return {true, ""};
      }
      return {false, "Cannot use item"};
    }

    // Equip: mirror the legacy context-menu path (UIRenderer.cpp). Bags go
    // into the bag slots; everything else equips through the resolved slot.
    const auto& itemComp = registry.get<NoMoreDay::ItemComponent>(item);
    if (itemComp.type == NoMoreDay::ItemType::Bag) {
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
        return {true, ""};
      }
      return {false, "Cannot equip bag"};
    }

    if (registry.template try_get<NoMoreDay::EquipmentComponent>(player) ==
        nullptr) {
      return {false, "Player has no equipment"};
    }
    // targetSlot stays None: the equip validation service resolves the slot
    // from the item (mirroring the legacy equipItem call).
    if (InventorySystem::equipItem(registry, player, item)) {
      return {true, ""};
    }
    return {false, "Cannot equip item"};
  }
  }

  const auto playerView = registry.template view<const PlayerTag>();
  if (playerView.begin() == playerView.end()) {
    return {false, "No player"};
  }
  const entt::entity player = playerView.front();

  const entt::entity item =
      static_cast<entt::entity>(static_cast<entt::id_type>(intent.domainId));
  if (!registry.valid(item) ||
      !registry.template all_of<NoMoreDay::ItemComponent>(item)) {
    return {false, "Invalid pickup target"};
  }

  if (registry.template try_get<NoMoreDay::InventoryComponent>(player) ==
      nullptr) {
    return {false, "Player has no inventory"};
  }

  if (!PickupWithinRange(registry, player, item)) {
    return {false, "Too far away"};
  }

  // The inventory system is the single owner of item mutations; it re-checks
  // entity validity and capacity before stacking/placing the item.
  if (InventorySystem::pickUpItem(registry, player, item)) {
    return {true, ""};
  }
  return {false, "Inventory is full"};
}

template GameUiResult GameUiCommandHandler::Execute<entt::registry>(
    entt::registry&, const GameUiIntent&) const;

} // namespace NoMoreDay::ui
