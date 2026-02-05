#pragma once
#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <unordered_map>
#include "game/components/Stats.hpp"
#include "game/components/SkillDefs.hpp"  // For DamageModifier
#include <nlohmann/json.hpp>

namespace NoMoreDay {

// ============================================================
// 六扇区天赋系统 - 新增类型定义 (V1.1)
// ============================================================

// 职业枚举 (六大基础职业)
enum class ProfessionID : uint8_t {
    BladeAscendant = 0,  // 剑修 (已实现)
    Mage           = 1,  // 法师
    Priest         = 2,  // 祭祀
    Knight         = 3,  // 骑士
    Ranger         = 4,  // 游侠
    Berserker      = 5,  // 狂战士
    COUNT          = 6
};

// 节点类型 (新定义 - 用于六扇区系统)
enum class TalentNodeType : uint8_t {
    Minor = 0,   // 小节点：普通增益
    Major,       // 大节点：显著增益
    Core         // 核心节点：定义性天赋，仅主修职业可解锁
};

// 轨道层级解锁阈值
struct TierThreshold {
    static constexpr int TIER_1 = 0;   // 基础层，无门槛
    static constexpr int TIER_2 = 10;  // 第二圈，需 10 亲和
    static constexpr int TIER_3 = 25;  // 核心层，需 25 亲和
};

// ============================================================
// 旧类型定义 (保留兼容性)
// ============================================================

// 旧的节点类型 (保留兼容性)
enum class StarNodeType : uint8_t {
    Minor = 0,
    Major,
    Keystone
};

enum class AstrolabeEffectType : uint8_t {
    GrantComponent = 0,
    ModifyIntent = 1,
    ModifyStat = 2,
    SpecialBehavior = 3
};

enum class TraitID : uint16_t {
    None = 0,
    SwordHeart = 100,
    SwordIntentUnlock = 101,
    MaxSwordIntent = 200,
    SwordIntentGain = 201,
    SwordIntentGrace = 202
};

struct AstrolabeNodeEffect {
    AstrolabeEffectType type;
    TraitID trait_id = TraitID::None;
    std::string value; // e.g., "SwordHeart"
    float numeric_value = 0.0f; // Pre-parsed numeric value (e.g. for ModifyIntent)
    float ratio = 0.0f; // Pre-parsed ratio (e.g. for conversions like IntToCritMult:0.3)
};

struct StarNode {
    uint32_t id = 0;
    std::string name_key;
    std::string desc_key;
    uint32_t constellation_id = 0;
    StarNodeType type = StarNodeType::Minor;
    
    float x = 0.0f;
    float y = 0.0f;
    
    std::vector<uint32_t> prerequisites;
    std::vector<uint32_t> connections;
    std::vector<StatModifier> modifiers;
    std::vector<DamageModifier> damage_modifiers;
    std::vector<StatConversion> conversions;
    std::vector<AstrolabeNodeEffect> effects;
    
    std::string icon_id;
};

struct Constellation {
    uint32_t id = 0;
    std::string name_key;
    std::string bg_icon;
    std::vector<uint32_t> star_ids;
};

struct AstrolabeMap {
    std::vector<Constellation> constellations;
    std::unordered_map<uint32_t, StarNode> stars;

    void Clear() {
        constellations.clear();
        stars.clear();
    }
};

// ============================================================
// 六扇区天赋系统 - 核心数据结构 (V1.1)
// ============================================================

// 新的天赋节点结构体 (六扇区系统)
// 注意：命名为 AstrolabeTalentNode 以区分 SkillDefs.hpp 中技能专精树的 TalentNode
struct AstrolabeTalentNode {
    uint32_t id = 0;
    std::string name_key;
    std::string desc_key;
    
    ProfessionID profession = ProfessionID::BladeAscendant;
    TalentNodeType type = TalentNodeType::Minor;
    uint8_t tier = 1;              // 1, 2, 3
    uint8_t sectorIndex = 0;       // 在扇区内的编号 (用于均匀分布)
    
    // 可投入的最大点数 (如 0/5)
    uint8_t maxPoints = 1;
    
    // 效果列表 (复用现有类型)
    std::vector<StatModifier> modifiers;
    std::vector<DamageModifier> damage_modifiers;
    std::vector<StatConversion> conversions;
    std::vector<AstrolabeNodeEffect> effects;
    std::vector<uint32_t> prerequisites; // Restore Topology
    
    std::string icon_id;
    
    // 动态计算的世界坐标 (运行时填充，不序列化)
    mutable float x = 0.0f;
    mutable float y = 0.0f;
};

// 职业本命星 (扇区中心)
struct ProfessionStar {
    ProfessionID profession = ProfessionID::BladeAscendant;
    std::string name_key;
    std::string desc_key;
    // 动态计算的世界坐标
    mutable float x = 0.0f;
    mutable float y = 0.0f;
};

// 完整的天赋图 (六扇区系统)
struct TalentGraph {
    std::array<ProfessionStar, 6> professionStars;
    std::unordered_map<uint32_t, AstrolabeTalentNode> nodes;
    
    void Clear() { 
        nodes.clear(); 
        // Reset profession stars to default
        for (auto& star : professionStars) {
            star = ProfessionStar{};
        }
    }
    
    // 查找节点
    [[nodiscard]] const AstrolabeTalentNode* findNode(uint32_t id) const {
        auto it = nodes.find(id);
        return it != nodes.end() ? &it->second : nullptr;
    }
    
    [[nodiscard]] AstrolabeTalentNode* findNode(uint32_t id) {
        auto it = nodes.find(id);
        return it != nodes.end() ? &it->second : nullptr;
    }
};

// ============================================================
// JSON Serialization helpers
// ============================================================

// --- 新类型 JSON 序列化 ---

inline void to_json(nlohmann::json& j, const ProfessionID& p) {
    j = static_cast<uint8_t>(p);
}
inline void from_json(const nlohmann::json& j, ProfessionID& p) {
    p = static_cast<ProfessionID>(j.get<uint8_t>());
}

inline void to_json(nlohmann::json& j, const TalentNodeType& t) {
    switch (t) {
        case TalentNodeType::Minor: j = "Minor"; break;
        case TalentNodeType::Major: j = "Major"; break;
        case TalentNodeType::Core:  j = "Core"; break;
    }
}
inline void from_json(const nlohmann::json& j, TalentNodeType& t) {
    if (j.is_number()) {
        t = static_cast<TalentNodeType>(j.get<uint8_t>());
        return;
    }
    std::string s = j.get<std::string>();
    if (s == "Minor") t = TalentNodeType::Minor;
    else if (s == "Major") t = TalentNodeType::Major;
    else if (s == "Core") t = TalentNodeType::Core;
}

// 前置依赖类型序列化 (AstrolabeTalentNode 需要这些)
inline void to_json(nlohmann::json& j, const AstrolabeEffectType& t) { j = static_cast<uint8_t>(t); }
inline void from_json(const nlohmann::json& j, AstrolabeEffectType& t) { t = static_cast<AstrolabeEffectType>(j.get<uint8_t>()); }

inline void to_json(nlohmann::json& j, const TraitID& t) { j = static_cast<uint16_t>(t); }
inline void from_json(const nlohmann::json& j, TraitID& t) { t = static_cast<TraitID>(j.get<uint16_t>()); }

inline void to_json(nlohmann::json& j, const AstrolabeNodeEffect& e) {
    j["type"] = e.type;
    j["trait_id"] = e.trait_id;
    j["value"] = e.value;
}
inline void from_json(const nlohmann::json& j, AstrolabeNodeEffect& e) {
    j.at("type").get_to(e.type);
    if (j.contains("trait_id")) j.at("trait_id").get_to(e.trait_id);
    j.at("value").get_to(e.value);

    // Pre-parsing optimization
    e.numeric_value = 0.0f;
    e.ratio = 0.0f;
    
    if (e.type == AstrolabeEffectType::ModifyIntent) {
        if (e.trait_id == TraitID::MaxSwordIntent) {
            try {
                e.numeric_value = std::stof(e.value);
            } catch(...) {}
        }
    } else if (e.type == AstrolabeEffectType::SpecialBehavior) {
        if (e.value.starts_with("IntToCritMult")) {
            size_t colon = e.value.find(':');
            if (colon != std::string::npos) {
                try {
                    e.ratio = std::stof(e.value.substr(colon + 1));
                } catch(...) {}
            }
        }
    }
}

inline void to_json(nlohmann::json& j, const AstrolabeTalentNode& n) {
    j["id"] = n.id;
    j["name_key"] = n.name_key;
    j["desc_key"] = n.desc_key;
    j["profession"] = n.profession;
    j["type"] = n.type;
    j["tier"] = n.tier;
    j["sector_index"] = n.sectorIndex;
    j["max_points"] = n.maxPoints;
    j["modifiers"] = n.modifiers;
    j["damage_modifiers"] = n.damage_modifiers;
    j["conversions"] = n.conversions;
    j["effects"] = n.effects;
    j["prerequisites"] = n.prerequisites;
    j["icon_id"] = n.icon_id;
}
inline void from_json(const nlohmann::json& j, AstrolabeTalentNode& n) {
    j.at("id").get_to(n.id);
    if (j.contains("name_key")) j.at("name_key").get_to(n.name_key);
    if (j.contains("desc_key")) j.at("desc_key").get_to(n.desc_key);
    if (j.contains("profession")) j.at("profession").get_to(n.profession);
    if (j.contains("type")) j.at("type").get_to(n.type);
    if (j.contains("tier")) j.at("tier").get_to(n.tier);
    if (j.contains("sector_index")) j.at("sector_index").get_to(n.sectorIndex);
    if (j.contains("max_points")) j.at("max_points").get_to(n.maxPoints);
    if (j.contains("modifiers")) j.at("modifiers").get_to(n.modifiers);
    if (j.contains("damage_modifiers")) j.at("damage_modifiers").get_to(n.damage_modifiers);
    if (j.contains("conversions")) j.at("conversions").get_to(n.conversions);
    // 手动解析 effects 数组以避免模板推导问题
    if (j.contains("effects")) {
        n.effects.clear();
        for (const auto& eff_json : j.at("effects")) {
            AstrolabeNodeEffect eff;
            eff_json.at("type").get_to(eff.type);
            if (eff_json.contains("trait_id")) eff_json.at("trait_id").get_to(eff.trait_id);
            if (eff_json.contains("value")) eff_json.at("value").get_to(eff.value);
            n.effects.push_back(std::move(eff));
        }
    }
    if (j.contains("prerequisites")) j.at("prerequisites").get_to(n.prerequisites);
    if (j.contains("icon_id")) j.at("icon_id").get_to(n.icon_id);
}

inline void to_json(nlohmann::json& j, const ProfessionStar& s) {
    j = nlohmann::json{
        {"profession", s.profession}, {"name_key", s.name_key}, {"desc_key", s.desc_key}
    };
}
inline void from_json(const nlohmann::json& j, ProfessionStar& s) {
    if (j.contains("profession")) j.at("profession").get_to(s.profession);
    if (j.contains("name_key")) j.at("name_key").get_to(s.name_key);
    if (j.contains("desc_key")) j.at("desc_key").get_to(s.desc_key);
}

// --- 旧类型 JSON 序列化 (保留兼容) ---

inline void to_json(nlohmann::json& j, const StarNodeType& t) {
    switch (t) {
        case StarNodeType::Minor: j = "Minor"; break;
        case StarNodeType::Major: j = "Major"; break;
        case StarNodeType::Keystone: j = "Keystone"; break;
    }
}

inline void from_json(const nlohmann::json& j, StarNodeType& t) {
    if (j.is_number()) {
        t = static_cast<StarNodeType>(j.get<uint8_t>());
        return;
    }
    std::string s = j.get<std::string>();
    if (s == "Minor") t = StarNodeType::Minor;
    else if (s == "Major") t = StarNodeType::Major;
    else if (s == "Keystone") t = StarNodeType::Keystone;
}

inline void to_json(nlohmann::json& j, const StarNode& n) {
    j["id"] = n.id;
    j["name_key"] = n.name_key;
    j["desc_key"] = n.desc_key;
    j["constellation_id"] = n.constellation_id;
    j["type"] = n.type;
    j["x"] = n.x;
    j["y"] = n.y;
    j["prerequisites"] = n.prerequisites;
    j["connections"] = n.connections;
    j["modifiers"] = n.modifiers;
    j["damage_modifiers"] = n.damage_modifiers;
    j["conversions"] = n.conversions;
    j["effects"] = n.effects;
    j["icon_id"] = n.icon_id;
}

inline void from_json(const nlohmann::json& j, StarNode& n) {
    j.at("id").get_to(n.id);
    if (j.contains("name_key")) j.at("name_key").get_to(n.name_key);
    if (j.contains("desc_key")) j.at("desc_key").get_to(n.desc_key);
    if (j.contains("constellation_id")) j.at("constellation_id").get_to(n.constellation_id);
    if (j.contains("type")) j.at("type").get_to(n.type);
    if (j.contains("x")) j.at("x").get_to(n.x);
    if (j.contains("y")) j.at("y").get_to(n.y);
    if (j.contains("prerequisites")) j.at("prerequisites").get_to(n.prerequisites);
    if (j.contains("connections")) j.at("connections").get_to(n.connections);
    if (j.contains("modifiers")) j.at("modifiers").get_to(n.modifiers);
    if (j.contains("damage_modifiers")) j.at("damage_modifiers").get_to(n.damage_modifiers);
    if (j.contains("conversions")) j.at("conversions").get_to(n.conversions);
    // 手动解析 effects 数组以避免模板推导问题
    if (j.contains("effects")) {
        n.effects.clear();
        for (const auto& eff_json : j.at("effects")) {
            AstrolabeNodeEffect eff;
            eff_json.at("type").get_to(eff.type);
            if (eff_json.contains("trait_id")) eff_json.at("trait_id").get_to(eff.trait_id);
            if (eff_json.contains("value")) eff_json.at("value").get_to(eff.value);
            n.effects.push_back(std::move(eff));
        }
    }
    if (j.contains("icon_id")) j.at("icon_id").get_to(n.icon_id);
}

inline void to_json(nlohmann::json& j, const Constellation& c) {
    j = nlohmann::json{
        {"id", c.id}, {"name_key", c.name_key}, {"bg_icon", c.bg_icon}, {"star_ids", c.star_ids}
    };
}

inline void from_json(const nlohmann::json& j, Constellation& c) {
    j.at("id").get_to(c.id);
    j.at("name_key").get_to(c.name_key);
    j.at("bg_icon").get_to(c.bg_icon);
    j.at("star_ids").get_to(c.star_ids);
}

} // namespace NoMoreDay
