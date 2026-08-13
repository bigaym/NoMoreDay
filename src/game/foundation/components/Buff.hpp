#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <entt/entt.hpp>
#include <nlohmann/json.hpp>
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/BuffIds.hpp"

namespace NoMoreDay {

// Enum for Buff/Debuff types to map to icons
enum class BuffType {
    None,
    // Attribute Buffs
    AttackUp,
    DefenseUp,
    SpeedUp,
    CritRateUp,
    CritDamageUp,
    PowerBoost, // Added for consistency with my test code
    
    // Attribute Debuffs
    AttackDown,
    DefenseDown,
    SpeedDown,
    
    // Status Effects
    Stun,
    Freeze,
    Burn,
    Shock,
    Poison,
    Bleed,
    
    // Special
    SwordIntent,  // 剑意
    Shield,       // 护盾
    Invincible,   // 无敌
    Bloodlust,    // 嗜血
    BloodSea,     // 血海
    Hurt,          // 受伤

    // Control Effects
    Root,         // 缠绕
    Silence,      // 沉默

    DamageOverTime
};

inline void to_json(nlohmann::json& j, const BuffType& e) { j = static_cast<int>(e); }
inline void from_json(const nlohmann::json& j, BuffType& e) { e = static_cast<BuffType>(j.get<int>()); }

struct BuffEffect {
    std::string id;             // Unique ID for the buff type (e.g., 'sword_intent', 'rage')
    std::string name;           // Display name
    std::string description;    // Tooltip description
    BuffType type = BuffType::None; // For icon mapping
    
    float duration = 0.0f;      // Total duration in seconds (-1 for infinite)
    float remaining = 0.0f;     // Remaining time in seconds
    int stacks = 1;             // Current stack count
    int max_stacks = 1;         // Max stack count
    float tick_interval = 1e10;
    float tick_damage = 0.0f;
    float tick_timer = 0.0f;
    Tag tick_damage_tag = Tag::Poison;
    
    bool is_debuff = false;     // True if it's a debuff (Red border), false for buff (Green/Gold)

    // Managed-ailment structured identity (written by AilmentEngine).
    // AilmentType is defined in game/contracts/CombatEvents.hpp; to avoid a
    // foundation -> contracts include edge (the contracts layer already
    // depends on foundation), the enum is stored as its underlying uint8_t
    // value here and converted at the AilmentEngine boundary, where the
    // layouts are pinned with static_asserts (see AilmentEngine.cpp).
    bool managed_ailment = false; // True when this effect is created by AilmentEngine
    uint8_t ailment_type = 0;     // Underlying value of AilmentType (AilmentType::None == 0)
    float ailment_power = 0.0f;   // Stored per-tick power snapshot (mirrors tick_damage)

    std::vector<StatModifier> modifiers; // Modifiers applied by this buff
    
    // Optional: Source entity ID for attribution
    entt::entity source = entt::null;
};

// Custom serialization for BuffEffect to handle entity
inline void to_json(nlohmann::json& j, const BuffEffect& b) {
    j = nlohmann::json{
        {"id", b.id}, {"name", b.name}, {"description", b.description},
        {"type", b.type}, {"duration", b.duration}, {"remaining", b.remaining},
        {"stacks", b.stacks}, {"max_stacks", b.max_stacks}, {"is_debuff", b.is_debuff},
        {"modifiers", b.modifiers},
        {"managed_ailment", b.managed_ailment},
        {"ailment_type", b.ailment_type},
        {"ailment_power", b.ailment_power}
    };
    // source entity is not serialized here as it's runtime transient usually, 
    // or requires UUID mapping which complexifies simple struct serialization.
    // For now, ignore source or set to null on load.
}

inline void from_json(const nlohmann::json& j, BuffEffect& b) {
    j.at("id").get_to(b.id);
    j.at("name").get_to(b.name);
    j.at("description").get_to(b.description);
    j.at("type").get_to(b.type);
    j.at("duration").get_to(b.duration);
    j.at("remaining").get_to(b.remaining);
    j.at("stacks").get_to(b.stacks);
    j.at("max_stacks").get_to(b.max_stacks);
    j.at("is_debuff").get_to(b.is_debuff);
    if (j.contains("modifiers")) j.at("modifiers").get_to(b.modifiers);
    // Managed-ailment fields are optional so that saves written before the
    // structured-identity change load with defaults (managed_ailment=false),
    // which routes them through the legacy "ailment:" id parse fallback.
    if (j.contains("managed_ailment")) j.at("managed_ailment").get_to(b.managed_ailment);
    if (j.contains("ailment_type")) b.ailment_type = j.at("ailment_type").get<uint8_t>();
    if (j.contains("ailment_power")) j.at("ailment_power").get_to(b.ailment_power);
    b.source = entt::null;
}

struct ActiveEffectsComponent {
    std::vector<BuffEffect> effects;
    
    // Helper to add or refresh a buff
    void AddOrRefresh(const BuffEffect& new_effect) {
        for (auto& effect : effects) {
            if (effect.id == new_effect.id) {
                // Refresh duration
                effect.duration = new_effect.duration;
                effect.remaining = new_effect.duration;
                
                // Update metadata in case it changed (e.g. from a stronger version of the same buff)
                effect.name = new_effect.name;
                effect.description = new_effect.description;
                effect.modifiers = new_effect.modifiers;
                
                // Handle Stacking
                if (effect.stacks < effect.max_stacks) {
                    // If the new effect has multiple stacks, add them but clamp to max
                    effect.stacks = std::min(effect.max_stacks, effect.stacks + new_effect.stacks);
                }
                return;
            }
        }
        effects.push_back(new_effect);
    }
    
    // Helper to remove a buff
    void Remove(const std::string& id) {
        std::erase_if(effects, [&](const auto& effect) { return effect.id == id; });
    }

    // Helper to remove a buff by enum id
    void Remove(BuffId id) {
        Remove(std::string(BuffIdToString(id)));
    }
    
    // Helper to get a buff
    BuffEffect* Get(const std::string& id) {
        for (auto& effect : effects) {
            if (effect.id == id) {
                return &effect;
            }
        }
        return nullptr;
    }

    // Helper to get a buff by enum id
    BuffEffect* Get(BuffId id) {
        return Get(std::string(BuffIdToString(id)));
    }

    // Const variant: read-only lookup by enum id
    const BuffEffect* Get(BuffId id) const {
        const std::string key(BuffIdToString(id));
        for (const auto& effect : effects) {
            if (effect.id == key) {
                return &effect;
            }
        }
        return nullptr;
    }

    // Helper: whether an effect with the given enum id exists.
    // NOTE: remaining-duration checks (if any) are the caller's responsibility.
    [[nodiscard]] bool Has(BuffId id) const {
        const std::string key(BuffIdToString(id));
        for (const auto& effect : effects) {
            if (effect.id == key) {
                return true;
            }
        }
        return false;
    }
    
    void Update(float dt) {
        std::erase_if(effects, [&](auto& effect) {
            if (effect.duration < 0) return false; // Infinite
            effect.remaining -= dt;
            return effect.remaining <= 0;
        });
    }
};

inline void to_json(nlohmann::json& j, const ActiveEffectsComponent& c) {
    j = nlohmann::json{{"effects", c.effects}};
}

inline void from_json(const nlohmann::json& j, ActiveEffectsComponent& c) {
    j.at("effects").get_to(c.effects);
}

} // namespace NoMoreDay
