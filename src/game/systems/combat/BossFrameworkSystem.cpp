#include "game/systems/combat/BossFrameworkSystem.hpp"

#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Stats.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace NoMoreDay::systems {
namespace {

uint64_t g_bossFrameworkFrame = 0u;

constexpr float kMinCounterWindowSeconds = 0.016f;
constexpr float kPhaseEpsilon = 0.0001f;

AIType MapBehaviorMode(BossBehaviorMode mode) {
  switch (mode) {
  case BossBehaviorMode::Passive:
    return AIType::IDLE;
  case BossBehaviorMode::Chase:
    return AIType::CHASE;
  case BossBehaviorMode::Burst:
    return AIType::ATTACK;
  case BossBehaviorMode::Frenzy:
    return AIType::NEMESIS_HUNTER;
  default:
    return AIType::CHASE;
  }
}

void EmitCounterEvent(entt::registry &registry, entt::entity boss,
                      BossCounterEventType eventType) {
  auto &hook = registry.get_or_emplace<BossCounterHookComponent>(boss);
  hook.last_event = eventType;
  hook.last_event_frame = g_bossFrameworkFrame;
  if (eventType == BossCounterEventType::Success) {
    ++hook.success_count;
  } else if (eventType == BossCounterEventType::Failure) {
    ++hook.failure_count;
  } else if (eventType == BossCounterEventType::Timeout) {
    ++hook.timeout_count;
  }
}

float ResolveAilmentMultiplier(const BossAilmentProfileComponent *profile,
                               AilmentType ailment) {
  if (!profile || ailment == AilmentType::None) {
    return 1.0f;
  }
  const size_t idx = static_cast<size_t>(ailment);
  if (idx >= profile->multipliers.size()) {
    return 1.0f;
  }
  return std::max(0.0f, profile->multipliers[idx]);
}

uint8_t ResolvePhaseIndex(const BossBattleComponent &boss, float hpRatio) {
  if (boss.phase_count == 0u) {
    return 0u;
  }

  uint8_t resolved = 0u;
  for (uint8_t i = 0u; i < boss.phase_count; ++i) {
    const float threshold = std::clamp(boss.phases[i].enter_hp_ratio, 0.0f, 1.0f);
    if (hpRatio <= threshold + kPhaseEpsilon) {
      resolved = i;
    }
  }
  return resolved;
}

void ApplyFailurePenalty(entt::registry &registry, entt::entity boss,
                         const BossCounterWindowComponent &counterWindow);

void ApplyPhase(entt::registry &registry, entt::entity boss,
                BossBattleComponent &battle, uint8_t phaseIndex) {
  if (phaseIndex >= battle.phase_count) {
    return;
  }

  const BossPhaseConfig &phase = battle.phases[phaseIndex];

  if (auto *ai = registry.try_get<AIComponent>(boss)) {
    ai->aiType = MapBehaviorMode(phase.behavior_mode);
  }

  auto &ailmentProfile = registry.get_or_emplace<BossAilmentProfileComponent>(boss);
  ailmentProfile.multipliers = phase.ailment_multipliers;
  ailmentProfile.source_phase = phaseIndex;

  if (phase.enable_counter_window && phase.counter_window_duration > 0.0f) {
    BossFrameworkSystem::OpenCounterWindow(registry, boss,
                                           phase.counter_window_duration,
                                           phase.expected_counter_action);
  }
}

void ApplyFailurePenalty(entt::registry &registry, entt::entity boss,
                         const BossCounterWindowComponent &counterWindow) {
  const auto *penalty = registry.try_get<BossFailurePenaltyComponent>(boss);
  if (!penalty || penalty->type == BossFailurePenaltyType::None) {
    return;
  }

  auto &runtime = registry.get_or_emplace<BossFailurePenaltyRuntimeComponent>(boss);

  switch (penalty->type) {
  case BossFailurePenaltyType::Retry:
    if (runtime.retries_used < penalty->max_retries &&
        counterWindow.duration > 0.0f) {
      ++runtime.retries_used;
      BossFrameworkSystem::OpenCounterWindow(
          registry, boss, counterWindow.duration, counterWindow.expected_action);
    }
    break;
  case BossFailurePenaltyType::Weaken: {
    const float weaken = std::clamp(penalty->weaken_amount, 0.0f, 0.95f);
    runtime.weaken_accumulated += weaken;
    if (auto *stats = registry.try_get<CombatStats>(boss)) {
      stats->attack_speed = std::max(0.1f, stats->attack_speed * (1.0f - weaken));
      for (float &multiplier : stats->damage_multipliers) {
        multiplier = std::max(0.05f, multiplier * (1.0f - weaken));
      }
    }
    break;
  }
  case BossFailurePenaltyType::Teleport:
    runtime.pending_player_teleport = true;
    runtime.pending_player_teleport_target = penalty->teleport_target;
    break;
  case BossFailurePenaltyType::None:
    break;
  }
}

} // namespace

void BossFrameworkSystem::Update(entt::registry &registry, float dt) {
  ++g_bossFrameworkFrame;
  const float clampedDt = std::max(0.0f, dt);

  std::vector<entt::entity> missingBossFramework;
  auto rarityView =
      registry.view<EnemyRarityComponent>(entt::exclude<BossBattleComponent>);
  for (auto entity : rarityView) {
    const auto &rarity = rarityView.get<EnemyRarityComponent>(entity);
    if (rarity.rarity == EnemyRarityComponent::BOSS) {
      missingBossFramework.push_back(entity);
    }
  }
  for (entt::entity boss : missingBossFramework) {
    AttachPrototype(registry, boss);
  }

  auto bossView = registry.view<BossBattleComponent, HealthComponent>();
  for (auto boss : bossView) {
    auto &battle = bossView.get<BossBattleComponent>(boss);
    auto &health = bossView.get<HealthComponent>(boss);
    battle.phase_changed_this_frame = false;
    if (battle.phase_count == 0u || health.max <= 0.0f) {
      continue;
    }

    const float hpRatio = std::clamp(health.current / health.max, 0.0f, 1.0f);
    const uint8_t resolvedPhase = ResolvePhaseIndex(battle, hpRatio);

    if (!battle.has_initialized) {
      battle.current_phase = resolvedPhase;
      battle.has_initialized = true;
      battle.phase_changed_this_frame = true;
      ApplyPhase(registry, boss, battle, resolvedPhase);
      continue;
    }

    if (resolvedPhase != battle.current_phase) {
      battle.current_phase = resolvedPhase;
      battle.phase_changed_this_frame = true;
      ApplyPhase(registry, boss, battle, resolvedPhase);
    }
  }

  auto counterView = registry.view<BossCounterWindowComponent>();
  for (auto boss : counterView) {
    auto &counter = counterView.get<BossCounterWindowComponent>(boss);
    if (!counter.active) {
      continue;
    }

    counter.remaining -= clampedDt;
    if (counter.remaining <= 0.0f) {
      counter.remaining = 0.0f;
      counter.active = false;
      counter.closed_frame = g_bossFrameworkFrame;
      EmitCounterEvent(registry, boss, BossCounterEventType::Timeout);
      ApplyFailurePenalty(registry, boss, counter);
    }
  }
}

void BossFrameworkSystem::AttachPrototype(entt::registry &registry,
                                          entt::entity boss) {
  if (!registry.valid(boss)) {
    return;
  }

  auto &battle = registry.get_or_emplace<BossBattleComponent>(boss);
  battle.phase_count = 3u;
  battle.current_phase = 0u;
  battle.has_initialized = false;
  battle.phase_changed_this_frame = false;

  battle.phases[0] = {};
  battle.phases[0].enter_hp_ratio = 1.0f;
  battle.phases[0].behavior_mode = BossBehaviorMode::Chase;
  battle.phases[0].ailment_multipliers = kBossDefaultAilmentMultipliers;
  battle.phases[0].enable_counter_window = false;
  battle.phases[0].counter_window_duration = 0.0f;
  battle.phases[0].expected_counter_action = BossCounterAction::Interrupt;

  battle.phases[1] = {};
  battle.phases[1].enter_hp_ratio = 0.70f;
  battle.phases[1].behavior_mode = BossBehaviorMode::Burst;
  battle.phases[1].ailment_multipliers = kBossDefaultAilmentMultipliers;
  battle.phases[1]
      .ailment_multipliers[static_cast<size_t>(AilmentType::Poison)] = 0.0f;
  battle.phases[1].enable_counter_window = true;
  battle.phases[1].counter_window_duration = 0.35f;
  battle.phases[1].expected_counter_action = BossCounterAction::Interrupt;

  battle.phases[2] = {};
  battle.phases[2].enter_hp_ratio = 0.35f;
  battle.phases[2].behavior_mode = BossBehaviorMode::Frenzy;
  battle.phases[2].ailment_multipliers = kBossDefaultAilmentMultipliers;
  battle.phases[2]
      .ailment_multipliers[static_cast<size_t>(AilmentType::Poison)] = 0.5f;
  battle.phases[2].enable_counter_window = true;
  battle.phases[2].counter_window_duration = 0.25f;
  battle.phases[2].expected_counter_action = BossCounterAction::PerfectDodge;

  (void)registry.get_or_emplace<BossAilmentProfileComponent>(boss);
  (void)registry.get_or_emplace<BossCounterHookComponent>(boss);

  auto &penalty = registry.get_or_emplace<BossFailurePenaltyComponent>(boss);
  penalty.type = BossFailurePenaltyType::Retry;
  penalty.max_retries = 1u;
  penalty.weaken_amount = 0.15f;
  penalty.teleport_target = {0.0f, 0.0f};
  (void)registry.get_or_emplace<BossFailurePenaltyRuntimeComponent>(boss);
}

bool BossFrameworkSystem::ApplyAilment(entt::registry &registry, entt::entity boss,
                                       const AilmentApplyRequest &request) {
  if (!registry.valid(boss) || request.ailment == AilmentType::None) {
    return false;
  }

  const auto *profile = registry.try_get<BossAilmentProfileComponent>(boss);
  const float multiplier = ResolveAilmentMultiplier(profile, request.ailment);
  if (multiplier <= 0.0f) {
    return false;
  }

  AilmentApplyRequest adjusted = request;
  adjusted.magnitude *= multiplier;
  return AilmentApplier::Apply(registry, boss, adjusted);
}

void BossFrameworkSystem::OpenCounterWindow(entt::registry &registry,
                                            entt::entity boss, float duration,
                                            BossCounterAction expectedAction) {
  if (!registry.valid(boss)) {
    return;
  }

  auto &counter = registry.get_or_emplace<BossCounterWindowComponent>(boss);
  counter.active = true;
  counter.duration = std::max(kMinCounterWindowSeconds, duration);
  counter.remaining = counter.duration;
  counter.expected_action = expectedAction;
  counter.opened_frame = g_bossFrameworkFrame;
  counter.closed_frame = 0u;

  EmitCounterEvent(registry, boss, BossCounterEventType::WindowOpened);
}

bool BossFrameworkSystem::TryResolveCounter(entt::registry &registry,
                                            entt::entity boss,
                                            BossCounterAction action) {
  if (!registry.valid(boss)) {
    return false;
  }

  auto *counter = registry.try_get<BossCounterWindowComponent>(boss);
  if (!counter || !counter->active) {
    return false;
  }

  if (action == counter->expected_action) {
    counter->active = false;
    counter->remaining = 0.0f;
    counter->closed_frame = g_bossFrameworkFrame;
    EmitCounterEvent(registry, boss, BossCounterEventType::Success);
    if (auto *ai = registry.try_get<AIComponent>(boss)) {
      ai->aiType = AIType::IDLE;
    }
    return true;
  }

  if (action != BossCounterAction::None) {
    counter->active = false;
    counter->remaining = 0.0f;
    counter->closed_frame = g_bossFrameworkFrame;
    EmitCounterEvent(registry, boss, BossCounterEventType::Failure);
    ApplyFailurePenalty(registry, boss, *counter);
  }
  return false;
}

uint64_t BossFrameworkSystem::GetFrameIndexForTests() noexcept {
  return g_bossFrameworkFrame;
}

void BossFrameworkSystem::ResetForTests() noexcept {
  g_bossFrameworkFrame = 0u;
}

} // namespace NoMoreDay::systems
