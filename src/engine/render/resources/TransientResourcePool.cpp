#include "engine/render/resources/TransientResourcePool.hpp"

namespace NoMoreDay::render::resources {
namespace {
constexpr uint64_t kFrameRetention = 120;
} // namespace

TransientResourcePool::~TransientResourcePool() { Shutdown(); }

RenderTexture2D TransientResourcePool::AcquireColorTarget(int width, int height) {
  if (width <= 0 || height <= 0) {
    return {};
  }

  for (Entry &entry : m_entries) {
    if (!entry.inUse && entry.width == width && entry.height == height) {
      entry.inUse = true;
      entry.lastTouchedFrame = m_frameIndex;
      return entry.texture;
    }
  }

  Entry created = {};
  created.texture = LoadRenderTexture(width, height);
  created.width = width;
  created.height = height;
  created.inUse = true;
  created.lastTouchedFrame = m_frameIndex;

  if (created.texture.id == 0) {
    LOG_ERROR("TransientResourcePool: Failed to create render target {}x{}",
              width, height);
    return {};
  }

  m_entries.push_back(created);
  return created.texture;
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
      UnloadRenderTexture(m_entries[readIndex].texture);
    }
  }

  m_entries.resize(writeIndex);
}

void TransientResourcePool::Shutdown() {
  for (Entry &entry : m_entries) {
    if (entry.texture.id != 0) {
      UnloadRenderTexture(entry.texture);
      entry.texture = {};
    }
  }
  m_entries.clear();
}

} // namespace NoMoreDay::render::resources
