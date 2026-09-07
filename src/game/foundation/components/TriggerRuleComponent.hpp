#pragma once

#include <array>
#include <cstdint>
#include <type_traits>
#include <entt/entity/entity.hpp>
#include "game/contracts/CombatEvents.hpp"
#include "game/foundation/data/TagRegistry.hpp"

namespace NoMoreDay {

/**
 * @brief 目标选择策略枚举
 */
enum class TriggerTargetPolicy : uint8_t {
    Self = 0,
    Victim,
    Attacker,
    GroundTarget
};

/**
 * @brief 纯 POD 触发规则定义
 */
struct TriggerRule {
    uint32_t rule_id = 0;
    NoMoreDay::CombatEventType listen_event = NoMoreDay::CombatEventType::OnSkillHit;
    Tag event_tag_filter = Tag::None;
    float base_chance = 1.0f;
    bool use_proc_scaling = true;
    float internal_cooldown = 0.0f;
    float current_cooldown = 0.0f;
    uint32_t cast_skill_id = 0;
    float effectiveness = 1.0f;
    TriggerTargetPolicy target_mode = TriggerTargetPolicy::Victim;
    uint32_t cooldown_refund_skill_id = 0;
    float cooldown_refund_amount = 0.0f;
};
static_assert(std::is_standard_layout_v<TriggerRule>);
static_assert(std::is_trivially_destructible_v<TriggerRule>);

/**
 * @brief 纯 POD 触发规则组件 (零堆分配)
 */
struct TriggerRuleComponent {
    static constexpr uint8_t kMaxRules = 8;
    std::array<TriggerRule, kMaxRules> rules{};
    uint8_t rule_count = 0;

    bool AddRule(const TriggerRule& rule) {
        for (uint8_t i = 0; i < rule_count; ++i) {
            if (rules[i].rule_id == rule.rule_id) {
                rules[i] = rule;
                return true;
            }
        }
        if (rule_count >= kMaxRules) {
            return false;
        }
        rules[rule_count++] = rule;
        return true;
    }

    bool RemoveRule(uint32_t rule_id) {
        for (uint8_t i = 0; i < rule_count; ++i) {
            if (rules[i].rule_id == rule_id) {
                for (uint8_t j = i; j + 1 < rule_count; ++j) {
                    rules[j] = rules[j + 1];
                }
                --rule_count;
                return true;
            }
        }
        return false;
    }

    bool HasRule(uint32_t rule_id) const {
        for (uint8_t i = 0; i < rule_count; ++i) {
            if (rules[i].rule_id == rule_id) return true;
        }
        return false;
    }

    void Clear() {
        rule_count = 0;
    }
};
static_assert(std::is_standard_layout_v<TriggerRuleComponent>);
static_assert(std::is_trivially_destructible_v<TriggerRuleComponent>);

} // namespace NoMoreDay
