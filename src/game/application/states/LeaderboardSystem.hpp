// src/game/application/states/LeaderboardSystem.hpp
// 本地排行榜系统 - 记录无尽梦魇模式的最佳成绩
#pragma once

#include "core/logging/Logger.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>


namespace NoMoreDay {

/// @brief 排行榜条目
struct LeaderboardEntry {
  std::string player_name{"玩家"};
  uint32_t highest_floor{0};
  float peak_dps{0.0f};
  uint32_t corruption_reached{0};
  int64_t timestamp{0}; // Unix timestamp

  // 用于排序
  [[nodiscard]] bool operator<(const LeaderboardEntry &other) const noexcept {
    // 先按层数，再按DPS
    if (highest_floor != other.highest_floor) {
      return highest_floor > other.highest_floor; // 降序
    }
    return peak_dps > other.peak_dps;
  }
};

// JSON 序列化
inline void to_json(nlohmann::json &j, const LeaderboardEntry &e) {
  j = {{"name", e.player_name},
       {"floor", e.highest_floor},
       {"dps", e.peak_dps},
       {"corruption", e.corruption_reached},
       {"time", e.timestamp}};
}

inline void from_json(const nlohmann::json &j, LeaderboardEntry &e) {
  e.player_name = j.value("name", std::string{"玩家"});
  e.highest_floor = j.value("floor", 0u);
  e.peak_dps = j.value("dps", 0.0f);
  e.corruption_reached = j.value("corruption", 0u);
  e.timestamp = j.value("time", int64_t{0});
}

/// @brief 本地排行榜系统
class LeaderboardSystem {
public:
  /// 获取单例
  [[nodiscard]] static LeaderboardSystem &Get() {
    static LeaderboardSystem instance;
    return instance;
  }

  static constexpr size_t kMaxEntries = 100;
  static constexpr std::string_view kDefaultPath = "saves/leaderboard.json";

  /// 添加新条目
  void addEntry(LeaderboardEntry entry) {
    // 设置时间戳
    if (entry.timestamp == 0) {
      entry.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
    }

    m_entries.push_back(std::move(entry));

    // 排序并保持最大数量
    std::sort(m_entries.begin(), m_entries.end());
    if (m_entries.size() > kMaxEntries) {
      m_entries.resize(kMaxEntries);
    }

    LOG_INFO("[Leaderboard] Added entry: Floor {}, DPS {:.0f}",
             m_entries.back().highest_floor, m_entries.back().peak_dps);
  }

  /// 获取按层数排序的前N名
  [[nodiscard]] std::vector<LeaderboardEntry> getTopByFloor(size_t n) const {
    auto sorted = m_entries; // Copy for different sort
    std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) {
      return a.highest_floor > b.highest_floor;
    });

    n = std::min(n, sorted.size());
    return std::vector<LeaderboardEntry>(sorted.begin(), sorted.begin() + n);
  }

  /// 获取按DPS排序的前N名
  [[nodiscard]] std::vector<LeaderboardEntry> getTopByDPS(size_t n) const {
    auto sorted = m_entries;
    std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) {
      return a.peak_dps > b.peak_dps;
    });

    n = std::min(n, sorted.size());
    return std::vector<LeaderboardEntry>(sorted.begin(), sorted.begin() + n);
  }

  /// 获取所有条目 (只读)
  [[nodiscard]] std::span<const LeaderboardEntry>
  getAllEntries() const noexcept {
    return m_entries;
  }

  /// 获取条目数量
  [[nodiscard]] size_t size() const noexcept { return m_entries.size(); }

  /// 获取最高层数记录
  [[nodiscard]] uint32_t getHighestFloorRecord() const noexcept {
    if (m_entries.empty())
      return 0;
    return std::max_element(m_entries.begin(), m_entries.end(),
                            [](const auto &a, const auto &b) {
                              return a.highest_floor < b.highest_floor;
                            })
        ->highest_floor;
  }

  /// 获取最高DPS记录
  [[nodiscard]] float getHighestDPSRecord() const noexcept {
    if (m_entries.empty())
      return 0.0f;
    return std::max_element(m_entries.begin(), m_entries.end(),
                            [](const auto &a, const auto &b) {
                              return a.peak_dps < b.peak_dps;
                            })
        ->peak_dps;
  }

  /// 保存到文件
  bool save(std::string_view path = kDefaultPath) const {
    const std::filesystem::path filepath{path};

    // 确保目录存在
    if (filepath.has_parent_path()) {
      std::filesystem::create_directories(filepath.parent_path());
    }

    try {
      nlohmann::json j;
      j["version"] = 1;
      j["entries"] = m_entries;

      std::ofstream file(filepath);
      if (!file.is_open()) {
        LOG_ERROR("[Leaderboard] Failed to create file: {}", path);
        return false;
      }

      file << j.dump(2);
      LOG_INFO("[Leaderboard] Saved {} entries to '{}'", m_entries.size(),
               path);
      return true;

    } catch (const std::exception &e) {
      LOG_ERROR("[Leaderboard] Save failed: {}", e.what());
      return false;
    }
  }

  /// 从文件加载
  bool load(std::string_view path = kDefaultPath) {
    const std::filesystem::path filepath{path};

    if (!std::filesystem::exists(filepath)) {
      LOG_INFO("[Leaderboard] No file found at '{}', starting fresh.", path);
      m_entries.clear();
      return true;
    }

    try {
      std::ifstream file(filepath);
      if (!file.is_open()) {
        LOG_ERROR("[Leaderboard] Failed to open file: {}", path);
        return false;
      }

      nlohmann::json j;
      file >> j;

      m_entries.clear();
      if (j.contains("entries") && j["entries"].is_array()) {
        for (const auto &ej : j["entries"]) {
          m_entries.push_back(ej.get<LeaderboardEntry>());
        }
      }

      // 确保有序
      std::sort(m_entries.begin(), m_entries.end());

      LOG_INFO("[Leaderboard] Loaded {} entries from '{}'", m_entries.size(),
               path);
      return true;

    } catch (const std::exception &e) {
      LOG_ERROR("[Leaderboard] Load failed: {}", e.what());
      return false;
    }
  }

  /// 清空排行榜
  void clear() {
    m_entries.clear();
    LOG_INFO("[Leaderboard] Cleared all entries");
  }

private:
  LeaderboardSystem() = default;

  std::vector<LeaderboardEntry> m_entries;
};

} // namespace NoMoreDay
