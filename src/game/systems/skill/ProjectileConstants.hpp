#pragma once

namespace NoMoreDay::Constants {

  // 技能、投射物与特殊机制设置
  namespace Skill
  {
    constexpr float PROJECTILE_RETURN_THRESHOLD =
        20.0f; // 回旋投射物返回持有者的判定半径
    constexpr float PROJECTILE_DEFAULT_RETURN_SPEED =
        800.0f; // 回旋投射物的返回初速度
    constexpr float PROJECTILE_PULL_RADIUS_MULTIPLIER =
        3.0f;                                           // 投射物吸引周围目标的半径倍数
    constexpr float PROJECTILE_TRAIL_VEL_SCALE = -0.1f; // 拖尾特效的初速度缩放
    constexpr float PROJECTILE_ROTATING_TRAIL_RADIUS =
        10.0f; // 旋转投射物特效的轨道半径
    constexpr float PROJECTILE_COLLISION_RADIUS_OFFSET =
        10.0f;                                    // 投射物碰撞体相对于贴图的偏移量
    constexpr float PROJECTILE_MIN_DAMAGE = 5.0f; // 投射单次最低伤害

    namespace BladeWard
    {
      constexpr float BASE_INTERCEPTION_CHANCE =
          0.15f; // “剑气护体”拦截投射物的初始几率
    }
  } // namespace Skill

} // namespace NoMoreDay::Constants
