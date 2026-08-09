#include "engine/render/shadow/StableLightIdTracker.hpp"

namespace NoMoreDay::render::shadow {

uint32_t StableLightIdTracker::Resolve(const uint64_t fingerprint,
                                       const uint32_t currentFrame) {
  const auto it = m_entries.find(fingerprint);
  if (it != m_entries.end()) {
    it->second.lastSeenFrame = currentFrame;
    return it->second.lightId;
  }
  const uint32_t lightId = m_nextLightId++;
  m_entries.emplace(fingerprint, Entry{.lightId = lightId,
                                       .lastSeenFrame = currentFrame});
  return lightId;
}

void StableLightIdTracker::Prune(const uint32_t currentFrame,
                                 const uint32_t retentionFrames) noexcept {
  if (retentionFrames == 0u) {
    return;
  }
  for (auto it = m_entries.begin(); it != m_entries.end();) {
    if (currentFrame - it->second.lastSeenFrame >= retentionFrames) {
      it = m_entries.erase(it);
    } else {
      ++it;
    }
  }
}

void StableLightIdTracker::Clear() noexcept {
  m_entries.clear();
  m_nextLightId = 1u;
}

} // namespace NoMoreDay::render::shadow
