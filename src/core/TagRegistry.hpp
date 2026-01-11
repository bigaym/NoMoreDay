#pragma once
#include <cstdint>
#include <string_view>
#include <optional>
#include <array>
#include <bit>
#include <vector>
#include <string>

namespace NoMoreDay {

enum class Tag : uint64_t {
    None = 0,

    // --- DamageType Tags (Bits 0-5) ---
    Physical    = 1ULL << 0,
    Fire        = 1ULL << 1,
    Cold        = 1ULL << 2,
    Lightning   = 1ULL << 3,
    Shadow      = 1ULL << 4,
    Poison      = 1ULL << 5,

    // --- Form Tags (Bits 16-21) ---
    Melee       = 1ULL << 16,
    Projectile  = 1ULL << 17,
    Area        = 1ULL << 18,
    Spell       = 1ULL << 19,
    Attack      = 1ULL << 20,
    Movement    = 1ULL << 21,

    // --- Mechanism Tags (Bits 32-37) ---
    Hit         = 1ULL << 32,
    Critical    = 1ULL << 33,
    DamageOverTime = 1ULL << 34,
    Buff        = 1ULL << 35,
    Aura        = 1ULL << 36,
    Channeled   = 1ULL << 37,

    // --- State Tags (Bits 48-53) ---
    Bleeding    = 1ULL << 48,
    Burning     = 1ULL << 49,
    Frozen      = 1ULL << 50,
    Shocked     = 1ULL << 51,
    Stunned     = 1ULL << 52,
    SwordRiding = 1ULL << 53,
};

// --- Operator Overloads ---
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

constexpr Tag& operator|=(Tag& lhs, Tag rhs) {
    lhs = lhs | rhs;
    return lhs;
}

constexpr Tag& operator&=(Tag& lhs, Tag rhs) {
    lhs = lhs & rhs;
    return lhs;
}

constexpr bool HasTag(Tag mask, Tag tag) {
    return (mask & tag) == tag;
}

constexpr bool HasAnyTag(Tag mask, Tag tags) {
    return (mask & tags) != Tag::None;
}

// --- Tag Metadata Structure ---
struct TagInfo {
    Tag tag;
    std::string_view id;       // JSON key (lowercase English, e.g., "physical")
    std::string_view name_cn;  // Chinese display name (e.g., "物理")
};

// Total number of defined tags (for iteration)
inline constexpr size_t kTagCount = 24;

// Master tag info table - ordered for easy lookup
// This is the single source of truth for all tag metadata
inline constexpr std::array<TagInfo, kTagCount> kTagInfoTable = {{
    // Damage Types (bits 0-5)
    {Tag::Physical,      "physical",      "物理"},
    {Tag::Fire,          "fire",          "火焰"},
    {Tag::Cold,          "cold",          "冰霜"},
    {Tag::Lightning,     "lightning",     "闪电"},
    {Tag::Shadow,        "shadow",        "暗影"},
    {Tag::Poison,        "poison",        "毒素"},
    
    // Form Tags (bits 16-21)
    {Tag::Melee,         "melee",         "近战"},
    {Tag::Projectile,    "projectile",    "投射物"},
    {Tag::Area,          "area",          "范围"},
    {Tag::Spell,         "spell",         "法术"},
    {Tag::Attack,        "attack",        "攻击"},
    {Tag::Movement,      "movement",      "位移"},
    
    // Mechanism Tags (bits 32-37)
    {Tag::Hit,           "hit",           "命中"},
    {Tag::Critical,      "critical",      "暴击"},
    {Tag::DamageOverTime,"dot",           "持续伤害"},
    {Tag::Buff,          "buff",          "增益"},
    {Tag::Aura,          "aura",          "光环"},
    {Tag::Channeled,     "channeled",     "引导"},
    
    // State Tags (bits 48-53)
    {Tag::Bleeding,      "bleeding",      "流血"},
    {Tag::Burning,       "burning",       "燃烧"},
    {Tag::Frozen,        "frozen",        "冻结"},
    {Tag::Shocked,       "shocked",       "感电"},
    {Tag::Stunned,       "stunned",       "眩晕"},
    {Tag::SwordRiding,   "swordriding",   "御剑"},
}};

// O(1) lookup: Get bit index from a single tag
constexpr int GetTagBitIndex(Tag tag) {
    if (tag == Tag::None) return -1;
    return std::countr_zero(static_cast<uint64_t>(tag));
}

// O(N) lookup: Find tag from string ID (for JSON parsing)
// N is small (24), so linear search is acceptable
constexpr std::optional<Tag> TagFromString(std::string_view id) {
    for (const auto& info : kTagInfoTable) {
        if (info.id == id) return info.tag;
    }
    return std::nullopt;
}

// Parse multiple string tags into a combined Tag bitmask
inline Tag ParseTagList(const std::vector<std::string>& tag_strings) {
    Tag result = Tag::None;
    for (const auto& s : tag_strings) {
        if (auto t = TagFromString(s)) {
            result = result | *t;
        }
    }
    return result;
}

// O(N) lookup: Get TagInfo from a single tag
constexpr const TagInfo* GetTagInfo(Tag tag) {
    for (const auto& info : kTagInfoTable) {
        if (info.tag == tag) return &info;
    }
    return nullptr;
}

// Get Chinese name for a SINGLE tag (for UI display)
constexpr std::string_view GetTagNameCN(Tag tag) {
    if (tag == Tag::None) return "无";
    const auto* info = GetTagInfo(tag);
    return info ? info->name_cn : "未知";
}

// Get English ID for a SINGLE tag (for serialization)
constexpr std::string_view GetTagId(Tag tag) {
    if (tag == Tag::None) return "none";
    const auto* info = GetTagInfo(tag);
    return info ? info->id : "unknown";
}

// Legacy compatibility: Get English name (same as ID, capitalized)
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
        case Tag::Buff: return "Buff";
        case Tag::Aura: return "Aura";
        case Tag::Channeled: return "Channeled";
        case Tag::Bleeding: return "Bleeding";
        case Tag::Burning: return "Burning";
        case Tag::Frozen: return "Frozen";
        case Tag::Shocked: return "Shocked";
        case Tag::Stunned: return "Stunned";
        case Tag::SwordRiding: return "SwordRiding";
        default: return "Unknown";
    }
}

// Build a comma-separated string of tag names from a bitmask (for debug/UI)
inline std::string GetTagListString(Tag tags, bool use_chinese = true) {
    std::string result;
    for (const auto& info : kTagInfoTable) {
        if (HasTag(tags, info.tag)) {
            if (!result.empty()) result += ", ";
            result += use_chinese ? info.name_cn : info.id;
        }
    }
    return result.empty() ? (use_chinese ? "无" : "none") : result;
}

} // namespace NoMoreDay
