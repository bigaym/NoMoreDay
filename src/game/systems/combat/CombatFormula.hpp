#pragma once
#include "game/components/Common.hpp"
#include "game/systems/combat/CombatConstants.hpp"
#include <algorithm>
#include <cmath>

namespace NoMoreDay::CombatFormula {

using namespace NoMoreDay::Constants::Combat;

// 计算等级因子
// level_factor = 10 + 0.5 * areaLevel + 0.05 * areaLevel²
inline float LevelFactor(int area_level) {
  float level = static_cast<float>(area_level);
  return Scaling::LEVEL_BASE + Scaling::LEVEL_LINEAR * level +
         Scaling::LEVEL_QUADRATIC * level * level;
}

// 计算护甲减伤 (正护甲 = 减伤, 负护甲 = 增伤)
// 返回值: [0, 1) 减伤时 (0.5 = 50% DR), >0 增伤时 (返回的是 multiplier
// 增量部分? Spec says "damage_multiplier = 1 + ...") Spec: if >= 0, returns dr.
// if < 0, returns multiplier. Wait, the spec says: if >= 0: damage_reduction =
// effective_armor / (effective_armor + level_factor) if < 0: damage_multiplier
// = 1 + |effective_armor| / (|effective_armor| + level_factor)
//
// API Contract in Plan says:
// CalculateArmorMultiplier(float effective_armor, int area_level);
// Returns: [0, 1) if DR? "Returns: [0, 1) 减伤时, >1 增伤时" ??
// A multiplier usually means "incoming_damage * multiplier".
// If DR is 50%, multiplier is 0.5.
// If valid armor: multiplier = 1 - DR = 1 - (armor / (armor + factor)) = factor
// / (armor + factor). If negative armor: multiplier = 1 + |armor| / (|armor| +
// factor).
//
// Let's stick to "CalculateArmorMultiplier" returning the final float to
// multiply damage by.
//
inline float CalculateArmorMultiplier(float effective_armor, int area_level) {
  float lf = LevelFactor(area_level);
  if (effective_armor >= 0.0f) {
    // Reduction
    // DR = Armor / (Armor + LF)
    // Multiplier = 1 - DR = 1 - Armor/(Armor+LF) = LF / (Armor + LF)
    return lf / (effective_armor + lf);
  } else {
    // Increase
    // Multiplier = 1 + |Armor| / (|Armor| + LF)
    float abs_armor = -effective_armor;
    return 1.0f + abs_armor / (abs_armor + lf);
  }
}

// 计算闪避率
// 返回值: [0, DODGE_MAX_CHANCE]
// dodge_chance = (1 - 1 / (dodge_numerator / level_factor + 1)) * 0.90
inline float CalculateDodgeChance(float dodge_rating, int area_level) {
  if (dodge_rating <= 0.0f)
    return 0.0f;

  float lf = LevelFactor(area_level);
  float numerator =
      Scaling::DODGE_RATING_LINEAR * dodge_rating +
      Scaling::DODGE_RATING_QUADRATIC * dodge_rating * dodge_rating;
  float raw_chance = 1.0f - 1.0f / (numerator / lf + 1.0f);

  return std::min(raw_chance * Scaling::DODGE_MAX_CHANCE,
                  Scaling::DODGE_MAX_CHANCE);
}

// 计算格挡效能
// 返回值: [0, 1) -> Represents DR provided by block
// block_effectiveness = block_amount / (block_amount + level_factor)
inline float CalculateBlockEffectiveness(float block_amount, int area_level) {
  if (block_amount <= 0.0f)
    return 0.0f;
  float lf = LevelFactor(area_level);
  return block_amount / (block_amount + lf);
}

} // namespace NoMoreDay::CombatFormula
