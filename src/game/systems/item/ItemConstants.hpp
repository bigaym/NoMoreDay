#pragma once

#include <algorithm> // std::max

namespace NoMoreDay::Constants {

  // 战斗系统核心常量
  // 掉落与装备系统常量
  namespace Items
  {
    constexpr float LEVEL_SCALING_FACTOR = 1.5f; // 100级时的额外属性增幅 (1.0 + 1.5 = 2.5x)
    constexpr int MAX_ITEM_LEVEL = 100;
    inline float GetLevelMultiplier(int level)
    {
      return 1.0f + (float)std::max(0, level - 1) * (LEVEL_SCALING_FACTOR / 99.0f);
    }
  } // namespace Items

  // 物品、背包与拾取相关
  namespace Item
  {
    constexpr float DROP_OFFSET_X = 20.0f; // 物品掉落时的横向飞散偏移
    constexpr float PICKUP_VISUAL_EFFECT_DURATION =
        0.3f;                                       // 拾取物品时的视觉动画时长
    constexpr float POTION_DEFAULT_COOLDOWN = 1.0f; // 药水的基础使用冷却
    constexpr float POTION_HEAL_AMOUNT = 50.0f;     // 测试用药水的基础回复量
    constexpr float POTION_MANA_AMOUNT = 50.0f;     // 测试用药水的基础回蓝量
    constexpr float SORT_COOLDOWN = 1.0f;           // 整理背包的冷却时间

    constexpr float DEFAULT_PICKUP_RANGE = 75.0f;       // 地面上物品的默认交互/拾取半径
    constexpr float MIN_EFFECTIVE_PICKUP_RANGE = 50.0f; // 有效拾取的最小距离限制
  } // namespace Item

} // namespace NoMoreDay::Constants
