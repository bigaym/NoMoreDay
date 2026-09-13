#pragma once
#include <entt/entt.hpp>
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"

#include <array>
#include <cstdint>
#include <string_view>

#include "game/foundation/components/SkillPointAccess.hpp"

namespace NoMoreDay {

struct ElementalConversion {
  Tag source_element = Tag::Physical;
  Tag target_element = Tag::None;
  Color projectile_color = WHITE;
  Color glow_color = WHITE;

  [[nodiscard]] bool IsActive() const noexcept {
    return target_element != Tag::None;
  }
};

// 节点 → 元素绑定表：元素转换映射的唯一来源。
struct ElementalNodeBinding {
  uint32_t node_id;
  Tag element;
  Color projectile_color;
  Color glow_color;
};

// 登记 (node_id -> 元素/颜色) 绑定；未登记的旧调用回退到按点数 1/2/3。
inline constexpr std::array<ElementalNodeBinding, 10> kElementalNodeTable{{
    {170, Tag::Fire, {255, 80, 20, 255}, {255, 160, 60, 180}},      // 劫火
    {172, Tag::Cold, {100, 200, 255, 255}, {150, 220, 255, 180}},   // 凛风
    {270, Tag::Cold, {100, 200, 255, 255}, {150, 220, 255, 180}},   // 霜寒之刃
    {272, Tag::Lightning, {200, 180, 255, 255}, {230, 200, 255, 180}}, // 雷光
    {370, Tag::Fire, {255, 80, 20, 255}, {255, 160, 60, 180}},      // 御剑术转火
    {372, Tag::Lightning, {200, 180, 255, 255}, {230, 200, 255, 180}}, // 御剑术转电
    {472, Tag::Lightning, {200, 180, 255, 255}, {230, 200, 255, 180}}, // 雷霆法环
    {474, Tag::Cold, {100, 200, 255, 255}, {150, 220, 255, 180}},   // 霜铠
    {570, Tag::Fire, {255, 80, 20, 255}, {255, 160, 60, 180}},      // 御剑术·劫火
    {572, Tag::Cold, {100, 200, 255, 255}, {150, 220, 255, 180}},   // 凛冬暴雪
}};

[[nodiscard]] inline ElementalConversion ResolveElementalConversion(
    uint32_t element_node_id, int points) noexcept {
    ElementalConversion conv;
    if (points <= 0) {
        return conv;
    }

    for (const auto &binding : kElementalNodeTable) {
        if (binding.node_id == element_node_id) {
            conv.target_element = binding.element;
            conv.projectile_color = binding.projectile_color;
            conv.glow_color = binding.glow_color;
            return conv;
        }
    }

    // Fallback for untyped or legacy calls
    switch (points) {
    case 1: // Fire
        conv.target_element = Tag::Fire;
        conv.projectile_color = {255, 80, 20, 255};
        conv.glow_color = {255, 160, 60, 180};
        break;
    case 2: // Ice / Frost
        conv.target_element = Tag::Cold;
        conv.projectile_color = {100, 200, 255, 255};
        conv.glow_color = {150, 220, 255, 180};
        break;
    case 3: // Lightning
        conv.target_element = Tag::Lightning;
        conv.projectile_color = {200, 180, 255, 255};
        conv.glow_color = {230, 200, 255, 180};
        break;
    default:
        break;
    }
    return conv;
}

namespace skills {

using NoMoreDay::ResolveElementalConversion;
using NoMoreDay::ElementalConversion;

} // namespace skills

/**
 * @brief CRTP base class for skill behaviors.
 * 
 * Provides zero-overhead static polymorphism for skill logic.
 * Derived classes should implement DoCast() and optionally DoHit().
 * 
 * Usage:
 *   struct FlowingThrust : SkillBehaviorBase<FlowingThrust> {
 *       static constexpr uint32_t kSkillId = 1;
 *       static void DoCast(entt::registry&, entt::entity, SkillExecution&);
 *   };
 */
template<typename Derived>
struct SkillBehaviorBase {
    // MSVC fix: Derived is incomplete during base class instantiation in CRTP.
    // static constexpr uint32_t SkillId = Derived::kSkillId;
    
    [[nodiscard]] static ElementalConversion ResolveElementalConversion(
        uint32_t element_node_id, int points) noexcept {
        return ::NoMoreDay::ResolveElementalConversion(element_node_id, points);
    }

    // Node trigger modifier parameter keys (replaces string-view comparisons).
    enum class ModifierParam : uint8_t {
      Effectiveness,
      RangeMultiplier,
    };

    [[nodiscard]] static float getModifier(uint32_t node_id,
                                           ModifierParam key,
                                           float default_val) {
      const auto *node_contract =
          SkillRegistry::Get().GetNodeContract(Derived::kSkillId, node_id);
      if (!node_contract) {
        return default_val;
      }
      switch (key) {
      case ModifierParam::Effectiveness:
        return node_contract->trigger.effectiveness;
      case ModifierParam::RangeMultiplier:
        return node_contract->trigger.range_mult;
      }
      return default_val;
    }

    /**
     * @brief Called when the skill effect should be executed.
     * Derived class MUST implement DoCast().
     */
    static void OnCast(entt::registry& reg, entt::entity owner, SkillExecution& exec) {
        Derived::DoCast(reg, owner, exec);
    }

    /**
     * @brief Called when skill hits a target.
     * Optional - for skills with special on-hit effects.
     */
    static void OnHit(entt::registry& reg, entt::entity attacker, entt::entity target, 
                      Tag hit_tags, bool is_crit) {
        if constexpr (requires { Derived::DoHit(reg, attacker, target, hit_tags, is_crit); }) {
            Derived::DoHit(reg, attacker, target, hit_tags, is_crit);
        }
    }
};

} // namespace NoMoreDay
