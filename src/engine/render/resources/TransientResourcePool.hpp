#pragma once

#include "raylib.h"
#include <cstdint>
#include <vector>

namespace NoMoreDay::render::resources {

class TransientResourcePool {
public:
  TransientResourcePool() = default;
  ~TransientResourcePool();

  RenderTexture2D AcquireColorTarget(int width, int height);
  void BeginFrame();
  void EndFrame();
  void Shutdown();

  size_t GetPoolSize() const { return m_entries.size(); }

private:
  struct Entry {
    RenderTexture2D texture = {};
    int width = 0;
    int height = 0;
    bool inUse = false;
    uint64_t lastTouchedFrame = 0;
  };

  std::vector<Entry> m_entries;
  uint64_t m_frameIndex = 0;
};

} // namespace NoMoreDay::render::resources
