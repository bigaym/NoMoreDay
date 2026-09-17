#pragma once

#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"

#include <cstdint>

namespace NoMoreDay {

// 技能 7 引导射程缺省基准：烘焙器（SkillSpecializationBaker case 7）与解析回退
// 必须共用同一常量，禁止各自写字面量导致缺省值漂移。
inline constexpr float kBeamChannelBaseRangeDefault = 350.0f;

// 技能 7（心剑·无影）引导射程单源解析：交付系统（BeamChannelDeliverySystem）与
// 渲染指示圈（GameplayState）必须共用本函数，禁止各自复算导致漂移。
// 仅接受 is_baked 的烘焙档案；哨兵档案（!is_baked）与空指针一律回退机制表
// 技能级 base_range 基准。本函数为只读路径，不得触发即时烘焙。
// 形参名刻意区别于 skillId：离线 schema 扫描器会把 skillId 解析到同名文件级常量，
// 使用独立命名可让其按"运行时变量键"归类，避免误报不存在的机制三元组。
[[nodiscard]] inline float ResolveBeamChannelMaxRange(
    const BakedSkillProfile *profile, uint32_t beamSkillId) {
  if (profile != nullptr && profile->is_baked && profile->delivery.range > 0.0f) {
    return profile->delivery.range;
  }
  return data::SkillMechanicsRegistry::Get().GetFloat(
      beamSkillId, 0u, "base_range", kBeamChannelBaseRangeDefault);
}

} // namespace NoMoreDay
