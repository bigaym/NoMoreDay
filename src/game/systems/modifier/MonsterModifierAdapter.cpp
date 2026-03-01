#include "game/systems/modifier/MonsterModifierAdapter.hpp"

#include "game/components/Stats.hpp"
#include "game/systems/modifier/ModifierContext.hpp"

#include <cmath>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace NoMoreDay {
namespace {

constexpr uint32_t kMonsterNodeIdBase = 800000u;

uint32_t EncodeMonsterAffixNodeId(const MonsterAffixType affixType) {
  return kMonsterNodeIdBase + static_cast<uint32_t>(affixType);
}

ModifierOpCode ToModifierOpCode(const ModifierMode mode) {
  switch (mode) {
  case ModifierMode::Flat:
    return ModifierOpCode::ADD_STAT_FLAT;
  case ModifierMode::PercentAdd:
    return ModifierOpCode::ADD_STAT_PERCENT_ADD;
  case ModifierMode::PercentMult:
    return ModifierOpCode::ADD_STAT_PERCENT_MULT;
  }
  return ModifierOpCode::ADD_STAT_FLAT;
}

float NormalizeValue(const ModifierMode mode, const float value) {
  switch (mode) {
  case ModifierMode::Flat:
    return value;
  case ModifierMode::PercentAdd:
  case ModifierMode::PercentMult:
    return value / 100.0f;
  }
  return value;
}

void AppendEventOpsForAffix(ModifierRecord &record, const MonsterAffixType affixType,
                            const AffixFlags &flags) {
  const uint32_t affixId = static_cast<uint32_t>(affixType);

  if (flags.hasUpdate) {
    ModifierOp updateOp;
    updateOp.opcode = ModifierOpCode::MONSTER_EVENT_ON_UPDATE;
    updateOp.param_u32 = affixId;
    record.ops.push_back(updateOp);
  }

  if (flags.hasOnHit) {
    ModifierOp onHitOp;
    onHitOp.opcode = ModifierOpCode::MONSTER_EVENT_ON_HIT;
    onHitOp.param_u32 = affixId;
    record.ops.push_back(onHitOp);
  }

  if (flags.hasOnDeath) {
    ModifierOp onDeathOp;
    onDeathOp.opcode = ModifierOpCode::MONSTER_EVENT_ON_DEATH;
    onDeathOp.param_u32 = affixId;
    record.ops.push_back(onDeathOp);
  }
}

void AppendBehaviorOpsForAffix(ModifierRecord &record,
                               const MonsterAffixType affixType) {
  ModifierOp behaviorOp;
  switch (affixType) {
  case MonsterAffixType::Molten:
    behaviorOp.opcode = ModifierOpCode::MONSTER_BEHAVIOR_MOLTEN_UPDATE;
    break;
  case MonsterAffixType::Teleporter:
    behaviorOp.opcode = ModifierOpCode::MONSTER_BEHAVIOR_TELEPORTER_UPDATE;
    break;
  case MonsterAffixType::Frozen:
    behaviorOp.opcode = ModifierOpCode::MONSTER_BEHAVIOR_FROZEN_UPDATE;
    break;
  case MonsterAffixType::ManaSiphon:
    behaviorOp.opcode = ModifierOpCode::MONSTER_BEHAVIOR_MANA_SIPHON_UPDATE;
    break;
  case MonsterAffixType::Shielding:
    behaviorOp.opcode = ModifierOpCode::MONSTER_BEHAVIOR_SHIELDING_UPDATE;
    break;
  case MonsterAffixType::Vortex:
    behaviorOp.opcode = ModifierOpCode::MONSTER_BEHAVIOR_VORTEX_UPDATE;
    break;
  case MonsterAffixType::Waller:
    behaviorOp.opcode = ModifierOpCode::MONSTER_BEHAVIOR_WALLER_UPDATE;
    break;
  case MonsterAffixType::Berserker:
    behaviorOp.opcode = ModifierOpCode::MONSTER_BEHAVIOR_BERSERKER_UPDATE;
    break;
  case MonsterAffixType::VoidZone:
    behaviorOp.opcode = ModifierOpCode::MONSTER_BEHAVIOR_VOIDZONE_UPDATE;
    break;
  case MonsterAffixType::Storm:
    behaviorOp.opcode = ModifierOpCode::MONSTER_BEHAVIOR_STORM_UPDATE;
    break;
  case MonsterAffixType::Suppressor:
    behaviorOp.opcode = ModifierOpCode::MONSTER_BEHAVIOR_SUPPRESSOR_UPDATE;
    break;
  case MonsterAffixType::SoulLink:
    behaviorOp.opcode = ModifierOpCode::MONSTER_BEHAVIOR_SOUL_LINK_UPDATE;
    break;
  case MonsterAffixType::Vampiric:
    behaviorOp.opcode = ModifierOpCode::MONSTER_BEHAVIOR_VAMPIRIC_ON_HIT;
    break;
  case MonsterAffixType::Nullifier:
    behaviorOp.opcode = ModifierOpCode::MONSTER_BEHAVIOR_NULLIFIER_ON_HIT;
    break;
  case MonsterAffixType::Entangler:
    behaviorOp.opcode = ModifierOpCode::MONSTER_BEHAVIOR_ENTANGLER_ON_HIT;
    break;
  case MonsterAffixType::MirrorImage:
    behaviorOp.opcode =
        ModifierOpCode::MONSTER_BEHAVIOR_MIRROR_IMAGE_ON_TAKE_DAMAGE;
    break;
  case MonsterAffixType::StormStrider:
    behaviorOp.opcode =
        ModifierOpCode::MONSTER_BEHAVIOR_STORM_STRIDER_ON_TAKE_DAMAGE;
    break;
  case MonsterAffixType::Void:
    behaviorOp.opcode = ModifierOpCode::MONSTER_BEHAVIOR_VOID_ON_HIT;
    break;
  case MonsterAffixType::Toxic:
    behaviorOp.opcode = ModifierOpCode::MONSTER_BEHAVIOR_TOXIC_ON_DEATH;
    break;
  case MonsterAffixType::SoulEater:
    behaviorOp.opcode = ModifierOpCode::MONSTER_BEHAVIOR_SOUL_EATER_ON_ENEMY_DEATH;
    break;
  case MonsterAffixType::Avenger:
    behaviorOp.opcode =
        ModifierOpCode::MONSTER_BEHAVIOR_AVENGER_ON_NEARBY_DEATH;
    break;
  default:
    return;
  }

  behaviorOp.param_u32 = static_cast<uint32_t>(affixType);
  record.ops.push_back(behaviorOp);
}

bool IsVampiricLifeStealStat(const MonsterAffixType affixType,
                             const MonsterAffixDef::StatMod &statMod) {
  return affixType == MonsterAffixType::Vampiric &&
         statMod.type == StatType::LifeSteal;
}

ModifierDelta EvaluateAffixDeltaInternal(const MonsterAffixComponent &affixComponent,
                                         const bool includeStats,
                                         const bool includeEvents,
                                         const bool includeBehaviorOps) {
  ModifierEvalContext ctx;
  std::vector<ModifierRecord> records;

  for (const auto affixType : affixComponent.affixes) {
    const auto &def = MonsterAffixRegistry::GetAffixDef(affixType);
    const uint32_t nodeId = EncodeMonsterAffixNodeId(affixType);
    ctx.active_node_ids.push_back(nodeId);

    ModifierRecord record;
    record.filter.node_id_whitelist = {nodeId};

    if (includeStats) {
      for (int statModIndex = 0; statModIndex < def.statModCount; ++statModIndex) {
        const auto &statMod = def.statMods[statModIndex];

        if (includeBehaviorOps && IsVampiricLifeStealStat(affixType, statMod)) {
          continue;
        }

        ModifierOp op;
        op.opcode = ToModifierOpCode(statMod.mode);
        op.param_u32 = static_cast<uint32_t>(statMod.type);
        op.param_f32 = NormalizeValue(statMod.mode, statMod.value);
        record.ops.push_back(op);
      }
    }

    if (includeEvents) {
      AppendEventOpsForAffix(record, affixType, def.flags);
    }

    if (includeBehaviorOps) {
      AppendBehaviorOpsForAffix(record, affixType);
    }

    if (record.ops.empty()) {
      continue;
    }

    records.push_back(std::move(record));
  }

  return ModifierEvaluator::Evaluate(
      std::span<const ModifierRecord>(records.data(), records.size()), ctx);
}

} // namespace

ModifierDelta
MonsterModifierAdapter::EvaluateAffixDelta(
    const MonsterAffixComponent &affixComponent) {
  return EvaluateAffixDeltaInternal(affixComponent, true, false, true);
}

MonsterModifierAdapter::MonsterAffixEventSet
MonsterModifierAdapter::EvaluateAffixEvents(
    const MonsterAffixComponent &affixComponent) {
  const ModifierDelta delta =
      EvaluateAffixDeltaInternal(affixComponent, false, true, false);

  MonsterAffixEventSet events;
  events.onUpdateAffixIds = delta.monster_event_on_update_affix_ids;
  events.onHitAffixIds = delta.monster_event_on_hit_affix_ids;
  events.onDeathAffixIds = delta.monster_event_on_death_affix_ids;
  return events;
}

MonsterModifierAdapter::MonsterAffixBehaviorOpSet
MonsterModifierAdapter::EvaluateBehaviorOps(
    const MonsterAffixComponent &affixComponent) {
  const ModifierDelta delta =
      EvaluateAffixDeltaInternal(affixComponent, false, false, true);

  MonsterAffixBehaviorOpSet behaviorOps;
  behaviorOps.onUpdateOpcodes = delta.monster_behavior_on_update_opcodes;
  behaviorOps.onHitOpcodes = delta.monster_behavior_on_hit_opcodes;
  behaviorOps.onDeathOpcodes = delta.monster_behavior_on_death_opcodes;
  return behaviorOps;
}

float MonsterModifierAdapter::GetBerserkWeaponDamageMultiplier(
    const MonsterAffixComponent &affixComponent) {
  if (!affixComponent.isBerserk) {
    return 1.0f;
  }
  return std::pow(MonsterAffixRegistry::Params::BERSERKER_DAMAGE_MULT,
                  static_cast<float>(affixComponent.affixes.size()));
}

} // namespace NoMoreDay
