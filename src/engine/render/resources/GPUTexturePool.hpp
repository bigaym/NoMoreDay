#pragma once

#include "engine/render/core/RenderConstants.hpp"
#include "engine/render/resources/FramebufferHandle.hpp"
#include <cstdint>
#include <functional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace NoMoreDay::render::resources {

enum class TextureSizeClass : uint8_t {
  FullHD = 0,     // 1920x1080 (1080p standard full-screen render target)
  HalfRes = 1,    // 960x540   (Half-resolution intermediate: GI cascade, bloom, etc.)
  QuarterRes = 2, // 480x270   (Quarter-resolution: coarse passes)
  Atlas2048 = 3,  // 2048x2048 (Shadow atlas, probe atlas, high-res atlas)
  Atlas1024 = 4,  // 1024x1024 (Secondary atlas, probe grid)
  Custom = 5,     // Explicit non-standard dimensions
};

[[nodiscard]] constexpr std::string_view ToString(TextureSizeClass sizeClass) {
  switch (sizeClass) {
  case TextureSizeClass::FullHD:
    return "FullHD";
  case TextureSizeClass::HalfRes:
    return "HalfRes";
  case TextureSizeClass::QuarterRes:
    return "QuarterRes";
  case TextureSizeClass::Atlas2048:
    return "Atlas2048";
  case TextureSizeClass::Atlas1024:
    return "Atlas1024";
  case TextureSizeClass::Custom:
  default:
    return "Custom";
  }
}

struct StandardExtent {
  int width = 0;
  int height = 0;
};

[[nodiscard]] inline StandardExtent GetStandardExtent(TextureSizeClass sizeClass,
                                                      int baseWidth = 1920,
                                                      int baseHeight = 1080) {
  const int fullW = (baseWidth > 0) ? baseWidth : 1920;
  const int fullH = (baseHeight > 0) ? baseHeight : 1080;
  switch (sizeClass) {
  case TextureSizeClass::FullHD:
    return {fullW, fullH};
  case TextureSizeClass::HalfRes:
    return {fullW / 2, fullH / 2};
  case TextureSizeClass::QuarterRes:
    return {fullW / 4, fullH / 4};
  case TextureSizeClass::Atlas2048:
    return {2048, 2048};
  case TextureSizeClass::Atlas1024:
    return {1024, 1024};
  case TextureSizeClass::Custom:
  default:
    return {0, 0};
  }
}

[[nodiscard]] inline TextureSizeClass ClassifySize(int width, int height,
                                                  int baseWidth = 1920,
                                                  int baseHeight = 1080) {
  const int fullW = (baseWidth > 0) ? baseWidth : 1920;
  const int fullH = (baseHeight > 0) ? baseHeight : 1080;

  if (width == fullW && height == fullH) {
    return TextureSizeClass::FullHD;
  }
  if (width == fullW / 2 && height == fullH / 2) {
    return TextureSizeClass::HalfRes;
  }
  if (width == fullW / 4 && height == fullH / 4) {
    return TextureSizeClass::QuarterRes;
  }
  if (width == 2048 && height == 2048) {
    return TextureSizeClass::Atlas2048;
  }
  if (width == 1024 && height == 1024) {
    return TextureSizeClass::Atlas1024;
  }
  return TextureSizeClass::Custom;
}

struct TexturePoolKey {
  uint32_t internalFormat = 0x8058; // GL_RGBA8 default
  core::QualityTier tier = core::QualityTier::Medium;
  TextureSizeClass sizeClass = TextureSizeClass::FullHD;
  int width = 0;
  int height = 0;
  bool withDepth = false;

  bool operator==(const TexturePoolKey &other) const noexcept {
    return internalFormat == other.internalFormat &&
           tier == other.tier &&
           sizeClass == other.sizeClass &&
           width == other.width &&
           height == other.height &&
           withDepth == other.withDepth;
  }
};

struct TexturePoolKeyHash {
  std::size_t operator()(const TexturePoolKey &key) const noexcept {
    std::size_t h1 = std::hash<uint32_t>{}(key.internalFormat);
    std::size_t h2 = std::hash<int>{}(static_cast<int>(key.tier));
    std::size_t h3 = std::hash<int>{}(static_cast<int>(key.sizeClass));
    std::size_t h4 = std::hash<int>{}(key.width);
    std::size_t h5 = std::hash<int>{}(key.height);
    std::size_t h6 = std::hash<bool>{}(key.withDepth);
    std::size_t seed = h1;
    seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= h3 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= h4 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= h5 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= h6 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
  }
};

class ResizeDebouncer {
public:
  static constexpr double kDebounceWindowSeconds = 0.200; // 200ms debounce window

  void RequestResize(int newWidth, int newHeight, double currentTimeSeconds);
  bool Update(double currentTimeSeconds, int &outAppliedWidth, int &outAppliedHeight);
  void Flush(int &outAppliedWidth, int &outAppliedHeight);
  void Reset(int initialWidth, int initialHeight);

  [[nodiscard]] bool IsDebouncing() const { return m_isDebouncing; }
  [[nodiscard]] int GetEffectiveWidth() const { return m_effectiveWidth; }
  [[nodiscard]] int GetEffectiveHeight() const { return m_effectiveHeight; }
  [[nodiscard]] int GetPendingWidth() const { return m_pendingWidth; }
  [[nodiscard]] int GetPendingHeight() const { return m_pendingHeight; }
  [[nodiscard]] double GetTimeRemaining(double currentTimeSeconds) const;

private:
  int m_effectiveWidth = 0;
  int m_effectiveHeight = 0;
  int m_pendingWidth = 0;
  int m_pendingHeight = 0;
  double m_lastResizeEventTime = 0.0;
  bool m_isDebouncing = false;
};

struct PendingRetireEntry {
  FramebufferHandle handle = {};
  TexturePoolKey key = {};
  uint64_t retireFrameIndex = 0;
  void *fence = nullptr; // GLsync
  bool destroyAfterRetire = false;
  // B3 (P2 AD-8): frames this entry has been polled while the retire fence has
  // NOT signaled. Used to bound the poll window (see kMaxRetireWaitFrames).
  uint64_t waitFrameCount = 0;
};

class GPUTexturePool {
public:
  struct PoolStats {
    uint64_t totalAcquires = 0;
    uint64_t poolHits = 0;
    uint64_t poolMisses = 0;
    size_t activeCount = 0;
    size_t availableCount = 0;
    size_t pendingRetireCount = 0;
    uint64_t trackedBytes = 0;

    [[nodiscard]] float GetHitRate() const {
      const uint64_t total = poolHits + poolMisses;
      return total > 0 ? static_cast<float>(poolHits) / static_cast<float>(total) : 0.0f;
    }
  };

  static GPUTexturePool &Get();

  void BeginFrame(uint64_t frameIndex);
  void EndFrame();
  void Shutdown();

  [[nodiscard]] FramebufferHandle Acquire(const TexturePoolKey &key);
  [[nodiscard]] FramebufferHandle Acquire(int width, int height,
                                          uint32_t internalFormat = 0x8058,
                                          bool withDepth = false,
                                          core::QualityTier tier = core::QualityTier::Medium);
  [[nodiscard]] FramebufferHandle AcquireStandard(TextureSizeClass sizeClass,
                                                  uint32_t internalFormat = 0x8058,
                                                  bool withDepth = false,
                                                  core::QualityTier tier = core::QualityTier::Medium,
                                                  int baseWidth = 1920, int baseHeight = 1080);

  void Release(FramebufferHandle &handle, void *retireFence = nullptr, bool recycleToPool = true);
  void RetireOldResource(FramebufferHandle &handle, void *retireFence = nullptr);

  ResizeDebouncer &GetResizeDebouncer() { return m_debouncer; }
  const ResizeDebouncer &GetResizeDebouncer() const { return m_debouncer; }

  [[nodiscard]] PoolStats GetStats() const;
  void ResetStats();

  void SetMinRetireFrames(uint64_t minFrames) { m_minRetireFrames = minFrames; }
  [[nodiscard]] uint64_t GetMinRetireFrames() const { return m_minRetireFrames; }
  void SetFrameRetention(uint64_t frames) { m_frameRetention = frames; }
  [[nodiscard]] uint64_t GetFrameRetention() const { return m_frameRetention; }

  void EvictStaleEntries();

  // B3 (P2 AD-8): test seam. Injects a sync-poll stub so retire-queue fence
  // handling can be tested deterministically without a GL context (the real
  // GPUUtils::ClientWaitSync is bypassed). Pass nullptr to restore real polling.
  using SyncPollFn = uint32_t (*)(void *sync, uint32_t flags, uint64_t timeout);
  void SetSyncPollForTesting(SyncPollFn fn) { m_syncPollOverride = fn; }

  // Convenience helper for headless testing and tick loops
  void AdvanceFrameForTesting(uint64_t count = 1) {
    for (uint64_t i = 0; i < count; ++i) {
      BeginFrame(m_currentFrameIndex + 1);
      EndFrame();
    }
  }

private:
  GPUTexturePool() = default;
  ~GPUTexturePool();
  GPUTexturePool(const GPUTexturePool &) = delete;
  GPUTexturePool &operator=(const GPUTexturePool &) = delete;

  void ProcessRetireQueue();

  // B3 (P2 AD-8): upper bound on retire-fence polling frames. 60 frames is
  // roughly one second at 60fps — a generous upper bound: a healthy fence
  // signals within a few frames. If it has not signaled within this window the
  // fence is likely broken; we log once and KEEP the entry pending, because
  // destroying the resource while the GPU may still reference it would be an
  // in-flight destroy / use-after-free (the hazard this cap exists to avoid).
  static constexpr uint64_t kMaxRetireWaitFrames = 60;

  struct PoolEntry {
    FramebufferHandle handle;
    uint64_t lastTouchedFrame = 0;
  };

  std::unordered_map<TexturePoolKey, std::vector<PoolEntry>, TexturePoolKeyHash> m_availablePool;
  std::vector<PendingRetireEntry> m_pendingRetire;
  ResizeDebouncer m_debouncer;
  SyncPollFn m_syncPollOverride = nullptr;

  uint64_t m_currentFrameIndex = 0;
  uint64_t m_minRetireFrames = 3;
  uint64_t m_frameRetention = 120;

  uint64_t m_totalAcquires = 0;
  uint64_t m_poolHits = 0;
  uint64_t m_poolMisses = 0;
  size_t m_activeCount = 0;
};

} // namespace NoMoreDay::render::resources
