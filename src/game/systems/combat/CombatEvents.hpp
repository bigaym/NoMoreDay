#pragma once
#include <cstdint>
#include <entt/entity/entity.hpp>
#include "game/data/TagRegistry.hpp"

namespace NoMoreDay {

/**
 * @brief Types of combat events that can be dispatched.
 * 
 * Hook points for talents, equipment affixes, AI, and other systems
 * to react to combat actions.
 */
enum class CombatEventType : uint8_t {
    OnSkillCast,      // 技能释放时
    OnSkillHit,       // 技能命中时
    OnCrit,           // 暴击时
    OnDodge,          // 闪避时
    OnBlock,          // 格挡时
    OnTakeDamage,     // 受到伤害时
    OnDealDamage,     // 造成伤害时
    OnKill,           // 击杀时
    OnHeal,           // 治疗时
    Count
};

/**
 * @brief POD structure containing combat event data.
 * 
 * Designed for cache efficiency - all commonly accessed fields
 * are placed together. Extended data can be added via optional fields.
 */
struct CombatEvent {
    CombatEventType type = CombatEventType::OnSkillHit;
    
    entt::entity source = entt::null;  // 事件来源实体 (攻击者/施法者)
    entt::entity target = entt::null;  // 目标实体 (受击者)
    
    uint32_t skill_id = 0;             // 相关技能ID (0表示非技能触发)
    Tag tags = Tag::None;              // 技能/伤害标签组合
    
    float value = 0.0f;                // 数值 (伤害量/治疗量等)
    
    bool is_crit = false;              // 是否暴击
    bool is_blocked = false;           // 是否被格挡
    bool is_dodged = false;            // 是否被闪避
    
    // Optional: source entity for projectiles/summons
    entt::entity source_entity = entt::null;
};

/**
 * @brief Helper to create common event types.
 */
namespace CombatEventFactory {

inline CombatEvent CreateDealDamage(
    entt::entity attacker, 
    entt::entity defender,
    uint32_t skill_id,
    Tag tags,
    float damage,
    bool is_crit,
    entt::entity source_entity = entt::null
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnDealDamage;
    evt.source = attacker;
    evt.target = defender;
    evt.skill_id = skill_id;
    evt.tags = tags;
    evt.value = damage;
    evt.is_crit = is_crit;
    evt.source_entity = source_entity;
    return evt;
}

inline CombatEvent CreateTakeDamage(
    entt::entity defender,
    entt::entity attacker,
    uint32_t skill_id,
    Tag tags,
    float damage,
    bool is_crit
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnTakeDamage;
    evt.source = defender;  // 受伤者是source
    evt.target = attacker;  // 攻击者是target
    evt.skill_id = skill_id;
    evt.tags = tags;
    evt.value = damage;
    evt.is_crit = is_crit;
    return evt;
}

inline CombatEvent CreateOnKill(
    entt::entity killer,
    entt::entity victim,
    float overkill_damage = 0.0f
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnKill;
    evt.source = killer;
    evt.target = victim;
    evt.value = overkill_damage;
    return evt;
}

inline CombatEvent CreateOnCrit(
    entt::entity attacker,
    entt::entity defender,
    uint32_t skill_id,
    Tag tags,
    float damage
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnCrit;
    evt.source = attacker;
    evt.target = defender;
    evt.skill_id = skill_id;
    evt.tags = tags;
    evt.value = damage;
    evt.is_crit = true;
    return evt;
}

inline CombatEvent CreateOnDodge(
    entt::entity dodger,
    entt::entity attacker,
    uint32_t skill_id = 0
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnDodge;
    evt.source = dodger;
    evt.target = attacker;
    evt.skill_id = skill_id;
    evt.is_dodged = true;
    return evt;
}

inline CombatEvent CreateOnBlock(
    entt::entity blocker,
    entt::entity attacker,
    float blocked_damage,
    uint32_t skill_id = 0
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnBlock;
    evt.source = blocker;
    evt.target = attacker;
    evt.skill_id = skill_id;
    evt.value = blocked_damage;
    evt.is_blocked = true;
    return evt;
}

inline CombatEvent CreateSkillHit(
    entt::entity attacker,
    entt::entity target,
    uint32_t skill_id,
    Tag tags,
    bool is_crit = false
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnSkillHit;
    evt.source = attacker;
    evt.target = target;
    evt.skill_id = skill_id;
    evt.tags = tags;
    evt.is_crit = is_crit;
    return evt;
}

} // namespace CombatEventFactory

} // namespace NoMoreDay
