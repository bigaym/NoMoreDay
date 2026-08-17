#include "engine/render/resources/GPUTexturePool.hpp"
#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/render/GPUUtils.hpp"
#include "core/logging/Logger.hpp"

namespace NoMoreDay::render::resources {

// ============================================================================
// ResizeDebouncer Implementation
// ============================================================================

void ResizeDebouncer::RequestResize(int newWidth, int newHeight,
                                    double currentTimeSeconds) {
  if (newWidth <= 0 || newHeight <= 0) {
    return;
  }

  if (newWidth == m_effectiveWidth && newHeight == m_effectiveHeight) {
    if (m_isDebouncing) {
      m_isDebouncing = false;
      m_pendingWidth = newWidth;
      m_pendingHeight = newHeight;
    }
    return;
  }

  if (!m_isDebouncing || newWidth != m_pendingWidth || newHeight != m_pendingHeight) {
    m_pendingWidth = newWidth;
    m_pendingHeight = newHeight;
    m_lastResizeEventTime = currentTimeSeconds;
    m_isDebouncing = true;
  }
}

bool ResizeDebouncer::Update(double currentTimeSeconds, int &outAppliedWidth,
                             int &outAppliedHeight) {
  if (!m_isDebouncing) {
    return false;
  }

  const double elapsed = currentTimeSeconds - m_lastResizeEventTime;
  if (elapsed >= kDebounceWindowSeconds) {
    m_effectiveWidth = m_pendingWidth;
    m_effectiveHeight = m_pendingHeight;
    m_isDebouncing = false;
    outAppliedWidth = m_effectiveWidth;
    outAppliedHeight = m_effectiveHeight;
    return true;
  }

  return false;
}

void ResizeDebouncer::Flush(int &outAppliedWidth, int &outAppliedHeight) {
  if (m_isDebouncing) {
    m_effectiveWidth = m_pendingWidth;
    m_effectiveHeight = m_pendingHeight;
    m_isDebouncing = false;
  }
  outAppliedWidth = m_effectiveWidth;
  outAppliedHeight = m_effectiveHeight;
}

void ResizeDebouncer::Reset(int initialWidth, int initialHeight) {
  m_effectiveWidth = initialWidth;
  m_effectiveHeight = initialHeight;
  m_pendingWidth = initialWidth;
  m_pendingHeight = initialHeight;
  m_isDebouncing = false;
  m_lastResizeEventTime = 0.0;
}

double ResizeDebouncer::GetTimeRemaining(double currentTimeSeconds) const {
  if (!m_isDebouncing) {
    return 0.0;
  }
  const double remaining = kDebounceWindowSeconds - (currentTimeSeconds - m_lastResizeEventTime);
  return remaining > 0.0 ? remaining : 0.0;
}

// ============================================================================
// GPUTexturePool Implementation
// ============================================================================

GPUTexturePool &GPUTexturePool::Get() {
  static GPUTexturePool s_instance;
  return s_instance;
}

GPUTexturePool::~GPUTexturePool() {
  Shutdown();
}

void GPUTexturePool::BeginFrame(uint64_t frameIndex) {
  m_currentFrameIndex = frameIndex;
  ProcessRetireQueue();
}

void GPUTexturePool::EndFrame() {
  EvictStaleEntries();
}

void GPUTexturePool::Shutdown() {
  for (PendingRetireEntry &entry : m_pendingRetire) {
    if (entry.fence != nullptr) {
      utils::GPUUtils::DeleteSync(entry.fence);
      entry.fence = nullptr;
    }
    if (entry.handle.IsValid()) {
      FramebufferManager::Destroy(entry.handle);
    }
  }
  m_pendingRetire.clear();

  for (auto &[key, entries] : m_availablePool) {
    for (PoolEntry &entry : entries) {
      if (entry.handle.IsValid()) {
        FramebufferManager::Destroy(entry.handle);
      }
    }
  }
  m_availablePool.clear();

  m_activeCount = 0;
  m_totalAcquires = 0;
  m_poolHits = 0;
  m_poolMisses = 0;
}

FramebufferHandle GPUTexturePool::Acquire(const TexturePoolKey &key) {
  if (key.width <= 0 || key.height <= 0) {
    LOG_ERROR("GPUTexturePool::Acquire invalid size {}x{}", key.width, key.height);
    return {};
  }

  m_totalAcquires++;

  auto it = m_availablePool.find(key);
  if (it != m_availablePool.end() && !it->second.empty()) {
    PoolEntry entry = it->second.back();
    it->second.pop_back();
    // H5 (P2 AD-8): keep the handle's tier in sync with the key it was served
    // under so Release() can rebuild the identical bucket key.
    entry.handle.tier = key.tier;
    m_poolHits++;
    m_activeCount++;
    return entry.handle;
  }

  // Cache miss: allocate new backing via FramebufferManager
  m_poolMisses++;
  FramebufferHandle created =
      FramebufferManager::Create(key.width, key.height, key.internalFormat, key.withDepth);
  if (created.IsValid()) {
    // H5 (P2 AD-8): record the Acquire-time tier on the handle; Release() uses
    // it to reconstruct the exact (format, tier, sizeClass) bucket key instead
    // of assuming a hardcoded tier.
    created.tier = key.tier;
    m_activeCount++;
  } else {
    LOG_ERROR("GPUTexturePool: Failed to create texture {}x{} format=0x{:X} tier={}",
              key.width, key.height, key.internalFormat, static_cast<int>(key.tier));
  }
  return created;
}

FramebufferHandle GPUTexturePool::Acquire(int width, int height,
                                         uint32_t internalFormat,
                                         bool withDepth,
                                         core::QualityTier tier) {
  if (width <= 0 || height <= 0) {
    return {};
  }
  const TextureSizeClass sizeClass = ClassifySize(width, height);
  TexturePoolKey key{internalFormat, tier, sizeClass, width, height, withDepth};
  return Acquire(key);
}

FramebufferHandle GPUTexturePool::AcquireStandard(TextureSizeClass sizeClass,
                                                 uint32_t internalFormat,
                                                 bool withDepth,
                                                 core::QualityTier tier,
                                                 int baseWidth,
                                                 int baseHeight) {
  const StandardExtent extent = GetStandardExtent(sizeClass, baseWidth, baseHeight);
  if (extent.width <= 0 || extent.height <= 0) {
    LOG_ERROR("GPUTexturePool::AcquireStandard unsupported standard size class {}",
              static_cast<int>(sizeClass));
    return {};
  }
  TexturePoolKey key{internalFormat, tier, sizeClass, extent.width, extent.height, withDepth};
  return Acquire(key);
}

void GPUTexturePool::Release(FramebufferHandle &handle, void *retireFence,
                             bool recycleToPool) {
  if (!handle.IsValid()) {
    return;
  }

  const TextureSizeClass sizeClass = ClassifySize(handle.width, handle.height);
  // H5 (P2 AD-8): rebuild the exact bucket key. The tier comes from the handle,
  // which GPUTexturePool::Acquire records at creation/reuse time — never a
  // hardcoded tier — so the returned resource lands in the same
  // (format, tier, sizeClass) bucket it was acquired from.
  TexturePoolKey key{
      handle.internalFormat,
      handle.tier,
      sizeClass,
      handle.width,
      handle.height,
      handle.depthRbo != 0};

  // If no external fence provided and GPU context is initialized, place a completion sync fence
  void *syncObject = retireFence;
  if (syncObject == nullptr && utils::GPUUtils::IsInitialized()) {
    constexpr uint32_t kGLSyncGpuCommandsComplete = 0x9117;
    syncObject = utils::GPUUtils::FenceSync(kGLSyncGpuCommandsComplete, 0);
  }

  PendingRetireEntry entry;
  entry.handle = handle;
  entry.key = key;
  entry.retireFrameIndex = m_currentFrameIndex;
  entry.fence = syncObject;
  entry.destroyAfterRetire = !recycleToPool;

  m_pendingRetire.push_back(entry);

  if (m_activeCount > 0) {
    m_activeCount--;
  }

  handle = {};
}

void GPUTexturePool::RetireOldResource(FramebufferHandle &handle, void *retireFence) {
  Release(handle, retireFence, false /*destroyAfterRetire*/);
}

void GPUTexturePool::ProcessRetireQueue() {
  size_t writeIdx = 0;
  for (size_t readIdx = 0; readIdx < m_pendingRetire.size(); ++readIdx) {
    PendingRetireEntry &entry = m_pendingRetire[readIdx];
    const uint64_t frameAge = (m_currentFrameIndex >= entry.retireFrameIndex)
                                  ? (m_currentFrameIndex - entry.retireFrameIndex)
                                  : 0u;

    bool isReady = false;
    if (frameAge >= m_minRetireFrames) {
      if (entry.fence != nullptr) {
        // Non-blocking poll (timeout = 0). Never call glFinish or block CPU execution!
        const uint32_t waitStatus =
            (m_syncPollOverride != nullptr)
                ? m_syncPollOverride(entry.fence, 0, 0)
                : utils::GPUUtils::ClientWaitSync(entry.fence, 0, 0);
        constexpr uint32_t kGLAlreadySignaled = 0x911A;
        constexpr uint32_t kGLTimeoutExpired = 0x911B;
        constexpr uint32_t kGLConditionSatisfied = 0x911C;
        constexpr uint32_t kGLWaitFailed = 0x911D;

        // B3 (P2 AD-8): ONLY a definitive signal retires the resource.
        // GL_TIMEOUT_EXPIRED / GL_WAIT_FAILED keep the entry pending: the GPU
        // may still be referencing it, so retiring now would be an in-flight
        // destroy / use-after-free. Poll again on a later frame instead.
        if (waitStatus == kGLAlreadySignaled || waitStatus == kGLConditionSatisfied) {
          utils::GPUUtils::DeleteSync(entry.fence);
          entry.fence = nullptr;
          isReady = true;
        } else {
          ++entry.waitFrameCount;
          // Safety cap (kMaxRetireWaitFrames, see header): log ONCE when the
          // fence has been polled without signaling for a full second (at 60fps).
          // The entry stays pending — destroying would reintroduce the very
          // in-flight destroy hazard this poll loop exists to prevent.
          if (entry.waitFrameCount == kMaxRetireWaitFrames) {
            LOG_WARN(
                "GPUTexturePool: retire fence not signaled after {} frames of "
                "polling (retired at frame {}, size {}x{}, format 0x{:X}); "
                "keeping resource pending to avoid in-flight destroy",
                entry.waitFrameCount, entry.retireFrameIndex, entry.handle.width,
                entry.handle.height, entry.key.internalFormat);
          }
        }
      } else {
        isReady = true;
      }
    }

    if (isReady) {
      if (entry.destroyAfterRetire) {
        FramebufferManager::Destroy(entry.handle);
      } else {
        m_availablePool[entry.key].push_back({entry.handle, m_currentFrameIndex});
      }
    } else {
      if (writeIdx != readIdx) {
        m_pendingRetire[writeIdx] = entry;
      }
      ++writeIdx;
    }
  }

  m_pendingRetire.resize(writeIdx);
}

void GPUTexturePool::EvictStaleEntries() {
  for (auto &[key, entries] : m_availablePool) {
    size_t writeIdx = 0;
    for (size_t readIdx = 0; readIdx < entries.size(); ++readIdx) {
      PoolEntry &entry = entries[readIdx];
      const uint64_t idleFrames = (m_currentFrameIndex >= entry.lastTouchedFrame)
                                      ? (m_currentFrameIndex - entry.lastTouchedFrame)
                                      : 0u;
      if (idleFrames <= m_frameRetention) {
        if (writeIdx != readIdx) {
          entries[writeIdx] = entry;
        }
        ++writeIdx;
      } else {
        FramebufferManager::Destroy(entry.handle);
      }
    }
    entries.resize(writeIdx);
  }
}

GPUTexturePool::PoolStats GPUTexturePool::GetStats() const {
  PoolStats stats;
  stats.totalAcquires = m_totalAcquires;
  stats.poolHits = m_poolHits;
  stats.poolMisses = m_poolMisses;
  stats.activeCount = m_activeCount;
  stats.pendingRetireCount = m_pendingRetire.size();
  stats.trackedBytes = FramebufferManager::GetTrackedBytes();

  size_t availCount = 0;
  for (const auto &[key, entries] : m_availablePool) {
    availCount += entries.size();
  }
  stats.availableCount = availCount;

  return stats;
}

void GPUTexturePool::ResetStats() {
  m_totalAcquires = 0;
  m_poolHits = 0;
  m_poolMisses = 0;
}

} // namespace NoMoreDay::render::resources
