#pragma once

#include "game/application/ui/GameUiCommandHandler.hpp"
#include "game/application/ui/UiCraftBurst.hpp"

#include <entt/entt.hpp>
#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/components/InventoryComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/PlayerState.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/systems/item/CraftingSystem.hpp"
#include "engine/render/GPUParticleSystem.hpp"

namespace NoMoreDay::ui::detail {

// Same world-space pickup threshold used by the snapshot builder and the
// legacy mouse click pickup path (UISystem.cpp: distSq <= 180.0f * 180.0f).
inline constexpr float kPickupRange = 180.0f;

inline entt::entity ToEntity(std::uint64_t domainId) {
  return static_cast<entt::entity>(static_cast<entt::id_type>(domainId));
}

inline bool PickupWithinRange(const entt::registry& registry, entt::entity player,
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

// R10 (收尾): crafting success feedback. Restores the fuse / salvage bursts
// that the R7 removal of the UIRenderer draw path dropped, by emitting through
// the existing world-space particle channel (same EmitBatch path as the R8
// astrolabe particles). The anchor is the player's world position; without a
// Position component (unit tests) the burst falls back to the origin.
template <typename Registry>
inline void EmitCraftSuccessBurst(Registry& registry, entt::entity player,
                                  UiCraftBurstKind kind) {
  Vector2 anchor{0.0f, 0.0f};
  if (const auto* pos = registry.template try_get<const Position>(player)) {
    anchor = {pos->x, pos->y};
  }
  NoMoreDay::systems::GPUParticleSystem::Get().EmitBatch(
      BuildCraftSuccessBurst(kind, anchor));
}

inline bool TryResolvePlayer(const entt::registry& registry, entt::entity& outPlayer) {
  const auto playerView = registry.template view<const PlayerTag>();
  if (playerView.begin() == playerView.end()) {
    return false;
  }
  outPlayer = playerView.front();
  return true;
}

inline bool IsValidItem(const entt::registry& registry, entt::entity entity) {
  return registry.valid(entity) &&
         registry.template all_of<NoMoreDay::ItemComponent>(entity);
}

// Ownership: the item must be in the player's inventory, a bag slot or an
// equipment slot.
inline bool IsItemOwnedByPlayer(const entt::registry& registry, entt::entity player,
                                entt::entity item) {
  if (const auto* inventory =
          registry.template try_get<NoMoreDay::InventoryComponent>(player)) {
    if (std::find(inventory->items.begin(), inventory->items.end(), item) !=
        inventory->items.end()) {
      return true;
    }
    if (std::find(inventory->bag_slots.begin(), inventory->bag_slots.end(),
                  item) != inventory->bag_slots.end()) {
      return true;
    }
  }
  if (const auto* equipment =
          registry.template try_get<NoMoreDay::EquipmentComponent>(player)) {
    if (std::find(equipment->slots.begin(), equipment->slots.end(), item) !=
        equipment->slots.end()) {
      return true;
    }
  }
  return false;
}

// Maps a crafting system result to the UI result contract.
inline GameUiResult ToCraftingResult(NoMoreDay::CraftingResult systemResult) {
  switch (systemResult) {
  case NoMoreDay::CraftingResult::Success:
  case NoMoreDay::CraftingResult::CriticalSuccess:
    return {true, GameUiResultCode::Success, "", {}};
  case NoMoreDay::CraftingResult::NoPotential:
    return {false, GameUiResultCode::DomainPrecondition,
            "Item has no potential left", {}};
  case NoMoreDay::CraftingResult::MaxTierReached:
    return {false, GameUiResultCode::DomainPrecondition,
            "Affix is already at max tier", {}};
  case NoMoreDay::CraftingResult::SlotFull:
    return {false, GameUiResultCode::DomainPrecondition,
            "No free affix slot", {}};
  case NoMoreDay::CraftingResult::MaterialMissing:
    return {false, GameUiResultCode::MaterialMissing, "Missing materials", {}};
  case NoMoreDay::CraftingResult::Failure:
  default:
    return {false, GameUiResultCode::CraftingFailure, "Crafting failed", {}};
  }
}

} // namespace NoMoreDay::ui::detail
