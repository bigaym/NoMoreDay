#pragma once
#include "engine/render/GPUData.hpp"
#include "game/foundation/components/Stats.hpp"
#include <entt/entt.hpp>
#include <memory_resource>
#include <vector>

namespace NoMoreDay {

// 计算上下文：用于在计算过程中传递环境信息
struct CalculationContext {
  Tag source_tags = Tag::None; // 来源标签 (e.g. Melee, Fire, Player)
  Tag target_tags = Tag::None; // 目标标签 (e.g. Boss, Burning)
  int level = 1;               // 实体等级 (用于缩放公式)
  float delta_time = 0.016f;   // 帧时间
};

class AttributePipeline {
public:
  // 计算入口
  static void Calculate(entt::registry &registry, entt::entity entity);

  // Convert CombatStats to GPUVisualStats for rendering
  static void ToGPU(const CombatStats &src,
                    NoMoreDay::components::GPUVisualStats &dst);

  // Helpers
  // (None currently)
};

} // namespace NoMoreDay