#include "engine/render/dev/ShaderHotReloadManager.hpp"

#include "core/logging/Logger.hpp"

#include "raylib.h"

#include <filesystem>
#include <functional>

namespace NoMoreDay::render::dev {
namespace {

uint64_t HashPathWriteTime(const std::string &path) {
  if (path.empty()) {
    return 0;
  }
  try {
    if (!std::filesystem::exists(path)) {
      return 0;
    }
    const auto ts = std::filesystem::last_write_time(path).time_since_epoch().count();
    const uint64_t pathHash = std::hash<std::string>{}(path);
    const uint64_t timeHash = static_cast<uint64_t>(ts);
    return (pathHash * 1099511628211ULL) ^ (timeHash + 0x9e3779b97f4a7c15ULL);
  } catch (...) {
    return 0;
  }
}

} // namespace

void ShaderHotReloadManager::SetPollIntervalSeconds(double intervalSeconds) {
  if (intervalSeconds > 0.0) {
    m_pollIntervalSeconds = intervalSeconds;
  }
}

void ShaderHotReloadManager::Clear() {
  m_watchSlots.clear();
  m_lastPollTimeSeconds = -1.0;
}

void ShaderHotReloadManager::Register(const ShaderWatchEntry &entry,
                                      ReloadCallback callback) {
  WatchSlot slot = {};
  slot.entry = entry;
  slot.entry.lastWriteHash = ComputeWriteHash(slot.entry);
  slot.callback = std::move(callback);
  m_watchSlots.push_back(std::move(slot));
}

void ShaderHotReloadManager::PollAndReload() {
  if (!m_enabled || m_watchSlots.empty()) {
    return;
  }

  const double now = GetTime();
  if (m_lastPollTimeSeconds >= 0.0 &&
      (now - m_lastPollTimeSeconds) < m_pollIntervalSeconds) {
    return;
  }
  m_lastPollTimeSeconds = now;

  for (WatchSlot &slot : m_watchSlots) {
    if (!slot.entry.enabled) {
      continue;
    }
    const uint64_t writeHash = ComputeWriteHash(slot.entry);
    if (writeHash == 0 || writeHash == slot.entry.lastWriteHash) {
      continue;
    }

    LOG_INFO("ShaderHotReload: detected change '{}'", slot.entry.debugName);
    bool success = false;
    if (slot.callback) {
      success = slot.callback();
    }
    if (success) {
      LOG_INFO("ShaderHotReload: reload succeeded '{}'", slot.entry.debugName);
    } else {
      LOG_WARN("ShaderHotReload: reload failed '{}', keeping previous shader",
               slot.entry.debugName);
    }
    slot.entry.lastWriteHash = writeHash;
  }
}

uint64_t ShaderHotReloadManager::ComputeWriteHash(const ShaderWatchEntry &entry) {
  uint64_t hash = 1469598103934665603ULL;
  auto mix = [&hash](uint64_t value) {
    hash ^= value;
    hash *= 1099511628211ULL;
  };

  mix(HashPathWriteTime(entry.vertexPath));
  mix(HashPathWriteTime(entry.fragmentPath));
  mix(HashPathWriteTime(entry.computePath));
  return hash;
}

} // namespace NoMoreDay::render::dev
