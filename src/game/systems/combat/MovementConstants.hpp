#pragma once

namespace NoMoreDay::Constants
{
  // 角色移动与姿态设置
  namespace Movement
  {
    constexpr float STANCE_THRESHOLD_SPEED = 50.0f; // 切换“静止/移动”姿态的速度阈值
    constexpr float STANCE_THRESHOLD_SPEED_SQ =
        STANCE_THRESHOLD_SPEED * STANCE_THRESHOLD_SPEED; // 阈值的平方（用于计算）
    constexpr float DEFAULT_REQUIRED_MOVE_TIME =
        2.0f; // 保持某个移动姿态所需的默认持续时间
  } // namespace Movement
} // namespace NoMoreDay::Constants
