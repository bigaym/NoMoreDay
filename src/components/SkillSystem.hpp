#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include "core/TagRegistry.hpp"

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

/**
 * @brief DamageModifier defines how a damage value is modified.
 */
struct DamageModifier {
    Tag source_tag = Tag::None; // Tag this modifier applies to (e.g., Physical, Melee)
    Tag target_tag = Tag::None; // Tag the output has (used for Convert/GainExtra)
    float value = 0.0f;
    ModifierType type = ModifierType::Flat;
};

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

} // namespace NoMoreDay
