#pragma once

#include "engine/render/resources/FramebufferHandle.hpp"
#include "raylib.h"
#include <cstdint>
#include <vector>

namespace NoMoreDay::render::resources {

class TransientResourcePool {
public:
  TransientResourcePool() = default;
  ~TransientResourcePool();

  FramebufferHandle AcquireColorTarget(int width, int height,
                                       uint32_t internalFormat = 0x8058);
  FramebufferHandle AcquireAliasedColorTarget(int width, int height,
                                              uint32_t internalFormat,
                                              uint32_t aliasGroupId);
  void BeginFrame();
  void EndFrame();
  void Shutdown();

  void SetAliasingEnabled(bool enabled) { m_aliasingEnabled = enabled; }
  bool IsAliasingEnabled() const { return m_aliasingEnabled; }

  size_t GetPoolSize() const { return m_entries.size(); }
  size_t GetAliasedReuseCount() const { return m_aliasedReuseCount; }

private:
  struct Entry {
    FramebufferHandle handle = {};
    int width = 0;
    int height = 0;
    uint32_t internalFormat = 0x8058;
    uint32_t aliasGroupId = 0;
    bool inUse = false;
    uint64_t lastTouchedFrame = 0;
  };

  std::vector<Entry> m_entries;
  uint64_t m_frameIndex = 0;
  bool m_aliasingEnabled = false;
  size_t m_aliasedReuseCount = 0;
};

} // namespace NoMoreDay::render::resources
