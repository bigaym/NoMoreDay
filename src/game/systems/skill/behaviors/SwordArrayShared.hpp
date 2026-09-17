#pragma once

namespace NoMoreDay {

// 技能 6（剑阵·诛仙）施法基准射程：Baker case 6 的 del.range 初值与行为层
// 未命中烘焙档案时的回退值共用此单一常量，禁止两处字面量各写一份造成双源漂移。
// 节点 603 的 SKILL_RANGE_MULT 在该基准上放大。
inline constexpr float kSwordArrayBaseCastRange = 400.0f;

} // namespace NoMoreDay
