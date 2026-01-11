#pragma once
#include <cstdint>
#include <entt/entity/entity.hpp>
#include "game/data/TagRegistry.hpp"

namespace NoMoreDay {

/**
 * @brief Types of combat events that can be dispatched.
 * 
 * Hook points for talents, equipment affixes, AI, and other systems
 * to react to combat actions. Organized by category.
 */
enum class CombatEventType : uint8_t {
    // === Combat (Core) ===
    OnSkillCast,      // 技能释放时
    OnSkillHit,       // 技能命中时
    OnCrit,           // 暴击时
    OnDodge,          // 闪避时
    OnBlock,          // 格挡时
    OnTakeDamage,     // 受到伤害时
    OnDealDamage,     // 造成伤害时
    OnKill,           // 击杀时
    OnHeal,           // 治疗时
    
    // === Combat (Extended) ===
    OnOverkill,       // 超杀时 (伤害超过目标剩余生命)
    OnMeleeHit,       // 近战命中时
    OnProjectileHit,  // 投射物命中时
    OnAreaHit,        // 范围技能命中时
    
    // === Status Effects ===
    OnApplyAilment,   // 施加异常状态时 (点燃/冰冻/感电等)
    OnReceiveAilment, // 受到异常状态时
    OnStun,           // 眩晕时
    
    // === Resources ===
    OnLowHealth,      // 低血量时 (<30%)
    OnFullHealth,     // 满血时
    OnManaSpent,      // 消耗魔力时
    OnUsePotion,      // 使用药水时
    
    // === Skills ===
    OnChannelTick,    // 引导技能每跳时
    OnChannelEnd,     // 引导结束时
    OnDash,           // 冲刺/位移技能时
    
    // === Minions ===
    OnSummon,         // 召唤物生成时
    OnMinionDeath,    // 召唤物死亡时
    OnMinionHit,      // 召唤物命中时
    
    Count // 26 total
};

/**
 * @brief Ailment types for OnApplyAilment/OnReceiveAilment events.
 */
enum class AilmentType : uint8_t {
    None = 0,
    Ignite,     // 点燃
    Chill,      // 冰缓
    Freeze,     // 冰冻
    Shock,      // 感电
    Poison,     // 中毒
    Bleed,      // 流血
    Stun,       // 眩晕
    Slow,       // 减速
    Blind,      // 致盲
    Count
};

/**
 * @brief POD structure containing combat event data.
 * 
 * Designed for cache efficiency - all commonly accessed fields
 * are placed together. Extended data uses optional fields.
 */
struct CombatEvent {
    CombatEventType type = CombatEventType::OnSkillHit;
    
    entt::entity source = entt::null;  // 事件来源实体 (攻击者/施法者)
    entt::entity target = entt::null;  // 目标实体 (受击者)
    
    uint32_t skill_id = 0;             // 相关技能ID (0表示非技能触发)
    Tag tags = Tag::None;              // 技能/伤害标签组合
    
    float value = 0.0f;                // 主数值 (伤害量/治疗量/消耗量等)
    float value2 = 0.0f;               // 辅数值 (超杀量/血量百分比等)
    
    bool is_crit = false;              // 是否暴击
    bool is_blocked = false;           // 是否被格挡
    bool is_dodged = false;            // 是否被闪避
    
    AilmentType ailment = AilmentType::None;  // 异常状态类型
    
    // Optional: source entity for projectiles/summons
    entt::entity source_entity = entt::null;
    entt::entity minion = entt::null;  // 召唤物实体
};

/**
 * @brief Helper to create common event types.
 */
namespace CombatEventFactory {

// === Combat Core ===

inline CombatEvent CreateDealDamage(
    entt::entity attacker, entt::entity defender,
    uint32_t skill_id, Tag tags, float damage, bool is_crit,
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
    entt::entity defender, entt::entity attacker,
    uint32_t skill_id, Tag tags, float damage, bool is_crit
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnTakeDamage;
    evt.source = defender;
    evt.target = attacker;
    evt.skill_id = skill_id;
    evt.tags = tags;
    evt.value = damage;
    evt.is_crit = is_crit;
    return evt;
}

inline CombatEvent CreateOnKill(
    entt::entity killer, entt::entity victim, float overkill = 0.0f
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnKill;
    evt.source = killer;
    evt.target = victim;
    evt.value = overkill;
    return evt;
}

inline CombatEvent CreateOnCrit(
    entt::entity attacker, entt::entity defender,
    uint32_t skill_id, Tag tags, float damage
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
    entt::entity dodger, entt::entity attacker, uint32_t skill_id = 0
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
    entt::entity blocker, entt::entity attacker,
    float blocked_damage, uint32_t skill_id = 0
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
    entt::entity attacker, entt::entity target,
    uint32_t skill_id, Tag tags, bool is_crit = false
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

inline CombatEvent CreateSkillCast(
    entt::entity caster, uint32_t skill_id, Tag tags, float mana_cost = 0.0f
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnSkillCast;
    evt.source = caster;
    evt.skill_id = skill_id;
    evt.tags = tags;
    evt.value = mana_cost;
    return evt;
}

inline CombatEvent CreateOnHeal(
    entt::entity target, entt::entity healer, float amount
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnHeal;
    evt.source = target;
    evt.target = healer;
    evt.value = amount;
    return evt;
}

// === Combat Extended ===

inline CombatEvent CreateOnOverkill(
    entt::entity killer, entt::entity victim,
    float overkill_damage, float total_damage
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnOverkill;
    evt.source = killer;
    evt.target = victim;
    evt.value = overkill_damage;
    evt.value2 = total_damage;
    return evt;
}

inline CombatEvent CreateMeleeHit(
    entt::entity attacker, entt::entity target,
    uint32_t skill_id, Tag tags, float damage, bool is_crit
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnMeleeHit;
    evt.source = attacker;
    evt.target = target;
    evt.skill_id = skill_id;
    evt.tags = tags;
    evt.value = damage;
    evt.is_crit = is_crit;
    return evt;
}

inline CombatEvent CreateProjectileHit(
    entt::entity attacker, entt::entity target,
    uint32_t skill_id, Tag tags, float damage, bool is_crit,
    entt::entity projectile = entt::null
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnProjectileHit;
    evt.source = attacker;
    evt.target = target;
    evt.skill_id = skill_id;
    evt.tags = tags;
    evt.value = damage;
    evt.is_crit = is_crit;
    evt.source_entity = projectile;
    return evt;
}

inline CombatEvent CreateAreaHit(
    entt::entity attacker, entt::entity target,
    uint32_t skill_id, Tag tags, float damage, bool is_crit
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnAreaHit;
    evt.source = attacker;
    evt.target = target;
    evt.skill_id = skill_id;
    evt.tags = tags;
    evt.value = damage;
    evt.is_crit = is_crit;
    return evt;
}

// === Status Effects ===

inline CombatEvent CreateApplyAilment(
    entt::entity source, entt::entity target,
    AilmentType ailment, float duration = 0.0f
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnApplyAilment;
    evt.source = source;
    evt.target = target;
    evt.ailment = ailment;
    evt.value = duration;
    return evt;
}

inline CombatEvent CreateReceiveAilment(
    entt::entity target, entt::entity source, AilmentType ailment
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnReceiveAilment;
    evt.source = target;
    evt.target = source;
    evt.ailment = ailment;
    return evt;
}

inline CombatEvent CreateOnStun(
    entt::entity target, entt::entity stunner, float duration
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnStun;
    evt.source = target;
    evt.target = stunner;
    evt.ailment = AilmentType::Stun;
    evt.value = duration;
    return evt;
}

// === Resources ===

inline CombatEvent CreateOnLowHealth(
    entt::entity entity, float current_hp, float max_hp
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnLowHealth;
    evt.source = entity;
    evt.value = current_hp;
    evt.value2 = current_hp / max_hp; // percentage
    return evt;
}

inline CombatEvent CreateOnFullHealth(entt::entity entity, float max_hp) {
    CombatEvent evt;
    evt.type = CombatEventType::OnFullHealth;
    evt.source = entity;
    evt.value = max_hp;
    evt.value2 = 1.0f;
    return evt;
}

inline CombatEvent CreateOnManaSpent(
    entt::entity entity, uint32_t skill_id, float amount
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnManaSpent;
    evt.source = entity;
    evt.skill_id = skill_id;
    evt.value = amount;
    return evt;
}

inline CombatEvent CreateOnUsePotion(
    entt::entity entity, uint32_t potion_type, float amount
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnUsePotion;
    evt.source = entity;
    evt.skill_id = potion_type; // reuse skill_id for potion type
    evt.value = amount;
    return evt;
}

// === Skills ===

inline CombatEvent CreateChannelTick(
    entt::entity caster, uint32_t skill_id, Tag tags, int tick_count
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnChannelTick;
    evt.source = caster;
    evt.skill_id = skill_id;
    evt.tags = tags;
    evt.value = (float)tick_count;
    return evt;
}

inline CombatEvent CreateChannelEnd(
    entt::entity caster, uint32_t skill_id, Tag tags, float total_duration
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnChannelEnd;
    evt.source = caster;
    evt.skill_id = skill_id;
    evt.tags = tags;
    evt.value = total_duration;
    return evt;
}

inline CombatEvent CreateOnDash(
    entt::entity entity, uint32_t skill_id, float distance
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnDash;
    evt.source = entity;
    evt.skill_id = skill_id;
    evt.value = distance;
    return evt;
}

// === Minions ===

inline CombatEvent CreateOnSummon(
    entt::entity owner, entt::entity minion, uint32_t skill_id
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnSummon;
    evt.source = owner;
    evt.minion = minion;
    evt.skill_id = skill_id;
    return evt;
}

inline CombatEvent CreateMinionDeath(
    entt::entity owner, entt::entity minion, entt::entity killer = entt::null
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnMinionDeath;
    evt.source = owner;
    evt.target = killer;
    evt.minion = minion;
    return evt;
}

inline CombatEvent CreateMinionHit(
    entt::entity owner, entt::entity minion,
    entt::entity target, float damage, bool is_crit
) {
    CombatEvent evt;
    evt.type = CombatEventType::OnMinionHit;
    evt.source = owner;
    evt.target = target;
    evt.minion = minion;
    evt.value = damage;
    evt.is_crit = is_crit;
    return evt;
}

} // namespace CombatEventFactory

} // namespace NoMoreDay
