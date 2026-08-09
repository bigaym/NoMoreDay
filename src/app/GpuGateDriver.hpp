#pragma once

// W6 (M0-C): concrete FixtureRenderDriver for the production game-binary
// hardware gate (NoMoreDay.exe --gpu-gate).
//
// Lives at the Game/App composition root so the engine-facing
// FixtureRenderDriver stays dependency-neutral (engine never includes
// game/app headers). This driver drives the real game registry, the real
// SharedContext/render context and the real gameplay render hooks through the
// standard RenderSystem render path, with an owned real-resolution RGBA16F
// composite target backing the gate's offscreen rendering.
//
// Determinism contract: scene construction uses a fixed xorshift32 generator
// (never std::srand/rand, whose sequences are implementation-defined) and the
// scene input hash is a deterministic FNV-1a 64 fed with the recipe name, the
// scene seed and every placed entity's identifying data. The same recipe +
// seed always yields the same scene and the same input hash.

#include "engine/render/validation/FixtureRenderDriver.hpp"

#include "game/foundation/SharedContext.hpp"
#include "engine/render/resources/FramebufferManager.hpp"

#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/EffectComponent.hpp"
#include "game/foundation/components/LightComponent.hpp"
#include "game/foundation/components/MapComponent.hpp"
#include "game/foundation/components/ShadowCasterComponent.hpp"

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

namespace NoMoreDay::render::validation {

class GpuGateDriver final : public FixtureRenderDriver {
public:
  // W6: the driver borrows the real game members for the gate lifetime. The
  // game must stay alive (and its GPU systems initialized) for the whole gate.
  GpuGateDriver(entt::registry *registry, NoMoreDay::SharedContext *context);
  ~GpuGateDriver() override;

  GpuGateDriver(const GpuGateDriver &) = delete;
  GpuGateDriver &operator=(const GpuGateDriver &) = delete;

  // FixtureRenderDriver -----------------------------------------------------
  bool PrepareFixture(const FixtureConfig &fixture) override;
  entt::registry &Registry() override;
  NoMoreDay::SharedContext &Context() override;
  NoMoreDay::render::RenderFrameInput RenderInput() const override;

  uint32_t CompositeFramebuffer() const override;
  int CompositeWidth() const override;
  int CompositeHeight() const override;

  uint64_t SceneInputHash() const override;
  std::string FixtureVersion() const override;
  std::string SceneSource() const override;

  // W6: returns the real gameplay render hooks installed by Game::init()
  // (m_gameplayRenderAdapter), so RenderSystem runs the full gameplay draw
  // path instead of the diagnostic zero-draw path.
  NoMoreDay::render::GameplayRenderHooks *RenderHooks() override;

  // W6: this is the production game-binary driver; a missing hooks binding
  // (e.g. compute unsupported so Game::init() never installed the adapter)
  // must fail the gate closed as NOT_RUN, never degrade to a hollow render.
  bool IsProductionDriver() const override {
    return true;
  }

private:
  bool BuildSceneOnly(const std::string &fixtureName, uint32_t seed);
  void ReleaseCompositeTarget();

  // Deterministic FNV-1a 64 hash accumulator (mirrors GameplayRuntimeHarness);
  // every placed entity feeds its identifying data so the scene input hash is
  // bound to the actual scene content.
  class GpuGateFnv1a64 {
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

  // Recipe emplacement helpers (mirror tests/integration/GameplayRuntimeHarness).
  void PlaceColor(float x, float y, uint8_t r, uint8_t g, uint8_t b,
                  uint8_t tag);
  void PlaceShadow(float x, float y, NoMoreDay::ShadowOccluderShape shape,
                   float height, uint8_t tag);
  void PlaceLight(float x, float y, float radius, float intensity, float r,
                  float g, float b, uint8_t tag);

  void BuildCaveScene(uint32_t seed);
  void BuildCombatScene(uint32_t seed);
  void BuildOutdoorScene(uint32_t seed);

  entt::registry *m_registry{nullptr};
  NoMoreDay::SharedContext *m_context{nullptr};
  NoMoreDay::render::resources::FramebufferHandle m_composite;
  GpuGateFnv1a64 m_hasher;
  uint64_t m_sceneInputHash{0};
};

} // namespace NoMoreDay::render::validation
