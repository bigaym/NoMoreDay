#pragma once
#include <cstdint>
#include <string_view>
#include <optional>
#include <bit>

namespace NoMoreDay {

enum class Tag : uint64_t {
    None = 0,

    // --- DamageType Tags (0-15) ---
    Physical = 1ULL << 0,
    Fire = 1ULL << 1,
    Cold = 1ULL << 2,
    Lightning = 1ULL << 3,
    Shadow = 1ULL << 4,
    Poison = 1ULL << 5,

    // --- Form Tags (16-31) ---
    Melee = 1ULL << 16,
    Projectile = 1ULL << 17,
    Area = 1ULL << 18,
    Spell = 1ULL << 19,
    Attack = 1ULL << 20,
    Movement = 1ULL << 21,

    // --- Mechanism Tags (32-47) ---
    Hit = 1ULL << 32,
    Critical = 1ULL << 33,
    DamageOverTime = 1ULL << 34,

    // --- State Tags (48-63) ---
    Bleeding = 1ULL << 48,
    Burning = 1ULL << 49,
    Frozen = 1ULL << 50,
    Shocked = 1ULL << 51,
    Stunned = 1ULL << 52,
};

constexpr Tag operator|(Tag lhs, Tag rhs) {
    return static_cast<Tag>(static_cast<uint64_t>(lhs) | static_cast<uint64_t>(rhs));
}

constexpr Tag operator&(Tag lhs, Tag rhs) {
    return static_cast<Tag>(static_cast<uint64_t>(lhs) & static_cast<uint64_t>(rhs));
}

constexpr Tag operator^(Tag lhs, Tag rhs) {
    return static_cast<Tag>(static_cast<uint64_t>(lhs) ^ static_cast<uint64_t>(rhs));
}

constexpr Tag operator~(Tag t) {
    return static_cast<Tag>(~static_cast<uint64_t>(t));
}

constexpr bool HasTag(Tag mask, Tag tag) {
    return (mask & tag) == tag;
}

// Helper to get string name of a SINGLE tag
constexpr std::string_view GetTagName(Tag tag) {
    switch(tag) {
        case Tag::Physical: return "Physical";
        case Tag::Fire: return "Fire";
        case Tag::Cold: return "Cold";
        case Tag::Lightning: return "Lightning";
        case Tag::Shadow: return "Shadow";
        case Tag::Poison: return "Poison";
        case Tag::Melee: return "Melee";
        case Tag::Projectile: return "Projectile";
        case Tag::Area: return "Area";
        case Tag::Spell: return "Spell";
        case Tag::Attack: return "Attack";
        case Tag::Movement: return "Movement";
        case Tag::Hit: return "Hit";
        case Tag::Critical: return "Critical";
        case Tag::DamageOverTime: return "DamageOverTime";
        case Tag::Bleeding: return "Bleeding";
        case Tag::Burning: return "Burning";
        case Tag::Frozen: return "Frozen";
        case Tag::Shocked: return "Shocked";
        case Tag::Stunned: return "Stunned";
        default: return "Unknown";
    }
}

} // namespace NoMoreDay

#include <nlohmann/json.hpp>
namespace NoMoreDay {
    inline void to_json(nlohmann::json& j, const Tag& t) { j = static_cast<uint64_t>(t); }
    inline void from_json(const nlohmann::json& j, Tag& t) { t = static_cast<Tag>(j.get<uint64_t>()); }
}
