#include "TestCommon.hpp"

#include "game/components/PlayerState.hpp"
#include "game/components/Progression.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/data/BladeMasteryRegistry.hpp"
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

TEST_CASE("[Unit] Blade Mastery Registry - Sword Saint profile loads") {
  TestSetupScope testScope;
  LoadBladeMasteries();

  const BladeMasteryProfile *profile =
      BladeMasteryRegistry::Get().GetProfile(BladeMasteryId::SwordSaint);
  REQUIRE(profile != nullptr);
  CHECK(profile->profession == ProfessionID::BladeAscendant);
  CHECK(profile->resource_kind == BladeResourceKind::SwordFlow);
  CHECK(profile->unlock_level == 50);
  CHECK(profile->debug_unlock_level_override == 5);
  CHECK(profile->signature_skill_id == 10);
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
