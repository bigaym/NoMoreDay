#include "game/application/ui/GameUiCommandHandler.hpp"
#include "game/application/ui/UiCraftBurst.hpp" // R10: crafting success bursts

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
#include "game/foundation/components/Stats.hpp" // StatsDirty (affix recalc)
#include "game/systems/item/CraftingSystem.hpp"
#include "game/systems/item/InventorySystem.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "game/systems/item/SalvageSystem.hpp"
#include "game/systems/item/StashSystem.hpp"
#include "game/contracts/impl/StatsSystem.hpp"
#include "game/foundation/components/SkillDefs.hpp" // ActiveSkillsComponent
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/foundation/data/BladeMasteryData.hpp"
#include "game/foundation/data/AstrolabeRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/BladeMasteryService.hpp"
#include "game/systems/skill/AstrolabeSystem.hpp"
#include "engine/render/GPUParticleSystem.hpp" // R8: astrolabe unlock particles

// R8: the UI surface never writes skill / mastery / astrolabe state directly.
// These executors are the authoritative gameplay-owned operations (delegating
// to SkillSystem / BladeMasteryService / AstrolabeSystem) and are the only
// writers reachable from skillHotbar / skillTree / UISkillHub / astrolabe.

// This translation unit is the ONLY place the handler touches the registry:
// GameplayState::OnUpdate calls Execute during the Update phase and the
// handler never runs during render. Every execution re-resolves the player,
// the target and the source/destination locations against the live registry,
// re-validates them (validity, ownership, distance, capacity, slot/tab/index,
// domain preconditions) and only then delegates to the authoritative gameplay
// systems. Component references are copied as small POD before a mutator and
// dropped immediately after; any data needed later is re-fetched (EnTT
// safety, remediation plan R1).

namespace NoMoreDay::ui {
namespace {

// Same world-space pickup threshold used by the snapshot builder and the
// legacy mouse click pickup path (UISystem.cpp: distSq <= 180.0f * 180.0f).
inline constexpr float kPickupRange = 180.0f;

entt::entity ToEntity(std::uint64_t domainId) {
  return static_cast<entt::entity>(static_cast<entt::id_type>(domainId));
}

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

// R10 (收尾): crafting success feedback. Restores the fuse / salvage bursts
// that the R7 removal of the UIRenderer draw path dropped, by emitting through
// the existing world-space particle channel (same EmitBatch path as the R8
// astrolabe particles). The anchor is the player's world position; without a
// Position component (unit tests) the burst falls back to the origin.
template <typename Registry>
void EmitCraftSuccessBurst(Registry& registry, entt::entity player,
                           UiCraftBurstKind kind) {
  Vector2 anchor{0.0f, 0.0f};
  if (const auto* pos = registry.template try_get<const Position>(player)) {
    anchor = {pos->x, pos->y};
  }
  NoMoreDay::systems::GPUParticleSystem::Get().EmitBatch(
      BuildCraftSuccessBurst(kind, anchor));
}

bool TryResolvePlayer(const entt::registry& registry, entt::entity& outPlayer) {
  const auto playerView = registry.template view<const PlayerTag>();
  if (playerView.begin() == playerView.end()) {
    return false;
  }
  outPlayer = playerView.front();
  return true;
}

bool IsValidItem(const entt::registry& registry, entt::entity entity) {
  return registry.valid(entity) &&
         registry.template all_of<NoMoreDay::ItemComponent>(entity);
}

// Ownership: the item must be in the player's inventory, a bag slot or an
// equipment slot.
bool IsItemOwnedByPlayer(const entt::registry& registry, entt::entity player,
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
GameUiResult ToCraftingResult(NoMoreDay::CraftingResult systemResult) {
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

} // namespace

template <typename Registry>
GameUiResult GameUiCommandHandler::Execute(Registry& registry,
                                           const GameUiIntent& intent) const {
  switch (intent.kind) {
  case GameUiIntentKind::PickupItem:
    return ExecutePickup(registry, intent.payload);
  case GameUiIntentKind::EquipItem:
    return ExecuteEquip(registry, intent.payload);
  case GameUiIntentKind::UnequipItem:
    return ExecuteUnequip(registry, intent.payload);
  case GameUiIntentKind::UseItem:
    return ExecuteUse(registry, intent.payload);
  case GameUiIntentKind::DropItem:
    return ExecuteDrop(registry, intent.payload);
  case GameUiIntentKind::DestroyItem:
    return ExecuteDestroy(registry, intent.payload);
  case GameUiIntentKind::LockItem:
    return ExecuteLock(registry, intent.payload, true);
  case GameUiIntentKind::UnlockItem:
    return ExecuteLock(registry, intent.payload, false);
  case GameUiIntentKind::OrganizeInventory:
    return ExecuteOrganize(registry, intent.payload);
  case GameUiIntentKind::MoveItem:
    return ExecuteMove(registry, intent.payload);
  case GameUiIntentKind::SwapItems:
    return ExecuteSwap(registry, intent.payload);
  case GameUiIntentKind::BagEquip:
  case GameUiIntentKind::BagUnequip:
    return ExecuteBag(registry, intent.payload);
  case GameUiIntentKind::SocketRune:
    return ExecuteSocket(registry, intent.payload);
  case GameUiIntentKind::UnsocketRune:
    return ExecuteUnsocket(registry, intent.payload);
  case GameUiIntentKind::StashTransfer:
    return ExecuteStashTransfer(registry, intent.payload);
  case GameUiIntentKind::StashDeposit:
    return ExecuteStashDeposit(registry, intent.payload);
  case GameUiIntentKind::StashWithdraw:
    return ExecuteStashWithdraw(registry, intent.payload);
  case GameUiIntentKind::StashUnlockTab:
    return ExecuteStashUnlockTab(registry, intent.payload);
  case GameUiIntentKind::StashSort:
    return ExecuteStashSort(registry, intent.payload);
  case GameUiIntentKind::StashAutoDeposit:
    return ExecuteStashAutoDeposit(registry, intent.payload);
  case GameUiIntentKind::CraftAffixUpgrade:
    return ExecuteCraftAffixUpgrade(registry, intent.payload);
  case GameUiIntentKind::CraftChaos:
    return ExecuteCraftChaos(registry, intent.payload);
  case GameUiIntentKind::CraftRefine:
    return ExecuteCraftRefine(registry, intent.payload);
  case GameUiIntentKind::CraftAddAffix:
    return ExecuteCraftAddAffix(registry, intent.payload);
  case GameUiIntentKind::CraftFuse:
    return ExecuteCraftFuse(registry, intent.payload);
  case GameUiIntentKind::CraftSalvage:
    return ExecuteCraftSalvage(registry, intent.payload);
  case GameUiIntentKind::CraftBatchSalvage:
    return ExecuteCraftBatchSalvage(registry, intent.payload);
  case GameUiIntentKind::ConfirmAttributeAllocation:
    return ExecuteConfirmAttributes(registry, intent.payload);
  case GameUiIntentKind::SkillAssign:
    return ExecuteSkillAssign(registry, intent.payload);
  case GameUiIntentKind::SkillUnassign:
    return ExecuteSkillUnassign(registry, intent.payload);
  case GameUiIntentKind::SkillResetTalents:
    return ExecuteSkillResetTalents(registry, intent.payload);
  case GameUiIntentKind::SkillAllocateTalentPoint:
    return ExecuteSkillAllocateTalentPoint(registry, intent.payload);
  case GameUiIntentKind::SkillSelectMastery:
    return ExecuteSkillSelectMastery(registry, intent.payload);
  case GameUiIntentKind::SkillSetAttunement:
    return ExecuteSkillSetAttunement(registry, intent.payload);
  case GameUiIntentKind::SkillSetDebugUnlock:
    return ExecuteSkillSetDebugUnlock(registry, intent.payload);
  case GameUiIntentKind::AstrolabeAddPoint:
    return ExecuteAstrolabeAddPoint(registry, intent.payload);
  case GameUiIntentKind::AstrolabeTakeVow:
    return ExecuteAstrolabeTakeVow(registry, intent.payload);
  case GameUiIntentKind::Count:
    break;
  }
  return {false, GameUiResultCode::DomainPrecondition, "Unknown intent kind",
          {}};
}

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
  const int invSlot = payload.sourceSlot >= 0 ? payload.sourceSlot : 0;
  const NoMoreDay::StashType type =
      static_cast<NoMoreDay::StashType>(payload.stashTarget);
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

// --- Crafting / salvage ----------------------------------------------------

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteCraftAffixUpgrade(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  const entt::entity item = ToEntity(payload.targetDomainId);
  if (!IsValidItem(registry, item)) {
    return {false, GameUiResultCode::InvalidTarget, "Invalid crafting target",
            {}};
  }
  if (!IsItemOwnedByPlayer(registry, player, item)) {
    return {false, GameUiResultCode::NotOwned,
            "Item is not owned by the player", {}};
  }
  // Validate the affix index against the live affix list, then pass a small
  // POD copy of the index to the mutator; the component reference is dropped
  // right after the call (EnTT safety).
  auto& itemComp = registry.get<NoMoreDay::ItemComponent>(item);
  if (payload.affixIndex < 0 ||
      payload.affixIndex >= static_cast<std::int32_t>(itemComp.affixes.size())) {
    return {false, GameUiResultCode::InvalidIndex, "Invalid affix index", {}};
  }
  const int affixIndex = payload.affixIndex;
  GameUiResult result = ToCraftingResult(
      NoMoreDay::CraftingSystem::upgradeAffix(itemComp, affixIndex));
  // R7: affix mutations change derived stats; mark the player dirty on success
  // (behavior parity with the legacy draw-path StatsDirty emplace).
  if (result.success) {
    registry.template get_or_emplace<NoMoreDay::StatsDirty>(player);
  }
  return result;
}

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteCraftChaos(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  const entt::entity item = ToEntity(payload.targetDomainId);
  if (!IsValidItem(registry, item)) {
    return {false, GameUiResultCode::InvalidTarget, "Invalid crafting target",
            {}};
  }
  if (!IsItemOwnedByPlayer(registry, player, item)) {
    return {false, GameUiResultCode::NotOwned,
            "Item is not owned by the player", {}};
  }
  auto& itemComp = registry.get<NoMoreDay::ItemComponent>(item);
  if (payload.affixIndex < 0 ||
      payload.affixIndex >= static_cast<std::int32_t>(itemComp.affixes.size())) {
    return {false, GameUiResultCode::InvalidIndex, "Invalid affix index", {}};
  }
  const int affixIndex = payload.affixIndex;
  GameUiResult result = ToCraftingResult(
      NoMoreDay::CraftingSystem::chaosAffix(itemComp, affixIndex));
  // R7: affix mutations change derived stats; mark the player dirty on success.
  if (result.success) {
    registry.template get_or_emplace<NoMoreDay::StatsDirty>(player);
  }
  return result;
}

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteCraftRefine(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  const entt::entity item = ToEntity(payload.targetDomainId);
  if (!IsValidItem(registry, item)) {
    return {false, GameUiResultCode::InvalidTarget, "Invalid crafting target",
            {}};
  }
  if (!IsItemOwnedByPlayer(registry, player, item)) {
    return {false, GameUiResultCode::NotOwned,
            "Item is not owned by the player", {}};
  }
  auto& itemComp = registry.get<NoMoreDay::ItemComponent>(item);
  if (payload.affixIndex < 0 ||
      payload.affixIndex >= static_cast<std::int32_t>(itemComp.affixes.size())) {
    return {false, GameUiResultCode::InvalidIndex, "Invalid affix index", {}};
  }
  const int affixIndex = payload.affixIndex;
  GameUiResult result = ToCraftingResult(
      NoMoreDay::CraftingSystem::refineAffixValues(itemComp, affixIndex));
  // R7: affix mutations change derived stats; mark the player dirty on success.
  if (result.success) {
    registry.template get_or_emplace<NoMoreDay::StatsDirty>(player);
  }
  return result;
}

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteCraftAddAffix(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  const entt::entity item = ToEntity(payload.targetDomainId);
  if (!IsValidItem(registry, item)) {
    return {false, GameUiResultCode::InvalidTarget, "Invalid crafting target",
            {}};
  }
  if (!IsItemOwnedByPlayer(registry, player, item)) {
    return {false, GameUiResultCode::NotOwned,
            "Item is not owned by the player", {}};
  }
  auto& itemComp = registry.get<NoMoreDay::ItemComponent>(item);
  const auto type = static_cast<NoMoreDay::AffixType>(payload.affixType);
  GameUiResult result = ToCraftingResult(
      NoMoreDay::CraftingSystem::addAffix(itemComp, type, payload.isPrefix));
  // R7: affix mutations change derived stats; mark the player dirty on success.
  if (result.success) {
    registry.template get_or_emplace<NoMoreDay::StatsDirty>(player);
  }
  return result;
}

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteCraftFuse(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  const entt::entity base = ToEntity(payload.sourceDomainId);
  const entt::entity fodder = ToEntity(payload.targetDomainId);
  if (!IsValidItem(registry, base) || !IsValidItem(registry, fodder)) {
    return {false, GameUiResultCode::InvalidTarget, "Invalid fusion target",
            {}};
  }
  if (!IsItemOwnedByPlayer(registry, player, base) ||
      !IsItemOwnedByPlayer(registry, player, fodder)) {
    return {false, GameUiResultCode::NotOwned,
            "Fusion items must belong to the player", {}};
  }

  const entt::entity catalyst = ToEntity(payload.catalystDomainId);
  GameUiResult uiResult;
  if (catalyst != entt::null && IsValidItem(registry, catalyst) &&
      IsItemOwnedByPlayer(registry, player, catalyst)) {
    // Legendary fusion path (merge panel): consumes fodder + catalyst.
    const NoMoreDay::CraftingResult result =
        NoMoreDay::CraftingSystem::fuseLegendary(
            registry, base, fodder, catalyst, payload.affixIndex);
    uiResult = ToCraftingResult(result);
    if (uiResult.success) {
      uiResult.clearedDomainIds.push_back(entt::to_integral(fodder));
      uiResult.clearedDomainIds.push_back(entt::to_integral(catalyst));
    }
  } else {
    // Plain fusion path (forging tab): mutates the base item in place.
    auto& baseComp = registry.get<NoMoreDay::ItemComponent>(base);
    auto& fodderComp = registry.get<NoMoreDay::ItemComponent>(fodder);
    uiResult = ToCraftingResult(NoMoreDay::CraftingSystem::fuseItems(
        baseComp, fodderComp));
  }
  // R10 (收尾): restore the fuse success burst (removed in R7).
  if (uiResult.success) {
    EmitCraftSuccessBurst(registry, player, UiCraftBurstKind::Fuse);
  }
  return uiResult;
}

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteCraftSalvage(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  const entt::entity item = ToEntity(payload.targetDomainId);
  if (!IsValidItem(registry, item)) {
    return {false, GameUiResultCode::InvalidTarget, "Invalid salvage target",
            {}};
  }
  if (!IsItemOwnedByPlayer(registry, player, item)) {
    return {false, GameUiResultCode::NotOwned,
            "Item is not owned by the player", {}};
  }
  // Locked items cannot be salvaged (mirrors SalvageSystem::CanSalvage).
  const auto& itemComp = registry.get<NoMoreDay::ItemComponent>(item);
  if (itemComp.isLocked) {
    return {false, GameUiResultCode::Locked, "Item is locked", {}};
  }
  if (!NoMoreDay::SalvageSystem::CanSalvage(itemComp)) {
    return {false, GameUiResultCode::DomainPrecondition,
            "Item cannot be salvaged", {}};
  }
  NoMoreDay::SalvageSystem::Execute(registry, item, player);
  // R10 (收尾): restore the salvage success burst (removed in R7).
  EmitCraftSuccessBurst(registry, player, UiCraftBurstKind::Salvage);
  std::vector<std::uint64_t> cleared{entt::to_integral(item)};
  return {true, GameUiResultCode::Success, "", std::move(cleared)};
}

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteCraftBatchSalvage(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  std::vector<entt::entity> salvaged;
  const int count = NoMoreDay::SalvageSystem::BatchExecuteFiltered(
      registry, player, payload.salvageRarityMask, payload.keepIfTier6Plus,
      payload.excludeLocked, &salvaged);
  std::vector<std::uint64_t> cleared;
  cleared.reserve(salvaged.size());
  for (const entt::entity e : salvaged) {
    cleared.push_back(entt::to_integral(e));
  }
  if (count > 0) {
    // R10 (收尾): mass-salvage feedback reuses the single salvage burst.
    EmitCraftSuccessBurst(registry, player, UiCraftBurstKind::Salvage);
    return {true, GameUiResultCode::Success,
            "Salvaged " + std::to_string(count) + " items", std::move(cleared)};
  }
  return {false, GameUiResultCode::DomainPrecondition,
          "No items match the salvage filter", std::move(cleared)};
}

// --- Character -------------------------------------------------------------

template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteConfirmAttributes(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  const bool allocated = NoMoreDay::StatsSystem::AllocateAttributePoints(
      registry, player, payload.allocationStrength,
      payload.allocationDexterity, payload.allocationIntelligence,
      payload.allocationVitality);
  if (!allocated) {
    return {false, GameUiResultCode::DomainPrecondition,
            "Cannot allocate attribute points", {}};
  }
  return {true, GameUiResultCode::Success, "Attribute points allocated", {}};
}

// --- Skill / mastery / astrolabe (R8) --------------------------------------

// Assigns a skill to a hotbar slot or a specialized (talent) slot. The write
// targets the authoritative ActiveSkillsComponent; the UI only asks. The
// specialized branch mirrors the legacy UISkillHub behaviour: reject a skill
// already present in another specialized slot, reset the previous skill's
// talents, then assign (R8 intent migration).
template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteSkillAssign(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  if (payload.skillId == 0 ||
      payload.skillId == NoMoreDay::INVALID_SKILL_ID) {
    return {false, GameUiResultCode::InvalidTarget, "Invalid skill id", {}};
  }
  auto* active =
      registry.template try_get<NoMoreDay::ActiveSkillsComponent>(player);
  if (active == nullptr) {
    return {false, GameUiResultCode::MissingComponent,
            "Player has no active skills", {}};
  }
  const std::size_t slot = static_cast<std::size_t>(payload.sourceSlot);
  if (slot >= active->slots.size()) {
    return {false, GameUiResultCode::InvalidSlot, "Invalid skill slot", {}};
  }
  const auto target =
      static_cast<NoMoreDay::ui::GameUiSkillTarget>(payload.skillTarget);
  if (target == GameUiSkillTarget::Hotbar) {
    active->slots[slot].id = payload.skillId;
    return {true, GameUiResultCode::Success, "", {}};
  }
  // Specialized slot: reject duplicates across the other specialized slots.
  for (std::size_t i = 0; i < active->specialized_slots.size(); ++i) {
    if (i != slot &&
        active->specialized_slots[i].skill_id == payload.skillId) {
      return {false, GameUiResultCode::InvalidSlot,
              "Skill is already assigned to another specialized slot", {}};
    }
  }
  auto& specialized = active->specialized_slots[slot];
  if (specialized.skill_id != NoMoreDay::INVALID_SKILL_ID) {
    NoMoreDay::SkillSystem::ResetTalents(registry, player,
                                                  specialized.skill_id);
  }
  specialized.skill_id = payload.skillId;
  specialized.bonus_levels = 0;
  specialized.allocated_points.clear();
  return {true, GameUiResultCode::Success, "", {}};
}

// Clears a specialized slot (and drops its talents). Mirrors the legacy
// right-click "unassign" behaviour of UISkillHub.
template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteSkillUnassign(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  auto* active =
      registry.template try_get<NoMoreDay::ActiveSkillsComponent>(player);
  if (active == nullptr) {
    return {false, GameUiResultCode::MissingComponent,
            "Player has no active skills", {}};
  }
  const std::size_t slot = static_cast<std::size_t>(payload.sourceSlot);
  if (slot >= active->specialized_slots.size()) {
    return {false, GameUiResultCode::InvalidSlot, "Invalid skill slot", {}};
  }
  auto& specialized = active->specialized_slots[slot];
  if (specialized.skill_id != NoMoreDay::INVALID_SKILL_ID) {
    NoMoreDay::SkillSystem::ResetTalents(registry, player,
                                                  specialized.skill_id);
  }
  specialized = NoMoreDay::SpecializedSkill{};
  return {true, GameUiResultCode::Success, "", {}};
}

// Resets the talent tree of a skill (skill tree "reset" button).
template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteSkillResetTalents(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  if (!NoMoreDay::SkillSystem::ResetTalents(registry, player,
                                                     payload.skillId)) {
    return {false, GameUiResultCode::DomainPrecondition,
            "Cannot reset talents", {}};
  }
  return {true, GameUiResultCode::Success, "", {}};
}

// Allocates one talent point on a skill-tree node (node click in the tree).
template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteSkillAllocateTalentPoint(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  if (!NoMoreDay::SkillSystem::AddTalentPoint(
          registry, player, payload.skillId, payload.astrolabeNodeId)) {
    return {false, GameUiResultCode::DomainPrecondition,
            "Cannot allocate talent point", {}};
  }
  return {true, GameUiResultCode::Success, "", {}};
}

// Selects a blade mastery; the failure string is the contractual popup text
// asserted by the legacy UI tests (UITests: Locked mastery selection shows
// popup).
template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteSkillSelectMastery(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  const auto mastery =
      static_cast<NoMoreDay::BladeMasteryId>(payload.masteryId);
  if (!NoMoreDay::systems::BladeMasteryService::SelectMastery(registry, player,
                                                              mastery)) {
    return {false, GameUiResultCode::DomainPrecondition,
            "等级或基础职业不满足职业专精条件", {}};
  }
  return {true, GameUiResultCode::Success, "", {}};
}

// Sets the Heavenly Sword attunement element.
template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteSkillSetAttunement(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  const auto attunement =
      static_cast<NoMoreDay::BladeAttunement>(payload.attunementElement);
  if (!NoMoreDay::systems::BladeMasteryService::SetHeavenlySwordAttunement(
          registry, player, attunement)) {
    return {false, GameUiResultCode::DomainPrecondition,
            "Cannot set Heavenly Sword attunement", {}};
  }
  return {true, GameUiResultCode::Success, "", {}};
}

// Toggles the debug unlock override and refreshes the player's mastery state.
template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteSkillSetDebugUnlock(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  NoMoreDay::systems::BladeMasteryService::SetDebugUnlockOverrideEnabled(
      payload.flag);
  NoMoreDay::systems::BladeMasteryService::RefreshPlayerState(registry, player);
  return {true, GameUiResultCode::Success, "", {}};
}

// Spends one astrolabe point on a node. The particle emission that used to
// live in AstrolabeController::HandleInteraction moved here so the render
// phase never mutates gameplay or spawns particles (R8).
template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteAstrolabeAddPoint(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  auto* astro =
      registry.template try_get<NoMoreDay::AstrolabeComponent>(player);
  if (astro == nullptr) {
    return {false, GameUiResultCode::MissingComponent,
            "Player has no astrolabe", {}};
  }
  const auto& graph = NoMoreDay::AstrolabeRegistry::Get().GetGraph();
  const auto nodeIt = graph.nodes.find(payload.astrolabeNodeId);
  if (nodeIt == graph.nodes.end()) {
    return {false, GameUiResultCode::InvalidTarget, "Node not found", {}};
  }
  int requiredAffinity = 0;
  const auto reason = NoMoreDay::AstrolabeSystem::tryUnlockNode(
      graph, *astro, payload.astrolabeNodeId, &requiredAffinity);
  switch (reason) {
  case NoMoreDay::AstrolabeSystem::UnlockFailReason::NoPoints:
    return {false, GameUiResultCode::DomainPrecondition, "星尘不足!", {}};
  case NoMoreDay::AstrolabeSystem::UnlockFailReason::TierLocked:
    return {false, GameUiResultCode::DomainPrecondition,
            "需要 " + std::to_string(requiredAffinity) +
                " 点亲和度 (当前: " +
                std::to_string(astro->getAffinity(
                    nodeIt->second.profession)) +
                ")",
            {}};
  case NoMoreDay::AstrolabeSystem::UnlockFailReason::CoreSealed:
    return {false, GameUiResultCode::DomainPrecondition,
            "核心节点需先立下誓约!", {}};
  case NoMoreDay::AstrolabeSystem::UnlockFailReason::MaxPointsReached:
    return {false, GameUiResultCode::DomainPrecondition, "节点已达上限!", {}};
  case NoMoreDay::AstrolabeSystem::UnlockFailReason::NodeNotFound:
  case NoMoreDay::AstrolabeSystem::UnlockFailReason::Success:
  default:
    break;
  }
  const bool added = NoMoreDay::AstrolabeSystem::addPointToNode(
      registry, player, graph, payload.astrolabeNodeId);
  if (!added) {
    return {false, GameUiResultCode::DomainPrecondition,
            "Cannot add astrolabe point", {}};
  }
  // Visual feedback: energy flow from the profession star to the node, and a
  // supernova when the node reaches its max points. Copied verbatim from the
  // old AstrolabeController::EmitEnergyFlow / EmitSupernova.
  const auto& node = nodeIt->second;
  {
    const auto& star = graph.professionStars[(int)node.profession];
    const Vector2 start = {star.x, star.y};
    const Vector2 end = {node.x, node.y};
    std::vector<components::GPUParticle> particles;
    for (int i = 0; i < 30; ++i) {
      components::GPUParticle p;
      p.position = start;
      const Vector2 dir =
          Vector2Normalize(Vector2Subtract(end, start));
      const float speed = 150.0f + (float)GetRandomValue(0, 150);
      p.velocity = Vector2Scale(dir, speed);
      p.acceleration = Vector2Scale(dir, 800.0f);
      p.lifetime = 0.8f;
      p.maxLifetime = 0.8f;
      p.scale = 2.5f;
      p.color = GOLD;
      p.growthRate = -1.0f;
      particles.push_back(p);
    }
    NoMoreDay::systems::GPUParticleSystem::Get().EmitBatch(particles);
  }
  const auto nodePoints =
      NoMoreDay::AstrolabeSystem::getNodePoints(graph, *astro,
                                                payload.astrolabeNodeId);
  if (nodePoints.first >= nodePoints.second && nodePoints.first > 0) {
    std::vector<components::GPUParticle> particles;
    for (int i = 0; i < 80; ++i) {
      components::GPUParticle p;
      p.position = {node.x, node.y};
      const float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
      const float speed = 250.0f + (float)GetRandomValue(0, 400);
      p.velocity = {cosf(angle) * speed, sinf(angle) * speed};
      p.lifetime = 1.2f;
      p.maxLifetime = 1.2f;
      p.scale = 5.0f;
      p.growthRate = -3.0f;
      p.color = GOLD;
      particles.push_back(p);
    }
    NoMoreDay::systems::GPUParticleSystem::Get().EmitBatch(particles);
  }
  return {true, GameUiResultCode::Success, "", {}};
}

// Confirms the (irreversible) astrolabe vow for a profession. Mirrors the old
// DrawVowDialog confirm branch (R8 intent migration).
template <typename Registry>
GameUiResult GameUiCommandHandler::ExecuteAstrolabeTakeVow(
    Registry& registry, const GameUiIntentPayload& payload) const {
  entt::entity player = entt::null;
  if (!TryResolvePlayer(registry, player)) {
    return {false, GameUiResultCode::NoPlayer, "No player", {}};
  }
  auto* astro =
      registry.template try_get<NoMoreDay::AstrolabeComponent>(player);
  if (astro == nullptr) {
    return {false, GameUiResultCode::MissingComponent,
            "Player has no astrolabe", {}};
  }
  const auto profession =
      static_cast<NoMoreDay::ProfessionID>(payload.professionId);
  if (!NoMoreDay::AstrolabeSystem::takeVow(registry, player, profession)) {
    return {false, GameUiResultCode::DomainPrecondition,
            "Cannot take the vow", {}};
  }
  return {true, GameUiResultCode::Success, "", {}};
}

// Explicit instantiations for the concrete registry type used in production.
template GameUiResult GameUiCommandHandler::Execute<entt::registry>(
    entt::registry&, const GameUiIntent&) const;

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
template GameUiResult GameUiCommandHandler::ExecuteCraftAffixUpgrade<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteCraftChaos<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteCraftRefine<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteCraftAddAffix<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteCraftFuse<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteCraftSalvage<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteCraftBatchSalvage<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteConfirmAttributes<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteSkillAssign<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteSkillUnassign<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteSkillResetTalents<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteSkillAllocateTalentPoint<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteSkillSelectMastery<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteSkillSetAttunement<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteSkillSetDebugUnlock<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteAstrolabeAddPoint<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;
template GameUiResult GameUiCommandHandler::ExecuteAstrolabeTakeVow<entt::registry>(
    entt::registry&, const GameUiIntentPayload&) const;

} // namespace NoMoreDay::ui
