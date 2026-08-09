#pragma once

#include "engine/vfx/VFXPlayerComponent.hpp"
#include "engine/vfx/VFXTypes.hpp"

#include <entt/entt.hpp>
#include <cstddef>

#include "raylib.h"

namespace NoMoreDay::vfx {

class VFXSequencerSystem {
public:
  static void Update(entt::registry &registry, float dt);
  static void ResetRuntimeStateForTesting();
  static size_t GetActiveDistortionCountForTesting();
  static size_t GetActiveMaterialSwapCountForTesting();
  static size_t GetActiveShadowPulseCountForTesting();
  static size_t GetActiveLightProfileBlendCountForTesting();
  static size_t GetActiveMaterialPhaseShiftCountForTesting();
  static size_t GetDistortionOverflowDropCountForTesting();
  static size_t GetDistortionOverflowEvictCountForTesting();

private:
  enum class DispatchMode : uint8_t {
    Normal = 0,
    Degraded = 1,
  };

  static void DispatchEvent(entt::registry &registry, entt::entity source,
                            const VFXEvent &event,
                            const VFXPlayerComponent &player, DispatchMode mode);
  static void ExecuteParticle(entt::registry &registry, Vector2 worldPos,
                              const ParticleEventParams &params);
  static void ExecuteTrail(entt::registry &registry, entt::entity source,
                           const TrailEventParams &params);
  static void ExecuteLight(entt::registry &registry, entt::entity source,
                           AnchorType anchor, Vector2 worldPos,
                           const LightEventParams &params);
  static void ExecuteShake(const ShakeEventParams &params);
  static void ExecuteDistortion(Vector2 worldPos,
                                const DistortionEventParams &params);
  static void ExecuteMaterialSwap(entt::registry &registry, entt::entity source,
                                  const VFXPlayerComponent &player,
                                  AnchorType anchor,
                                  const MaterialSwapParams &params);
  static void ExecuteShadowPulse(const ShadowPulseParams &params, DispatchMode mode);
  static void ExecuteLightProfileBlend(const LightProfileBlendParams &params,
                                       DispatchMode mode);
  static void ExecuteMaterialPhaseShift(const MaterialPhaseShiftParams &params,
                                        DispatchMode mode);
  static void ExecuteSound(const SoundEventParams &params);
};

} // namespace NoMoreDay::vfx
