/*
 * NoMoreDay - Player Profile Components
 * (c) 2026 NoMoreDay Team
 */
#pragma once

#include <algorithm>
#include <cstdint>
#include <string>

namespace NoMoreDay {

/**
 * @brief Component for storing player's chosen name.
 */
struct PlayerName {
  std::string value = "玩家0";
};

/**
 * @brief Component for tracking lifetime playtime in seconds.
 */
struct PlayerPlaytime {
  int64_t accumulated_seconds = 0;
  double session_start_time = 0.0;

  [[nodiscard]] int64_t NonNegativeAccumulated() const {
    return (std::max)(int64_t{0}, accumulated_seconds);
  }
};

} // namespace NoMoreDay
