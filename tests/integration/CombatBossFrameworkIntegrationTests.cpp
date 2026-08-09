#include "TestCommon.hpp"

#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/EnemyComponent.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/systems/combat/AilmentEngine.hpp"
#include "game/systems/combat/BossFrameworkSystem.hpp"

namespace NoMoreDay {

namespace {

const BuffEffect *FindAilmentEffect(const ActiveEffectsComponent &effects,
                                    AilmentType ailment) {
  for (const auto &effect : effects.effects) {
    const auto mapped = systems::AilmentAdapter::TryMapLegacyBuff(effect);
    if (mapped && *mapped == ailment) {
      return &effect;
    }
  }
  return nullptr;
}

} // namespace

TEST_CASE("[Integration] CombatBossFramework - prototype phase flow switches behavior and ailment policy") {
  TestSetupScope scope;
  systems::BossFrameworkSystem::ResetForTests();
  auto &ailmentRegistry = systems::AilmentRegistry::Get();
  ailmentRegistry.ResetForTests();
  REQUIRE(ailmentRegistry.EnsureLoaded());

  entt::registry registry;
  const auto boss = registry.create();
  registry.emplace<EnemyRarityComponent>(boss, EnemyRarityComponent::BOSS);
  registry.emplace<AIComponent>(boss);
  registry.emplace<HealthComponent>(boss, 1000.0f, 1000.0f);
  registry.emplace<CombatStats>(boss);
  registry.emplace<Position>(boss, 0.0f, 0.0f);

  systems::BossFrameworkSystem::AttachPrototype(registry, boss);
  systems::BossFrameworkSystem::Update(registry, 1.0f / 60.0f);
  const auto &battle = registry.get<BossBattleComponent>(boss);
  REQUIRE(battle.phase_count == 3u);
  CHECK(battle.current_phase == 0u);

  registry.get<HealthComponent>(boss).current = 650.0f;
  systems::BossFrameworkSystem::Update(registry, 1.0f / 60.0f);
  CHECK(registry.get<BossBattleComponent>(boss).current_phase == 1u);
  CHECK(registry.get<AIComponent>(boss).aiType == AIType::ATTACK);

  systems::AilmentApplyRequest poison;
  poison.ailment = AilmentType::Poison;
  poison.magnitude = 12.0f;
  poison.duration = 2.0f;
  poison.stacks = 1;
  CHECK_FALSE(systems::BossFrameworkSystem::ApplyAilment(registry, boss, poison));

  registry.get<HealthComponent>(boss).current = 300.0f;
  systems::BossFrameworkSystem::Update(registry, 1.0f / 60.0f);
  CHECK(registry.get<BossBattleComponent>(boss).current_phase == 2u);
  CHECK(registry.get<AIComponent>(boss).aiType == AIType::NEMESIS_HUNTER);

  CHECK(systems::BossFrameworkSystem::ApplyAilment(registry, boss, poison));
  const auto &effects = registry.get<ActiveEffectsComponent>(boss);
  const BuffEffect *poisonEffect = FindAilmentEffect(effects, AilmentType::Poison);
  REQUIRE(poisonEffect != nullptr);
  CHECK(poisonEffect->tick_damage == doctest::Approx(6.0f).epsilon(0.0001f));

  const auto &counter = registry.get<BossCounterWindowComponent>(boss);
  CHECK(counter.active);
  CHECK(counter.expected_action == BossCounterAction::PerfectDodge);
  CHECK(systems::BossFrameworkSystem::TryResolveCounter(
      registry, boss, BossCounterAction::PerfectDodge));

  const auto &hooks = registry.get<BossCounterHookComponent>(boss);
  CHECK(hooks.success_count == 1u);
}

TEST_CASE("[Integration] CombatBossFramework - counter window timeout precision stays within one frame") {
  TestSetupScope scope;
  systems::BossFrameworkSystem::ResetForTests();

  entt::registry registry;
  const auto boss = registry.create();

  const float dt = 1.0f / 60.0f;
  systems::BossFrameworkSystem::OpenCounterWindow(registry, boss, dt * 2.0f,
                                                  BossCounterAction::Interrupt);
  uint64_t openedFrame = registry.get<BossCounterWindowComponent>(boss).opened_frame;

  systems::BossFrameworkSystem::Update(registry, dt);
  CHECK(registry.get<BossCounterWindowComponent>(boss).active);

  systems::BossFrameworkSystem::Update(registry, dt);
  const auto &counter = registry.get<BossCounterWindowComponent>(boss);
  CHECK_FALSE(counter.active);

  const uint64_t expectedFrames = 2u;
  const uint64_t elapsed = counter.closed_frame - openedFrame;
  CHECK(elapsed >= expectedFrames - 1u);
  CHECK(elapsed <= expectedFrames + 1u);

  const auto &hooks = registry.get<BossCounterHookComponent>(boss);
  CHECK(hooks.timeout_count == 1u);
}

TEST_CASE("[Integration] CombatBossFramework - retry failure penalty is configurable and bounded") {
  TestSetupScope scope;
  systems::BossFrameworkSystem::ResetForTests();

  entt::registry registry;
  const auto boss = registry.create();
  auto &penalty = registry.emplace<BossFailurePenaltyComponent>(boss);
  penalty.type = BossFailurePenaltyType::Retry;
  penalty.max_retries = 1u;

  const float dt = 1.0f / 60.0f;
  systems::BossFrameworkSystem::OpenCounterWindow(registry, boss, dt,
                                                  BossCounterAction::Interrupt);

  systems::BossFrameworkSystem::Update(registry, dt);
  const auto &runtimeAfterFirst =
      registry.get<BossFailurePenaltyRuntimeComponent>(boss);
  CHECK(runtimeAfterFirst.retries_used == 1u);
  CHECK(registry.get<BossCounterWindowComponent>(boss).active);

  systems::BossFrameworkSystem::Update(registry, dt);
  CHECK_FALSE(registry.get<BossCounterWindowComponent>(boss).active);
  const auto &runtimeAfterSecond =
      registry.get<BossFailurePenaltyRuntimeComponent>(boss);
  CHECK(runtimeAfterSecond.retries_used == 1u);

  const auto &hooks = registry.get<BossCounterHookComponent>(boss);
  CHECK(hooks.timeout_count == 2u);
}

} // namespace NoMoreDay
