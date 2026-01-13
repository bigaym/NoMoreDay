// src/game/systems/ui/TutorialSystem.hpp
// 新手教程系统
#pragma once

#include "core/logging/Logger.hpp"
#include <filesystem>
#include <fstream>
#include <functional>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>


namespace NoMoreDay {

/// @brief 教程步骤
struct TutorialStep {
  std::string id;              // 唯一标识
  std::string title;           // 标题
  std::string message;         // 说明文字
  std::string highlight;       // 高亮 UI 元素 ID (可选)
  bool requires_action{false}; // 是否需要玩家操作才能继续

  // 触发条件类型
  enum class Trigger : uint8_t {
    Immediate,       // 立即显示
    OnFirstKill,     // 首次击杀
    OnLevelUp,       // 升级时
    OnItemDrop,      // 物品掉落
    OnOpenInventory, // 打开背包
    OnEquip,         // 装备物品
    OnSkillUse,      // 使用技能
    OnFloorClear     // 清层
  };
  Trigger trigger{Trigger::Immediate};
};

// JSON 序列化
inline void to_json(nlohmann::json &j, const TutorialStep &s) {
  j = {{"id", s.id},
       {"title", s.title},
       {"message", s.message},
       {"highlight", s.highlight},
       {"requires_action", s.requires_action},
       {"trigger", static_cast<int>(s.trigger)}};
}

inline void from_json(const nlohmann::json &j, TutorialStep &s) {
  s.id = j.value("id", "");
  s.title = j.value("title", "");
  s.message = j.value("message", "");
  s.highlight = j.value("highlight", "");
  s.requires_action = j.value("requires_action", false);
  s.trigger = static_cast<TutorialStep::Trigger>(j.value("trigger", 0));
}

/// @brief 教程系统
class TutorialSystem {
public:
  /// 获取单例
  [[nodiscard]] static TutorialSystem &Get() {
    static TutorialSystem instance;
    return instance;
  }

  static constexpr std::string_view kProgressPath =
      "saves/tutorial_progress.json";

  using StepCallback = std::function<void(const TutorialStep &)>;

  /// 加载教程定义
  bool LoadTutorials(std::string_view path = "assets/data/tutorials.json") {
    const std::filesystem::path filepath{path};

    if (!std::filesystem::exists(filepath)) {
      LOG_WARN("[Tutorial] Definition file not found: {}", path);
      return false;
    }

    try {
      std::ifstream file(filepath);
      nlohmann::json j;
      file >> j;

      m_steps.clear();
      if (j.is_array()) {
        for (const auto &sj : j) {
          m_steps.push_back(sj.get<TutorialStep>());
        }
      }

      LOG_INFO("[Tutorial] Loaded {} tutorial steps.", m_steps.size());
      return true;

    } catch (const std::exception &e) {
      LOG_ERROR("[Tutorial] Failed to load tutorials: {}", e.what());
      return false;
    }
  }

  /// 设置显示回调
  void SetDisplayCallback(StepCallback callback) {
    m_displayCallback = std::move(callback);
  }

  /// 设置隐藏回调
  void SetHideCallback(std::function<void()> callback) {
    m_hideCallback = std::move(callback);
  }

  /// 开始教程序列
  void StartTutorial() {
    if (m_steps.empty())
      return;

    m_currentIndex = 0;
    m_isActive = true;
    ShowCurrentStep();
  }

  /// 触发特定类型的教程
  void TriggerStep(TutorialStep::Trigger trigger) {
    if (!m_isActive)
      return;

    // 查找下一个匹配触发器的步骤
    for (size_t i = m_currentIndex; i < m_steps.size(); ++i) {
      if (m_steps[i].trigger == trigger && !IsCompleted(m_steps[i].id)) {
        m_currentIndex = i;
        ShowCurrentStep();
        return;
      }
    }
  }

  /// 完成当前步骤并推进
  void CompleteCurrentStep() {
    if (!m_isActive || m_currentIndex >= m_steps.size())
      return;

    MarkCompleted(m_steps[m_currentIndex].id);

    if (m_hideCallback) {
      m_hideCallback();
    }

    // 找下一个未完成的步骤
    m_currentIndex++;
    while (m_currentIndex < m_steps.size()) {
      const auto &step = m_steps[m_currentIndex];
      if (!IsCompleted(step.id) &&
          step.trigger == TutorialStep::Trigger::Immediate) {
        ShowCurrentStep();
        return;
      }
      m_currentIndex++;
    }

    // 所有立即触发的步骤完成
    LOG_INFO("[Tutorial] All immediate steps completed.");
  }

  /// 跳过教程
  void SkipAll() {
    for (const auto &step : m_steps) {
      MarkCompleted(step.id);
    }
    m_isActive = false;

    if (m_hideCallback) {
      m_hideCallback();
    }

    LOG_INFO("[Tutorial] Tutorial skipped.");
  }

  /// 获取当前步骤
  [[nodiscard]] std::optional<TutorialStep> GetCurrentStep() const {
    if (!m_isActive || m_currentIndex >= m_steps.size()) {
      return std::nullopt;
    }
    return m_steps[m_currentIndex];
  }

  /// 检查步骤是否已完成
  [[nodiscard]] bool IsCompleted(std::string_view id) const {
    return m_completedSteps.count(std::string(id)) > 0;
  }

  /// 检查教程是否全部完成
  [[nodiscard]] bool IsAllCompleted() const {
    for (const auto &step : m_steps) {
      if (!IsCompleted(step.id))
        return false;
    }
    return true;
  }

  [[nodiscard]] bool IsActive() const noexcept { return m_isActive; }

  /// 保存进度
  bool SaveProgress() const {
    const std::filesystem::path filepath{kProgressPath};

    if (filepath.has_parent_path()) {
      std::filesystem::create_directories(filepath.parent_path());
    }

    try {
      nlohmann::json j;
      j["completed"] = std::vector<std::string>(m_completedSteps.begin(),
                                                m_completedSteps.end());

      std::ofstream file(filepath);
      file << j.dump(2);
      return true;

    } catch (...) {
      return false;
    }
  }

  /// 加载进度
  bool LoadProgress() {
    const std::filesystem::path filepath{kProgressPath};

    if (!std::filesystem::exists(filepath)) {
      return true;
    }

    try {
      std::ifstream file(filepath);
      nlohmann::json j;
      file >> j;

      if (j.contains("completed") && j["completed"].is_array()) {
        m_completedSteps.clear();
        for (const auto &id : j["completed"]) {
          m_completedSteps.insert(id.get<std::string>());
        }
      }
      return true;

    } catch (...) {
      return false;
    }
  }

  /// 重置教程进度
  void ResetProgress() {
    m_completedSteps.clear();
    m_currentIndex = 0;
    m_isActive = false;
  }

private:
  TutorialSystem() = default;

  void ShowCurrentStep() {
    if (m_currentIndex >= m_steps.size())
      return;

    const auto &step = m_steps[m_currentIndex];
    LOG_DEBUG("[Tutorial] Showing step: {}", step.id);

    if (m_displayCallback) {
      m_displayCallback(step);
    }
  }

  void MarkCompleted(std::string_view id) {
    m_completedSteps.insert(std::string(id));
  }

  std::vector<TutorialStep> m_steps;
  std::unordered_set<std::string> m_completedSteps;
  size_t m_currentIndex{0};
  bool m_isActive{false};
  StepCallback m_displayCallback;
  std::function<void()> m_hideCallback;
};

} // namespace NoMoreDay
