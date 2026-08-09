// src/game/application/states/AchievementSystem.hpp
// 成就系统框架
#pragma once

#include "core/logging/Logger.hpp"
#include <filesystem>
#include <fstream>
#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace NoMoreDay {

/// @brief 成就条目
struct Achievement {
  std::string id;          // 唯一标识
  std::string name;        // 显示名称
  std::string description; // 描述文本
  std::string icon;        // 图标路径
  bool unlocked{false};    // 是否已解锁
  int64_t unlock_time{0};  // 解锁时间戳

  // 进度类成就
  int current_progress{0};
  int target_progress{1}; // 目标值 (1 = 无进度成就)

  /// 检查是否完成
  [[nodiscard]] bool isComplete() const noexcept {
    return current_progress >= target_progress;
  }

  /// 获取进度百分比
  [[nodiscard]] float getProgressPercent() const noexcept {
    if (target_progress <= 0)
      return 0.0f;
    return std::min(1.0f,
                    static_cast<float>(current_progress) / target_progress);
  }
};

// JSON 序列化
inline void to_json(nlohmann::json &j, const Achievement &a) {
  j = {{"id", a.id},
       {"name", a.name},
       {"desc", a.description},
       {"icon", a.icon},
       {"unlocked", a.unlocked},
       {"unlock_time", a.unlock_time},
       {"progress", a.current_progress},
       {"target", a.target_progress}};
}

inline void from_json(const nlohmann::json &j, Achievement &a) {
  a.id = j.value("id", "");
  a.name = j.value("name", "");
  a.description = j.value("desc", "");
  a.icon = j.value("icon", "");
  a.unlocked = j.value("unlocked", false);
  a.unlock_time = j.value("unlock_time", int64_t{0});
  a.current_progress = j.value("progress", 0);
  a.target_progress = j.value("target", 1);
}

/// @brief 成就系统
class AchievementSystem {
public:
  /// 获取单例
  [[nodiscard]] static AchievementSystem &Get() {
    static AchievementSystem instance;
    return instance;
  }

  static constexpr std::string_view kSavePath = "saves/achievements.json";

  using UnlockCallback = std::function<void(const Achievement &)>;

  /// 注册成就定义
  void RegisterAchievement(Achievement achievement) {
    m_achievements[achievement.id] = std::move(achievement);
  }

  /// 设置解锁回调 (用于 UI 通知)
  void SetUnlockCallback(UnlockCallback callback) {
    m_unlockCallback = std::move(callback);
  }

  /// 解锁成就
  bool Unlock(std::string_view id) {
    auto it = m_achievements.find(std::string(id));
    if (it == m_achievements.end()) {
      LOG_WARN("[Achievement] Unknown achievement: {}", id);
      return false;
    }

    if (it->second.unlocked) {
      return false; // 已解锁
    }

    it->second.unlocked = true;
    it->second.unlock_time =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    it->second.current_progress = it->second.target_progress;

    LOG_INFO("[Achievement] Unlocked: {} - {}", it->second.name,
             it->second.description);

    if (m_unlockCallback) {
      m_unlockCallback(it->second);
    }

    return true;
  }

  /// 更新成就进度
  bool UpdateProgress(std::string_view id, int amount = 1) {
    auto it = m_achievements.find(std::string(id));
    if (it == m_achievements.end()) {
      return false;
    }

    if (it->second.unlocked) {
      return false;
    }

    it->second.current_progress = std::min(it->second.current_progress + amount,
                                           it->second.target_progress);

    // 自动解锁
    if (it->second.isComplete()) {
      return Unlock(id);
    }

    return false;
  }

  /// 设置成就进度 (绝对值)
  void SetProgress(std::string_view id, int value) {
    auto it = m_achievements.find(std::string(id));
    if (it == m_achievements.end() || it->second.unlocked) {
      return;
    }

    it->second.current_progress = std::min(value, it->second.target_progress);

    if (it->second.isComplete()) {
      Unlock(id);
    }
  }

  /// 获取成就
  [[nodiscard]] const Achievement *GetAchievement(std::string_view id) const {
    auto it = m_achievements.find(std::string(id));
    return (it != m_achievements.end()) ? &it->second : nullptr;
  }

  /// 获取所有成就
  [[nodiscard]] std::vector<const Achievement *> GetAllAchievements() const {
    std::vector<const Achievement *> result;
    result.reserve(m_achievements.size());
    for (const auto &[id, ach] : m_achievements) {
      result.push_back(&ach);
    }
    return result;
  }

  /// 获取已解锁成就
  [[nodiscard]] std::vector<const Achievement *>
  GetUnlockedAchievements() const {
    std::vector<const Achievement *> result;
    for (const auto &[id, ach] : m_achievements) {
      if (ach.unlocked) {
        result.push_back(&ach);
      }
    }
    return result;
  }

  /// 获取解锁进度
  [[nodiscard]] float GetCompletionPercent() const {
    if (m_achievements.empty())
      return 0.0f;

    size_t unlocked = 0;
    for (const auto &[id, ach] : m_achievements) {
      if (ach.unlocked)
        unlocked++;
    }
    return static_cast<float>(unlocked) / m_achievements.size();
  }

  /// 保存到文件
  bool Save() const {
    const std::filesystem::path filepath{kSavePath};

    if (filepath.has_parent_path()) {
      std::filesystem::create_directories(filepath.parent_path());
    }

    try {
      nlohmann::json j;
      j["version"] = 1;

      nlohmann::json achievements = nlohmann::json::array();
      for (const auto &[id, ach] : m_achievements) {
        achievements.push_back(ach);
      }
      j["achievements"] = achievements;

      std::ofstream file(filepath);
      if (!file.is_open()) {
        LOG_ERROR("[Achievement] Failed to save to {}", std::string(kSavePath));
        return false;
      }

      file << j.dump(2);
      LOG_INFO("[Achievement] Saved {} achievements.", m_achievements.size());
      return true;

    } catch (const std::exception &e) {
      LOG_ERROR("[Achievement] Save failed: {}", e.what());
      return false;
    }
  }

  /// 从文件加载
  bool Load() {
    const std::filesystem::path filepath{kSavePath};

    if (!std::filesystem::exists(filepath)) {
      LOG_INFO("[Achievement] No save file found, using defaults.");
      return true;
    }

    try {
      std::ifstream file(filepath);
      if (!file.is_open()) {
        LOG_ERROR("[Achievement] Failed to open {}", std::string(kSavePath));
        return false;
      }

      nlohmann::json j;
      file >> j;

      if (j.contains("achievements") && j["achievements"].is_array()) {
        for (const auto &aj : j["achievements"]) {
          Achievement loaded = aj.get<Achievement>();

          // 合并到现有定义 (保留进度)
          auto it = m_achievements.find(loaded.id);
          if (it != m_achievements.end()) {
            it->second.unlocked = loaded.unlocked;
            it->second.unlock_time = loaded.unlock_time;
            it->second.current_progress = loaded.current_progress;
          }
        }
      }

      LOG_INFO("[Achievement] Loaded save data.");
      return true;

    } catch (const std::exception &e) {
      LOG_ERROR("[Achievement] Load failed: {}", e.what());
      return false;
    }
  }

  /// 重置所有成就进度 (保留注册)
  void ResetAll() {
    for (auto &[id, ach] : m_achievements) {
      ach.unlocked = false;
      ach.unlock_time = 0;
      ach.current_progress = 0;
    }
    LOG_INFO("[Achievement] Reset all progress.");
  }

  /// 清空所有成就 (包括注册)
  void ClearAll() {
    m_achievements.clear();
    m_unlockCallback = nullptr;
  }

private:
  AchievementSystem() = default;

  std::unordered_map<std::string, Achievement> m_achievements;
  UnlockCallback m_unlockCallback;
};

/// @brief 预定义成就 ID
namespace Achievements {
// 进度类
constexpr std::string_view kKill100Enemies = "kill_100_enemies";
constexpr std::string_view kKill1000Enemies = "kill_1000_enemies";
constexpr std::string_view kReachFloor10 = "reach_floor_10";
constexpr std::string_view kReachFloor50 = "reach_floor_50";
constexpr std::string_view kReachFloor100 = "reach_floor_100";

// 单次事件
constexpr std::string_view kFirstBlood = "first_blood";
constexpr std::string_view kFirstBoss = "first_boss";
constexpr std::string_view kFirstLegendary = "first_legendary";
constexpr std::string_view kFirstHeirloom = "first_heirloom";
constexpr std::string_view kMaxCorruption = "max_corruption";
} // namespace Achievements

} // namespace NoMoreDay
