#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <entt/entt.hpp>
#include <nlohmann/json.hpp>

// Enum for Buff/Debuff types to map to icons
enum class BuffType {
    None,
    // Attribute Buffs
    AttackUp,
    DefenseUp,
    SpeedUp,
    CritRateUp,
    CritDamageUp,
    
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
    Invincible    // 无敌
};

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
    
    // Optional: Source entity ID for attribution
    entt::entity source = entt::null;
};

struct ActiveEffectsComponent {
    std::vector<BuffEffect> effects;
    
    // Helper to add or refresh a buff
    void AddOrRefresh(const BuffEffect& new_effect) {
        for (auto& effect : effects) {
            if (effect.id == new_effect.id) {
                // Refresh logic
                effect.duration = new_effect.duration;
                effect.remaining = new_effect.duration;
                if (effect.stacks < effect.max_stacks) {
                    effect.stacks++;
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

