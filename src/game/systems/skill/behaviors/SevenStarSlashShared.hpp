#pragma once

#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/SkillPointAccess.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/skill/BladeResourceService.hpp"
#include "game/systems/skill/SkillSystem.hpp"

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
  registry.get_or_emplace<StatsDirty>(owner);
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

// 技能 10（七星斩）节点 id：行为实现与单测共享的唯一定义，禁止在其他 TU 重复声明。
namespace SevenStarSlashNodes {
constexpr uint32_t TargetLock = 1000;
constexpr uint32_t CritChance = 1001;
constexpr uint32_t FinalSlash = 1002;
constexpr uint32_t QuickStar = 1003;
constexpr uint32_t ExposedWeakness = 1004;
constexpr uint32_t PoJun = 1005;
constexpr uint32_t ZhanJiang = 1006;
constexpr uint32_t SevenFocus = 1007;
constexpr uint32_t SolitaryStar = 1008;
constexpr uint32_t FlowReturn = 1009;
constexpr uint32_t RevolvingEdge = 1010;
constexpr uint32_t StarScarFollow = 1011;
constexpr uint32_t ChaseStep = 1012;
constexpr uint32_t EndlessSeven = 1013;
constexpr uint32_t VoidTread = 1015;
constexpr uint32_t FallingStarSwitch = 1016;
constexpr uint32_t SwordStepMirage = 1017;
constexpr uint32_t StarVeil = 1018;
constexpr uint32_t GateOfLife = 1019;
constexpr uint32_t LingeringScar = 1020;
constexpr uint32_t PoleStarOrbit = 1021;
constexpr uint32_t Starfall = 1022;
constexpr uint32_t ShatteredConstellation = 1023;
constexpr uint32_t ScarRuin = 1024;
constexpr uint32_t ReturningStep = 1025;
} // namespace SevenStarSlashNodes

// 七星斩 SpecState（POD）：DoCast 内一次性解析，循环内只读字段。
// 行为实现与单测共享同一定义，避免测试镜像漂移（设计 §4.7 DoD#7）。
struct SevenStarSlashSpecState {
  int targetLockPoints = 0;
  int critChancePoints = 0;
  int finalSlashPoints = 0;
  int quickStarPoints = 0;
  int exposedWeaknessPoints = 0;
  int poJunPoints = 0;
  int zhanJiangPoints = 0;
  bool sevenFocus = false;
  int solitaryStarPoints = 0;
  int flowReturnPoints = 0;
  int revolvingEdgePoints = 0;
  bool starScarFollow = false;
  int chaseStepPoints = 0;
  bool endlessSeven = false;
  int voidTreadPoints = 0;
  bool fallingStarSwitch = false;
  bool swordStepMirage = false;
  int starVeilPoints = 0;
  int gateOfLifePoints = 0;
  int lingeringScarPoints = 0;
  bool poleStarOrbit = false;
  bool starfall = false;
  int shatteredConstellationPoints = 0;
  int scarRuinPoints = 0;
  bool returningStep = false;
};

// 静态绑定表：节点 id -> SpecState 成员指针。以表驱动替代逐字段赋值，
// 行为实现与单测共享同一张表；不含字符串比较或堆分配（设计 §4.4 项 2）。
struct SevenStarSlashPointBinding {
  uint32_t node;
  int SevenStarSlashSpecState::*points;
};

struct SevenStarSlashFlagBinding {
  uint32_t node;
  bool SevenStarSlashSpecState::*flag;
};

inline constexpr std::array<SevenStarSlashPointBinding, 17>
    kSevenStarSlashPointBindings{{
        {SevenStarSlashNodes::TargetLock,
         &SevenStarSlashSpecState::targetLockPoints},
        {SevenStarSlashNodes::CritChance,
         &SevenStarSlashSpecState::critChancePoints},
        {SevenStarSlashNodes::FinalSlash,
         &SevenStarSlashSpecState::finalSlashPoints},
        {SevenStarSlashNodes::QuickStar,
         &SevenStarSlashSpecState::quickStarPoints},
        {SevenStarSlashNodes::ExposedWeakness,
         &SevenStarSlashSpecState::exposedWeaknessPoints},
        {SevenStarSlashNodes::PoJun, &SevenStarSlashSpecState::poJunPoints},
        {SevenStarSlashNodes::ZhanJiang,
         &SevenStarSlashSpecState::zhanJiangPoints},
        {SevenStarSlashNodes::SolitaryStar,
         &SevenStarSlashSpecState::solitaryStarPoints},
        {SevenStarSlashNodes::FlowReturn,
         &SevenStarSlashSpecState::flowReturnPoints},
        {SevenStarSlashNodes::RevolvingEdge,
         &SevenStarSlashSpecState::revolvingEdgePoints},
        {SevenStarSlashNodes::ChaseStep,
         &SevenStarSlashSpecState::chaseStepPoints},
        {SevenStarSlashNodes::VoidTread,
         &SevenStarSlashSpecState::voidTreadPoints},
        {SevenStarSlashNodes::StarVeil,
         &SevenStarSlashSpecState::starVeilPoints},
        {SevenStarSlashNodes::GateOfLife,
         &SevenStarSlashSpecState::gateOfLifePoints},
        {SevenStarSlashNodes::LingeringScar,
         &SevenStarSlashSpecState::lingeringScarPoints},
        {SevenStarSlashNodes::ShatteredConstellation,
         &SevenStarSlashSpecState::shatteredConstellationPoints},
        {SevenStarSlashNodes::ScarRuin,
         &SevenStarSlashSpecState::scarRuinPoints},
    }};

inline constexpr std::array<SevenStarSlashFlagBinding, 6>
    kSevenStarSlashFlagBindings{{
        {SevenStarSlashNodes::SevenFocus, &SevenStarSlashSpecState::sevenFocus},
        {SevenStarSlashNodes::StarScarFollow,
         &SevenStarSlashSpecState::starScarFollow},
        {SevenStarSlashNodes::EndlessSeven,
         &SevenStarSlashSpecState::endlessSeven},
        {SevenStarSlashNodes::FallingStarSwitch,
         &SevenStarSlashSpecState::fallingStarSwitch},
        {SevenStarSlashNodes::SwordStepMirage,
         &SevenStarSlashSpecState::swordStepMirage},
        {SevenStarSlashNodes::ReturningStep,
         &SevenStarSlashSpecState::returningStep},
    }};

// 解析技能 10 的 SpecState：首个匹配槽位一次性填表（HasNode 语义 = 已投入点数 > 0），
// 再叠加运行时激活转质节点（1021/1022 非点读，保留在表循环外）。
[[nodiscard]] inline SevenStarSlashSpecState
ResolveSpecState(const entt::registry &registry, entt::entity owner) {
  SevenStarSlashSpecState state;
  if (const auto *active = registry.try_get<ActiveSkillsComponent>(owner)) {
    for (const auto &spec : active->specialized_slots) {
      if (spec.skill_id != seven_star_shared::kSevenStarSlashSkillId) {
        continue;
      }
      for (const auto &binding : kSevenStarSlashPointBindings) {
        state.*(binding.points) = ReadPoints(spec, binding.node);
      }
      for (const auto &binding : kSevenStarSlashFlagBindings) {
        state.*(binding.flag) = HasNode(spec, binding.node);
      }
      break;
    }
  }

  const uint32_t activeTransmuter = SkillSystem::GetActiveTransmuterNode(
      registry, owner, seven_star_shared::kSevenStarSlashSkillId);
  state.poleStarOrbit = activeTransmuter == SevenStarSlashNodes::PoleStarOrbit;
  state.starfall = activeTransmuter == SevenStarSlashNodes::Starfall;
  return state;
}

} // namespace NoMoreDay::skills
