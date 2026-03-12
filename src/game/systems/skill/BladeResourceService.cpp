#include "game/systems/skill/BladeResourceService.hpp"

#include "core/logging/Logger.hpp"
#include "game/components/Common.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"

#include <algorithm>

namespace NoMoreDay::systems {

namespace {

constexpr float kDemonBladeHealthCostMultiplier = 2.0f;
constexpr float kDemonBladeLowLifeThreshold = 0.35f;
constexpr float kBloodthirstDamagePerStack = 0.05f;
constexpr float kBloodthirstDamageTakenPerStack = 0.03f;

void MarkStatsDirty(entt::registry &registry, entt::entity entity) {
  registry.emplace_or_replace<StatsDirty>(entity);
}

bool IsBloodthirstResource(const BladeResourceComponent *resource) {
  return resource != nullptr && resource->kind == BladeResourceKind::Bloodthirst;
}

int GetBloodthirstStacks(const BladeResourceComponent *resource) {
  return IsBloodthirstResource(resource)
             ? std::clamp(resource->current, 0, resource->max)
             : 0;
}

void CopyResourceState(SwordIntentComponent &intent,
                       const BladeResourceComponent &resource) {
  intent.stacks = resource.current;
  intent.max_stacks = resource.max;
  intent.time_since_last_gain = resource.time_since_last_gain;
  intent.grace_period = resource.grace_period;
  intent.decay_tick_timer = resource.decay_tick_timer;
  intent.decay_interval = resource.decay_interval;
  intent.hit_tracking = resource.hit_tracking;
}

void UpdateBladeResourceTimers(BladeResourceComponent &resource, float dt) {
  if (resource.current <= 0) {
    resource.time_since_last_gain = 0.0f;
    return;
  }

  resource.time_since_last_gain += dt;
  if (resource.time_since_last_gain >= resource.grace_period) {
    resource.current = 0;
    resource.time_since_last_gain = 0.0f;
    resource.decay_tick_timer = 0.0f;
  }
}

void CleanupTracking(std::unordered_map<uint64_t, BladeResourceHitTracking> &tracking,
                     float now) {
  for (auto it = tracking.begin(); it != tracking.end();) {
    if ((now - it->second.last_gain_time) > 10.0f) {
      it = tracking.erase(it);
    } else {
      ++it;
    }
  }
}

} // namespace

bool BladeResourceService::HasBladeResource(const entt::registry &registry,
                                            entt::entity entity) {
  return registry.all_of<BladeResourceComponent>(entity);
}

BladeResourceKind BladeResourceService::GetResourceKind(
    const entt::registry &registry, entt::entity entity) {
  if (const auto *resource = registry.try_get<BladeResourceComponent>(entity)) {
    return resource->kind;
  }
  if (registry.all_of<SwordIntentComponent>(entity)) {
    return BladeResourceKind::SwordIntent;
  }
  return BladeResourceKind::None;
}

void BladeResourceService::EnsureBladeResource(entt::registry &registry,
                                               entt::entity entity,
                                               BladeResourceKind kind,
                                               int max_resource,
                                               float grace_period,
                                               float decay_interval) {
  auto &resource = registry.get_or_emplace<BladeResourceComponent>(entity);
  resource.kind = kind;
  resource.max = std::max(0, max_resource);
  resource.current = std::clamp(resource.current, 0, resource.max);
  resource.grace_period = grace_period;
  resource.decay_interval = decay_interval;
  SyncLegacySwordIntent(registry, entity);
}

void BladeResourceService::RemoveBladeResource(entt::registry &registry,
                                               entt::entity entity) {
  if (registry.all_of<BladeResourceComponent>(entity)) {
    registry.remove<BladeResourceComponent>(entity);
  }
  if (registry.all_of<SwordIntentComponent>(entity)) {
    registry.remove<SwordIntentComponent>(entity);
  }
}

void BladeResourceService::SetMaxResource(entt::registry &registry,
                                          entt::entity entity,
                                          int max_resource) {
  auto *resource = registry.try_get<BladeResourceComponent>(entity);
  if (resource == nullptr) {
    return;
  }

  resource->max = std::max(0, max_resource);
  resource->current = std::clamp(resource->current, 0, resource->max);
  SyncLegacySwordIntent(registry, entity);
}

bool BladeResourceService::Gain(entt::registry &registry, entt::entity entity,
                                int amount, uint32_t source_skill_id) {
  (void)source_skill_id;
  auto *resource = registry.try_get<BladeResourceComponent>(entity);
  if (resource == nullptr || amount <= 0) {
    return false;
  }

  const int previous = resource->current;
  resource->current = std::clamp(resource->current + amount, 0, resource->max);
  resource->time_since_last_gain = 0.0f;
  resource->decay_tick_timer = 0.0f;
  SyncLegacySwordIntent(registry, entity);

  if (resource->current != previous) {
    MarkStatsDirty(registry, entity);
    return true;
  }
  return false;
}

bool BladeResourceService::Consume(entt::registry &registry, entt::entity entity,
                                   int amount, uint32_t source_skill_id) {
  auto *resource = registry.try_get<BladeResourceComponent>(entity);
  if (resource == nullptr || amount <= 0 || resource->current < amount) {
    return false;
  }

  const bool armRestartWindow =
      resource->kind == BladeResourceKind::SwordFlow && resource->current >= 10 &&
      amount == resource->current;

  resource->current -= amount;
  resource->time_since_last_gain = 0.0f;
  resource->decay_tick_timer = 0.0f;
  if (armRestartWindow) {
    resource->restart_window_timer = 3.0f;
    resource->restart_window_ready = true;
  }
  SyncLegacySwordIntent(registry, entity);
  MarkStatsDirty(registry, entity);

  CombatEventDispatcher::Dispatch(
      registry, CombatEventFactory::CreateResourceConsumed(
                    entity, Tag::SwordSkill, amount, source_skill_id));
  return true;
}

int BladeResourceService::ConsumeUpTo(entt::registry &registry, entt::entity entity,
                                      int amount, uint32_t source_skill_id) {
  auto *resource = registry.try_get<BladeResourceComponent>(entity);
  if (resource == nullptr || amount <= 0 || resource->current <= 0) {
    return 0;
  }

  const int spend = std::min(amount, resource->current);
  return Consume(registry, entity, spend, source_skill_id) ? spend : 0;
}

bool BladeResourceService::TryGrantSwordFlowCritBonus(entt::registry &registry,
                                                      entt::entity entity,
                                                      uint32_t source_skill_id,
                                                      float current_time,
                                                      float proc_roll) {
  auto *resource = registry.try_get<BladeResourceComponent>(entity);
  if (resource == nullptr || resource->kind != BladeResourceKind::SwordFlow) {
    return false;
  }

  if ((current_time - resource->last_crit_bonus_time) < 0.2f) {
    return false;
  }
  if (proc_roll > 0.2f) {
    return false;
  }

  const bool gained = Gain(registry, entity, 1, source_skill_id);
  if (gained) {
    resource->last_crit_bonus_time = current_time;
    resource->crit_bonus_feedback_timer = 0.8f;
  }
  return gained;
}

bool BladeResourceService::IsDemonBladeActive(const entt::registry &registry,
                                              entt::entity entity) {
  const auto *mastery = registry.try_get<BladeMasteryComponent>(entity);
  const auto *resource = registry.try_get<BladeResourceComponent>(entity);
  return mastery != nullptr && mastery->selected == BladeMasteryId::DemonBlade &&
         mastery->blood_oath_active && IsBloodthirstResource(resource);
}

bool BladeResourceService::TrySpendLifeForDemonBladeCast(
    entt::registry &registry, entt::entity entity, float mana_cost,
    uint32_t source_skill_id) {
  auto *stats = registry.try_get<CombatStats>(entity);
  if (stats == nullptr || !IsDemonBladeActive(registry, entity) ||
      mana_cost <= 0.0f) {
    return false;
  }

  const float life_cost = std::max(1.0f, mana_cost * kDemonBladeHealthCostMultiplier);
  if (stats->health <= life_cost) {
    return false;
  }

  stats->health = std::max(1.0f, stats->health - life_cost);
  MarkStatsDirty(registry, entity);
  (void)Gain(registry, entity, 1, source_skill_id);
  return true;
}

bool BladeResourceService::TryGainBloodthirstOnLowLifeMeleeHit(
    entt::registry &registry, entt::entity entity, uint64_t tracking_key,
    float current_time, uint32_t source_skill_id) {
  auto *stats = registry.try_get<CombatStats>(entity);
  auto *resource = registry.try_get<BladeResourceComponent>(entity);
  if (stats == nullptr || !IsBloodthirstResource(resource) ||
      stats->max_health <= 0.0f ||
      (stats->health / stats->max_health) > kDemonBladeLowLifeThreshold) {
    return false;
  }

  auto &tracking = resource->hit_tracking[tracking_key != 0 ? tracking_key : source_skill_id];
  if (tracking.stacks_gained > 0) {
    return false;
  }

  tracking.last_gain_time = current_time;
  tracking.stacks_gained = 1;
  return Gain(registry, entity, 1, source_skill_id);
}

bool BladeResourceService::TryGainBloodthirstFromOverflowHeal(
    entt::registry &registry, entt::entity entity, float attempted_heal,
    float actual_heal, uint32_t source_skill_id) {
  auto *resource = registry.try_get<BladeResourceComponent>(entity);
  if (!IsBloodthirstResource(resource)) {
    return false;
  }

  const float overflow = attempted_heal - actual_heal;
  if (overflow < 5.0f) {
    return false;
  }

  return Gain(registry, entity, 1, source_skill_id);
}

int BladeResourceService::ConsumeAll(entt::registry &registry, entt::entity entity,
                                     uint32_t source_skill_id) {
  auto *resource = registry.try_get<BladeResourceComponent>(entity);
  if (resource == nullptr || resource->current <= 0) {
    return 0;
  }
  const int current = resource->current;
  return Consume(registry, entity, current, source_skill_id) ? current : 0;
}

float BladeResourceService::GetBloodthirstDamageMultiplier(
    const entt::registry &registry, entt::entity entity) {
  const auto *resource = registry.try_get<BladeResourceComponent>(entity);
  return 1.0f + static_cast<float>(GetBloodthirstStacks(resource)) *
                    kBloodthirstDamagePerStack;
}

float BladeResourceService::GetBloodthirstDamageTakenMultiplier(
    const entt::registry &registry, entt::entity entity) {
  const auto *resource = registry.try_get<BladeResourceComponent>(entity);
  return 1.0f + static_cast<float>(GetBloodthirstStacks(resource)) *
                    kBloodthirstDamageTakenPerStack;
}

bool BladeResourceService::TryConsumeSwordFlowRestartWindow(
    entt::registry &registry, entt::entity entity, uint32_t source_skill_id) {
  auto *resource = registry.try_get<BladeResourceComponent>(entity);
  if (resource == nullptr || resource->kind != BladeResourceKind::SwordFlow ||
      !resource->restart_window_ready || resource->restart_window_timer <= 0.0f) {
    return false;
  }

  resource->restart_window_ready = false;
  resource->restart_window_timer = 0.0f;
  return Gain(registry, entity, 2, source_skill_id);
}

void BladeResourceService::Update(entt::registry &registry, float dt) {
  auto view = registry.view<BladeResourceComponent>();
  for (const entt::entity entity : view) {
    auto &resource = view.get<BladeResourceComponent>(entity);
    const int previous = resource.current;
    resource.crit_bonus_feedback_timer =
        std::max(0.0f, resource.crit_bonus_feedback_timer - dt);
    resource.restart_window_timer =
        std::max(0.0f, resource.restart_window_timer - dt);
    if (resource.restart_window_timer <= 0.0f) {
      resource.restart_window_ready = false;
    }
    UpdateBladeResourceTimers(resource, dt);
    CleanupTracking(resource.hit_tracking, resource.time_since_last_gain);
    if (resource.current != previous) {
      MarkStatsDirty(registry, entity);
    }
    SyncLegacySwordIntent(registry, entity);
  }
}

bool BladeResourceService::ShouldAutoEmpowerOnCast(
    const entt::registry &registry, entt::entity entity) {
  const auto *resource = registry.try_get<BladeResourceComponent>(entity);
  if (resource == nullptr) {
    return registry.all_of<SwordIntentComponent>(entity);
  }
  switch (resource->kind) {
  case BladeResourceKind::SwordIntent:
  case BladeResourceKind::SwordFlow:
  case BladeResourceKind::SpiritBladeTier:
  case BladeResourceKind::Bloodthirst:
    return true;
  case BladeResourceKind::None:
  default:
    return false;
  }
}

BladeAttunement BladeResourceService::GetHeavenlyAttunement(
    const entt::registry &registry, entt::entity entity) {
  const auto *mastery = registry.try_get<BladeMasteryComponent>(entity);
  if (mastery == nullptr || mastery->selected != BladeMasteryId::HeavenlySword) {
    return BladeAttunement::None;
  }
  return mastery->heavenly_attunement;
}

Tag BladeResourceService::GetHeavenlyAttunementElementTag(
    const entt::registry &registry, entt::entity entity) {
  switch (GetHeavenlyAttunement(registry, entity)) {
  case BladeAttunement::Lightning:
    return Tag::Lightning;
  case BladeAttunement::Frost:
    return Tag::Cold;
  case BladeAttunement::Fire:
    return Tag::Fire;
  case BladeAttunement::None:
  default:
    return Tag::None;
  }
}

void BladeResourceService::SyncLegacySwordIntent(entt::registry &registry,
                                                 entt::entity entity) {
  const auto *resource = registry.try_get<BladeResourceComponent>(entity);
  if (resource == nullptr || resource->kind == BladeResourceKind::None) {
    if (registry.all_of<SwordIntentComponent>(entity)) {
      registry.remove<SwordIntentComponent>(entity);
    }
    return;
  }

  auto &intent = registry.get_or_emplace<SwordIntentComponent>(entity);
  CopyResourceState(intent, *resource);
}

} // namespace NoMoreDay::systems
