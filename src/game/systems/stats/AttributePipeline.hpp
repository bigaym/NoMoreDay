#pragma once
#include "engine/render/GPUData.hpp"
#include "game/components/Stats.hpp"
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

  // Internal Phases exposed for testing
  static void Phase2_ResolvePrimary(CombatStats &stats,
                                    const std::vector<StatModifier> &modifiers,
                                    int level);

  static void
  Phase3_ResolveSecondary(CombatStats &stats,
                          const std::vector<StatModifier> &modifiers);

  static void Phase4_BakeEffective(CombatStats &stats, int level);

private:
  // Pipeline Phases
  static void Phase0_Initialize(entt::registry &registry, entt::entity entity,
                                CombatStats &stats, CalculationContext &ctx);

  static void Phase1_GatherModifiers(entt::registry &registry,
                                     entt::entity entity,
                                     const CalculationContext &ctx,
                                     std::vector<StatModifier> &out_modifiers);

  static void Phase5_GPUSync(entt::registry &registry, entt::entity entity,
                             const CombatStats &stats);

  // Helpers
  static float CalculateFinalValue(float base, StatType type,
                                   const std::vector<StatModifier> &modifiers);
};

} // namespace NoMoreDay