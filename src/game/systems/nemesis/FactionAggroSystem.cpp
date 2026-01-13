#include "game/systems/nemesis/FactionAggroSystem.hpp"
#include "core/logging/Logger.hpp"
#include "game/components/PlayerState.hpp"
#include "game/data/NemesisDataStore.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"


namespace NoMoreDay {

uint32_t FactionAggroSystem::s_handler_id = 0;

void FactionAggroSystem::Init() {
  // Register OnKill handler with high priority
  s_handler_id = CombatEventDispatcher::Register(
      CombatEventType::OnKill, &FactionAggroSystem::OnKillHandler,
      100 // High priority to ensure we capture kills early
  );

  LOG_INFO("FactionAggroSystem: Initialized");
}

void FactionAggroSystem::Shutdown() {
  if (s_handler_id != 0) {
    CombatEventDispatcher::Unregister(CombatEventType::OnKill, s_handler_id);
    s_handler_id = 0;
  }
  LOG_INFO("FactionAggroSystem: Shutdown");
}

void FactionAggroSystem::Update(entt::registry &registry) {
  // Sync persistent data store with player component (if needed)
  auto view = registry.view<PlayerTag, PlayerFactionAggro>();
  if (view.size_hint() == 0) {
    // Ensure player has the aggro component
    for (auto [entity] : registry.view<PlayerTag>().each()) {
      if (!registry.any_of<PlayerFactionAggro>(entity)) {
        auto &aggro = registry.emplace<PlayerFactionAggro>(entity);
        // Load from persistent store
        auto &store = NemesisDataStore::Get();
        for (size_t i = 0; i < aggro.aggro.size(); ++i) {
          aggro.aggro[i] = store.faction_aggro[i];
        }
        LOG_DEBUG("FactionAggroSystem: Initialized player aggro from store");
      }
    }
  }
}

bool FactionAggroSystem::ShouldSpawnNemesis(entt::registry &registry,
                                            FactionType &outFaction) {
  auto view = registry.view<PlayerTag, PlayerFactionAggro>();
  for (auto [entity, aggro] : view.each()) {
    if (aggro.HasTriggeredNemesis(outFaction)) {
      return true;
    }
  }
  return false;
}

void FactionAggroSystem::ConsumeNemesisTrigger(entt::registry &registry,
                                               FactionType faction) {
  auto view = registry.view<PlayerTag, PlayerFactionAggro>();
  for (auto [entity, aggro] : view.each()) {
    aggro.ResetAggro(faction);

    // Also reset in persistent store
    auto &store = NemesisDataStore::Get();
    store.faction_aggro[static_cast<size_t>(faction)] = 0.0f;
  }

  LOG_INFO("FactionAggroSystem: Consumed Nemesis trigger for faction {}",
           FactionTypeName(faction));
}

void FactionAggroSystem::OnKillHandler(entt::registry &registry,
                                       const CombatEvent &evt) {
  // Check if the source is the player
  if (!registry.valid(evt.source) || !registry.any_of<PlayerTag>(evt.source)) {
    return;
  }

  // Check if the target is a Nemesis
  if (registry.all_of<NemesisComponent, NemesisTag>(evt.target)) {
    NemesisDataStore::Get().EvolveActiveNemesis();
    LOG_INFO("FactionAggroSystem: Player defeated a Nemesis! Evolution tier increased.");
  }

  // Check if the target has a faction
  if (!registry.valid(evt.target) ||
      !registry.any_of<FactionComponent>(evt.target)) {
    return;
  }

  const auto &faction_comp = registry.get<FactionComponent>(evt.target);
  FactionType faction = faction_comp.faction;

  // Determine aggro amount based on enemy rarity
  float aggro_amount = PlayerFactionAggro::AGGRO_NORMAL;

  if (auto *rarity = registry.try_get<EnemyRarityComponent>(evt.target)) {
    switch (rarity->rarity) {
    case EnemyRarityComponent::ELITE:
      aggro_amount = PlayerFactionAggro::AGGRO_ELITE;
      // Record elite affixes for Nemesis synthesis
      for (const auto &affix : rarity->affixes) {
        NemesisDataStore::Get().RecordKillAffix(affix);
      }
      break;
    case EnemyRarityComponent::BOSS:
      aggro_amount = PlayerFactionAggro::AGGRO_BOSS;
      break;
    default:
      break;
    }
  }

  // Add aggro to player component
  if (auto *player_aggro = registry.try_get<PlayerFactionAggro>(evt.source)) {
    bool triggered = player_aggro->AddAggro(faction, aggro_amount);

    // Sync to persistent store
    auto &store = NemesisDataStore::Get();
    store.faction_aggro[static_cast<size_t>(faction)] =
        player_aggro->GetAggro(faction);

    LOG_DEBUG("FactionAggroSystem: Added {:.1f} aggro to {} (total: {:.1f})",
              aggro_amount, FactionTypeName(faction),
              player_aggro->GetAggro(faction));

    if (triggered) {
      LOG_INFO("FactionAggroSystem: Nemesis threshold reached for {}!",
               FactionTypeName(faction));
    }
  }
}

} // namespace NoMoreDay
