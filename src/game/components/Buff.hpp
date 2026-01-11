#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <entt/entt.hpp>
#include <nlohmann/json.hpp>
#include "game/components/Stats.hpp"

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
    Poison,
    Bleed,
    
    // Special
    SwordIntent,  // 剑意
    Shield,       // 护盾
    Invincible,   // 无敌
    Bloodlust,    // 嗜血
    Hurt          // 受伤
};

inline void to_json(nlohmann::json& j, const BuffType& e) { j = static_cast<int>(e); }
inline void from_json(const nlohmann::json& j, BuffType& e) { e = static_cast<BuffType>(j.get<int>()); }

struct BuffEffect {
    std::string id;             // Unique ID for the buff type (e.g., 'sword_intent', 'rage')
    std::string name;           // Display name
    std::string description;    // Tooltip description
    BuffType type;              // For icon mapping
    
    float duration;             // Total duration in seconds (-1 for infinite)
    float remaining;            // Remaining time in seconds
    int stacks;                 // Current stack count
    int max_stacks;             // Max stack count
    
    bool is_debuff;             // True if it's a debuff (Red border), false for buff (Green/Gold)
    
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
        {"modifiers", b.modifiers}
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
    
    // Helper to get a buff
    BuffEffect* Get(const std::string& id) {
        for (auto& effect : effects) {
            if (effect.id == id) {
                return &effect;
            }
        }
        return nullptr;
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