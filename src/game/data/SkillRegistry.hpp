#pragma once
#include "game/components/SkillDefs.hpp"
#include "game/data/SkillContract.hpp"
#include "game/data/TagRegistry.hpp"
#include <string>
#include <unordered_map>
#include <vector>

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

  std::unordered_map<std::string, float> params;

  float GetParam(const std::string &key, float default_val = 0.0f) const {
    auto it = params.find(key);
    if (it != params.end())
      return it->second;
    return default_val;
  }
};

class SkillRegistry {
public:
  static SkillRegistry &Get();

  void LoadFromJson(const std::string &path);
  const SkillData *GetSkill(uint32_t id) const;
  const SkillTreeDefinition *GetSkillTree(uint32_t skill_id) const;
  const SkillContractDefinition *GetSkillContractDefinition(
      uint32_t skill_id) const;
  const SkillContract *GetSkillContract(uint32_t skill_id) const;
  const NodeContractData *GetNodeContract(uint32_t skill_id,
                                          uint32_t node_id) const;
  bool ValidateSkillContract(uint32_t skill_id,
                             std::string *error = nullptr) const;
  const std::unordered_map<uint32_t, SkillData> &GetAllSkills() const {
    return skills_;
  }

  void RegisterSkill(const SkillData &data) { skills_[data.id] = data; }
  void RegisterSkillTree(const SkillTreeDefinition &tree) {
    skill_trees_[tree.skill_id] = tree;
  }

private:
  SkillRegistry() = default;
  std::unordered_map<uint32_t, SkillData> skills_;
  std::unordered_map<uint32_t, SkillTreeDefinition> skill_trees_;
  std::unordered_map<uint32_t, SkillContractDefinition> skill_contracts_;
};

} // namespace NoMoreDay
