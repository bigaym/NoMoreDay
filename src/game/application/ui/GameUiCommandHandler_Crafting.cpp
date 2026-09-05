#include "game/application/ui/GameUiCommandHandler.hpp"
#include "game/application/ui/GameUiCommandHandlerInternal.hpp"

#include "game/systems/item/CraftingSystem.hpp"
#include "game/systems/item/SalvageSystem.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "game/contracts/impl/StatsSystem.hpp"

using namespace NoMoreDay::ui::detail;

namespace NoMoreDay::ui {

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


// Explicit instantiations for the concrete registry type used in production.
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

} // namespace NoMoreDay::ui
