#include "game/application/ui/GameUiCommandHandler.hpp"
#include "game/application/ui/GameUiCommandHandlerInternal.hpp"

using namespace NoMoreDay::ui::detail;

namespace NoMoreDay::ui {

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


// Explicit instantiations for the concrete registry type used in production.
template GameUiResult GameUiCommandHandler::Execute<entt::registry>(
    entt::registry&, const GameUiIntent&) const;


} // namespace NoMoreDay::ui
