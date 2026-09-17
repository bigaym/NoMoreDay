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

// 武器类别掩码通配判定：0 / 0xFFFFFFFF / 0xFFFF 均视为"不过滤"。
// 既有数据用 65535 与 4294967295 表示全部武器类别；徒手上下文置
// WeaponSubtype::None（bit 0），仍能匹配这些通配掩码。
bool IsWeaponMaskWildcard(const uint32_t weaponMask) {
  return weaponMask == 0u || weaponMask == 0xFFFFFFFFu ||
         weaponMask == 0xFFFFu;
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

  const uint32_t weaponMask = filter.weapon_class_mask;
  if (!IsWeaponMaskWildcard(weaponMask) &&
      (weaponMask & ctx.weapon_class_mask) == 0u) {
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

  const uint32_t weaponMask = filter.weapon_class_mask;
  if (!IsWeaponMaskWildcard(weaponMask) &&
      (weaponMask & ctx.weapon_class_mask) == 0u) {
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

// 解析记录级有效点数：
// 1. 白名单为空 -> 1（非专精域记录单次生效）；
// 2. 未填 node_points -> 1（兼容回退：只填 active_node_ids 的既有调用点行为不变）；
// 3. 否则取白名单中第一个已加点（points > 0）的节点，全部为 0 点返回 0（跳过记录）。
// 不做上限截断：节点最大点数的校验归加点/存档校验层。
uint16_t ResolveEffectivePoints(const std::span<const uint32_t> nodeWhitelist,
                                const ModifierEvalContext &ctx) {
  if (nodeWhitelist.empty()) {
    return 1;
  }
  if (ctx.node_points.empty()) {
    return 1;
  }
  for (const uint32_t nodeId : nodeWhitelist) {
    const uint16_t points = ctx.GetPointsForNode(nodeId);
    if (points > 0) {
      return points;
    }
  }
  return 0;
}

// 算子类别归属：用于窄入口按类别掩码跳过无关算子组。
ModifierOpCategory CategoryOfOp(const ModifierOpCode opcode) {
  switch (opcode) {
  case ModifierOpCode::ADD_STAT_FLAT:
  case ModifierOpCode::ADD_STAT_PERCENT_ADD:
  case ModifierOpCode::ADD_STAT_PERCENT_MULT:
  case ModifierOpCode::ADD_SKILL_LEVEL:
  case ModifierOpCode::MANA_COST_MULT:
    return ModifierOpCategory::Stats;
  case ModifierOpCode::MONSTER_EVENT_ON_UPDATE:
  case ModifierOpCode::MONSTER_EVENT_ON_HIT:
  case ModifierOpCode::MONSTER_EVENT_ON_DEATH:
    return ModifierOpCategory::Events;
  case ModifierOpCode::MONSTER_BEHAVIOR_MOLTEN_UPDATE:
  case ModifierOpCode::MONSTER_BEHAVIOR_VAMPIRIC_ON_HIT:
  case ModifierOpCode::MONSTER_BEHAVIOR_TELEPORTER_UPDATE:
  case ModifierOpCode::MONSTER_BEHAVIOR_FROZEN_UPDATE:
  case ModifierOpCode::MONSTER_BEHAVIOR_MANA_SIPHON_UPDATE:
  case ModifierOpCode::MONSTER_BEHAVIOR_SHIELDING_UPDATE:
  case ModifierOpCode::MONSTER_BEHAVIOR_VORTEX_UPDATE:
  case ModifierOpCode::MONSTER_BEHAVIOR_WALLER_UPDATE:
  case ModifierOpCode::MONSTER_BEHAVIOR_NULLIFIER_ON_HIT:
  case ModifierOpCode::MONSTER_BEHAVIOR_ENTANGLER_ON_HIT:
  case ModifierOpCode::MONSTER_BEHAVIOR_TOXIC_ON_DEATH:
  case ModifierOpCode::MONSTER_BEHAVIOR_MIRROR_IMAGE_ON_TAKE_DAMAGE:
  case ModifierOpCode::MONSTER_BEHAVIOR_STORM_STRIDER_ON_TAKE_DAMAGE:
  case ModifierOpCode::MONSTER_BEHAVIOR_SOUL_EATER_ON_ENEMY_DEATH:
  case ModifierOpCode::MONSTER_BEHAVIOR_BERSERKER_UPDATE:
  case ModifierOpCode::MONSTER_BEHAVIOR_VOIDZONE_UPDATE:
  case ModifierOpCode::MONSTER_BEHAVIOR_SUPPRESSOR_UPDATE:
  case ModifierOpCode::MONSTER_BEHAVIOR_AVENGER_ON_NEARBY_DEATH:
  case ModifierOpCode::MONSTER_BEHAVIOR_SOUL_LINK_UPDATE:
  case ModifierOpCode::MONSTER_BEHAVIOR_STORM_UPDATE:
  case ModifierOpCode::MONSTER_BEHAVIOR_VOID_ON_HIT:
    return ModifierOpCategory::Behavior;
  case ModifierOpCode::SKILL_MORE_DAMAGE_MULT:
  case ModifierOpCode::SKILL_COOLDOWN_FLAT:
  case ModifierOpCode::SKILL_COOLDOWN_MULT:
  case ModifierOpCode::SKILL_CHARGES_ADD:
  case ModifierOpCode::SKILL_BONUS_CRIT:
  case ModifierOpCode::SKILL_AREA_MULT:
  case ModifierOpCode::SKILL_MANA_COST_MULT:
  case ModifierOpCode::SKILL_PROJECTILES_ADD:
  case ModifierOpCode::SKILL_MANA_COST_FLAT:
  case ModifierOpCode::SKILL_BONUS_CRIT_DAMAGE:
  case ModifierOpCode::SKILL_RANGE_MULT:
  case ModifierOpCode::SKILL_DURATION_FLAT:
  case ModifierOpCode::SKILL_SPEED_MULT:
    return ModifierOpCategory::SkillDelivery;
  }
  return ModifierOpCategory::None;
}

// 施加单条算子：effectivePercentMult 仅替代 ADD_STAT_PERCENT_MULT 的数值
// （命中运行时覆盖时为滚值，否则为离线模板值）；points 仅作用于技能交付算子，
// 既有属性/事件/行为算子忽略 points。运行时记录路径与静态记录路径共用本函数，
// 避免两处 switch 漂移。
void ApplyOp(const ModifierOpCode opcode, const uint32_t paramU32,
             const float paramF32, const uint16_t points,
             const float effectivePercentMult, ModifierDelta &out) {
  const float pts = static_cast<float>(points);
  switch (opcode) {
  case ModifierOpCode::ADD_STAT_FLAT:
    out.AddFlat(paramU32, paramF32);
    break;
  case ModifierOpCode::ADD_STAT_PERCENT_ADD:
    out.AddPercentAdd(paramU32, paramF32);
    break;
  case ModifierOpCode::ADD_STAT_PERCENT_MULT:
    out.AddPercentMult(paramU32, effectivePercentMult);
    break;
  case ModifierOpCode::ADD_SKILL_LEVEL:
    out.AddSkillLevel(paramU32, paramF32);
    break;
  case ModifierOpCode::MANA_COST_MULT:
    out.AddManaCostMultiplier(paramU32, paramF32);
    break;
  case ModifierOpCode::MONSTER_EVENT_ON_UPDATE:
    out.AddMonsterEventOnUpdate(paramU32);
    break;
  case ModifierOpCode::MONSTER_EVENT_ON_HIT:
    out.AddMonsterEventOnHit(paramU32);
    break;
  case ModifierOpCode::MONSTER_EVENT_ON_DEATH:
    out.AddMonsterEventOnDeath(paramU32);
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
  case ModifierOpCode::MONSTER_BEHAVIOR_STORM_UPDATE:
    out.AddMonsterBehaviorOnUpdate(static_cast<uint16_t>(opcode));
    break;
  case ModifierOpCode::MONSTER_BEHAVIOR_VAMPIRIC_ON_HIT:
  case ModifierOpCode::MONSTER_BEHAVIOR_NULLIFIER_ON_HIT:
  case ModifierOpCode::MONSTER_BEHAVIOR_ENTANGLER_ON_HIT:
  case ModifierOpCode::MONSTER_BEHAVIOR_MIRROR_IMAGE_ON_TAKE_DAMAGE:
  case ModifierOpCode::MONSTER_BEHAVIOR_STORM_STRIDER_ON_TAKE_DAMAGE:
  case ModifierOpCode::MONSTER_BEHAVIOR_VOID_ON_HIT:
    out.AddMonsterBehaviorOnHit(static_cast<uint16_t>(opcode));
    break;
  case ModifierOpCode::MONSTER_BEHAVIOR_TOXIC_ON_DEATH:
  case ModifierOpCode::MONSTER_BEHAVIOR_SOUL_EATER_ON_ENEMY_DEATH:
  case ModifierOpCode::MONSTER_BEHAVIOR_AVENGER_ON_NEARBY_DEATH:
    out.AddMonsterBehaviorOnDeath(static_cast<uint16_t>(opcode));
    break;
  // 交付乘法算子（More / 冷却 / 范围）刻意不做上限截断：设计 §3.1 明确求值层
  // 不截断，每点幅度按线性外推，节点点数上限由加点/存档校验层保证；此处截断
  // 反而会掩盖加点越界错误。三个算子共用此语义。
  case ModifierOpCode::SKILL_MORE_DAMAGE_MULT:
    out.AddSkillMoreDamageMult(paramU32, 1.0f + paramF32 * pts);
    break;
  case ModifierOpCode::SKILL_COOLDOWN_FLAT:
    out.AddSkillCooldownFlat(paramU32, paramF32 * pts);
    break;
  case ModifierOpCode::SKILL_COOLDOWN_MULT:
    out.AddSkillCooldownMult(paramU32, 1.0f + paramF32 * pts);
    break;
  case ModifierOpCode::SKILL_CHARGES_ADD:
    out.AddSkillCharges(paramU32, static_cast<int>(paramF32 * pts));
    break;
  case ModifierOpCode::SKILL_BONUS_CRIT:
    out.AddSkillBonusCrit(paramU32, paramF32 * pts);
    break;
  case ModifierOpCode::SKILL_AREA_MULT:
    out.AddSkillAreaMult(paramU32, 1.0f + paramF32 * pts);
    break;
  case ModifierOpCode::SKILL_MANA_COST_MULT:
    // 仅法耗下限 0：负数法力消耗物理上无意义；该下限不构成对其余交付算子的截断。
    out.AddManaCostMultiplier(paramU32, std::max(0.0f, 1.0f - paramF32 * pts));
    break;
  case ModifierOpCode::SKILL_PROJECTILES_ADD:
    out.AddSkillProjectiles(paramU32, static_cast<int>(paramF32 * pts));
    break;
  case ModifierOpCode::SKILL_MANA_COST_FLAT:
    out.AddSkillManaCostFlat(paramU32, paramF32 * pts);
    break;
  case ModifierOpCode::SKILL_BONUS_CRIT_DAMAGE:
    out.AddSkillBonusCritDamage(paramU32, paramF32 * pts);
    break;
  case ModifierOpCode::SKILL_RANGE_MULT:
    out.AddSkillRangeMult(paramU32, 1.0f + paramF32 * pts);
    break;
  // Batch 2 交付算子：持续时间每点秒数按加性缩放，弹速每点相对偏移按
  // 1 + param_f32 * pts 线性外推为乘性系数；同样不做上限截断，
  // 数值下限（如持续时间/弹速不为负）由消费端负责。
  case ModifierOpCode::SKILL_DURATION_FLAT:
    out.AddSkillDurationFlat(paramU32, paramF32 * pts);
    break;
  case ModifierOpCode::SKILL_SPEED_MULT:
    out.AddSkillSpeedMult(paramU32, 1.0f + paramF32 * pts);
    break;
  }
}

// 对单条 registry 记录做 filter 匹配并按类别掩码累积算子。
// request 非空时，其 overrides 仅作用于 op.param_u32 == target_stat 的乘算算子。
void EvaluateRuntimeRecord(const ModifierRuntimeRegistry &registry,
                           const ModifierRuntimeRecord &record,
                           const ModifierEvalContext &ctx,
                           const ModifierRecordRequest *request,
                           const ModifierOpCategory categories,
                           ModifierDelta &out) {
  const ModifierRuntimeFilter *filter = registry.GetFilter(record);
  if (filter == nullptr) {
    return;
  }

  const auto skillWhitelist = registry.GetSkillWhitelist(*filter);
  const auto nodeWhitelist = registry.GetNodeWhitelist(*filter);
  if (!MatchesFilters(*filter, skillWhitelist, nodeWhitelist, ctx)) {
    return;
  }

  const uint16_t effectivePoints = ResolveEffectivePoints(nodeWhitelist, ctx);
  if (effectivePoints == 0) {
    return;
  }

  for (const auto &op : registry.GetOps(record)) {
    const ModifierOpCode opcode = static_cast<ModifierOpCode>(op.opcode);
    if (!HasOpCategory(categories, CategoryOfOp(opcode))) {
      continue;
    }

    float effectivePercentMult = op.param_f32;
    if (request != nullptr && request->override_percent_mult &&
        opcode == ModifierOpCode::ADD_STAT_PERCENT_MULT &&
        (request->target_stat == kAllStatTargets ||
         request->target_stat == op.param_u32)) {
      effectivePercentMult = request->param_f32;
    }

    ApplyOp(opcode, op.param_u32, op.param_f32, effectivePoints,
            effectivePercentMult, out);
  }
}

} // namespace

bool IsSkillDeliveryOpCode(const ModifierOpCode opcode) {
  return CategoryOfOp(opcode) == ModifierOpCategory::SkillDelivery;
}

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

void ModifierDelta::AddManaCostMultiplier(const uint32_t skillId,
                                          const float mult) {
  const auto it = mana_cost_mult.find(skillId);
  if (it == mana_cost_mult.end()) {
    mana_cost_mult.emplace(skillId, mult);
    return;
  }
  it->second *= mult;
}

void ModifierDelta::AddSkillMoreDamageMult(const uint32_t skillId,
                                           const float mult) {
  const auto it = skill_more_damage_mult.find(skillId);
  if (it == skill_more_damage_mult.end()) {
    skill_more_damage_mult.emplace(skillId, mult);
    return;
  }
  it->second *= mult;
}

void ModifierDelta::AddSkillCooldownFlat(const uint32_t skillId,
                                         const float value) {
  skill_cooldown_flat[skillId] += value;
}

void ModifierDelta::AddSkillCooldownMult(const uint32_t skillId,
                                         const float mult) {
  const auto it = skill_cooldown_mult.find(skillId);
  if (it == skill_cooldown_mult.end()) {
    skill_cooldown_mult.emplace(skillId, mult);
    return;
  }
  it->second *= mult;
}

void ModifierDelta::AddSkillCharges(const uint32_t skillId, const int value) {
  skill_charges_add[skillId] += value;
}

void ModifierDelta::AddSkillBonusCrit(const uint32_t skillId,
                                      const float value) {
  skill_bonus_crit[skillId] += value;
}

void ModifierDelta::AddSkillAreaMult(const uint32_t skillId,
                                     const float mult) {
  const auto it = skill_area_mult.find(skillId);
  if (it == skill_area_mult.end()) {
    skill_area_mult.emplace(skillId, mult);
    return;
  }
  it->second *= mult;
}

void ModifierDelta::AddSkillProjectiles(const uint32_t skillId,
                                        const int value) {
  skill_projectiles_add[skillId] += value;
}

void ModifierDelta::AddSkillManaCostFlat(const uint32_t skillId,
                                         const float value) {
  skill_mana_cost_flat[skillId] += value;
}

void ModifierDelta::AddSkillBonusCritDamage(const uint32_t skillId,
                                            const float value) {
  skill_bonus_crit_damage[skillId] += value;
}

void ModifierDelta::AddSkillRangeMult(const uint32_t skillId,
                                      const float mult) {
  const auto it = skill_range_mult.find(skillId);
  if (it == skill_range_mult.end()) {
    skill_range_mult.emplace(skillId, mult);
    return;
  }
  it->second *= mult;
}

void ModifierDelta::AddSkillDurationFlat(const uint32_t skillId,
                                         const float delta) {
  skill_duration_flat[skillId] += delta;
}

void ModifierDelta::AddSkillSpeedMult(const uint32_t skillId,
                                      const float mult) {
  const auto it = skill_speed_mult.find(skillId);
  if (it == skill_speed_mult.end()) {
    skill_speed_mult.emplace(skillId, mult);
    return;
  }
  it->second *= mult;
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

float ModifierDelta::GetManaCostMultiplier(const uint32_t skillId) const {
  // key 0 为全局通配，对任意技能生效；skillId == 0 时只返回全局系数
  const float globalMult = ReadOr(mana_cost_mult, 0u, 1.0f);
  const float skillMult =
      (skillId != 0u) ? ReadOr(mana_cost_mult, skillId, 1.0f) : 1.0f;
  return globalMult * skillMult;
}

float ModifierDelta::GetSkillMoreDamageMult(const uint32_t skillId) const {
  return ReadOr(skill_more_damage_mult, skillId, 1.0f);
}

float ModifierDelta::GetSkillCooldownFlat(const uint32_t skillId) const {
  return ReadOr(skill_cooldown_flat, skillId, 0.0f);
}

float ModifierDelta::GetSkillCooldownMult(const uint32_t skillId) const {
  return ReadOr(skill_cooldown_mult, skillId, 1.0f);
}

int ModifierDelta::GetSkillCharges(const uint32_t skillId) const {
  const auto it = skill_charges_add.find(skillId);
  return it != skill_charges_add.end() ? it->second : 0;
}

float ModifierDelta::GetSkillBonusCrit(const uint32_t skillId) const {
  return ReadOr(skill_bonus_crit, skillId, 0.0f);
}

float ModifierDelta::GetSkillAreaMult(const uint32_t skillId) const {
  return ReadOr(skill_area_mult, skillId, 1.0f);
}

int ModifierDelta::GetSkillProjectiles(const uint32_t skillId) const {
  const auto it = skill_projectiles_add.find(skillId);
  return it != skill_projectiles_add.end() ? it->second : 0;
}

float ModifierDelta::GetSkillManaCostFlat(const uint32_t skillId) const {
  return ReadOr(skill_mana_cost_flat, skillId, 0.0f);
}

float ModifierDelta::GetSkillBonusCritDamage(const uint32_t skillId) const {
  return ReadOr(skill_bonus_crit_damage, skillId, 0.0f);
}

float ModifierDelta::GetSkillRangeMult(const uint32_t skillId) const {
  return ReadOr(skill_range_mult, skillId, 1.0f);
}

float ModifierDelta::GetSkillDurationFlat(const uint32_t skillId) const {
  return ReadOr(skill_duration_flat, skillId, 0.0f);
}

float ModifierDelta::GetSkillSpeedMult(const uint32_t skillId) const {
  return ReadOr(skill_speed_mult, skillId, 1.0f);
}

void ModifierDelta::MergeFrom(const ModifierDelta &other) {
  if (this == &other) {
    return;
  }

  // 加性容器累加；乘性容器以缺省单位元为基准累乘（首次出现直接落值）。
  const auto mergeAdditive = [](auto &dst, const auto &src) {
    for (const auto &[key, value] : src) {
      dst[key] += value;
    }
  };
  const auto mergeMultiplicative = [](auto &dst, const auto &src) {
    for (const auto &[key, value] : src) {
      const auto it = dst.find(key);
      if (it == dst.end()) {
        dst.emplace(key, value);
      } else {
        it->second *= value;
      }
    }
  };
  const auto mergeSet = [](auto &dst, const auto &src) {
    dst.insert(src.begin(), src.end());
  };

  mergeAdditive(flat, other.flat);
  mergeAdditive(percent_add, other.percent_add);
  mergeMultiplicative(percent_mult, other.percent_mult);
  mergeAdditive(skill_levels, other.skill_levels);
  mergeMultiplicative(mana_cost_mult, other.mana_cost_mult);
  mergeAdditive(skill_cooldown_flat, other.skill_cooldown_flat);
  mergeAdditive(skill_charges_add, other.skill_charges_add);
  mergeAdditive(skill_bonus_crit, other.skill_bonus_crit);
  mergeMultiplicative(skill_more_damage_mult, other.skill_more_damage_mult);
  mergeMultiplicative(skill_cooldown_mult, other.skill_cooldown_mult);
  mergeMultiplicative(skill_area_mult, other.skill_area_mult);
  mergeAdditive(skill_projectiles_add, other.skill_projectiles_add);
  mergeAdditive(skill_mana_cost_flat, other.skill_mana_cost_flat);
  mergeAdditive(skill_bonus_crit_damage, other.skill_bonus_crit_damage);
  mergeMultiplicative(skill_range_mult, other.skill_range_mult);
  mergeAdditive(skill_duration_flat, other.skill_duration_flat);
  mergeMultiplicative(skill_speed_mult, other.skill_speed_mult);
  mergeSet(monster_event_on_update_affix_ids,
           other.monster_event_on_update_affix_ids);
  mergeSet(monster_event_on_hit_affix_ids, other.monster_event_on_hit_affix_ids);
  mergeSet(monster_event_on_death_affix_ids,
           other.monster_event_on_death_affix_ids);
  mergeSet(monster_behavior_on_update_opcodes,
           other.monster_behavior_on_update_opcodes);
  mergeSet(monster_behavior_on_hit_opcodes, other.monster_behavior_on_hit_opcodes);
  mergeSet(monster_behavior_on_death_opcodes,
           other.monster_behavior_on_death_opcodes);
}

ModifierDelta ModifierEvaluator::Evaluate(
    const std::span<const ModifierRecord> records, const ModifierEvalContext &ctx) {
  ModifierDelta out;
  for (const auto &record : records) {
    if (!MatchesFilters(record.filter, ctx)) {
      continue;
    }
    const uint16_t effectivePoints =
        ResolveEffectivePoints(record.filter.node_id_whitelist, ctx);
    if (effectivePoints == 0) {
      continue;
    }
    for (const auto &op : record.ops) {
      ApplyOp(op.opcode, op.param_u32, op.param_f32, effectivePoints,
              op.param_f32, out);
    }
  }
  return out;
}

ModifierDelta ModifierEvaluator::Evaluate(
    const ModifierRuntimeRegistry &registry,
    const std::span<const ModifierRecordRequest> requests,
    const ModifierEvalContext &ctx, const ModifierOpCategory categories) {
  ModifierDelta out;
  for (const auto &request : requests) {
    const ModifierRuntimeRecord *record =
        registry.FindRecordById(request.record_id);
    if (record == nullptr) {
      continue;
    }
    EvaluateRuntimeRecord(registry, *record, ctx, &request, categories, out);
  }
  return out;
}

ModifierDelta ModifierEvaluator::Evaluate(
    const ModifierRuntimeRegistry &registry,
    const std::span<const uint32_t> recordIds, const ModifierEvalContext &ctx,
    const ModifierOpCategory categories) {
  ModifierDelta out;
  for (const uint32_t recordId : recordIds) {
    const ModifierRuntimeRecord *record = registry.FindRecordById(recordId);
    if (record == nullptr) {
      continue;
    }
    EvaluateRuntimeRecord(registry, *record, ctx, nullptr, categories, out);
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
