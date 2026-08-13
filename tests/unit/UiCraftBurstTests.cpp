#include "doctest.h"

#include "game/application/ui/UiCraftBurst.hpp"

#include "raylib.h"
#include "raymath.h"

#include <cmath>

namespace NoMoreDay::ui {

namespace {

// Speed magnitude of a particle's initial velocity.
float SpeedOf(const components::GPUParticle& p) {
  return Vector2Length(p.velocity);
}

// raylib Color has no operator==; compare channel by channel.
bool SameColor(const components::GPUParticle& p, const Color& expected) {
  return p.color.r == expected.r && p.color.g == expected.g &&
         p.color.b == expected.b && p.color.a == expected.a;
}

} // namespace

TEST_CASE("[Unit] UiCraftBurst - fuse burst: 20 gold sparks + 20 red trails") {
  const Vector2 anchor = {123.0f, -45.0f};
  const auto particles = BuildCraftSuccessBurst(UiCraftBurstKind::Fuse, anchor);

  REQUIRE(particles.size() == 40);

  // Gold sparks (legacy: CreateSpark GOLD, scale 2.5 -> 2.0 after *0.8).
  for (int i = 0; i < 20; ++i) {
    CAPTURE(i);
    CHECK(particles[i].position.x == doctest::Approx(anchor.x));
    CHECK(particles[i].position.y == doctest::Approx(anchor.y));
    CHECK(SameColor(particles[i], GOLD));
    CHECK(particles[i].scale == doctest::Approx(2.0f));
    const float speed = SpeedOf(particles[i]);
    CHECK(speed >= 100.0f);
    CHECK(speed <= 300.0f);
  }

  // Ancient-red ink trails (legacy: CreateInkTrail 2.0/0.8, recolored).
  for (int i = 20; i < 40; ++i) {
    CAPTURE(i);
    CHECK(particles[i].position.x == doctest::Approx(anchor.x));
    CHECK(particles[i].position.y == doctest::Approx(anchor.y));
    CHECK(SameColor(particles[i], Color{230, 0, 0, 200}));
    CHECK(particles[i].scale == doctest::Approx(1.0f)); // 2.0 * 0.5
    CHECK(particles[i].lifetime == doctest::Approx(0.8f));
    const float speed = SpeedOf(particles[i]);
    CHECK(speed >= 100.0f);
    CHECK(speed <= 300.0f);
  }
}

TEST_CASE("[Unit] UiCraftBurst - salvage burst: 30 red sparks") {
  const Vector2 anchor = {-7.0f, 250.0f};
  const auto particles =
      BuildCraftSuccessBurst(UiCraftBurstKind::Salvage, anchor);

  REQUIRE(particles.size() == 30);
  for (int i = 0; i < 30; ++i) {
    CAPTURE(i);
    CHECK(particles[i].position.x == doctest::Approx(anchor.x));
    CHECK(particles[i].position.y == doctest::Approx(anchor.y));
    CHECK(SameColor(particles[i], RED));
    CHECK(particles[i].scale == doctest::Approx(1.6f)); // 2.0 * 0.8
    const float speed = SpeedOf(particles[i]);
    CHECK(speed >= 150.0f);
    CHECK(speed <= 400.0f);
  }
}

} // namespace NoMoreDay::ui
