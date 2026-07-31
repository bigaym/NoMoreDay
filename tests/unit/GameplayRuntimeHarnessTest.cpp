#include "doctest.h"

#include "integration/GameplayRuntimeHarness.hpp"

#include "game/components/AIComponent.hpp"
#include "game/components/Common.hpp"
#include "game/components/EffectComponent.hpp"
#include "game/components/LightComponent.hpp"
#include "game/components/ShadowCasterComponent.hpp"

namespace {

NoMoreDay::render::validation::FixtureConfig MakeFixture(const std::string &name,
                                                          uint32_t seed) {
  NoMoreDay::render::validation::FixtureConfig cfg;
  cfg.name = name;
  cfg.sceneSeed = seed;
  cfg.width = 1280;
  cfg.height = 720;
  return cfg;
}

} // namespace

TEST_CASE("[Unit] S6 GameplayRuntimeHarness - cave recipe builds real components") {
  using namespace NoMoreDay::render::validation;

  GameplayRuntimeHarness harness;
  REQUIRE(harness.BuildSceneOnly("cave_color_bleed", 0xCA000001));

  auto &registry = harness.RegistryForTesting();
  CHECK(registry.storage<::Position>().size() > 0);
  CHECK(registry.storage<::ColorComponent>().size() > 0);
  CHECK(registry.storage<NoMoreDay::LightComponent>().size() > 0);
  CHECK(registry.storage<NoMoreDay::ShadowCasterComponent>().size() > 0);
  CHECK(registry.storage<::MapTileComponent>().size() > 0);

  CHECK(harness.InputHashForTesting() != 0);
}

TEST_CASE("[Unit] S6 GameplayRuntimeHarness - combat recipe builds player/enemy/VFX") {
  using namespace NoMoreDay::render::validation;

  GameplayRuntimeHarness harness;
  REQUIRE(harness.BuildSceneOnly("dynamic_combat_emissive", 0xC0CB0002));

  auto &registry = harness.RegistryForTesting();
  CHECK(registry.storage<::PlayerTag>().size() == 1);
  CHECK(registry.storage<::EnemyTag>().size() >= 8);
  CHECK(registry.storage<::VisualEffect>().size() > 0);
  CHECK(registry.storage<::AttackEffect>().size() > 0);
  CHECK(registry.storage<::VisionComponent>().size() > 0);
}

TEST_CASE("[Unit] S6 GameplayRuntimeHarness - outdoor recipe builds light pressure") {
  using namespace NoMoreDay::render::validation;

  GameplayRuntimeHarness harness;
  REQUIRE(harness.BuildSceneOnly("outdoor_light_pressure", 0x00000003));

  auto &registry = harness.RegistryForTesting();
  CHECK(registry.storage<NoMoreDay::LightComponent>().size() >= 200);
  CHECK(registry.storage<::MapTileComponent>().size() > 0);
  CHECK(registry.storage<NoMoreDay::ShadowCasterComponent>().size() > 0);
}

TEST_CASE("[Unit] S6 GameplayRuntimeHarness - input hash is deterministic") {
  using namespace NoMoreDay::render::validation;

  GameplayRuntimeHarness a;
  GameplayRuntimeHarness b;
  REQUIRE(a.BuildSceneOnly("cave_color_bleed", 0xCA000001));
  REQUIRE(b.BuildSceneOnly("cave_color_bleed", 0xCA000001));
  CHECK(a.InputHashForTesting() == b.InputHashForTesting());
  CHECK(a.InputHashForTesting() != 0);

  GameplayRuntimeHarness c;
  REQUIRE(c.BuildSceneOnly("cave_color_bleed", 0xCA000002));
  CHECK(c.InputHashForTesting() != a.InputHashForTesting());
}

TEST_CASE("[Unit] S6 GameplayRuntimeHarness - unknown recipe is rejected") {
  using namespace NoMoreDay::render::validation;

  GameplayRuntimeHarness harness;
  CHECK_FALSE(harness.BuildSceneOnly("not_a_recipe", 0x1234));
}
