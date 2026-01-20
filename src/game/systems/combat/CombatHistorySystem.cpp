#include "game/systems/combat/CombatHistorySystem.hpp"
#include "game/components/Common.hpp"
#include "game/data/PlayerCombatHistory.hpp"
#include "game/data/TagRegistry.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include <cmath>


namespace NoMoreDay {

uint32_t CombatHistorySystem::s_handlerIdDealDamage = 0;
uint32_t CombatHistorySystem::s_handlerIdKill = 0;

void CombatHistorySystem::Init() {
  s_handlerIdDealDamage = CombatEventDispatcher::Register(
      CombatEventType::OnDealDamage, CombatHistorySystem::OnDealDamage);
  s_handlerIdKill = CombatEventDispatcher::Register(
      CombatEventType::OnKill, CombatHistorySystem::OnKill);
}

void CombatHistorySystem::Shutdown() {
  if (s_handlerIdDealDamage != 0) {
    CombatEventDispatcher::Unregister(CombatEventType::OnDealDamage,
                                      s_handlerIdDealDamage);
    s_handlerIdDealDamage = 0;
  }
  if (s_handlerIdKill != 0) {
    CombatEventDispatcher::Unregister(CombatEventType::OnKill, s_handlerIdKill);
    s_handlerIdKill = 0;
  }
}

void CombatHistorySystem::Update(entt::registry &registry, float dt) {
  // Decay history logic
  auto view = registry.view<PlayerCombatHistory>();
  for (auto [entity, history] : view.each()) {
    float decayFactor = 1.0f - (HISTORY_DECAY_RATE * dt);
    if (decayFactor < 0.0f)
      decayFactor = 0.0f;

    history.damageDealtPhysical *= decayFactor;
    history.damageDealtFire *= decayFactor;
    history.damageDealtCold *= decayFactor;
    history.damageDealtLightning *= decayFactor;
    history.damageDealtPoison *= decayFactor;

    // Decay peak slowly so it reflects "Recent Peak"
    history.burstDamagePeak *= (1.0f - (0.01f * dt));
  }
}

void CombatHistorySystem::OnDealDamage(entt::registry &registry,
                                       const CombatEvent &evt) {
  if (!registry.valid(evt.source))
    return;
  if (!registry.any_of<PlayerTag>(evt.source))
    return;

  auto *historyPtr = registry.try_get<PlayerCombatHistory>(evt.source);
  if (!historyPtr)
    return;
  auto &history = *historyPtr;

  float damage = evt.value;

  // Update Damage Profile
  if (HasTag(evt.tags, Tag::Physical))
    history.damageDealtPhysical += damage;
  if (HasTag(evt.tags, Tag::Fire))
    history.damageDealtFire += damage;
  if (HasTag(evt.tags, Tag::Cold))
    history.damageDealtCold += damage;
  if (HasTag(evt.tags, Tag::Lightning))
    history.damageDealtLightning += damage;
  if (HasTag(evt.tags, Tag::Poison))
    history.damageDealtPoison += damage;

  // Update Engagement Distance
  if (registry.valid(evt.target) && registry.all_of<Position>(evt.target) &&
      registry.all_of<Position>(evt.source)) {
    const auto &posSource = registry.get<Position>(evt.source);
    const auto &posTarget = registry.get<Position>(evt.target);

    float dist =
        std::hypot(posSource.x - posTarget.x, posSource.y - posTarget.y);

    // EMA
    history.avgEngagementDistance =
        (DISTANCE_EMA_ALPHA * dist) +
        ((1.0f - DISTANCE_EMA_ALPHA) * history.avgEngagementDistance);
  }

  // Update Peak
  if (damage > history.burstDamagePeak) {
    history.burstDamagePeak = damage;
  }
}

void CombatHistorySystem::OnKill(entt::registry &registry,
                                 const CombatEvent &evt) {
  if (!registry.valid(evt.source))
    return;
  if (!registry.any_of<PlayerTag>(evt.source))
    return;

  auto *historyPtr = registry.try_get<PlayerCombatHistory>(evt.source);
  if (!historyPtr)
    return;
  auto &history = *historyPtr;

  if (HasTag(evt.tags, Tag::Elite) || HasTag(evt.tags, Tag::Boss)) {
    history.elitesKilled++;
  }
}

} // namespace NoMoreDay
