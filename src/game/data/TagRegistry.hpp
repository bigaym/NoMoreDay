#pragma once
#include <array>
#include <cstdint>
#include <string_view>
#include <string>
#include <vector>
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
    Void = 1ULL << 6,

    // --- Form Tags (16-31) ---
    Melee = 1ULL << 16,
    Projectile = 1ULL << 17,
    Area = 1ULL << 18,
    Spell = 1ULL << 19,
    Attack = 1ULL << 20,
    Movement = 1ULL << 21,
    SwordSkill = 1ULL << 22,

    // --- Mechanism Tags (32-47) ---
    Hit = 1ULL << 32,
    Critical = 1ULL << 33,
    DamageOverTime = 1ULL << 34,
    Buff = 1ULL << 35,
    Aura = 1ULL << 36,
    Channeled = 1ULL << 37,

    // --- State Tags (48-63) ---
    Bleeding = 1ULL << 48,
    Burning = 1ULL << 49,
    Frozen = 1ULL << 50,
    Shocked = 1ULL << 51,
    Stunned = 1ULL << 52,
    SwordRiding = 1ULL << 53,
    Elite = 1ULL << 54,
    Boss = 1ULL << 55,
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

struct TagInfo {
    Tag tag;
    std::string_view id;
};

static constexpr std::array<TagInfo, 28> kTagInfoTable = {{
    {Tag::Physical, "Physical"},
    {Tag::Fire, "Fire"},
    {Tag::Cold, "Cold"},
    {Tag::Lightning, "Lightning"},
    {Tag::Shadow, "Shadow"},
    {Tag::Poison, "Poison"},
    {Tag::Void, "Void"},
    {Tag::Melee, "Melee"},
    {Tag::Projectile, "Projectile"},
    {Tag::Area, "Area"},
    {Tag::Spell, "Spell"},
    {Tag::Attack, "Attack"},
    {Tag::Movement, "Movement"},
    {Tag::SwordSkill, "SwordSkill"},
    {Tag::Hit, "Hit"},
    {Tag::Critical, "Critical"},
    {Tag::DamageOverTime, "DamageOverTime"},
    {Tag::Buff, "Buff"},
    {Tag::Aura, "Aura"},
    {Tag::Channeled, "Channeled"},
    {Tag::Bleeding, "Bleeding"},
    {Tag::Burning, "Burning"},
    {Tag::Frozen, "Frozen"},
    {Tag::Shocked, "Shocked"},
    {Tag::Stunned, "Stunned"},
    {Tag::SwordRiding, "SwordRiding"},
    {Tag::Elite, "Elite"},
    {Tag::Boss, "Boss"},

}};

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
        case Tag::Void: return "Void";
        case Tag::Melee: return "Melee";
        case Tag::Projectile: return "Projectile";
        case Tag::Area: return "Area";
        case Tag::Spell: return "Spell";
        case Tag::Attack: return "Attack";
        case Tag::Movement: return "Movement";
        case Tag::SwordSkill: return "SwordSkill";
        case Tag::Hit: return "Hit";
        case Tag::Critical: return "Critical";
        case Tag::DamageOverTime: return "DamageOverTime";
        case Tag::Buff: return "Buff";
        case Tag::Aura: return "Aura";
        case Tag::Channeled: return "Channeled";
        case Tag::Bleeding: return "Bleeding";
        case Tag::Burning: return "Burning";
        case Tag::Frozen: return "Frozen";
        case Tag::Shocked: return "Shocked";
        case Tag::Stunned: return "Stunned";
        case Tag::SwordRiding: return "SwordRiding";
        case Tag::Elite: return "Elite";
        case Tag::Boss: return "Boss";
        default: return "Unknown";
    }
}

// Helper to get Tag from string name
constexpr std::optional<Tag> TagFromString(std::string_view name) {
    if (name == "Physical") return Tag::Physical;
    if (name == "Fire") return Tag::Fire;
    if (name == "Cold") return Tag::Cold;
    if (name == "Lightning") return Tag::Lightning;
    if (name == "Shadow") return Tag::Shadow;
    if (name == "Poison") return Tag::Poison;
    if (name == "Void") return Tag::Void;
    if (name == "Melee") return Tag::Melee;
    if (name == "Projectile") return Tag::Projectile;
    if (name == "Area") return Tag::Area;
    if (name == "Spell") return Tag::Spell;
    if (name == "Attack") return Tag::Attack;
    if (name == "Movement") return Tag::Movement;
    if (name == "SwordSkill") return Tag::SwordSkill;
    if (name == "Hit") return Tag::Hit;
    if (name == "Critical") return Tag::Critical;
    if (name == "DamageOverTime") return Tag::DamageOverTime;
    if (name == "Buff") return Tag::Buff;
    if (name == "Aura") return Tag::Aura;
    if (name == "Channeled") return Tag::Channeled;
    if (name == "Bleeding") return Tag::Bleeding;
    if (name == "Burning") return Tag::Burning;
    if (name == "Frozen") return Tag::Frozen;
    if (name == "Shocked") return Tag::Shocked;
    if (name == "Stunned") return Tag::Stunned;
    if (name == "SwordRiding") return Tag::SwordRiding;
    if (name == "Elite") return Tag::Elite;
    if (name == "Boss") return Tag::Boss;
    return std::nullopt;
}

// Helper to parse a list of strings into a Tag mask
inline Tag ParseTagList(const std::vector<std::string>& tags) {
    Tag mask = Tag::None;
    for (const auto& str : tags) {
        if (auto t = TagFromString(str)) {
            mask = mask | *t;
        }
    }
    return mask;
}

} // namespace NoMoreDay
