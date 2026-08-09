#include "TestCommon.hpp"

#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "game/contracts/impl/ProcBudgetManager.hpp"
#include <array>

namespace NoMoreDay {

TEST_CASE("[Integration] ProcBudget - high-speed multi-summon hit gain bounded") {
  TestSetupScope scope;
  entt::registry registry;

  auto &manager = ProcBudgetManager::Get();
  ProcBudgetConfig config;
  config.life_on_hit_per_sec = 24.0f;
  config.mana_on_hit_per_sec = 30.0f;
  config.ailment_proc_per_sec = 100.0f;
  config.trigger_proc_per_sec = 100.0f;
  config.event_emit_per_frame = 2000;
  manager.SetConfigForTests(config);
  manager.BeginFrame(0.0f);

  const auto owner = registry.create();
  auto &stats = registry.emplace<CombatStats>(owner);
  stats.life_on_hit = 6.0f;
  stats.mana_on_hit = 5.0f;
  stats.mana = 0.0f;
  stats.max_mana = 500.0f;
  registry.emplace<HealthComponent>(owner, 120.0f, 500.0f);

  const auto target = registry.create();
  registry.emplace<HealthComponent>(target, 500.0f, 500.0f);

  std::array<entt::entity, 12> summons{};
  for (auto &summon : summons) {
    summon = registry.create();
  }

  for (int burst = 0; burst < 6; ++burst) {
    for (const auto summon : summons) {
      auto evt = CombatEventFactory::CreateSkillHit(
          owner, target, 6, Tag::Hit | Tag::Melee, false);
      CombatEventFactory::AttachSummonAttribution(evt, owner, summon, 6);
      CombatEventDispatcher::Dispatch(registry, evt);
    }
  }

  const auto &ownerHealth = registry.get<HealthComponent>(owner);
  CHECK(stats.mana <= doctest::Approx(30.0f).epsilon(0.001f));
  CHECK(ownerHealth.current <= doctest::Approx(144.0f).epsilon(0.001f));
  CHECK(stats.mana > 0.0f);
  CHECK(ownerHealth.current > 120.0f);

  const float manaAfterFirstFrame = stats.mana;
  const float hpAfterFirstFrame = ownerHealth.current;

  manager.BeginFrame(1.0f);
  auto evt = CombatEventFactory::CreateSkillHit(
      owner, target, 6, Tag::Hit | Tag::Melee, false);
  CombatEventDispatcher::Dispatch(registry, evt);

  CHECK(stats.mana > manaAfterFirstFrame);
  CHECK(registry.get<HealthComponent>(owner).current > hpAfterFirstFrame);
}

TEST_CASE("[Integration] ProcBudget - dispatcher event cap clamps frame fan-out") {
  TestSetupScope scope;
  entt::registry registry;

  auto &manager = ProcBudgetManager::Get();
  ProcBudgetConfig config;
  config.life_on_hit_per_sec = 1000.0f;
  config.mana_on_hit_per_sec = 1000.0f;
  config.ailment_proc_per_sec = 1000.0f;
  config.trigger_proc_per_sec = 1000.0f;
  config.event_emit_per_frame = 5;
  manager.SetConfigForTests(config);
  manager.BeginFrame(0.0f);

  int processed = 0;
  CombatEventDispatcher::Register(
      CombatEventType::OnSkillHit,
      [&processed](entt::registry &, const CombatEvent &) { ++processed; },
      200);

  const auto source = registry.create();
  registry.emplace<CombatStats>(source);
  const auto target = registry.create();

  for (int i = 0; i < 20; ++i) {
    CombatEventDispatcher::Dispatch(
        registry, CombatEventFactory::CreateSkillHit(
                      source, target, 1, Tag::Hit | Tag::Melee, false));
  }

  CHECK(processed == 5);
}

} // namespace NoMoreDay
