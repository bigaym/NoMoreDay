// src/game/systems/world/CorruptionSystem.hpp
// 腐化系统 - 管理无尽梦魇模式的难度递增
#pragma once

#include "core/logging/Logger.hpp"
#include <cmath>
#include <cstdint>
#include <nlohmann/json.hpp>


namespace NoMoreDay {

/// @brief 腐化系统 - 管理无尽梦魇的难度递增和掉落加成
/// @details 每过一层增加腐化值,怪物属性和掉落品质随之提升
class CorruptionSystem {
public:
  /// 获取单例实例
  [[nodiscard]] static CorruptionSystem &Get() {
    static CorruptionSystem instance;
    return instance;
  }

  // 基础配置
  static constexpr uint32_t kCorruptionPerFloor = 5; // 每层增加的腐化值
  static constexpr float kTierStatExponent = 1.08f;  // 每层属性倍率底数
  static constexpr float kDoubleT7ChancePerCorruption =
      0.001f; // 每点腐化的双T7几率

  /// 获取当前腐化值
  [[nodiscard]] constexpr uint32_t getCorruption() const noexcept {
    return m_corruption;
  }

  /// 获取当前层数
  [[nodiscard]] constexpr uint32_t getCurrentFloor() const noexcept {
    return m_currentFloor;
  }

  /// 推进到下一层
  void advanceFloor() noexcept {
    m_currentFloor++;
    m_corruption += kCorruptionPerFloor;
    LOG_DEBUG("[CorruptionSystem] Advanced to floor {}, corruption: {}",
              m_currentFloor, m_corruption);
  }

  /// 增加额外腐化值 (来自特殊事件)
  void addCorruption(uint32_t amount) noexcept { m_corruption += amount; }

  /// 计算怪物属性倍率
  /// @details Stats = Base × (tierExponent^floor) × (1 + Corruption/100)
  [[nodiscard]] float calculateStatMultiplier() const noexcept {
    const float tier_mult =
        std::pow(kTierStatExponent, static_cast<float>(m_currentFloor));
    const float corruption_mult =
        1.0f + (static_cast<float>(m_corruption) / 100.0f);
    return tier_mult * corruption_mult;
  }

  /// 计算怪物生命倍率 (比属性增长更快)
  /// @details HP = Base × (1.12^floor) × (1 + Corruption/50)
  [[nodiscard]] float calculateHealthMultiplier() const noexcept {
    const float floor_mult =
        std::pow(1.12f, static_cast<float>(m_currentFloor));
    const float corruption_mult =
        1.0f + (static_cast<float>(m_corruption) / 50.0f);
    return floor_mult * corruption_mult;
  }

  /// 计算 T7 双词缀几率
  /// @return 几率值 [0.0, 1.0]
  [[nodiscard]] float calculateDoubleT7Chance() const noexcept {
    return std::min(1.0f, static_cast<float>(m_corruption) *
                              kDoubleT7ChancePerCorruption);
  }

  /// 计算额外装备掉落率加成
  /// @return 加成百分比 (例如 0.5 = +50% 掉落率)
  [[nodiscard]] float calculateDropRateBonus() const noexcept {
    // 每 20 腐化 +10% 掉落率
    return static_cast<float>(m_corruption) / 20.0f * 0.1f;
  }

  /// 计算经验值加成
  [[nodiscard]] float calculateXPMultiplier() const noexcept {
    // 每层 +5% 经验, 腐化每 10 点 +1%
    return 1.0f + (m_currentFloor * 0.05f) + (m_corruption * 0.001f);
  }

  /// 判断当前层是否为Boss层 (每10层一个Boss)
  [[nodiscard]] bool isBossFloor() const noexcept {
    return m_currentFloor > 0 && (m_currentFloor % 10 == 0);
  }

  /// 重置系统 (新游戏)
  void reset() noexcept {
    m_corruption = 0;
    m_currentFloor = 0;
    m_highestFloorReached = 0;
    m_peakDPS = 0.0f;
    LOG_INFO("[CorruptionSystem] Reset to initial state");
  }

  /// 记录最高层数
  void recordHighestFloor() noexcept {
    if (m_currentFloor > m_highestFloorReached) {
      m_highestFloorReached = m_currentFloor;
    }
  }

  /// 记录峰值DPS
  void recordPeakDPS(float dps) noexcept {
    if (dps > m_peakDPS) {
      m_peakDPS = dps;
    }
  }

  [[nodiscard]] uint32_t getHighestFloor() const noexcept {
    return m_highestFloorReached;
  }
  [[nodiscard]] float getPeakDPS() const noexcept { return m_peakDPS; }

  // JSON 序列化
  [[nodiscard]] nlohmann::json toJson() const {
    return {{"corruption", m_corruption},
            {"currentFloor", m_currentFloor},
            {"highestFloor", m_highestFloorReached},
            {"peakDPS", m_peakDPS}};
  }

  void fromJson(const nlohmann::json &j) {
    m_corruption = j.value("corruption", 0u);
    m_currentFloor = j.value("currentFloor", 0u);
    m_highestFloorReached = j.value("highestFloor", 0u);
    m_peakDPS = j.value("peakDPS", 0.0f);
  }

private:
  CorruptionSystem() = default;

  uint32_t m_corruption{0};
  uint32_t m_currentFloor{0};
  uint32_t m_highestFloorReached{0};
  float m_peakDPS{0.0f};
};

} // namespace NoMoreDay
