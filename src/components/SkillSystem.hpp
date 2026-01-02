#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include "core/TagRegistry.hpp"
#include "raylib.h"

namespace NoMoreDay {

/**
 * @brief DamagePool stores flat damage values for multiple types.
 * Indexed by the bit position of the DamageType tag (0-15).
 */
struct DamagePool {
    std::array<float, 16> values = {0.0f};

    void Clear() {
        values.fill(0.0f);
    }

    void Add(Tag type, float amount) {
        // Find bit index
        uint64_t val = static_cast<uint64_t>(type);
        if (val == 0) return;
        
        // __builtin_ctzll finds the number of trailing zeros, which is the bit index for single bit tags
        int index = std::countr_zero(val);
        if (index < 16) {
            values[index] += amount;
        }
    }

    float Get(Tag type) const {
        uint64_t val = static_cast<uint64_t>(type);
        if (val == 0) return 0.0f;
        int index = std::countr_zero(val);
        if (index < 16) {
            return values[index];
        }
        return 0.0f;
    }

    void Merge(const DamagePool& other) {
        for (size_t i = 0; i < 16; ++i) {
            values[i] += other.values[i];
        }
    }
};

enum class ModifierType : uint8_t {
    Flat,           // Added to base damage
    Increased,      // Summed together (1 + inc1 + inc2 + ...)
    More,           // Multiplied independently (* more1 * more2 * ...)
    Convert,        // Convert X% of A to B
    GainExtra,      // Gain X% of A as extra B
};

inline void to_json(nlohmann::json& j, const ModifierType& e) { j = static_cast<uint8_t>(e); }
inline void from_json(const nlohmann::json& j, ModifierType& e) { e = static_cast<ModifierType>(j.get<uint8_t>()); }

/**
 * @brief DamageModifier defines how a damage value is modified.
 */
struct DamageModifier {
    Tag source_tag = Tag::None; // Tag this modifier applies to (e.g., Physical, Melee)
    Tag target_tag = Tag::None; // Tag the output has (used for Convert/GainExtra)
    float value = 0.0f;
    ModifierType type = ModifierType::Flat;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DamageModifier, source_tag, target_tag, value, type)

/**
 * @brief Component for skills to store their specific modifiers.
 */
struct SkillModifierComponent {
    std::vector<DamageModifier> modifiers;
};

/**
 * @brief Global modifiers from gear, passives, etc.
 */
struct GlobalModifierComponent {
    std::vector<DamageModifier> modifiers;
};

/**
 * @brief Represents an active skill slot.
 */
struct SkillSlot {
    uint32_t id = 0;
    float cooldown = 0.0f;
    int current_charges = 0;
};

/**
 * @brief Attached to entities (players) that can use active skills.
 */
struct ActiveSkillsComponent {
    std::array<SkillSlot, 5> slots; // Q, W, E, R, RMB
};

/**
 * @brief Identifies an entity as a skill execution instance (e.g., a projectile or area effect).
 */
struct SkillComponent {
    uint32_t skill_id = 0;
    entt::entity owner = entt::null;
};

/**
 * @brief Marker for skills cast by shadow/afterimage instead of player.
 */
struct ShadowCastTag {};

/**
 * @brief Marker for the shadow entity itself.
 */
struct ShadowEntityTag {};

struct ShadowLifetime {
    float remaining = 1.0f;
};

/**
 * @brief Simple animation state for entities (players/NPCs).
 */
enum class EntityAnimState : uint8_t {
    Idle,
    Move,
    SkillWindup,
    SkillCasting,
    SkillRecovery,
    Hurt,
    Dead
};

struct AnimationStateComponent {
    EntityAnimState state = EntityAnimState::Idle;
    float state_timer = 0.0f;
};

/**
 * @brief Blade Ascendant specific resource.
 */
struct SwordIntentComponent {
    int stacks = 0;
    int max_stacks = 10;
    float decay_timer = 0.0f;
    float decay_interval = 2.0f; // Start decaying after 2s of no gain
};

} // namespace NoMoreDay
