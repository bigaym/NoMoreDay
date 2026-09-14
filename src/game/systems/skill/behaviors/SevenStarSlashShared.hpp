#pragma once

#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/SkillPointAccess.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/generated/SevenStarSlashSpecState.gen.hpp"

#include <entt/entt.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace NoMoreDay::skills::seven_star_shared {

inline constexpr uint32_t kSevenStarSlashSkillId = 10;
inline constexpr uint32_t kFlowingThrustSkillId = 1;

inline constexpr uint32_t kNodeDipperReturn = 1014;

inline constexpr const char *kRevolvingEdgeBuffId = "seven_star_revolving_edge";
inline constexpr const char *kQiyaoBuffId = "seven_star_qiyao";
inline constexpr const char *kReturningStepBuffId = "seven_star_returning_step";
inline constexpr const char *kReturningStepDefenseBuffId =
    "seven_star_returning_step_defense";

inline int GetAllocatedPoints(const entt::registry &registry, entt::entity owner,
                              uint32_t skillId, uint32_t nodeId) {
  const auto *active = registry.try_get<ActiveSkillsComponent>(owner);
  if (active == nullptr) {
    return 0;
  }

  for (const auto &spec : active->specialized_slots) {
    if (spec.skill_id != skillId) {
      continue;
    }
    return skills::ReadPoints(spec, nodeId);
  }
  return 0;
}

inline BuffEffect *FindBuff(entt::registry &registry, entt::entity owner,
                            std::string_view id) {
  auto *effects = registry.try_get<ActiveEffectsComponent>(owner);
  if (effects == nullptr) {
    return nullptr;
  }
  return effects->Get(id);
}

inline const BuffEffect *FindBuff(const entt::registry &registry,
                                  entt::entity owner, std::string_view id) {
  const auto *effects = registry.try_get<ActiveEffectsComponent>(owner);
  if (effects == nullptr) {
    return nullptr;
  }
  for (const auto &effect : effects->effects) {
    if (effect.id == id) {
      return &effect;
    }
  }
  return nullptr;
}

inline void RefundSkillCooldownPercent(entt::registry &registry,
                                       entt::entity owner, uint32_t skillId,
                                       float pct) {
  if (pct <= 0.0f) {
    return;
  }
  auto *active = registry.try_get<ActiveSkillsComponent>(owner);
  if (active == nullptr) {
    return;
  }

  for (auto &slot : active->slots) {
    if (slot.id != skillId || slot.cooldown <= 0.0f) {
      continue;
    }
    slot.cooldown = std::max(0.0f, slot.cooldown * (1.0f - pct));
  }
}

inline void ResetSkillCooldown(entt::registry &registry, entt::entity owner,
                               uint32_t skillId) {
  auto *active = registry.try_get<ActiveSkillsComponent>(owner);
  if (active == nullptr) {
    return;
  }
  for (auto &slot : active->slots) {
    if (slot.id != skillId) {
      continue;
    }
    slot.cooldown = 0.0f;
  }
}

inline void RefundMovementCooldownsPercent(entt::registry &registry,
                                           entt::entity owner, float pct) {
  RefundSkillCooldownPercent(registry, owner, kFlowingThrustSkillId, pct);
}

inline void RestoreSkillCharge(entt::registry &registry, entt::entity owner,
                               uint32_t skillId) {
  auto *active = registry.try_get<ActiveSkillsComponent>(owner);
  const auto *skill = SkillRegistry::Get().GetSkill(skillId);
  if (active == nullptr || skill == nullptr || skill->max_charges <= 1) {
    return;
  }

  for (auto &slot : active->slots) {
    if (slot.id != skillId) {
      continue;
    }
    const int nextCharges = std::min(skill->max_charges,
                                     static_cast<int>(slot.current_charges) + 1);
    slot.current_charges = static_cast<uint8_t>(nextCharges);
    break;
  }
}

inline void RefundManaCost(entt::registry &registry, entt::entity owner,
                           uint32_t skillId) {
  auto *stats = registry.try_get<CombatStats>(owner);
  const auto *skill = SkillRegistry::Get().GetSkill(skillId);
  if (stats == nullptr || skill == nullptr || skill->mana_cost <= 0.0f) {
    return;
  }
  stats->mana = std::min(stats->max_mana, stats->mana + skill->mana_cost);
}

inline void GrantSwordStep(entt::registry &registry, entt::entity owner,
                           float duration = 2.0f, float moveSpeedPct = 30.0f) {
  auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(owner);
  BuffEffect swift;
  swift.id = std::string(BuffIdToString(BuffId::SwordStep));
  swift.name = "Sword Step";
  swift.type = BuffType::SpeedUp;
  swift.duration = duration;
  swift.remaining = duration;
  swift.modifiers.push_back({.value = moveSpeedPct,
                             .type = StatType::MoveSpeed,
                             .mode = ModifierMode::PercentAdd});
  effects.AddOrRefresh(swift);
  registry.emplace_or_replace<PhaseTag>(owner);
  (void)registry.get_or_emplace<StatsDirty>(owner);
}

inline void GrantReturningStepDefense(entt::registry &registry,
                                      entt::entity owner,
                                      float duration = 1.5f,
                                      float reductionPct = 15.0f) {
  auto &effects = registry.get_or_emplace<ActiveEffectsComponent>(owner);
  BuffEffect defense;
  defense.id = kReturningStepDefenseBuffId;
  defense.name = "Returning Step";
  defense.type = BuffType::Shield;
  defense.duration = duration;
  defense.remaining = duration;
  defense.modifiers.push_back({.value = reductionPct,
                               .type = StatType::GlobalDamageReduction,
                               .mode = ModifierMode::Flat});
  effects.AddOrRefresh(defense);
}

inline bool DeterministicRoll(uint64_t seed, uint32_t salt, float pct) {
  if (pct <= 0.0f) {
    return false;
  }
  if (pct >= 100.0f) {
    return true;
  }

  uint64_t value = seed ^ (static_cast<uint64_t>(salt) * 0x9E3779B97F4A7C15ull);
  value ^= (value >> 30);
  value *= 0xBF58476D1CE4E5B9ull;
  value ^= (value >> 27);
  value *= 0x94D049BB133111EBull;
  value ^= (value >> 31);
  const float roll = static_cast<float>(value % 10000ull) / 100.0f;
  return roll < pct;
}

struct LinkConsumption {
  float damage_multiplier = 1.0f;
  int qiyao_stacks = 0;
  bool consumed_any = false;
  bool consume_returning_step = false;
};

inline LinkConsumption ConsumeLinkBuffs(entt::registry &registry,
                                        entt::entity owner, uint32_t skillId,
                                        bool isMovementSkill,
                                        uint64_t seed) {
  LinkConsumption result;
  auto *effects = registry.try_get<ActiveEffectsComponent>(owner);
  if (effects == nullptr) {
    return result;
  }

  if (BuffEffect *revolving = effects->Get(kRevolvingEdgeBuffId)) {
    result.damage_multiplier *= 1.0f + 0.10f * revolving->stacks;
    result.consumed_any = true;
    effects->Remove(kRevolvingEdgeBuffId);
  }

  if (BuffEffect *qiyao = effects->Get(kQiyaoBuffId)) {
    result.qiyao_stacks = std::max(0, qiyao->stacks);
    result.damage_multiplier *= 1.0f + 0.08f * result.qiyao_stacks;
    result.consumed_any = true;
    if (isMovementSkill && result.qiyao_stacks > 0) {
      RefundSkillCooldownPercent(registry, owner, skillId,
                                 0.05f * static_cast<float>(result.qiyao_stacks));
    }
    effects->Remove(kQiyaoBuffId);
  }

  if (isMovementSkill && effects->Get(kReturningStepBuffId) != nullptr) {
    result.consume_returning_step = true;
    effects->Remove(kReturningStepBuffId);
  }

  const int dipperPoints =
      GetAllocatedPoints(registry, owner, kSevenStarSlashSkillId, kNodeDipperReturn);
  if (result.consumed_any && dipperPoints > 0) {
    if (DeterministicRoll(seed, skillId + kNodeDipperReturn,
                          10.0f * static_cast<float>(dipperPoints))) {
      (void)systems::BladeResourceService::Gain(registry, owner, 1, skillId);
    }
    if (!isMovementSkill) {
      RefundMovementCooldownsPercent(registry, owner,
                                     0.05f * static_cast<float>(dipperPoints));
    }
  }

  return result;
}

inline void ApplyReturningStepOverride(entt::registry &registry,
                                       entt::entity owner, uint32_t skillId) {
  RefundSkillCooldownPercent(registry, owner, skillId, 0.5f);
  RestoreSkillCharge(registry, owner, skillId);
  RefundManaCost(registry, owner, skillId);
  GrantReturningStepDefense(registry, owner);
}

} // namespace NoMoreDay::skills::seven_star_shared

namespace NoMoreDay::skills {

// 技能 10（七星斩）节点常量与 SpecState 由 A-02 生成头统一提供（设计 §2.2），
// 此处仅以别名承接既有引用点，不再手写节点常量、POD 与绑定表。
namespace SevenStarSlashNodes = SevenStarSlashNodesGen;
using SevenStarSlashSpecState = SevenStarSlashSpecStateGen;

// 解析技能 10 的 SpecState：委托统一模板一次性填 17 个点读字段与 8 个 flag
//（6 个数据驱动 + 1021/1022 转质占位）。转质节点 1021/1022 在表内的 HasNode 填充仅为占位，
// 最终语义一律以 GetActiveTransmuterNode
// 的互斥单选为准，故此处对两字段无条件覆盖（设计 §2.2；绕过本包装函数者不得读取这两字段）。
[[nodiscard]] inline SevenStarSlashSpecState
ResolveSpecState(const entt::registry &registry, entt::entity owner) {
  auto state = ResolveSpecState(registry, owner,
                                seven_star_shared::kSevenStarSlashSkillId,
                                kSevenStarSlashTableGen);
  const uint32_t activeTransmuter = SkillSystem::GetActiveTransmuterNode(
      registry, owner, seven_star_shared::kSevenStarSlashSkillId);
  state.poleStarOrbit = activeTransmuter == SevenStarSlashNodes::PoleStarOrbit;
  state.starfall = activeTransmuter == SevenStarSlashNodes::Starfall;
  return state;
}

} // namespace NoMoreDay::skills
