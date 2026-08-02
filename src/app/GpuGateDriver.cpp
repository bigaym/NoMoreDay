#include "app/GpuGateDriver.hpp"

#include "engine/render/GameplayRenderHooks.hpp"
#include "raylib.h"

#include <cmath>
#include <cstring>

namespace NoMoreDay::render::validation {

namespace {

constexpr uint32_t kGpuGateRgba16f = 0x881A; // GL_RGBA16F

// Fixed-sequence generator; identical on every platform/compiler.
class GpuGateDeterministicRng {
public:
  explicit GpuGateDeterministicRng(uint32_t seed) : m_state(seed != 0 ? seed : 1) {}

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

GpuGateDriver::GpuGateDriver(entt::registry *registry,
                             NoMoreDay::SharedContext *context)
    : m_registry(registry), m_context(context) {}

GpuGateDriver::~GpuGateDriver() { ReleaseCompositeTarget(); }

bool GpuGateDriver::PrepareFixture(const FixtureConfig &fixture) {
  ReleaseCompositeTarget();
  if (!BuildSceneOnly(fixture.name, fixture.sceneSeed)) {
    return false;
  }
  m_composite = NoMoreDay::render::resources::FramebufferManager::Create(
      fixture.width, fixture.height, kGpuGateRgba16f, true);
  if (!m_composite.IsValid()) {
    return false;
  }
  return true;
}

entt::registry &GpuGateDriver::Registry() { return *m_registry; }
NoMoreDay::SharedContext &GpuGateDriver::Context() { return *m_context; }

NoMoreDay::render::RenderFrameInput GpuGateDriver::RenderInput() const {
  // W6: mirrors GameplayState::OnRender exactly (real resources, render alpha,
  // render context and camera zoom) so the gate exercises the production path.
  NoMoreDay::render::RenderFrameInput input;
  input.resources = m_context->resources;
  input.renderAlpha = m_context->renderAlpha;
  input.renderContext = m_context->renderContext;
  input.cameraZoom = (m_context->settings != nullptr)
                         ? m_context->settings->cameraZoom
                         : 1.5f;
  return input;
}

uint32_t GpuGateDriver::CompositeFramebuffer() const { return m_composite.fbo; }
int GpuGateDriver::CompositeWidth() const { return m_composite.width; }
int GpuGateDriver::CompositeHeight() const { return m_composite.height; }

uint64_t GpuGateDriver::SceneInputHash() const { return m_sceneInputHash; }
std::string GpuGateDriver::FixtureVersion() const { return "g6-v1.0"; }
std::string GpuGateDriver::SceneSource() const {
  return "src/app/GpuGateDriver.cpp (real game registry, deterministic recipe, "
         "production game-binary gate)";
}

NoMoreDay::render::GameplayRenderHooks *GpuGateDriver::RenderHooks() {
  return (m_context != nullptr) ? m_context->gameplayRenderHooks : nullptr;
}

bool GpuGateDriver::BuildSceneOnly(const std::string &fixtureName,
                                   uint32_t seed) {
  // The gate owns the real registry for its lifetime; a deterministic fixture
  // scene replaces any bootstrap/MainMenu entities. Real GPU-system observers
  // attached to the registry only track lifecycle and stay consistent because
  // every emplacement below mirrors production component types.
  m_registry->clear();
  m_hasher = GpuGateFnv1a64();
  m_hasher.FeedStr("GpuGateDriver/");
  m_hasher.FeedStr(fixtureName);
  m_hasher.FeedU32(seed);
  m_sceneInputHash = 0;

  if (fixtureName == "cave_color_bleed") {
    BuildCaveScene(seed);
  } else if (fixtureName == "dynamic_combat_emissive") {
    BuildCombatScene(seed);
  } else if (fixtureName == "outdoor_light_pressure") {
    BuildOutdoorScene(seed);
  } else {
    return false;
  }
  m_sceneInputHash = m_hasher.hash;
  return true;
}

void GpuGateDriver::PlaceColor(float x, float y, uint8_t r, uint8_t g,
                               uint8_t b, uint8_t tag) {
  const auto e = m_registry->create();
  m_registry->emplace<::Position>(e, x, y);
  m_registry->emplace<::ColorComponent>(e, Color{r, g, b, 255});
  m_hasher.FeedU32(tag);
  m_hasher.FeedF32(x);
  m_hasher.FeedF32(y);
}

void GpuGateDriver::PlaceShadow(float x, float y,
                                NoMoreDay::ShadowOccluderShape shape,
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

void GpuGateDriver::PlaceLight(float x, float y, float radius, float intensity,
                               float r, float g, float b, uint8_t tag) {
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

void GpuGateDriver::BuildCaveScene(uint32_t seed) {
  GpuGateDeterministicRng rng(seed);
  for (int i = 0; i < 16; ++i) {
    const float angle = static_cast<float>(i) * (3.14159265f * 2.0f / 16.0f);
    const float cx = std::cos(angle) * 340.0f;
    const float cy = std::sin(angle) * 340.0f;
    PlaceShadow(cx, cy, NoMoreDay::ShadowOccluderShape::Box, 2.0f, 0x10);
  }
  for (int i = 0; i < 40; ++i) {
    const float x = rng.NextFloat(-240.0f, 240.0f);
    const float y = rng.NextFloat(-240.0f, 240.0f);
    const int hue = static_cast<int>(rng.Next()) % 3;
    if (hue == 0) {
      PlaceColor(x, y, 255, 60, 40, 0x20);
    } else if (hue == 1) {
      PlaceColor(x, y, 60, 80, 255, 0x21);
    } else {
      PlaceColor(x, y, 200, 60, 220, 0x22);
    }
  }
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

void GpuGateDriver::BuildCombatScene(uint32_t seed) {
  GpuGateDeterministicRng rng(seed);
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

void GpuGateDriver::BuildOutdoorScene(uint32_t seed) {
  GpuGateDeterministicRng rng(seed);
  for (int i = 0; i < 48; ++i) {
    const float x = rng.NextFloat(-560.0f, 560.0f);
    const float y = rng.NextFloat(-560.0f, 560.0f);
    const float dx = x;
    const float dy = y;
    const float distSq = dx * dx + dy * dy;
    if (distSq < 160.0f * 160.0f) {
      continue;
    }
    PlaceShadow(x, y, NoMoreDay::ShadowOccluderShape::Circle, 3.0f, 0x60);
  }
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
  for (int i = 0; i < 60; ++i) {
    const float x = rng.NextFloat(-520.0f, 520.0f);
    const float y = rng.NextFloat(-520.0f, 520.0f);
    const int tint = static_cast<int>(rng.Next()) % 3;
    if (tint == 0) {
      PlaceColor(x, y, 130, 200, 120, 0x62);
    } else if (tint == 1) {
      PlaceColor(x, y, 180, 150, 100, 0x63);
    } else {
      PlaceColor(x, y, 150, 180, 210, 0x64);
    }
  }
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

void GpuGateDriver::ReleaseCompositeTarget() {
  if (m_composite.IsValid()) {
    NoMoreDay::render::resources::FramebufferManager::Destroy(m_composite);
  }
  m_composite = {};
}

} // namespace NoMoreDay::render::validation
