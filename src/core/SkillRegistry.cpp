#include "SkillRegistry.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include "../tools/Logger.hpp"

namespace NoMoreDay {

using json = nlohmann::json;

static Tag StringToTag(const std::string& str) {
    if (str == "Physical") return Tag::Physical;
    if (str == "Fire") return Tag::Fire;
    if (str == "Cold") return Tag::Cold;
    if (str == "Lightning") return Tag::Lightning;
    if (str == "Shadow") return Tag::Shadow;
    if (str == "Poison") return Tag::Poison;
    if (str == "Melee") return Tag::Melee;
    if (str == "Projectile") return Tag::Projectile;
    if (str == "Area") return Tag::Area;
    if (str == "Spell") return Tag::Spell;
    if (str == "Attack") return Tag::Attack;
    if (str == "Movement") return Tag::Movement;
    if (str == "Hit") return Tag::Hit;
    if (str == "Critical") return Tag::Critical;
    if (str == "DamageOverTime") return Tag::DamageOverTime;
    if (str == "Bleeding") return Tag::Bleeding;
    if (str == "Burning") return Tag::Burning;
    if (str == "Frozen") return Tag::Frozen;
    if (str == "Shocked") return Tag::Shocked;
    if (str == "Stunned") return Tag::Stunned;
    return Tag::None;
}

SkillRegistry& SkillRegistry::Get() {
    static SkillRegistry instance;
    return instance;
}

void SkillRegistry::LoadFromJson(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open skills JSON: {}", path);
        return;
    }

    json j;
    try {
        file >> j;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse skills JSON: {}", e.what());
        return;
    }

    skills_.clear();
    skill_trees_.clear();
    for (const auto& item : j["skills"]) {
        SkillData data;
        data.id = item["id"];
        data.name_key = item.value("name_key", "");
        data.desc_key = item.value("desc_key", "");
        data.mana_cost = item.value("mana_cost", 0.0f);
        data.cooldown = item.value("cooldown", 0.0f);
        
        data.tags = Tag::None;
        if (item.contains("tags")) {
            for (const auto& tagStr : item["tags"]) {
                data.tags = data.tags | StringToTag(tagStr.get<std::string>());
            }
        }

        data.base_damage = item.value("base_damage", 0.0f);
        data.weapon_damage_mult = item.value("weapon_damage_mult", 1.0f);
        data.added_damage_effectiveness = item.value("added_damage_effectiveness", 1.0f);
        data.max_charges = item.value("charge_count", 1);
        data.icon_id = item.value("icon_id", 0);

        skills_[data.id] = data;

        // Parse Talent Tree
        if (item.contains("talent_tree")) {
            SkillTreeDefinition tree;
            tree.skill_id = data.id;
            for (const auto& nodeItem : item["talent_tree"]) {
                TalentNode node = nodeItem.get<TalentNode>();
                tree.nodes[node.id] = node;
            }
            skill_trees_[data.id] = tree;
        }
    }
    LOG_INFO("Loaded {} skills and {} trees from {}", skills_.size(), skill_trees_.size(), path);
}

const SkillData* SkillRegistry::GetSkill(uint32_t id) const {
    auto it = skills_.find(id);
    if (it != skills_.end()) {
        return &it->second;
    }
    return nullptr;
}

const SkillTreeDefinition* SkillRegistry::GetSkillTree(uint32_t skill_id) const {
    auto it = skill_trees_.find(skill_id);
    if (it != skill_trees_.end()) {
        return &it->second;
    }
    return nullptr;
}

}
