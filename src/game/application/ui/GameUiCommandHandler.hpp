#pragma once

// Executes validated gameplay requests on behalf of the UI (design §3.1,
// §6.2, remediation plan R1). The UI never mutates the world directly: it
// issues GameUiIntent and the handler re-validates the domain payload
// (entity validity, ownership, distance, capacity, slot/tab/index, domain
// preconditions) against the live registry before touching any gameplay
// system.
//
// Execute is templated on the registry type so this header stays free of
// entt includes (design §3.2); the concrete entt::registry instantiation is
// explicitly instantiated in GameUiCommandHandler.cpp. The handler touches
// the registry only during the Update phase (called from
// GameplayState::OnUpdate), never during render.

#include "game/application/ui/GameUiIntent.hpp"

namespace NoMoreDay::ui {

class GameUiCommandHandler {
public:
  // Registry is entt::registry in practice; see the explicit instantiation
  // in GameUiCommandHandler.cpp. Single public entry: dispatches to the
  // private domain handlers by intent kind. Intents are executed in FIFO
  // order; a failing intent does not block later independent intents but
  // always returns an observable failure result.
  template <typename Registry>
  GameUiResult Execute(Registry& registry, const GameUiIntent& intent) const;

private:
  // --- Domain dispatch helpers (all registry access happens here) ---

  // Ground pickup (design §6.2).
  template <typename Registry>
  GameUiResult ExecutePickup(Registry& registry,
                             const GameUiIntentPayload& payload) const;

  // Inventory / equipment / bag domain.
  template <typename Registry>
  GameUiResult ExecuteEquip(Registry& registry,
                            const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteUnequip(Registry& registry,
                              const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteUse(Registry& registry,
                          const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteDrop(Registry& registry,
                           const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteDestroy(Registry& registry,
                              const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteLock(Registry& registry,
                           const GameUiIntentPayload& payload,
                           bool locked) const;
  template <typename Registry>
  GameUiResult ExecuteOrganize(Registry& registry,
                               const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteMove(Registry& registry,
                           const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteSwap(Registry& registry,
                           const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteBag(Registry& registry,
                          const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteSocket(Registry& registry,
                             const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteUnsocket(Registry& registry,
                               const GameUiIntentPayload& payload) const;

  // Stash domain.
  template <typename Registry>
  GameUiResult ExecuteStashTransfer(Registry& registry,
                                    const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteStashDeposit(Registry& registry,
                                   const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteStashWithdraw(Registry& registry,
                                    const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteStashUnlockTab(Registry& registry,
                                     const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteStashSort(Registry& registry,
                                const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteStashAutoDeposit(Registry& registry,
                                       const GameUiIntentPayload& payload) const;

  // Crafting / salvage domain.
  template <typename Registry>
  GameUiResult ExecuteCraftAffixUpgrade(Registry& registry,
                                        const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteCraftChaos(Registry& registry,
                                 const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteCraftRefine(Registry& registry,
                                  const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteCraftAddAffix(Registry& registry,
                                    const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteCraftFuse(Registry& registry,
                                const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteCraftSalvage(Registry& registry,
                                   const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteCraftBatchSalvage(Registry& registry,
                                        const GameUiIntentPayload& payload) const;

  // Character domain.
  template <typename Registry>
  GameUiResult ExecuteConfirmAttributes(Registry& registry,
                                        const GameUiIntentPayload& payload) const;

  // Skill / mastery / astrolabe domain (R8). These own the gameplay writes
  // behind skillHotbar / skillTree / UISkillHub / astrolabe interactions and
  // delegate to the authoritative systems (SkillSystem, BladeMasteryService,
  // AstrolabeSystem).
  template <typename Registry>
  GameUiResult ExecuteSkillAssign(Registry& registry,
                                  const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteSkillUnassign(Registry& registry,
                                    const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteSkillResetTalents(Registry& registry,
                                        const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteSkillAllocateTalentPoint(
      Registry& registry, const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteSkillSelectMastery(Registry& registry,
                                         const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteSkillSetAttunement(Registry& registry,
                                         const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteSkillSetDebugUnlock(Registry& registry,
                                          const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteAstrolabeAddPoint(Registry& registry,
                                        const GameUiIntentPayload& payload) const;
  template <typename Registry>
  GameUiResult ExecuteAstrolabeTakeVow(Registry& registry,
                                       const GameUiIntentPayload& payload) const;
};

} // namespace NoMoreDay::ui
