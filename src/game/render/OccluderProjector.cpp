#include "game/render/OccluderProjector.hpp"

#include "game/components/Common.hpp"
#include "game/components/ShadowCasterComponent.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace NoMoreDay {
namespace {

constexpr uint64_t kFnvOffset = 1469598103934665603ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

void HashAppend(uint64_t &hash, const uint64_t value) noexcept {
  hash ^= value;
  hash *= kFnvPrime;
}

uint64_t BuildOccluderWord(
    const NoMoreDay::components::GPUShadowCaster &caster) noexcept {
  const uint32_t qx = static_cast<uint32_t>(std::lround(caster.posX * 16.0f));
  const uint32_t qy = static_cast<uint32_t>(std::lround(caster.posY * 16.0f));
  const uint32_t qr = static_cast<uint32_t>(std::lround(caster.radius * 16.0f));
  const uint32_t qh =
      static_cast<uint32_t>(std::lround(caster.occluderHeight * 16.0f));

  uint64_t word = static_cast<uint64_t>(qx);
  word = (word << 16) ^ static_cast<uint64_t>(qy & 0xFFFFu);
  word = (word << 16) ^ static_cast<uint64_t>(qr & 0xFFFFu);
  word = (word << 16) ^ static_cast<uint64_t>(qh & 0xFFFFu);
  word ^= static_cast<uint64_t>(caster.shapeIndex) << 8;
  word ^= static_cast<uint64_t>(caster.dynamicFlag) << 1;
  return word;
}

uint64_t FinalizeSignature(std::vector<uint64_t> words) {
  std::sort(words.begin(), words.end());
  uint64_t hash = kFnvOffset;
  for (const uint64_t word : words) {
    HashAppend(hash, word);
  }
  HashAppend(hash, static_cast<uint64_t>(words.size()));
  return hash;
}

} // namespace

OccluderProjection OccluderProjector::Project(entt::registry &registry) {
  OccluderProjection projection;

  std::vector<uint64_t> staticWords;
  std::vector<uint64_t> dynamicWords;
  staticWords.reserve(256);
  dynamicWords.reserve(256);

  auto view =
      registry.view<const Position, const NoMoreDay::ShadowCasterComponent>();
  projection.casters.reserve(static_cast<size_t>(view.size_hint()));
  for (const entt::entity entity : view) {
    const auto &[position, casterComponent] =
        view.get<const Position, const NoMoreDay::ShadowCasterComponent>(entity);

    float radius = 24.0f;
    if (const auto *vision = registry.try_get<VisionComponent>(entity);
        vision != nullptr && vision->radius > 0.0f) {
      radius = vision->radius;
    }

    NoMoreDay::components::GPUShadowCaster caster = {
        .posX = position.x,
        .posY = position.y,
        .radius = radius,
        .occluderHeight = casterComponent.occluderHeight,
        .shapeIndex = static_cast<uint32_t>(casterComponent.shape),
        .dynamicFlag = casterComponent.dynamicFlag,
        .reserved0 = 0u,
        .reserved1 = 0u,
    };
    projection.casters.push_back(caster);

    const uint64_t word = BuildOccluderWord(caster);
    if (caster.dynamicFlag != 0u) {
      dynamicWords.push_back(word);
      ++projection.dynamicCount;
    } else {
      staticWords.push_back(word);
      ++projection.staticCount;
    }
  }

  projection.staticSignature = FinalizeSignature(std::move(staticWords));
  projection.dynamicSignature = FinalizeSignature(std::move(dynamicWords));
  return projection;
}

} // namespace NoMoreDay
