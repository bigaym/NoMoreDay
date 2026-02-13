#pragma once

#include "engine/vfx/VFXPlayerComponent.hpp"
#include "engine/vfx/VFXTypes.hpp"

#include <entt/entt.hpp>

#include "raylib.h"

namespace NoMoreDay::vfx {

class VFXSequencerSystem {
public:
  static void Update(entt::registry &registry, float dt);

private:
  static void DispatchEvent(entt::registry &registry, entt::entity source,
                            const VFXEvent &event,
                            const VFXPlayerComponent &player);
  static void ExecuteParticle(entt::registry &registry, Vector2 worldPos,
                              const ParticleEventParams &params);
  static void ExecuteTrail(entt::registry &registry, entt::entity source,
                           const TrailEventParams &params);
  static void ExecuteLight(entt::registry &registry, Vector2 worldPos,
                           const LightEventParams &params);
  static void ExecuteShake(const ShakeEventParams &params);
  static void ExecuteDistortion(Vector2 worldPos,
                                const DistortionEventParams &params);
  static void ExecuteSound(const SoundEventParams &params);
};

} // namespace NoMoreDay::vfx
