#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include "TagRegistry.hpp"
#include "../components/SkillSystem.hpp"

namespace NoMoreDay {

struct SkillData {
    uint32_t id;
    std::string name_key;
    std::string desc_key;
    float mana_cost;
    float cooldown;
    Tag tags = Tag::None;
    float base_damage;
    float weapon_damage_mult;
    float added_damage_effectiveness;
    int max_charges = 1;
    uint32_t icon_id = 0;
};

class SkillRegistry {
public:
    static SkillRegistry& Get();
    
    void LoadFromJson(const std::string& path);
    const SkillData* GetSkill(uint32_t id) const;
    const SkillTreeDefinition* GetSkillTree(uint32_t skill_id) const;
    const std::unordered_map<uint32_t, SkillData>& GetAllSkills() const { return skills_; }

private:
    SkillRegistry() = default;
    std::unordered_map<uint32_t, SkillData> skills_;
    std::unordered_map<uint32_t, SkillTreeDefinition> skill_trees_;
};

}
