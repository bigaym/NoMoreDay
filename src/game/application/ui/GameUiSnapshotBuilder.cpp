#include "game/application/ui/GameUiSnapshotBuilder.hpp"

#include <entt/entt.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/InventoryComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/PlayerState.hpp"

namespace NoMoreDay::ui {
namespace {

// Same world-space pickup threshold used by the mouse click pickup path in
// UISystem::Draw (UISystem.cpp:681, distSq <= 180.0f * 180.0f).
inline constexpr float kPickupRange = 180.0f;

// Occupied inventory slots: non-null entries in the items vector.
int CountUsedSlots(const NoMoreDay::InventoryComponent& inventory) {
  int used = 0;
  for (const entt::entity slot : inventory.items) {
    if (slot != entt::null) {
      ++used;
    }
  }
  return used;
}

} // namespace

template <typename Registry>
GameUiSnapshot GameUiSnapshotBuilder::Build(const Registry& registry) const {
  GameUiSnapshot snapshot;

  // --- Player snapshot -------------------------------------------------
  // Panel fields come from the actual player components the existing panels
  // read (PlayerStats.level, HealthComponent, InventoryComponent).
  float playerX = 0.0f;
  float playerY = 0.0f;
  bool hasPlayerPosition = false;

  const auto playerView = registry.template view<const PlayerTag>();
  if (playerView.begin() != playerView.end()) {
    const entt::entity player = playerView.front();
    GameUiPlayerSnapshot& playerSnap = snapshot.player;
    playerSnap.hasPlayer = true;

    if (const auto* health = registry.template try_get<HealthComponent>(player)) {
      playerSnap.health = health->current;
      playerSnap.maxHealth = health->max;
    }
    if (const auto* stats = registry.template try_get<PlayerStats>(player)) {
      playerSnap.level = stats->level;
    }
    if (const auto* inventory =
            registry.template try_get<NoMoreDay::InventoryComponent>(player)) {
      playerSnap.inventoryUsed = CountUsedSlots(*inventory);
      playerSnap.inventoryCapacity = inventory->capacity;
      playerSnap.gold = inventory->gold;
    }
    if (const auto* position = registry.template try_get<Position>(player)) {
      playerX = position->x;
      playerY = position->y;
      hasPlayerPosition = true;
    }
  }

  // --- Pickup targets --------------------------------------------------
  // Ground items are queried directly from the registry (ItemComponent +
  // Position), mirroring the semantics of the existing pickup path: the item
  // must be a valid world item within pickup range of the player. This is a
  // pure read; no static state (UiShared::VisibleItemCache) is touched and
  // the builder never calls InventorySystem::pickUpItem.
  if (hasPlayerPosition) {
    const auto itemView =
        registry.template view<const NoMoreDay::ItemComponent, const Position>();
    for (const entt::entity item : itemView) {
      if (!registry.valid(item)) {
        continue; // Defensive: skip stale entities.
      }
      const auto& itemPos = itemView.template get<const Position>(item);
      const float dx = itemPos.x - playerX;
      const float dy = itemPos.y - playerY;
      const float distSq = dx * dx + dy * dy;
      if (distSq > kPickupRange * kPickupRange) {
        continue;
      }

      GameUiPickupSnapshot pickup;
      pickup.domainId = entt::to_integral(item);
      pickup.distance = std::sqrt(distSq);
      pickup.source = GameUiPickupSource::World;
      snapshot.pickups.push_back(pickup);
    }

    // Deterministic order: nearest first.
    std::sort(snapshot.pickups.begin(), snapshot.pickups.end(),
              [](const GameUiPickupSnapshot& lhs,
                 const GameUiPickupSnapshot& rhs) {
                return lhs.distance < rhs.distance;
              });
  }

  // notifications are produced by GameUiCommandHandler (U6b); the builder
  // leaves the queue empty for the UI to consume.

  return snapshot;
}

template GameUiSnapshot GameUiSnapshotBuilder::Build<entt::registry>(
    const entt::registry&) const;

} // namespace NoMoreDay::ui
