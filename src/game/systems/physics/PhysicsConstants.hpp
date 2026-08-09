#pragma once

namespace NoMoreDay::Constants
{
  // 物理与碰撞处理常量
  namespace Physics
  {
    constexpr float DEFAULT_ENTITY_RADIUS = 5.0f;    // 实体默认碰撞半径
    constexpr float SEPARATION_DIST_MULT = 2.0f;     // 实体间分离距离倍数
    constexpr float REPULSION_STRENGTH = 200.0f;     // 实体间互相排斥的力度
    constexpr float MAX_VELOCITY = 2000.0f;          // 最大速度限制
    constexpr float EPSILON_VELOCITY = 0.001f;       // 速度归一化/停止判定阈值
    constexpr float MIN_DIST_SQ_THRESHOLD = 0.0001f; // 距离计算的极小过滤阈值

    // [NEW] Added from audit (2026-01-24)
    constexpr float WALL_REPULSION_FACTOR = 20.0f;      // 墙壁排斥力系数
    constexpr float ENTITY_DAMPING_FACTOR = 0.92f;      // 瀹炰綋闃诲凹 (绌烘皵闃诲姏)
    constexpr float CCD_STEP_SIZE = 10.0f;              // 杩炵画纰版挒妫€娴嬫闀
    constexpr float MAP_COLLISION_RADIUS_FACTOR = 0.8f; // 鍦板浘纰版挒鍗婂緞缂╁皬绯鏁
  } // namespace Physics
} // namespace NoMoreDay::Constants
