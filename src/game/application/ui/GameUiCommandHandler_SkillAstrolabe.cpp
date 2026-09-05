#include "game/application/ui/GameUiCommandHandler.hpp"
#include "game/application/ui/GameUiCommandHandlerInternal.hpp"

#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/foundation/data/BladeMasteryData.hpp"
#include "game/foundation/data/AstrolabeRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/BladeMasteryService.hpp"
#include "game/systems/skill/AstrolabeSystem.hpp"
#include "game/contracts/impl/StatsSystem.hpp"

using namespace NoMoreDay::ui::detail;

namespace NoMoreDay::ui {

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
