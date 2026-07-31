#pragma once

#include "engine/render/validation/GPUHardwareValidationGate.hpp"

#include <entt/entt.hpp>

#include <cstdint>
#include <string>

namespace NoMoreDay {
struct SharedContext;
}

namespace NoMoreDay::render::validation {

// S6 (M0-C R1.2): abstract contract between the hardware gate and a concrete
// gameplay fixture harness. The harness owns the real ECS registry, a minimal
// SharedContext, the fixture recipes, and the RGBA16F composite target; the
// gate consumes it through this interface so that engine code never takes a
// direct dependency on game/app headers (module boundary 71/71 preserved).
class FixtureRenderDriver {
public:
  virtual ~FixtureRenderDriver() = default;

  // Builds the deterministic scene for the given fixture (seed-driven recipe).
  // Returns false if the scene could not be prepared (fail-closed).
  virtual bool PrepareFixture(const FixtureConfig &fixture) = 0;

  // Access to the live ECS registry and SharedContext used for rendering.
  virtual entt::registry &Registry() = 0;
  virtual NoMoreDay::SharedContext &Context() = 0;

  // Owned RGBA16F composite target backing the gate's offscreen rendering.
  // fbo == 0 means "no valid composite target" -> fixture failure.
  virtual uint32_t CompositeFramebuffer() const = 0;
  virtual int CompositeWidth() const = 0;
  virtual int CompositeHeight() const = 0;

  // T6.5 artifact/version contract: reproducible input hash, recipe version
  // and provenance recorded alongside gate output.
  virtual uint64_t SceneInputHash() const = 0;
  virtual std::string FixtureVersion() const = 0;
  virtual std::string SceneSource() const = 0;
};

} // namespace NoMoreDay::render::validation
