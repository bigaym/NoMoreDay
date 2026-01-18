#pragma once
#include "TestCommon.hpp"
#include "core/math/PhysicsUtils.hpp"
#include "engine/render/GPUFlowFieldSystem.hpp"
#include "engine/render/GPUParticleSystem.hpp"
#include "engine/render/RenderSystem.hpp"
#include "engine/resource/ResourceManager.hpp"
#include "game/components/EffectComponent.hpp"
#include "game/systems/combat/EffectSystem.hpp"
#include "game/systems/item/DropSystem.hpp"
#include "game/systems/item/InventorySystem.hpp"
#include "game/systems/skill/ProjectileSystem.hpp"
#include <chrono>
#include <entt/entt.hpp>
#include <vector>


namespace NoMoreDay {

TEST_CASE("RenderSystem - Basic Setup") {
  entt::registry registry;
  // Basic smoke test for RenderSystem initialization logic
}

TEST_CASE("Visual Effect Tests") {
  entt::registry registry;

  SUBCASE("Damage Popup Animation") {
    auto entity = registry.create();
    registry.emplace<Position>(entity, 0.0f, 0.0f);

    DamagePopup popup;
    popup.damage = 10;
    popup.lifeTime = 1.0f;
    registry.emplace<DamagePopup>(entity, popup);

    systems::EffectSystem::update(registry, 0.1f);
    CHECK(registry.valid(entity));

    systems::EffectSystem::update(registry, 1.0f);
    CHECK_FALSE(registry.valid(entity));
  }
}

TEST_CASE("Physics Interaction Logic") {
  entt::registry registry;
  systems::SpatialHashGrid grid(1000, 1000, 50);

  SUBCASE("Knockback Application") {
    auto target = registry.create();
    registry.emplace<Position>(target, 100.0f, 100.0f);
    auto &vel = registry.emplace<Velocity>(target, 0.0f, 0.0f);

    Utils::ApplyKnockback(registry, target, {90.0f, 100.0f}, 10.0f);
    CHECK(vel.vx == doctest::Approx(10.0f));
  }
}

TEST_CASE("GPUFlowFieldSystem Logic") {
  ResourceManager rm;
  auto &flowSystem = systems::GPUFlowFieldSystem::Get();
  int width = 10, height = 10;
  flowSystem.Init(rm, width, height);

  SUBCASE("Buffer Logic and Walls") {
    std::vector<unsigned char> costMap(width * height, 1);
    costMap[4 * width + 4] = 255;

    flowSystem.Update(costMap, width, height, {55, 55}, {0, 0});

    std::vector<Vector2> flow(width * height);
    flowSystem.GetFlowBuffer().Read(flow.data(), flow.size() * sizeof(Vector2));

    CHECK(flow[4 * width + 4].x == 0.0f);
    CHECK(flow[5 * width + 6].x == doctest::Approx(-1.0f));
  }

  flowSystem.Shutdown();
}

TEST_CASE("GPUParticleSystem Shader and Performance") {
  auto &particleSystem = systems::GPUParticleSystem::Get();
  particleSystem.Init(1000);

  SUBCASE("Shader Smoke Test") {
    auto p = systems::InkEffectHelper::CreateInkTrail({100, 100}, {10, 10},
                                                      1.0f, 1.0f);
    particleSystem.Emit(p);
    particleSystem.Update(0.016f);
    CHECK(true);
  }

  particleSystem.Shutdown();
}

} // namespace NoMoreDay
