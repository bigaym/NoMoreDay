#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace NoMoreDay::render::dev {

struct ShaderWatchEntry {
  std::string debugName;
  std::string vertexPath;
  std::string fragmentPath;
  std::string computePath;
  uint64_t lastWriteHash = 0;
  bool enabled = true;
};

class ShaderHotReloadManager {
public:
  using ReloadCallback = std::function<bool()>;

  void SetEnabled(bool enabled) { m_enabled = enabled; }
  [[nodiscard]] bool IsEnabled() const { return m_enabled; }

  void SetPollIntervalSeconds(double intervalSeconds);
  void Clear();
  void Register(const ShaderWatchEntry &entry, ReloadCallback callback);
  void PollAndReload();

private:
  struct WatchSlot {
    ShaderWatchEntry entry = {};
    ReloadCallback callback = {};
  };

  [[nodiscard]] static uint64_t ComputeWriteHash(const ShaderWatchEntry &entry);

  std::vector<WatchSlot> m_watchSlots = {};
  double m_pollIntervalSeconds = 0.5;
  double m_lastPollTimeSeconds = -1.0;
  bool m_enabled = false;
};

} // namespace NoMoreDay::render::dev
