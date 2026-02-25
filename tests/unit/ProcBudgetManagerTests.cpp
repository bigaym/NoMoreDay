#include "TestCommon.hpp"

#include "game/systems/combat/ProcBudgetManager.hpp"

namespace NoMoreDay {

TEST_CASE("[Unit] ProcBudgetManager - token budget enforces thresholds") {
  TestSetupScope scope;

  auto &manager = ProcBudgetManager::Get();
  ProcBudgetConfig config;
  config.life_on_hit_per_sec = 10.0f;
  config.mana_on_hit_per_sec = 10.0f;
  config.ailment_proc_per_sec = 10.0f;
  config.trigger_proc_per_sec = 2.0f;
  config.event_emit_per_frame = 128;
  manager.SetConfigForTests(config);

  const entt::entity owner = static_cast<entt::entity>(1u);
  manager.BeginFrame(0.0f);

  CHECK(manager.RequestProc(owner, ProcBudgetType::TriggerProc, 1.0f));
  CHECK(manager.RequestProc(owner, ProcBudgetType::TriggerProc, 1.0f));
  CHECK_FALSE(manager.RequestProc(owner, ProcBudgetType::TriggerProc, 1.0f));

  CHECK(manager.RequestProc(owner, ProcBudgetType::LifeOnHit, 6.0f));
  CHECK_FALSE(manager.RequestProc(owner, ProcBudgetType::LifeOnHit, 6.0f));
}

TEST_CASE("[Unit] ProcBudgetManager - deterministic downsampling over refill") {
  TestSetupScope scope;

  auto &manager = ProcBudgetManager::Get();
  ProcBudgetConfig config;
  config.life_on_hit_per_sec = 100.0f;
  config.mana_on_hit_per_sec = 100.0f;
  config.ailment_proc_per_sec = 100.0f;
  config.trigger_proc_per_sec = 2.0f;
  config.event_emit_per_frame = 1024;
  manager.SetConfigForTests(config);

  const entt::entity owner = static_cast<entt::entity>(2u);
  manager.BeginFrame(0.0f);
  CHECK(manager.RequestProc(owner, ProcBudgetType::TriggerProc, 1.0f));
  CHECK(manager.RequestProc(owner, ProcBudgetType::TriggerProc, 1.0f));
  CHECK_FALSE(manager.RequestProc(owner, ProcBudgetType::TriggerProc, 1.0f));
  CHECK_FALSE(manager.RequestProc(owner, ProcBudgetType::TriggerProc, 1.0f));

  manager.BeginFrame(0.5f);
  CHECK(manager.RequestProc(owner, ProcBudgetType::TriggerProc, 1.0f));
  CHECK_FALSE(manager.RequestProc(owner, ProcBudgetType::TriggerProc, 1.0f));

  manager.BeginFrame(0.5f);
  CHECK(manager.RequestProc(owner, ProcBudgetType::TriggerProc, 1.0f));
}

TEST_CASE("[Unit] ProcBudgetManager - event cap resets every frame") {
  TestSetupScope scope;

  auto &manager = ProcBudgetManager::Get();
  ProcBudgetConfig config;
  config.life_on_hit_per_sec = 100.0f;
  config.mana_on_hit_per_sec = 100.0f;
  config.ailment_proc_per_sec = 100.0f;
  config.trigger_proc_per_sec = 100.0f;
  config.event_emit_per_frame = 3;
  manager.SetConfigForTests(config);

  manager.BeginFrame(0.0f);
  CHECK(manager.RequestEventEmit());
  CHECK(manager.RequestEventEmit());
  CHECK(manager.RequestEventEmit());
  CHECK_FALSE(manager.RequestEventEmit());

  manager.BeginFrame(0.016f);
  CHECK(manager.RequestEventEmit());
}

} // namespace NoMoreDay
