#include "game/render/EmissiveStampAdapter.hpp"

#include "engine/render/MaterialManager.hpp"
#include "engine/vfx/VFXTypes.hpp"
#include "game/components/Common.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace NoMoreDay {

EmissiveProjection EmissiveStampAdapter::BuildEmissiveStamps(
    entt::registry &registry) {
  EmissiveProjection projection;

  auto view = registry.view<const Position,
                            const NoMoreDay::vfx::ActiveMaterialSwap>(
      entt::exclude<KilledTag>);
  projection.stamps.reserve(view.size_hint());
  for (const auto entity : view) {
    const auto &swap = view.get<NoMoreDay::vfx::ActiveMaterialSwap>(entity);
    if (swap.materialId <= 0) {
      continue;
    }

    const auto &gpuMaterial =
        NoMoreDay::render::MaterialManager::Get().GetGpuMaterialForTesting(
            swap.materialId);
    const int maskLayer = static_cast<int>(std::lround(gpuMaterial.textureSlots.z));
    if (maskLayer < 0) {
      continue;
    }

    const float emissiveIntensity =
        std::max(0.0f, gpuMaterial.emissiveAndIntensity.w);
    const float emissiveR = std::max(0.0f, gpuMaterial.emissiveAndIntensity.x);
    const float emissiveG = std::max(0.0f, gpuMaterial.emissiveAndIntensity.y);
    const float emissiveB = std::max(0.0f, gpuMaterial.emissiveAndIntensity.z);
    if (emissiveIntensity <= 0.0001f ||
        (emissiveR + emissiveG + emissiveB) <= 0.0001f) {
      continue;
    }

    const auto &position = view.get<Position>(entity);
    float worldHalfExtent = 24.0f;
    if (const auto *radius = registry.try_get<Radius>(entity)) {
      worldHalfExtent = std::max(worldHalfExtent, radius->value);
    }
    if (const auto *sprite = registry.try_get<SpriteComponent>(entity);
        sprite != nullptr && sprite->texture.id != 0) {
      const float spriteHalfExtent = 0.5f *
                                     std::max(static_cast<float>(sprite->texture.width),
                                              static_cast<float>(sprite->texture.height)) *
                                     std::max(0.05f, sprite->scale);
      worldHalfExtent = std::max(worldHalfExtent, spriteHalfExtent);
    }

    components::EmissiveStampInput stamp = {};
    stamp.worldPos = {position.x, position.y};
    stamp.worldHalfExtent = worldHalfExtent;
    stamp.maskLayer = maskLayer;
    stamp.emissionRGBA = {emissiveR, emissiveG, emissiveB, emissiveIntensity};
    projection.stamps.push_back(stamp);
  }

  return projection;
}

} // namespace NoMoreDay
