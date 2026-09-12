#pragma once

#include "game/foundation/components/Stats.hpp"

#include <cstddef>

namespace NoMoreDay {
namespace damage {

// 元素池数量：对应 DamageType 的 0..5（Physical/Fire/Cold/Lightning/
// Poison/Shadow），亦是按池索引对齐数组的长度。DamageType::True/Count 不在池内。
inline constexpr int kElementCount = 6;

// 伤害元素存在两条不同的“位序”，仅 Poison/Shadow 两位互换，必须显式映射：
//
//   位序 A —— DamagePool / Tag 位序（结算内核的 j）
//     Tag::Physical=1<<0, Fire=1<<1, Cold=1<<2, Lightning=1<<3,
//     Shadow=1<<4, Poison=1<<5
//     => 池索引 4=Shadow、5=Poison
//
//   位序 B —— DamageType 与 CombatStats.resistances[] 的索引
//     DamageType::Physical=0, Fire=1, Cold=2, Lightning=3, Poison=4, Shadow=5
//     resistances[] 由 StatType::ResistPhysical+i 填充，故索引 4=Poison、5=Shadow
//
// 0~3 两条位序一致，只有 4/5 互换。任何“以池索引 j 去读 DamageType 序数组
// 或去构造 DamageType”的地方都必须经过本文件映射，否则 Shadow/Poison 抗性
// 会被错配。

// 池索引（Tag 位序）→ DamageType（枚举序）。越界返回 DamageType::Count。
[[nodiscard]] constexpr DamageType PoolIndexToDamageType(size_t pool_index) {
  switch (pool_index) {
  case 0:
    return DamageType::Physical;
  case 1:
    return DamageType::Fire;
  case 2:
    return DamageType::Cold;
  case 3:
    return DamageType::Lightning;
  case 4: // 池索引 4 是 Shadow，而 DamageType::Shadow 枚举值为 5
    return DamageType::Shadow;
  case 5: // 池索引 5 是 Poison，而 DamageType::Poison 枚举值为 4
    return DamageType::Poison;
  default:
    return DamageType::Count;
  }
}

// 池索引（Tag 位序）→ CombatStats.resistances[] / StatType 序索引。
// 越界返回 DamageType::Count（即 kElementCount），调用方需自行判界。
[[nodiscard]] constexpr size_t PoolIndexToResistIndex(size_t pool_index) {
  switch (pool_index) {
  case 0:
    return 0; // Physical
  case 1:
    return 1; // Fire
  case 2:
    return 2; // Cold
  case 3:
    return 3; // Lightning
  case 4:
    return 5; // Shadow 在 resistances[] 中位于索引 5
  case 5:
    return 4; // Poison 在 resistances[] 中位于索引 4
  default:
    return static_cast<size_t>(DamageType::Count);
  }
}

// DamageType（枚举序）→ 池索引（Tag 位序）。用于“已知 DamageType、需回读
// 按池索引对齐的数组”的少量场景，语义与 PoolIndexToDamageType 互逆。
[[nodiscard]] constexpr size_t DamageTypeToPoolIndex(DamageType type) {
  switch (static_cast<size_t>(type)) {
  case 0:
    return 0;
  case 1:
    return 1;
  case 2:
    return 2;
  case 3:
    return 3;
  case 4: // DamageType::Poison 对应池索引 5
    return 5;
  case 5: // DamageType::Shadow 对应池索引 4
    return 4;
  default:
    return static_cast<size_t>(DamageType::Count);
  }
}

// DamageType（枚举序）→ 伤害元素 Tag（池位序）。两者仅 Poison/Shadow 两位
// 互换，必须经 DamageTypeToPoolIndex 映射，禁止 1ULL << 枚举值的裸位移
// （否则 DamageType::Shadow(5) 会错映射为 Tag::Poison）。
// 非元素类型（True/Count/越界）返回 Tag::None。
[[nodiscard]] constexpr Tag ElementTagOf(DamageType type) {
  const size_t pool_index = DamageTypeToPoolIndex(type);
  if (pool_index >= static_cast<size_t>(kElementCount)) {
    return Tag::None;
  }
  return static_cast<Tag>(1ULL << pool_index);
}

} // namespace damage
} // namespace NoMoreDay
