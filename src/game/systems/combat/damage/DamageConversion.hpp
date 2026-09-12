#pragma once

#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/TagRegistry.hpp"
#include "game/systems/combat/damage/DamageTypes.hpp"
#include "core/logging/Logger.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace NoMoreDay {
namespace damage {

// 元素伤害池数量由 DamageTypes.hpp 的 kElementCount 统一定义：数组按
// 池索引（Tag 位序）对齐，即 0=Physical、1=Fire、2=Cold、3=Lightning、
// 4=Shadow、5=Poison。注意 4/5 与 DamageType 枚举序（4=Poison、5=Shadow）
// 互换，任何下标运算都必须经 DamageTypeToPoolIndex / PoolIndexToDamageType 映射。

// 全部元素标签掩码（bit 0..15 范围内实际只用到 0..6）。
// 转换后的产物必须剥离其中全部旧元素标签，只保留目标元素标签。
inline constexpr Tag kAllDamageTypeTags = Tag::Physical | Tag::Fire | Tag::Cold |
                                          Tag::Lightning | Tag::Shadow | Tag::Poison |
                                          Tag::Void;

// 统一转换规则输入：替代此前散落在 global_mods / SkillModifierComponent /
// 专精树三处的 DamageModifier（Convert/GainExtra）。
// src_type/dst_type 为真实 DamageType（枚举序），由 ApplyConversion 负责映射到
// 池索引；调用方不得预先按池下标构造枚举值。
// pct 为总量比例：0.25f 表示把源元素伤害的 25% 转为目标元素。
// is_gain_extra=true 时为 GainExtra：不扣减源元素、不参与 Convert 的 100% 上限缩放。
struct ConversionRule {
  DamageType src_type = DamageType::Physical;
  DamageType dst_type = DamageType::Physical;
  float pct = 0.0f;
  bool is_gain_extra = false;
};

// 标签剥离规范（设计 §4.4）：去掉全部旧元素标签，仅赋目标元素标签；
// Melee/Projectile/Hit 等非元素动作标签原样保留，避免转换后重复收益。
// 目标元素标签经 ElementTagOf 映射，保证 Shadow/Poison 池位正确。
[[nodiscard]] inline Tag TransformTagsOnConversion(Tag original_tags,
                                                   DamageType target_type) {
  const Tag non_elemental_tags = static_cast<Tag>(
      static_cast<uint64_t>(original_tags) & ~static_cast<uint64_t>(kAllDamageTypeTags));
  const Tag target_element_tag = ElementTagOf(target_type);
  return non_elemental_tags | target_element_tag;
}

// 转换产物：按元素池索引（Tag 位序）对齐的数值与标签。
struct ConversionOutput {
  std::array<float, kElementCount> values{};
  std::array<Tag, kElementCount> tags{};
};

// 快照式纯函数（设计 §4.4，单遍、无级联、守恒）：
//   retained = max(0, 1 - sum(Convert.pct))
//   Convert 总和 > 100% 时按 scale = 1/sum 等比缩放
//   GainExtra 纯百分比不封顶、不扣减源元素
// 输入 source_values/source_tags 按池索引对齐，与输出完全隔离，不读写组件状态。
// 规则字段为真实 DamageType：源匹配把池索引还原为 DamageType 后比较，落点经
// DamageTypeToPoolIndex 映射回池索引；非元素类型视为非法跳过。
[[nodiscard]] inline ConversionOutput
ApplyConversion(const std::array<float, kElementCount> &source_values,
                const std::array<Tag, kElementCount> &source_tags,
                std::span<const ConversionRule> rules) {
  ConversionOutput output;
  output.tags = source_tags;

  // 池索引合法性：返回 kElementCount 表示该 DamageType 不映射到元素池。
  auto pool_of = [](DamageType type) -> size_t {
    return DamageTypeToPoolIndex(type);
  };
  const size_t element_count = static_cast<size_t>(kElementCount);

  // 入参防御：非有限 (NaN/Inf)、非正 pct、或 src/dst 为非元素 DamageType 的规则
  // 会破坏守恒式或产生非法下标，统一跳过并告警。规则数很小，先预检一遍，
  // 避免在逐源元素循环里对同一非法规则重复告警。
  for (const ConversionRule &rule : rules) {
    if (!std::isfinite(rule.pct) || rule.pct <= 0.0f) {
      LOG_WARN("DamageConversion: ignored invalid conversion rule pct={} "
               "(src={}, dst={}, gain_extra={})",
               rule.pct, static_cast<int>(rule.src_type),
               static_cast<int>(rule.dst_type), rule.is_gain_extra);
      continue;
    }
    if (pool_of(rule.src_type) >= element_count ||
        pool_of(rule.dst_type) >= element_count) {
      LOG_WARN("DamageConversion: ignored rule with non-element DamageType "
               "(src={}, dst={})",
               static_cast<int>(rule.src_type),
               static_cast<int>(rule.dst_type));
    }
  }

  for (int src = 0; src < kElementCount; ++src) {
    const float initial_amount = source_values[static_cast<size_t>(src)];
    if (initial_amount <= 0.0f) {
      continue;
    }

    // 池索引 src 是 Tag 位序，必须还原为真实 DamageType 再与规则字段比较。
    const DamageType src_type = PoolIndexToDamageType(static_cast<size_t>(src));

    // 单遍统计该源元素的 Convert 总比例（GainExtra 不计入上限）。
    float sum_convert_pct = 0.0f;
    for (const ConversionRule &rule : rules) {
      if (!std::isfinite(rule.pct) || rule.pct <= 0.0f) {
        continue; // 非法规则已在预检中告警
      }
      if (pool_of(rule.src_type) >= element_count) {
        continue; // 非元素源已在预检中告警
      }
      if (rule.src_type == src_type && !rule.is_gain_extra) {
        sum_convert_pct += rule.pct;
      }
    }

    const float retained_pct = (std::max)(0.0f, 1.0f - sum_convert_pct);
    output.values[static_cast<size_t>(src)] += initial_amount * retained_pct;

    const float scale = (sum_convert_pct > 1.0f) ? (1.0f / sum_convert_pct) : 1.0f;
    for (const ConversionRule &rule : rules) {
      if (!std::isfinite(rule.pct) || rule.pct <= 0.0f) {
        continue; // 非法规则已在预检中告警
      }
      if (rule.src_type != src_type) {
        continue;
      }
      const size_t dst = pool_of(rule.dst_type);
      if (dst >= element_count) {
        continue; // 非元素落点在预检中告警
      }
      const float amount =
          initial_amount * (rule.pct * (rule.is_gain_extra ? 1.0f : scale));
      output.values[dst] += amount;
      output.tags[dst] = TransformTagsOnConversion(output.tags[dst], rule.dst_type);
    }
  }

  return output;
}

} // namespace damage
} // namespace NoMoreDay
