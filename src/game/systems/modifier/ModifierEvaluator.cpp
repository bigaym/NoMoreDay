#include "game/systems/modifier/ModifierEvaluator.hpp"

#include <algorithm>

namespace NoMoreDay {
namespace {

bool ContainsSkill(const std::vector<uint32_t> &whitelist, const uint32_t skillId) {
  if (whitelist.empty()) {
    return true;
  }
  return std::find(whitelist.begin(), whitelist.end(), skillId) != whitelist.end();
}

bool ContainsSkill(const std::span<const uint32_t> whitelist,
                   const uint32_t skillId) {
  if (whitelist.empty()) {
    return true;
  }
  return std::find(whitelist.begin(), whitelist.end(), skillId) != whitelist.end();
}

bool ContainsAnyNode(const std::vector<uint32_t> &requiredNodes,
                     const std::vector<uint32_t> &activeNodes) {
  if (requiredNodes.empty()) {
    return true;
  }
  for (const uint32_t nodeId : requiredNodes) {
    if (std::find(activeNodes.begin(), activeNodes.end(), nodeId) !=
        activeNodes.end()) {
      return true;
    }
  }
  return false;
}

bool ContainsAnyNode(const std::span<const uint32_t> requiredNodes,
                     const std::vector<uint32_t> &activeNodes) {
  if (requiredNodes.empty()) {
    return true;
  }
  for (const uint32_t nodeId : requiredNodes) {
    if (std::find(activeNodes.begin(), activeNodes.end(), nodeId) !=
        activeNodes.end()) {
      return true;
    }
  }
  return false;
}

bool MatchesFilters(const ModifierFilter &filter, const ModifierEvalContext &ctx) {
  if (filter.profession_mask != 0ull) {
    if (ctx.profession_id >= 64u) {
      return false;
    }
    const uint64_t professionBit = 1ull << ctx.profession_id;
    if ((filter.profession_mask & professionBit) == 0ull) {
      return false;
    }
  }

  if (!ContainsSkill(filter.skill_id_whitelist, ctx.skill_id)) {
    return false;
  }

  const uint64_t skillTags = static_cast<uint64_t>(ctx.skill_tags);
  if ((skillTags & filter.required_skill_tags_all) !=
      filter.required_skill_tags_all) {
    return false;
  }
  if ((skillTags & filter.forbidden_skill_tags_any) != 0ull) {
    return false;
  }

  if (filter.weapon_class_mask != 0u &&
      (filter.weapon_class_mask & ctx.weapon_class_mask) == 0u) {
    return false;
  }

  if (filter.equip_slot_mask != 0u &&
      (filter.equip_slot_mask & ctx.equip_slot_mask) == 0u) {
    return false;
  }

  return ContainsAnyNode(filter.node_id_whitelist, ctx.active_node_ids);
}

bool MatchesFilters(const ModifierRuntimeFilter &filter,
                    const std::span<const uint32_t> skillWhitelist,
                    const std::span<const uint32_t> nodeWhitelist,
                    const ModifierEvalContext &ctx) {
  if (filter.profession_mask != 0ull) {
    if (ctx.profession_id >= 64u) {
      return false;
    }
    const uint64_t professionBit = 1ull << ctx.profession_id;
    if ((filter.profession_mask & professionBit) == 0ull) {
      return false;
    }
  }

  if (!ContainsSkill(skillWhitelist, ctx.skill_id)) {
    return false;
  }

  const uint64_t skillTags = static_cast<uint64_t>(ctx.skill_tags);
  if ((skillTags & filter.required_skill_tags_all) !=
      filter.required_skill_tags_all) {
    return false;
  }
  if ((skillTags & filter.forbidden_skill_tags_any) != 0ull) {
    return false;
  }

  if (filter.weapon_class_mask != 0u &&
      (filter.weapon_class_mask & ctx.weapon_class_mask) == 0u) {
    return false;
  }

  if (filter.equip_slot_mask != 0u &&
      (filter.equip_slot_mask & ctx.equip_slot_mask) == 0u) {
    return false;
  }

  return ContainsAnyNode(nodeWhitelist, ctx.active_node_ids);
}

float ReadOr(const std::unordered_map<uint32_t, float> &map,
             const uint32_t key, const float defaultValue) {
  const auto it = map.find(key);
  if (it == map.end()) {
    return defaultValue;
  }
  return it->second;
}

} // namespace

void ModifierDelta::AddFlat(const uint32_t statType, const float value) {
  flat[statType] += value;
}

void ModifierDelta::AddPercentAdd(const uint32_t statType, const float value) {
  percent_add[statType] += value;
}

void ModifierDelta::AddPercentMult(const uint32_t statType, const float value) {
  const auto it = percent_mult.find(statType);
  if (it == percent_mult.end()) {
    percent_mult.emplace(statType, 1.0f + value);
    return;
  }
  it->second *= (1.0f + value);
}

void ModifierDelta::AddSkillLevel(const uint32_t skillId, const float value) {
  skill_levels[skillId] += value;
}

void ModifierDelta::AddMonsterEventOnUpdate(const uint32_t affixId) {
  monster_event_on_update_affix_ids.insert(affixId);
}

void ModifierDelta::AddMonsterEventOnHit(const uint32_t affixId) {
  monster_event_on_hit_affix_ids.insert(affixId);
}

void ModifierDelta::AddMonsterEventOnDeath(const uint32_t affixId) {
  monster_event_on_death_affix_ids.insert(affixId);
}

void ModifierDelta::AddMonsterBehaviorOnUpdate(const uint16_t opcode) {
  monster_behavior_on_update_opcodes.insert(opcode);
}

void ModifierDelta::AddMonsterBehaviorOnHit(const uint16_t opcode) {
  monster_behavior_on_hit_opcodes.insert(opcode);
}

void ModifierDelta::AddMonsterBehaviorOnDeath(const uint16_t opcode) {
  monster_behavior_on_death_opcodes.insert(opcode);
}

float ModifierDelta::GetSkillLevelBonus(const uint32_t skillId) const {
  const float wildcard = ReadOr(skill_levels, 0u, 0.0f);
  return wildcard + ReadOr(skill_levels, skillId, 0.0f);
}

ModifierDelta ModifierEvaluator::Evaluate(
    const std::span<const ModifierRecord> records, const ModifierEvalContext &ctx) {
  ModifierDelta out;
  for (const auto &record : records) {
    if (!MatchesFilters(record.filter, ctx)) {
      continue;
    }
    for (const auto &op : record.ops) {
      switch (op.opcode) {
      case ModifierOpCode::ADD_STAT_FLAT:
        out.AddFlat(op.param_u32, op.param_f32);
        break;
      case ModifierOpCode::ADD_STAT_PERCENT_ADD:
        out.AddPercentAdd(op.param_u32, op.param_f32);
        break;
      case ModifierOpCode::ADD_STAT_PERCENT_MULT:
        out.AddPercentMult(op.param_u32, op.param_f32);
        break;
      case ModifierOpCode::ADD_SKILL_LEVEL:
        out.AddSkillLevel(op.param_u32, op.param_f32);
        break;
      case ModifierOpCode::MONSTER_EVENT_ON_UPDATE:
        out.AddMonsterEventOnUpdate(op.param_u32);
        break;
      case ModifierOpCode::MONSTER_EVENT_ON_HIT:
        out.AddMonsterEventOnHit(op.param_u32);
        break;
      case ModifierOpCode::MONSTER_EVENT_ON_DEATH:
        out.AddMonsterEventOnDeath(op.param_u32);
        break;
      case ModifierOpCode::MONSTER_BEHAVIOR_MOLTEN_UPDATE:
      case ModifierOpCode::MONSTER_BEHAVIOR_TELEPORTER_UPDATE:
      case ModifierOpCode::MONSTER_BEHAVIOR_FROZEN_UPDATE:
      case ModifierOpCode::MONSTER_BEHAVIOR_MANA_SIPHON_UPDATE:
      case ModifierOpCode::MONSTER_BEHAVIOR_SHIELDING_UPDATE:
      case ModifierOpCode::MONSTER_BEHAVIOR_VORTEX_UPDATE:
      case ModifierOpCode::MONSTER_BEHAVIOR_WALLER_UPDATE:
      case ModifierOpCode::MONSTER_BEHAVIOR_BERSERKER_UPDATE:
      case ModifierOpCode::MONSTER_BEHAVIOR_VOIDZONE_UPDATE:
      case ModifierOpCode::MONSTER_BEHAVIOR_SUPPRESSOR_UPDATE:
      case ModifierOpCode::MONSTER_BEHAVIOR_SOUL_LINK_UPDATE:
        out.AddMonsterBehaviorOnUpdate(static_cast<uint16_t>(op.opcode));
        break;
      case ModifierOpCode::MONSTER_BEHAVIOR_VAMPIRIC_ON_HIT:
      case ModifierOpCode::MONSTER_BEHAVIOR_NULLIFIER_ON_HIT:
      case ModifierOpCode::MONSTER_BEHAVIOR_ENTANGLER_ON_HIT:
      case ModifierOpCode::MONSTER_BEHAVIOR_MIRROR_IMAGE_ON_TAKE_DAMAGE:
      case ModifierOpCode::MONSTER_BEHAVIOR_STORM_STRIDER_ON_TAKE_DAMAGE:
        out.AddMonsterBehaviorOnHit(static_cast<uint16_t>(op.opcode));
        break;
      case ModifierOpCode::MONSTER_BEHAVIOR_TOXIC_ON_DEATH:
      case ModifierOpCode::MONSTER_BEHAVIOR_SOUL_EATER_ON_ENEMY_DEATH:
      case ModifierOpCode::MONSTER_BEHAVIOR_AVENGER_ON_NEARBY_DEATH:
        out.AddMonsterBehaviorOnDeath(static_cast<uint16_t>(op.opcode));
        break;
      default:
        break;
      }
    }
  }
  return out;
}

ModifierDelta ModifierEvaluator::Evaluate(const ModifierRuntimeRegistry &registry,
                                          const std::span<const uint32_t> recordIds,
                                          const ModifierEvalContext &ctx) {
  ModifierDelta out;
  for (const uint32_t recordId : recordIds) {
    const ModifierRuntimeRecord *record = registry.FindRecordById(recordId);
    if (record == nullptr) {
      continue;
    }

    const ModifierRuntimeFilter *filter = registry.GetFilter(*record);
    if (filter == nullptr) {
      continue;
    }

    const auto skillWhitelist = registry.GetSkillWhitelist(*filter);
    const auto nodeWhitelist = registry.GetNodeWhitelist(*filter);
    if (!MatchesFilters(*filter, skillWhitelist, nodeWhitelist, ctx)) {
      continue;
    }

    for (const auto &op : registry.GetOps(*record)) {
      switch (static_cast<ModifierOpCode>(op.opcode)) {
      case ModifierOpCode::ADD_STAT_FLAT:
        out.AddFlat(op.param_u32, op.param_f32);
        break;
      case ModifierOpCode::ADD_STAT_PERCENT_ADD:
        out.AddPercentAdd(op.param_u32, op.param_f32);
        break;
      case ModifierOpCode::ADD_STAT_PERCENT_MULT:
        out.AddPercentMult(op.param_u32, op.param_f32);
        break;
      case ModifierOpCode::ADD_SKILL_LEVEL:
        out.AddSkillLevel(op.param_u32, op.param_f32);
        break;
      case ModifierOpCode::MONSTER_EVENT_ON_UPDATE:
        out.AddMonsterEventOnUpdate(op.param_u32);
        break;
      case ModifierOpCode::MONSTER_EVENT_ON_HIT:
        out.AddMonsterEventOnHit(op.param_u32);
        break;
      case ModifierOpCode::MONSTER_EVENT_ON_DEATH:
        out.AddMonsterEventOnDeath(op.param_u32);
        break;
      case ModifierOpCode::MONSTER_BEHAVIOR_MOLTEN_UPDATE:
      case ModifierOpCode::MONSTER_BEHAVIOR_TELEPORTER_UPDATE:
      case ModifierOpCode::MONSTER_BEHAVIOR_FROZEN_UPDATE:
      case ModifierOpCode::MONSTER_BEHAVIOR_MANA_SIPHON_UPDATE:
      case ModifierOpCode::MONSTER_BEHAVIOR_SHIELDING_UPDATE:
      case ModifierOpCode::MONSTER_BEHAVIOR_VORTEX_UPDATE:
      case ModifierOpCode::MONSTER_BEHAVIOR_WALLER_UPDATE:
      case ModifierOpCode::MONSTER_BEHAVIOR_BERSERKER_UPDATE:
      case ModifierOpCode::MONSTER_BEHAVIOR_VOIDZONE_UPDATE:
      case ModifierOpCode::MONSTER_BEHAVIOR_SUPPRESSOR_UPDATE:
      case ModifierOpCode::MONSTER_BEHAVIOR_SOUL_LINK_UPDATE:
        out.AddMonsterBehaviorOnUpdate(op.opcode);
        break;
      case ModifierOpCode::MONSTER_BEHAVIOR_VAMPIRIC_ON_HIT:
      case ModifierOpCode::MONSTER_BEHAVIOR_NULLIFIER_ON_HIT:
      case ModifierOpCode::MONSTER_BEHAVIOR_ENTANGLER_ON_HIT:
      case ModifierOpCode::MONSTER_BEHAVIOR_MIRROR_IMAGE_ON_TAKE_DAMAGE:
      case ModifierOpCode::MONSTER_BEHAVIOR_STORM_STRIDER_ON_TAKE_DAMAGE:
        out.AddMonsterBehaviorOnHit(op.opcode);
        break;
      case ModifierOpCode::MONSTER_BEHAVIOR_TOXIC_ON_DEATH:
      case ModifierOpCode::MONSTER_BEHAVIOR_SOUL_EATER_ON_ENEMY_DEATH:
      case ModifierOpCode::MONSTER_BEHAVIOR_AVENGER_ON_NEARBY_DEATH:
        out.AddMonsterBehaviorOnDeath(op.opcode);
        break;
      default:
        break;
      }
    }
  }
  return out;
}

float ModifierEvaluator::ApplyStat(const float baseValue, const uint32_t statType,
                                   const ModifierDelta &delta) {
  const float flat = ReadOr(delta.flat, statType, 0.0f);
  const float percentAdd = ReadOr(delta.percent_add, statType, 0.0f);
  const float percentMult = ReadOr(delta.percent_mult, statType, 1.0f);
  return (baseValue + flat) * (1.0f + percentAdd) * percentMult;
}

} // namespace NoMoreDay
