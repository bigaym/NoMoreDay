#include "engine/render/resources/TransientResourcePool.hpp"

#include "engine/render/resources/FramebufferManager.hpp"

namespace NoMoreDay::render::resources {
namespace {
constexpr uint64_t kFrameRetention = 120;
} // namespace

TransientResourcePool::~TransientResourcePool() { Shutdown(); }

FramebufferHandle TransientResourcePool::AcquireColorTarget(int width, int height,
                                                            uint32_t internalFormat) {
  if (width <= 0 || height <= 0) {
    return {};
  }

  for (Entry &entry : m_entries) {
    if (!entry.inUse && entry.width == width && entry.height == height &&
        entry.internalFormat == internalFormat) {
      entry.inUse = true;
      entry.lastTouchedFrame = m_frameIndex;
      return entry.handle;
    }
  }

  Entry created = {};
  created.handle =
      FramebufferManager::Create(width, height, internalFormat, false);
  created.width = width;
  created.height = height;
  created.internalFormat = internalFormat;
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
      FramebufferManager::Destroy(m_entries[readIndex].handle);
    }
  }

  m_entries.resize(writeIndex);
}

void TransientResourcePool::Shutdown() {
  for (Entry &entry : m_entries) {
    FramebufferManager::Destroy(entry.handle);
  }
  m_entries.clear();
}

} // namespace NoMoreDay::render::resources
