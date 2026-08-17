#include "engine/render/resources/TransientResourcePool.hpp"

#include "engine/render/resources/FramebufferManager.hpp"
#include "engine/render/resources/GPUTexturePool.hpp"
#include "engine/render/GPUUtils.hpp"
#include "core/logging/Logger.hpp"

namespace NoMoreDay::render::resources {
namespace {
constexpr uint64_t kFrameRetention = 120;
} // namespace

TransientResourcePool::~TransientResourcePool() { Shutdown(); }

FramebufferHandle TransientResourcePool::AcquireColorTarget(int width, int height,
                                                            uint32_t internalFormat) {
  return AcquireAliasedColorTarget(width, height, internalFormat, 0);
}

FramebufferHandle TransientResourcePool::AcquireAliasedColorTarget(int width, int height,
                                                                   uint32_t internalFormat,
                                                                   uint32_t aliasGroupId) {
  if (width <= 0 || height <= 0) {
    return {};
  }

  // 1. If aliasing is enabled and an aliasGroupId is provided, prioritize matching alias group
  if (m_aliasingEnabled && aliasGroupId != 0) {
    for (Entry &entry : m_entries) {
      if (!entry.inUse && entry.width == width && entry.height == height &&
          entry.internalFormat == internalFormat && entry.aliasGroupId == aliasGroupId) {
        entry.inUse = true;
        entry.lastTouchedFrame = m_frameIndex;
        m_aliasedReuseCount++;
        return entry.handle;
      }
    }
  }

  // 2. Standard exact match path.
  // M2: only reuse an entry whose alias-group ownership is compatible
  // (unassigned group 0, or the same group). Previously this path blindly
  // overwrote entry.aliasGroupId, so a different alias group could steal an
  // entry that belongs to an active group, corrupting group attribution when
  // two groups are active in the same frame (it was latent only because
  // aliasGroupId was always 0 in practice).
  for (Entry &entry : m_entries) {
    if (!entry.inUse && entry.width == width && entry.height == height &&
        entry.internalFormat == internalFormat &&
        (entry.aliasGroupId == 0 || entry.aliasGroupId == aliasGroupId)) {
      entry.inUse = true;
      entry.aliasGroupId = aliasGroupId;
      entry.lastTouchedFrame = m_frameIndex;
      return entry.handle;
    }
  }

  Entry created = {};
  created.handle =
      GPUTexturePool::Get().Acquire(width, height, internalFormat, false);
  created.width = width;
  created.height = height;
  created.internalFormat = internalFormat;
  created.aliasGroupId = aliasGroupId;
  created.inUse = true;
  created.lastTouchedFrame = m_frameIndex;

  if (!created.handle.IsValid()) {
    LOG_ERROR(
        "TransientResourcePool: Failed to create render target {}x{} format=0x{:X}",
        width, height, internalFormat);
    return {};
  }

  m_entries.push_back(created);
  return created.handle;
}

void TransientResourcePool::BeginFrame() {
  ++m_frameIndex;
  for (Entry &entry : m_entries) {
    entry.inUse = false;
  }
}

void TransientResourcePool::EndFrame() {
  const auto keepIt = [this](Entry &entry) {
    if (entry.inUse) {
      return true;
    }
    return (m_frameIndex - entry.lastTouchedFrame) <= kFrameRetention;
  };

  size_t writeIndex = 0;
  for (size_t readIndex = 0; readIndex < m_entries.size(); ++readIndex) {
    if (keepIt(m_entries[readIndex])) {
      if (writeIndex != readIndex) {
        m_entries[writeIndex] = m_entries[readIndex];
      }
      ++writeIndex;
    } else {
      if (!NoMoreDay::utils::GPUUtils::IsInitialized()) {
        FramebufferManager::Destroy(m_entries[readIndex].handle);
      } else {
        GPUTexturePool::Get().RetireOldResource(m_entries[readIndex].handle);
      }
    }
  }

  m_entries.resize(writeIndex);
}

void TransientResourcePool::Shutdown() {
  for (Entry &entry : m_entries) {
    if (!NoMoreDay::utils::GPUUtils::IsInitialized()) {
      FramebufferManager::Destroy(entry.handle);
    } else {
      GPUTexturePool::Get().RetireOldResource(entry.handle);
    }
  }
  m_entries.clear();
}

} // namespace NoMoreDay::render::resources
