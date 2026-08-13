#pragma once

// S6 (M0-C R1.2) GameplayRuntimeHarness.
//
// Concrete FixtureRenderDriver that builds REAL ECS scenes for the hardware
// validation gate: real game component types (PlayerTag/EnemyTag/Position/
// LightComponent/ShadowCasterComponent/MapTileComponent/VisualEffect/etc.) on
// a real entt::registry, a minimal NoMoreDay::SharedContext, and an owned
// RGBA16F composite framebuffer backing the gate's offscreen rendering.
//
// The harness lives under tests/ so it may reference game/app headers without
// creating engine->game reverse dependencies (module boundary 71/71).
//
// Determinism contract: scene construction uses a fixed xorshift32 generator
// (never std::srand/rand, whose sequences are implementation-defined), and the
// scene input hash is a deterministic FNV-1a 64 fed with the recipe name, the
// scene seed and every placed entity's identifying data. The same recipe +
// seed always yields the same scene and the same input hash.

#include "engine/render/validation/FixtureRenderDriver.hpp"

#include "game/foundation/SharedContext.hpp"
#include "game/foundation/Settings.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/EffectComponent.hpp"
#include "game/foundation/components/LightComponent.hpp"
#include "game/foundation/components/MapComponent.hpp"
#include "game/foundation/components/ShadowCasterComponent.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

namespace NoMoreDay::render::validation {

namespace {

constexpr uint32_t kHarnessRgba16f = 0x881A; // GL_RGBA16F

// Deterministic FNV-1a 64 hash accumulator.
class Fnv1a64 {
public:
  uint64_t hash = 14695981039346656037ULL; // FNV offset basis

  void Feed(const void *data, size_t size) {
    const auto *bytes = static_cast<const unsigned char *>(data);
    for (size_t i = 0; i < size; ++i) {
      hash ^= bytes[i];
      hash *= 1099511628211ULL; // FNV prime
    }
  }

  void FeedU32(uint32_t value) { Feed(&value, sizeof(value)); }
  void FeedF32(float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    FeedU32(bits);
  }
  void FeedStr(const std::string &value) { Feed(value.data(), value.size()); }
};

// Fixed-sequence generator; identical on every platform/compiler.
class DeterministicRng {
public:
  explicit DeterministicRng(uint32_t seed) : m_state(seed != 0 ? seed : 1) {}

  uint32_t Next() {
    uint32_t x = m_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    m_state = x;
    return x;
  }

  float NextFloat(float lo, float hi) {
    const float t = static_cast<float>(Next() & 0xFFFFu) / 65535.0f;
    return lo + (hi - lo) * t;
  }

private:
  uint32_t m_state;
};

} // namespace

class GameplayRuntimeHarness final : public FixtureRenderDriver {
public:
  GameplayRuntimeHarness() {
    m_registry = std::make_unique<entt::registry>();
    m_settings = std::make_unique<NoMoreDay::GameSettings>();
    m_context = std::make_unique<NoMoreDay::SharedContext>();
    m_context->registry = m_registry.get();
    m_context->settings = m_settings.get();
    m_context->renderAlpha = 1.0f;
  }

  ~GameplayRuntimeHarness() override {
    ReleaseCompositeTarget();
    m_context.reset();
    m_settings.reset();
    m_registry.reset();
  }

  GameplayRuntimeHarness(const GameplayRuntimeHarness &) = delete;
  GameplayRuntimeHarness &operator=(const GameplayRuntimeHarness &) = delete;

  // Builds only the scene (no GPU work); used by unit tests that have no GL
  // context. Also used internally by PrepareFixture.
  bool BuildSceneOnly(GpuFixtureType fixtureType, uint32_t seed) {
    m_registry->clear();
    m_hasher = Fnv1a64();
    m_hasher.FeedStr("GameplayRuntimeHarness/");
    // T8: scene input hash feeds the canonical fixture name so historical
    // harness hashes stay byte-stable.
    m_hasher.FeedStr(std::string(GpuFixtureTypeToString(fixtureType)));
    m_hasher.FeedU32(seed);
    m_sceneInputHash = 0;

    switch (fixtureType) {
      case GpuFixtureType::CaveColorBleed:
        BuildCaveScene(seed);
        break;
      case GpuFixtureType::DynamicCombatEmissive:
        BuildCombatScene(seed);
        break;
      case GpuFixtureType::OutdoorLightPressure:
        BuildOutdoorScene(seed);
        break;
      case GpuFixtureType::None:
      case GpuFixtureType::Count:
      default:
        return false;
    }
    m_sceneInputHash = m_hasher.hash;
    return true;
  }

  // FixtureRenderDriver -----------------------------------------------------
  bool PrepareFixture(const FixtureConfig &fixture) override {
    m_currentFixture = fixture;
    ReleaseCompositeTarget();
    if (!BuildSceneOnly(fixture.type, fixture.sceneSeed)) {
      return false;
    }
    m_composite = NoMoreDay::render::resources::FramebufferManager::Create(
        fixture.width, fixture.height, kHarnessRgba16f, true);
    if (!m_composite.IsValid()) {
      return false;
    }
    return true;
  }

  entt::registry &Registry() override { return *m_registry; }
  NoMoreDay::SharedContext &Context() override { return *m_context; }

  NoMoreDay::render::RenderFrameInput RenderInput() const override {
    NoMoreDay::render::RenderFrameInput input;
    input.resources = nullptr;
    input.renderAlpha = m_context->renderAlpha;
    input.renderContext = nullptr;
    input.cameraZoom = (m_context->settings != nullptr)
                           ? m_context->settings->cameraZoom
                           : 1.0f;
    return input;
  }

  uint32_t CompositeFramebuffer() const override { return m_composite.fbo; }
  int CompositeWidth() const override { return m_composite.width; }
  int CompositeHeight() const override { return m_composite.height; }

  uint64_t SceneInputHash() const override { return m_sceneInputHash; }
  // s6-v1.0 was frozen before the outdoor light grid was corrected from 9x9 to
  // 15x15; that post-freeze recipe fix bumped the version to s6-v1.1. The
  // version string is NOT fed into the FNV input hash, so the hash value is
  // unchanged by this bump.
  std::string FixtureVersion() const override { return "s6-v1.1"; }
  std::string SceneSource() const override {
    return "tests/integration/GameplayRuntimeHarness.hpp "
           "(deterministic recipe, real game components, no level snapshot)";
  }

  // Exposed for tests that have no GL context.
  entt::registry &RegistryForTesting() { return *m_registry; }
  uint64_t InputHashForTesting() const { return m_sceneInputHash; }

private:
  // Emplace helpers ---------------------------------------------------------

  void PlaceColor(float x, float y, uint8_t r, uint8_t g, uint8_t b, uint8_t tag) {
    const auto e = m_registry->create();
    m_registry->emplace<::Position>(e, x, y);
    m_registry->emplace<::ColorComponent>(e, Color{r, g, b, 255});
    m_hasher.FeedU32(tag);
    m_hasher.FeedF32(x);
    m_hasher.FeedF32(y);
  }

  void PlaceShadow(float x, float y, NoMoreDay::ShadowOccluderShape shape,
                   float height, uint8_t tag) {
    const auto e = m_registry->create();
    m_registry->emplace<::Position>(e, x, y);
    m_registry->emplace<::Radius>(e, 12.0f);
    m_registry->emplace<::ColliderComponent>(e, 24.0f, 24.0f,
                                             ::ColliderType::Static, uint8_t{1}, uint8_t{1});
    auto &shadow = m_registry->emplace<NoMoreDay::ShadowCasterComponent>(e);
    shadow.shape = shape;
    shadow.occluderHeight = height;
    m_hasher.FeedU32(tag);
    m_hasher.FeedF32(x);
    m_hasher.FeedF32(y);
  }

  void PlaceLight(float x, float y, float radius, float intensity, float r,
                  float g, float b, uint8_t tag) {
    const auto e = m_registry->create();
    m_registry->emplace<::Position>(e, x, y);
    auto &light = m_registry->emplace<NoMoreDay::LightComponent>(e);
    light.enabled = true;
    light.radius = radius;
    light.intensity = intensity;
    light.colorR = r;
    light.colorG = g;
    light.colorB = b;
    light.priority = 128;
    m_hasher.FeedU32(tag);
    m_hasher.FeedF32(x);
    m_hasher.FeedF32(y);
    m_hasher.FeedF32(radius);
    m_hasher.FeedF32(intensity);
  }

  // Recipes -----------------------------------------------------------------

  void BuildCaveScene(uint32_t seed) {
    // Cave biome: tight cluster of warm/cool emissive crystals with heavy
    // shadow occlusion to stress GI color bleed.
    DeterministicRng rng(seed);

    // Cave walls: box-shaped occluders ringing the arena.
    for (int i = 0; i < 16; ++i) {
      const float angle = static_cast<float>(i) * (3.14159265f * 2.0f / 16.0f);
      const float cx = std::cos(angle) * 340.0f;
      const float cy = std::sin(angle) * 340.0f;
      PlaceShadow(cx, cy, NoMoreDay::ShadowOccluderShape::Box, 2.0f, 0x10);
    }

    // Emissive crystals: saturated red/blue/purple color blocks.
    for (int i = 0; i < 40; ++i) {
      const float x = rng.NextFloat(-240.0f, 240.0f);
      const float y = rng.NextFloat(-240.0f, 240.0f);
      const int hue = static_cast<int>(rng.Next()) % 3;
      if (hue == 0) {
        PlaceColor(x, y, 255, 60, 40, 0x20); // warm red bleed
      } else if (hue == 1) {
        PlaceColor(x, y, 60, 80, 255, 0x21); // cool blue bleed
      } else {
        PlaceColor(x, y, 200, 60, 220, 0x22); // purple bleed
      }
    }

    // Ambient + accent lights.
    PlaceLight(0.0f, 0.0f, 420.0f, 1.4f, 0.9f, 0.5f, 0.35f, 0x30);
    for (int i = 0; i < 12; ++i) {
      const float x = rng.NextFloat(-280.0f, 280.0f);
      const float y = rng.NextFloat(-280.0f, 280.0f);
      const int tint = static_cast<int>(rng.Next()) % 3;
      if (tint == 0) {
        PlaceLight(x, y, 160.0f, 2.2f, 1.0f, 0.25f, 0.2f, 0x31);
      } else if (tint == 1) {
        PlaceLight(x, y, 160.0f, 2.2f, 0.25f, 0.4f, 1.0f, 0x32);
      } else {
        PlaceLight(x, y, 180.0f, 2.4f, 0.7f, 0.2f, 1.0f, 0x33);
      }
    }

    // Floor tile markers for height-field consumption.
    for (int gx = -8; gx <= 8; gx += 2) {
      for (int gy = -8; gy <= 8; gy += 2) {
        const auto e = m_registry->create();
        m_registry->emplace<::Position>(e, static_cast<float>(gx) * 32.0f,
                                        static_cast<float>(gy) * 32.0f);
        m_registry->emplace<::MapTileComponent>(e, gx, gy, Tile::Type::FLOOR);
        m_registry->emplace<::ColliderComponent>(e, 32.0f, 32.0f,
                                                 ::ColliderType::Static, uint8_t{1}, uint8_t{1});
        m_hasher.FeedU32(0x40);
        m_hasher.FeedF32(static_cast<float>(gx));
        m_hasher.FeedF32(static_cast<float>(gy));
      }
    }
  }

  void BuildCombatScene(uint32_t seed) {
    // Dynamic combat: player + enemies + moving occluders + emissive VFX.
    DeterministicRng rng(seed);

    // Player.
    {
      const auto e = m_registry->create();
      m_registry->emplace<::Position>(e, 0.0f, 0.0f);
      m_registry->emplace<::PrevPosition>(e, 0.0f, 0.0f);
      m_registry->emplace<::Velocity>(e, 0.0f, 0.0f);
      m_registry->emplace<::PlayerTag>(e);
      m_registry->emplace<::Radius>(e, 14.0f);
      m_registry->emplace<::ColorComponent>(e, Color{240, 240, 255, 255});
      m_registry->emplace<::ColliderComponent>(e, 28.0f, 28.0f,
                                               ::ColliderType::Dynamic, uint8_t{1}, uint8_t{1});
      m_hasher.FeedU32(0x50);
      m_hasher.FeedF32(0.0f);
      m_hasher.FeedF32(0.0f);
    }

    // Enemies with vision + dynamic shadow casters.
    for (int i = 0; i < 10; ++i) {
      const float x = rng.NextFloat(-260.0f, 260.0f);
      const float y = rng.NextFloat(-260.0f, 260.0f);
      const auto e = m_registry->create();
      m_registry->emplace<::Position>(e, x, y);
      m_registry->emplace<::PrevPosition>(e, x, y);
      m_registry->emplace<::Velocity>(e, rng.NextFloat(-40.0f, 40.0f),
                                      rng.NextFloat(-40.0f, 40.0f));
      m_registry->emplace<::EnemyTag>(e);
      m_registry->emplace<::Radius>(e, 10.0f);
      m_registry->emplace<::VisionComponent>(e, 160.0f);
      m_registry->emplace<::ColorComponent>(e, Color{230, 80, 80, 255});
      m_registry->emplace<::ColliderComponent>(e, 20.0f, 20.0f,
                                               ::ColliderType::Dynamic, uint8_t{1}, uint8_t{1});
      auto &shadow = m_registry->emplace<NoMoreDay::ShadowCasterComponent>(e);
      shadow.shape = NoMoreDay::ShadowOccluderShape::Circle;
      shadow.occluderHeight = 1.2f;
      shadow.dynamicFlag = 1;
      m_hasher.FeedU32(0x51);
      m_hasher.FeedF32(x);
      m_hasher.FeedF32(y);
    }

    // Moving occluder pillars.
    for (int i = 0; i < 8; ++i) {
      const float x = rng.NextFloat(-300.0f, 300.0f);
      const float y = rng.NextFloat(-300.0f, 300.0f);
      const auto e = m_registry->create();
      m_registry->emplace<::Position>(e, x, y);
      m_registry->emplace<::Velocity>(e, rng.NextFloat(-60.0f, 60.0f),
                                      rng.NextFloat(-60.0f, 60.0f));
      m_registry->emplace<::Radius>(e, 18.0f);
      m_registry->emplace<::ColliderComponent>(e, 36.0f, 36.0f,
                                               ::ColliderType::Dynamic, uint8_t{1}, uint8_t{1});
      auto &shadow = m_registry->emplace<NoMoreDay::ShadowCasterComponent>(e);
      shadow.shape = NoMoreDay::ShadowOccluderShape::Capsule;
      shadow.occluderHeight = 2.5f;
      shadow.dynamicFlag = 1;
      m_hasher.FeedU32(0x52);
      m_hasher.FeedF32(x);
      m_hasher.FeedF32(y);
    }

    // Emissive combat VFX: sword-intent bursts and pickups.
    for (int i = 0; i < 24; ++i) {
      const float x = rng.NextFloat(-280.0f, 280.0f);
      const float y = rng.NextFloat(-280.0f, 280.0f);
      const auto e = m_registry->create();
      m_registry->emplace<::Position>(e, x, y);
      auto &vfx = m_registry->emplace<::VisualEffect>(e);
      vfx.type = (i % 2 == 0) ? ::VisualEffectType::SwordIntentBurst
                              : ::VisualEffectType::Pickup;
      vfx.timer = 0.0f;
      vfx.lifeTime = 0.5f;
      vfx.startScale = 1.0f;
      vfx.endScale = 2.2f;
      vfx.color = (i % 2 == 0) ? Color{120, 240, 255, 255}
                               : Color{255, 220, 90, 255};
      vfx.param1 = 40.0f;
      m_hasher.FeedU32(0x53);
      m_hasher.FeedF32(x);
      m_hasher.FeedF32(y);
    }

    // Attack arcs.
    for (int i = 0; i < 6; ++i) {
      const float x = rng.NextFloat(-220.0f, 220.0f);
      const float y = rng.NextFloat(-220.0f, 220.0f);
      const auto e = m_registry->create();
      m_registry->emplace<::Position>(e, x, y);
      auto &arc = m_registry->emplace<::AttackEffect>(e);
      arc.timer = 0.0f;
      arc.lifeTime = 0.2f;
      arc.rotation = rng.NextFloat(0.0f, 360.0f);
      arc.range = 55.0f;
      arc.arcAngle = 110.0f;
      arc.color = Color{255, 200, 120, 255};
      m_hasher.FeedU32(0x54);
      m_hasher.FeedF32(x);
      m_hasher.FeedF32(y);
    }

    // Combat lights: warm flickering point lights.
    for (int i = 0; i < 10; ++i) {
      const float x = rng.NextFloat(-260.0f, 260.0f);
      const float y = rng.NextFloat(-260.0f, 260.0f);
      const auto e = m_registry->create();
      m_registry->emplace<::Position>(e, x, y);
      auto &light = m_registry->emplace<NoMoreDay::LightComponent>(e);
      light.enabled = true;
      light.radius = 150.0f;
      light.intensity = 2.0f;
      light.colorR = 1.0f;
      light.colorG = 0.75f;
      light.colorB = 0.4f;
      light.flicker = (i % 3 == 0);
      light.flickerSpeed = 6.0f;
      light.flickerAmplitude = 0.15f;
      m_hasher.FeedU32(0x55);
      m_hasher.FeedF32(x);
      m_hasher.FeedF32(y);
    }
  }

  void BuildOutdoorScene(uint32_t seed) {
    // Outdoor high light pressure: many lights, wide open field, treeline.
    DeterministicRng rng(seed);

    // Treeline occluders around the arena perimeter.
    for (int i = 0; i < 48; ++i) {
      const float x = rng.NextFloat(-560.0f, 560.0f);
      const float y = rng.NextFloat(-560.0f, 560.0f);
      const float dx = x;
      const float dy = y;
      const float distSq = dx * dx + dy * dy;
      if (distSq < 160.0f * 160.0f) {
        continue; // keep the field center clear
      }
      PlaceShadow(x, y, NoMoreDay::ShadowOccluderShape::Circle, 3.0f, 0x60);
    }

    // Wide floor tiles.
    for (int gx = -10; gx <= 10; gx += 2) {
      for (int gy = -10; gy <= 10; gy += 2) {
        const auto e = m_registry->create();
        m_registry->emplace<::Position>(e, static_cast<float>(gx) * 48.0f,
                                        static_cast<float>(gy) * 48.0f);
        m_registry->emplace<::MapTileComponent>(e, gx, gy, Tile::Type::FLOOR);
        m_registry->emplace<::ColliderComponent>(e, 48.0f, 48.0f,
                                                 ::ColliderType::Static, uint8_t{1}, uint8_t{1});
        m_hasher.FeedU32(0x61);
        m_hasher.FeedF32(static_cast<float>(gx));
        m_hasher.FeedF32(static_cast<float>(gy));
      }
    }

    // Scattered ground color patches.
    for (int i = 0; i < 60; ++i) {
      const float x = rng.NextFloat(-520.0f, 520.0f);
      const float y = rng.NextFloat(-520.0f, 520.0f);
      const int tint = static_cast<int>(rng.Next()) % 3;
      if (tint == 0) {
        PlaceColor(x, y, 130, 200, 120, 0x62); // grass
      } else if (tint == 1) {
        PlaceColor(x, y, 180, 150, 100, 0x63); // dirt
      } else {
        PlaceColor(x, y, 150, 180, 210, 0x64); // sky reflection
      }
    }

    // Maximum light pressure: dense grid of overlapping lights.
    int lightCount = 0;
    for (int gy = -7; gy <= 7; ++gy) {
      for (int gx = -7; gx <= 7; ++gx) {
        if (lightCount >= 220) {
          break;
        }
        const float x = static_cast<float>(gx) * 110.0f + rng.NextFloat(-20.0f, 20.0f);
        const float y = static_cast<float>(gy) * 110.0f + rng.NextFloat(-20.0f, 20.0f);
        const int tint = static_cast<int>(rng.Next()) % 4;
        if (tint == 0) {
          PlaceLight(x, y, 130.0f, 1.8f, 1.0f, 0.9f, 0.7f, 0x70);
        } else if (tint == 1) {
          PlaceLight(x, y, 130.0f, 1.8f, 0.7f, 1.0f, 0.8f, 0x71);
        } else if (tint == 2) {
          PlaceLight(x, y, 130.0f, 1.8f, 0.8f, 0.8f, 1.0f, 0x72);
        } else {
          PlaceLight(x, y, 140.0f, 2.0f, 1.0f, 0.6f, 0.5f, 0x73);
        }
        ++lightCount;
      }
    }
  }

  void ReleaseCompositeTarget() {
    if (m_composite.IsValid()) {
      NoMoreDay::render::resources::FramebufferManager::Destroy(m_composite);
    }
    m_composite = {};
  }

  std::unique_ptr<entt::registry> m_registry;
  std::unique_ptr<NoMoreDay::SharedContext> m_context;
  std::unique_ptr<NoMoreDay::GameSettings> m_settings;
  NoMoreDay::render::resources::FramebufferHandle m_composite;
  FixtureConfig m_currentFixture;
  Fnv1a64 m_hasher;
  uint64_t m_sceneInputHash{0};
};

} // namespace NoMoreDay::render::validation
