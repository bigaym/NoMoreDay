#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace NoMoreDay::render::shadow {

// Maps a stable light fingerprint to a stable atlas light id. The same logical
// light reuses the same id across frames as long as its fingerprint is
// re-encountered within the retention window, which keeps atlas tiles from
// churning when the active-light array is reordered every frame. Fingerprints
// that age out are pruned so the id space stays bounded.
class StableLightIdTracker final {
public:
  StableLightIdTracker() = default;

  // Returns the id assigned to `fingerprint`, reusing the previous assignment
  // when one exists and refreshing its last-seen frame.
  [[nodiscard]] uint32_t Resolve(uint64_t fingerprint, uint32_t currentFrame);

  // Drops entries not seen within `retentionFrames` of `currentFrame`.
  void Prune(uint32_t currentFrame, uint32_t retentionFrames) noexcept;

  void Clear() noexcept;

  [[nodiscard]] size_t GetEntryCount() const noexcept { return m_entries.size(); }
  [[nodiscard]] uint32_t GetNextLightId() const noexcept { return m_nextLightId; }

private:
  struct Entry {
    uint32_t lightId = 0;
    uint32_t lastSeenFrame = 0;
  };

  std::unordered_map<uint64_t, Entry> m_entries;
  uint32_t m_nextLightId = 1u;
};

} // namespace NoMoreDay::render::shadow
