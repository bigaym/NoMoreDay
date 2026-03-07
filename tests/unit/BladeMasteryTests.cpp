#include "TestCommon.hpp"

#include "game/components/PlayerState.hpp"
#include "game/components/Progression.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/data/BladeMasteryRegistry.hpp"
#include "game/systems/combat/StatsSystem.hpp"
#include "game/systems/skill/BladeMasteryService.hpp"
#include "game/systems/skill/BladeResourceService.hpp"

using namespace NoMoreDay;
using namespace NoMoreDay::components;
using namespace NoMoreDay::data;
using namespace NoMoreDay::systems;

namespace {

void LoadBladeMasteries() {
  REQUIRE(
      BladeMasteryRegistry::Get().LoadFromJson("assets/data/blade_masteries.json"));
}

entt::entity CreateBladeAscendant(entt::registry &registry, int level) {
  const entt::entity player = registry.create();

  auto &stats = registry.emplace<PlayerStats>(player);
  stats.level = level;

  registry.emplace<CombatStats>(player);

  auto &astrolabe = registry.emplace<AstrolabeComponent>(player);
  astrolabe.mainProfession = static_cast<int>(ProfessionID::BladeAscendant);
  return player;
}

} // namespace

TEST_CASE("[Unit] Blade Mastery Registry - all Blade Ascendant profiles load") {
  TestSetupScope testScope;
  LoadBladeMasteries();

  const auto &profiles = BladeMasteryRegistry::Get().GetAllProfiles();
  REQUIRE(profiles.size() == 3);

  const BladeMasteryProfile *swordSaint =
      BladeMasteryRegistry::Get().GetProfile(BladeMasteryId::SwordSaint);
  REQUIRE(swordSaint != nullptr);
  CHECK(swordSaint->profession == ProfessionID::BladeAscendant);
  CHECK(swordSaint->resource_kind == BladeResourceKind::SwordFlow);
  CHECK(swordSaint->unlock_level == 50);
  CHECK(swordSaint->debug_unlock_level_override == 5);
  CHECK(swordSaint->signature_skill_id == 10);

  const BladeMasteryProfile *heavenlySword =
      BladeMasteryRegistry::Get().GetProfile(BladeMasteryId::HeavenlySword);
  REQUIRE(heavenlySword != nullptr);
  CHECK(heavenlySword->resource_kind == BladeResourceKind::SpiritBladeTier);
  CHECK(heavenlySword->signature_skill_id == 11);
  CHECK(heavenlySword->max_resource == 10);

  const BladeMasteryProfile *demonBlade =
      BladeMasteryRegistry::Get().GetProfile(BladeMasteryId::DemonBlade);
  REQUIRE(demonBlade != nullptr);
  CHECK(demonBlade->resource_kind == BladeResourceKind::Bloodthirst);
  CHECK(demonBlade->signature_skill_id == 12);
  CHECK(demonBlade->max_resource == 10);
}

TEST_CASE(
    "[Unit] Blade Mastery Service - formal unlock and debug override selection") {
  TestSetupScope testScope;
  LoadBladeMasteries();

  entt::registry registry;
  const entt::entity player49 = CreateBladeAscendant(registry, 49);
  const entt::entity player50 = CreateBladeAscendant(registry, 50);
  const entt::entity player5 = CreateBladeAscendant(registry, 5);
  const entt::entity player4 = CreateBladeAscendant(registry, 4);

  const bool previousDebugOverride =
      BladeMasteryService::IsDebugUnlockOverrideEnabled();
  BladeMasteryService::SetDebugUnlockOverrideEnabled(false);

  BladeMasteryService::RefreshPlayerState(registry, player49);
  BladeMasteryService::RefreshPlayerState(registry, player50);
  BladeMasteryService::RefreshPlayerState(registry, player5);
  BladeMasteryService::RefreshPlayerState(registry, player4);

  CHECK_FALSE(BladeMasteryService::IsMasteryUnlocked(
      registry, player49, BladeMasteryId::SwordSaint));
  CHECK(BladeMasteryService::IsMasteryUnlocked(registry, player50,
                                              BladeMasteryId::SwordSaint));
  CHECK_FALSE(BladeMasteryService::IsMasteryUnlocked(
      registry, player5, BladeMasteryId::SwordSaint));

  BladeMasteryService::SetDebugUnlockOverrideEnabled(true);
  CHECK(BladeMasteryService::IsMasteryUnlocked(registry, player5,
                                              BladeMasteryId::SwordSaint));
  CHECK_FALSE(BladeMasteryService::IsMasteryUnlocked(
      registry, player4, BladeMasteryId::SwordSaint));
  CHECK(BladeMasteryService::SelectMastery(registry, player5,
                                           BladeMasteryId::SwordSaint));

  const auto &mastery = registry.get<BladeMasteryComponent>(player5);
  const auto &resource = registry.get<BladeResourceComponent>(player5);
  const auto &signature = registry.get<BladeSignatureSkillComponent>(player5);

  CHECK(mastery.selected == BladeMasteryId::SwordSaint);
  CHECK(resource.kind == BladeResourceKind::SwordFlow);
  CHECK(signature.skill_id == 10);
  CHECK(signature.unlocked);

  BladeMasteryService::SetDebugUnlockOverrideEnabled(previousDebugOverride);
}

TEST_CASE("[Unit] Blade Resource Service - mirrored legacy sword intent") {
  TestSetupScope testScope;
  LoadBladeMasteries();

  entt::registry registry;
  const entt::entity player = CreateBladeAscendant(registry, 50);

  const bool previousDebugOverride =
      BladeMasteryService::IsDebugUnlockOverrideEnabled();
  BladeMasteryService::SetDebugUnlockOverrideEnabled(true);

  BladeMasteryService::RefreshPlayerState(registry, player);
  REQUIRE(BladeMasteryService::SelectMastery(registry, player,
                                             BladeMasteryId::SwordSaint));

  CHECK(BladeResourceService::Gain(registry, player, 3, 1u));
  REQUIRE(registry.all_of<BladeResourceComponent, SwordIntentComponent>(player));

  const auto &resource = registry.get<BladeResourceComponent>(player);
  const auto &intent = registry.get<SwordIntentComponent>(player);
  CHECK(resource.kind == BladeResourceKind::SwordFlow);
  CHECK(resource.current == 3);
  CHECK(intent.stacks == 3);
  CHECK(intent.max_stacks == resource.max);

  CHECK(BladeResourceService::Consume(registry, player, 2, 2u));
  CHECK(registry.get<BladeResourceComponent>(player).current == 1);
  CHECK(registry.get<SwordIntentComponent>(player).stacks == 1);

  BladeResourceService::Update(registry, 6.0f);
  CHECK(registry.get<BladeResourceComponent>(player).current == 0);
  CHECK(registry.get<SwordIntentComponent>(player).stacks == 0);

  BladeMasteryService::SetDebugUnlockOverrideEnabled(previousDebugOverride);
}

TEST_CASE(
    "[Unit] Blade Mastery Service - Heavenly Sword and Demon Blade shared state") {
  TestSetupScope testScope;
  LoadBladeMasteries();

  entt::registry registry;
  const entt::entity player = CreateBladeAscendant(registry, 50);

  const bool previousDebugOverride =
      BladeMasteryService::IsDebugUnlockOverrideEnabled();
  BladeMasteryService::SetDebugUnlockOverrideEnabled(true);

  BladeMasteryService::RefreshPlayerState(registry, player);
  REQUIRE(BladeMasteryService::SelectMastery(registry, player,
                                             BladeMasteryId::HeavenlySword));

  const auto &heavenlyMastery = registry.get<BladeMasteryComponent>(player);
  const auto &heavenlyResource = registry.get<BladeResourceComponent>(player);
  const auto &heavenlySignature =
      registry.get<BladeSignatureSkillComponent>(player);

  CHECK(heavenlyMastery.selected == BladeMasteryId::HeavenlySword);
  CHECK(heavenlyMastery.heavenly_attunement == BladeAttunement::None);
  CHECK_FALSE(heavenlyMastery.blood_oath_active);
  CHECK(heavenlyResource.kind == BladeResourceKind::SpiritBladeTier);
  CHECK(heavenlySignature.skill_id == 11);
  CHECK(heavenlySignature.unlocked);

  REQUIRE(BladeMasteryService::SelectMastery(registry, player,
                                             BladeMasteryId::DemonBlade));

  const auto &demonMastery = registry.get<BladeMasteryComponent>(player);
  const auto &demonResource = registry.get<BladeResourceComponent>(player);
  const auto &demonSignature = registry.get<BladeSignatureSkillComponent>(player);

  CHECK(demonMastery.selected == BladeMasteryId::DemonBlade);
  CHECK(demonMastery.heavenly_attunement == BladeAttunement::None);
  CHECK(demonMastery.blood_oath_active);
  CHECK(demonResource.kind == BladeResourceKind::Bloodthirst);
  CHECK(demonSignature.skill_id == 12);
  CHECK(demonSignature.unlocked);

  BladeMasteryService::SetDebugUnlockOverrideEnabled(previousDebugOverride);
}

TEST_CASE("[Unit] Sword Flow - grants Sword Saint crit and attack speed") {
  TestSetupScope testScope;
  LoadBladeMasteries();

  entt::registry registry;
  const entt::entity player = CreateBladeAscendant(registry, 50);

  const bool previousDebugOverride =
      BladeMasteryService::IsDebugUnlockOverrideEnabled();
  BladeMasteryService::SetDebugUnlockOverrideEnabled(true);

  BladeMasteryService::RefreshPlayerState(registry, player);
  REQUIRE(BladeMasteryService::SelectMastery(registry, player,
                                             BladeMasteryId::SwordSaint));

  StatsSystem::Recalculate(registry, player);
  const CombatStats baseline = registry.get<CombatStats>(player);

  REQUIRE(BladeResourceService::Gain(registry, player, 5, 10u));
  StatsSystem::Recalculate(registry, player);
  const CombatStats buffed = registry.get<CombatStats>(player);

  CHECK(buffed.crit_chance == doctest::Approx(baseline.crit_chance + 0.15f));
  CHECK(buffed.attack_speed == doctest::Approx(baseline.attack_speed * 1.10f));
  CHECK(buffed.move_speed == doctest::Approx(baseline.move_speed));

  BladeMasteryService::SetDebugUnlockOverrideEnabled(previousDebugOverride);
}

TEST_CASE("[Unit] Sword Flow - crit bonus grants extra stack with cooldown") {
  TestSetupScope testScope;
  LoadBladeMasteries();

  entt::registry registry;
  const entt::entity player = CreateBladeAscendant(registry, 50);

  const bool previousDebugOverride =
      BladeMasteryService::IsDebugUnlockOverrideEnabled();
  BladeMasteryService::SetDebugUnlockOverrideEnabled(true);

  BladeMasteryService::RefreshPlayerState(registry, player);
  REQUIRE(BladeMasteryService::SelectMastery(registry, player,
                                             BladeMasteryId::SwordSaint));
  REQUIRE(BladeResourceService::Gain(registry, player, 1, 10u));

  CHECK(BladeResourceService::TryGrantSwordFlowCritBonus(registry, player, 10u,
                                                         1.0f, 0.0f));
  CHECK(registry.get<BladeResourceComponent>(player).current == 2);
  CHECK(registry.get<BladeResourceComponent>(player).crit_bonus_feedback_timer >
        0.0f);

  CHECK_FALSE(BladeResourceService::TryGrantSwordFlowCritBonus(
      registry, player, 10u, 1.1f, 0.0f));
  CHECK(registry.get<BladeResourceComponent>(player).current == 2);

  CHECK_FALSE(BladeResourceService::TryGrantSwordFlowCritBonus(
      registry, player, 10u, 1.3f, 0.5f));
  CHECK(registry.get<BladeResourceComponent>(player).current == 2);

  CHECK(BladeResourceService::TryGrantSwordFlowCritBonus(registry, player, 10u,
                                                         1.3f, 0.1f));
  CHECK(registry.get<BladeResourceComponent>(player).current == 3);

  BladeResourceService::Update(registry, 1.0f);
  CHECK(registry.get<BladeResourceComponent>(player).crit_bonus_feedback_timer ==
        doctest::Approx(0.0f));

  BladeMasteryService::SetDebugUnlockOverrideEnabled(previousDebugOverride);
}

TEST_CASE("[Unit] Sword Flow restart - full spend arms restart window") {
  TestSetupScope testScope;
  LoadBladeMasteries();

  entt::registry registry;
  const entt::entity player = CreateBladeAscendant(registry, 50);

  const bool previousDebugOverride =
      BladeMasteryService::IsDebugUnlockOverrideEnabled();
  BladeMasteryService::SetDebugUnlockOverrideEnabled(true);

  BladeMasteryService::RefreshPlayerState(registry, player);
  REQUIRE(BladeMasteryService::SelectMastery(registry, player,
                                             BladeMasteryId::SwordSaint));
  REQUIRE(BladeResourceService::Gain(registry, player, 10, 10u));
  REQUIRE(BladeResourceService::Consume(registry, player, 10, 10u));

  const auto& resource = registry.get<BladeResourceComponent>(player);
  CHECK(resource.current == 0);
  CHECK(resource.restart_window_timer > 0.0f);
  CHECK(resource.restart_window_ready);

  BladeMasteryService::SetDebugUnlockOverrideEnabled(previousDebugOverride);
}

TEST_CASE("[Unit] Heavenly Sword resource spend - cast consumes up to capped Spirit Blade Tier") {
  TestSetupScope testScope;
  LoadBladeMasteries();

  entt::registry registry;
  const entt::entity player = CreateBladeAscendant(registry, 50);

  const bool previousDebugOverride =
      BladeMasteryService::IsDebugUnlockOverrideEnabled();
  BladeMasteryService::SetDebugUnlockOverrideEnabled(true);

  BladeMasteryService::RefreshPlayerState(registry, player);
  REQUIRE(BladeMasteryService::SelectMastery(registry, player,
                                             BladeMasteryId::HeavenlySword));

  REQUIRE(BladeResourceService::Gain(registry, player, 6, 11u));
  CHECK(BladeResourceService::ConsumeUpTo(registry, player, 5, 11u) == 5);
  CHECK(registry.get<BladeResourceComponent>(player).current == 1);

  CHECK(BladeResourceService::ConsumeUpTo(registry, player, 5, 11u) == 1);
  CHECK(registry.get<BladeResourceComponent>(player).current == 0);

  CHECK_FALSE(BladeResourceService::ShouldAutoEmpowerOnCast(registry, player));

  BladeMasteryService::SetDebugUnlockOverrideEnabled(previousDebugOverride);
}

TEST_CASE("[Unit] Demon Blade - life-spend casting grants Bloodthirst and scales stats") {
  TestSetupScope testScope;
  LoadBladeMasteries();

  entt::registry registry;
  const entt::entity player = CreateBladeAscendant(registry, 50);

  auto &combat = registry.get<CombatStats>(player);
  combat.max_health = 200.0f;
  combat.health = 200.0f;
  combat.max_mana = 100.0f;
  combat.mana = 100.0f;

  const bool previousDebugOverride =
      BladeMasteryService::IsDebugUnlockOverrideEnabled();
  BladeMasteryService::SetDebugUnlockOverrideEnabled(true);

  BladeMasteryService::RefreshPlayerState(registry, player);
  REQUIRE(BladeMasteryService::SelectMastery(registry, player,
                                             BladeMasteryId::DemonBlade));

  StatsSystem::Recalculate(registry, player);
  const CombatStats baseline = registry.get<CombatStats>(player);

  REQUIRE(BladeResourceService::TrySpendLifeForDemonBladeCast(registry, player,
                                                              12.0f, 12u));
  const auto &resource = registry.get<BladeResourceComponent>(player);
  CHECK(resource.kind == BladeResourceKind::Bloodthirst);
  CHECK(resource.current == 1);
  CHECK(registry.get<CombatStats>(player).health == doctest::Approx(176.0f));

  StatsSystem::Recalculate(registry, player);
  const CombatStats buffed = registry.get<CombatStats>(player);
  CHECK(buffed.damage_multipliers[0] == doctest::Approx(
      baseline.damage_multipliers[0] * 1.05f));
  CHECK(BladeResourceService::GetBloodthirstDamageTakenMultiplier(registry,
                                                                  player) ==
        doctest::Approx(1.03f));

  BladeMasteryService::SetDebugUnlockOverrideEnabled(previousDebugOverride);
}
