#pragma once

#include "SkillBehaviorBase.hpp"

#include "game/systems/physics/SpatialGrid.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace NoMoreDay {
struct CombatEvent;
}

namespace NoMoreDay::skills {

// 技能 11（天剑降临）节点 id：行为实现与单测共享的唯一定义，禁止在其他 TU 重复声明。
namespace HeavenlySwordNodes {
constexpr uint32_t SwordCoreCalibration = 1100;
constexpr uint32_t CelestialDomain = 1101;
constexpr uint32_t SkyEdgeInfusion = 1102;
constexpr uint32_t ResidualPressure = 1103;
constexpr uint32_t WorldsplitCore = 1104;
constexpr uint32_t KingslayerIntent = 1105;
constexpr uint32_t MeteorCore = 1106;
constexpr uint32_t SkyPiercingFall = 1107;
constexpr uint32_t SkyRendAftershock = 1108;
constexpr uint32_t EdgeOffering = 1109;
constexpr uint32_t OverflowingTiers = 1110;
constexpr uint32_t SwordRainEcho = 1111;
constexpr uint32_t SpinningHeavens = 1112;
constexpr uint32_t CycleOfAllForms = 1113;
constexpr uint32_t ReturnToTheSheath = 1114;
constexpr uint32_t DomainLock = 1115;
constexpr uint32_t FieldResonance = 1116;
constexpr uint32_t ArraySynchrony = 1117;
constexpr uint32_t TideSpread = 1118;
constexpr uint32_t EnduringHeaven = 1119;
constexpr uint32_t AttunementPolarization = 1120;
constexpr uint32_t LightningTribunal = 1121;
constexpr uint32_t FrozenDominion = 1122;
constexpr uint32_t SolarIncineration = 1123;
constexpr uint32_t ElementalRazing = 1124;
} // namespace HeavenlySwordNodes

inline constexpr uint32_t kHeavenlySwordSkillId = 11;

// 天剑降临施法信封（POD）：DoCast 内一次性解析，循环内只读字段。
// 行为实现与单测共享同一定义，避免测试镜像漂移（设计 §4.7 DoD#7）。
struct HeavenlySwordCastSpec {
  // 各节点已投入点数（ReadPoints）。
  int swordCoreCalibrationPoints = 0;
  int celestialDomainPoints = 0;
  int skyEdgeInfusionPoints = 0;
  int residualPressurePoints = 0;
  int worldsplitCorePoints = 0;
  int kingslayerIntentPoints = 0;
  int meteorCorePoints = 0;
  int skyRendAftershockPoints = 0;
  int edgeOfferingPoints = 0;
  int overflowingTiersPoints = 0;
  int spinningHeavensPoints = 0;
  int returnToTheSheathPoints = 0;
  int fieldResonancePoints = 0;
  int tideSpreadPoints = 0;
  int enduringHeavenPoints = 0;
  int elementalRazingPoints = 0;
  // 节点点亮标志（HasNode 语义 = 已投入点数 > 0）。
  bool cycleOfAllForms = false;
  bool skyPiercingFall = false;
  bool swordRainEcho = false;
  bool domainLock = false;
  bool arraySynchrony = false;
  bool attunementPolarization = false;
  bool lightningTribunal = false;
  bool frozenDominion = false;
  bool solarIncineration = false;
  // 一次性解析的机制系数（GetMech，fallback = 迁移前字面量）。
  float swordCoreCalibrationPerPoint = 0.10f;
  float worldsplitCorePerPoint = 0.10f;
  float kingslayerEliteImpactPerPoint = 0.08f;
  float kingslayerEliteFieldPerPoint = 0.04f;
  float skyEdgeInfusionPerPointPerTier = 0.04f;
  float cycleOfAllFormsDamageMult = 0.8f;
  float celestialDomainPerPoint = 0.08f;
  float skyPiercingRangeMult = 0.7f;
  float skyPiercingDamageMult = 0.35f;
  float edgeOfferingPerPointPerTier = 0.04f;
  float enduringHeavenPerPoint = 0.5f;
  float residualPressurePerPoint = 0.05f;
  float fieldResonancePerPoint = 0.08f;
  float skyRendScarPerPoint = 0.18f;
  float spinningHeavensPerPoint = 0.15f;
  float returnToSheathPerPoint = 0.08f;
  float tideSpreadPerPoint = 0.10f;
  float elementalRazingPerPoint = 2.0f;
  float elementalRazingCap = 12.0f;
  float swordRainEchoPerTier = 0.10f;
  // 技能级参数回退（skills.json params 缺失时的兜底值）。这些字段的默认值单源自
  // skills.json（contract）；skill_mechanics.json 节点 0 不再重复同名键。
  float impactRadiusFallback = 90.0f;
  float fieldRadiusFallback = 140.0f;
  float fieldDurationFallback = 5.0f;
  float tierDamageBonusFallback = 0.18f;
  float tierRadiusBonusFallback = 14.0f;
};

// 静态绑定表：节点 id -> SpecState 成员指针。以表驱动替代逐字段赋值，
// 行为实现与单测共享同一张表；不含字符串比较或堆分配（设计 §4.4 项 2）。
struct HeavenlySwordPointBinding {
  uint32_t node;
  int HeavenlySwordCastSpec::*points;
};

struct HeavenlySwordFlagBinding {
  uint32_t node;
  bool HeavenlySwordCastSpec::*flag;
};

// 机制系数的表驱动绑定：node/key/member/default 四元组，单次循环填充。
struct HeavenlySwordMechBinding {
  uint32_t node;
  const char *key;
  float HeavenlySwordCastSpec::*coeff;
  float default_value;
};

inline constexpr std::array<HeavenlySwordPointBinding, 16>
    kHeavenlySwordPointBindings{{
        {HeavenlySwordNodes::SwordCoreCalibration,
         &HeavenlySwordCastSpec::swordCoreCalibrationPoints},
        {HeavenlySwordNodes::CelestialDomain,
         &HeavenlySwordCastSpec::celestialDomainPoints},
        {HeavenlySwordNodes::SkyEdgeInfusion,
         &HeavenlySwordCastSpec::skyEdgeInfusionPoints},
        {HeavenlySwordNodes::ResidualPressure,
         &HeavenlySwordCastSpec::residualPressurePoints},
        {HeavenlySwordNodes::WorldsplitCore,
         &HeavenlySwordCastSpec::worldsplitCorePoints},
        {HeavenlySwordNodes::KingslayerIntent,
         &HeavenlySwordCastSpec::kingslayerIntentPoints},
        {HeavenlySwordNodes::MeteorCore,
         &HeavenlySwordCastSpec::meteorCorePoints},
        {HeavenlySwordNodes::SkyRendAftershock,
         &HeavenlySwordCastSpec::skyRendAftershockPoints},
        {HeavenlySwordNodes::EdgeOffering,
         &HeavenlySwordCastSpec::edgeOfferingPoints},
        {HeavenlySwordNodes::OverflowingTiers,
         &HeavenlySwordCastSpec::overflowingTiersPoints},
        {HeavenlySwordNodes::SpinningHeavens,
         &HeavenlySwordCastSpec::spinningHeavensPoints},
        {HeavenlySwordNodes::ReturnToTheSheath,
         &HeavenlySwordCastSpec::returnToTheSheathPoints},
        {HeavenlySwordNodes::FieldResonance,
         &HeavenlySwordCastSpec::fieldResonancePoints},
        {HeavenlySwordNodes::TideSpread,
         &HeavenlySwordCastSpec::tideSpreadPoints},
        {HeavenlySwordNodes::EnduringHeaven,
         &HeavenlySwordCastSpec::enduringHeavenPoints},
        {HeavenlySwordNodes::ElementalRazing,
         &HeavenlySwordCastSpec::elementalRazingPoints},
    }};

inline constexpr std::array<HeavenlySwordFlagBinding, 9>
    kHeavenlySwordFlagBindings{{
        {HeavenlySwordNodes::CycleOfAllForms,
         &HeavenlySwordCastSpec::cycleOfAllForms},
        {HeavenlySwordNodes::SkyPiercingFall,
         &HeavenlySwordCastSpec::skyPiercingFall},
        {HeavenlySwordNodes::SwordRainEcho,
         &HeavenlySwordCastSpec::swordRainEcho},
        {HeavenlySwordNodes::DomainLock, &HeavenlySwordCastSpec::domainLock},
        {HeavenlySwordNodes::ArraySynchrony,
         &HeavenlySwordCastSpec::arraySynchrony},
        {HeavenlySwordNodes::AttunementPolarization,
         &HeavenlySwordCastSpec::attunementPolarization},
        {HeavenlySwordNodes::LightningTribunal,
         &HeavenlySwordCastSpec::lightningTribunal},
        {HeavenlySwordNodes::FrozenDominion,
         &HeavenlySwordCastSpec::frozenDominion},
        {HeavenlySwordNodes::SolarIncineration,
         &HeavenlySwordCastSpec::solarIncineration},
    }};

inline constexpr std::array<HeavenlySwordMechBinding, 20>
    kHeavenlySwordMechBindings{{
        {HeavenlySwordNodes::SwordCoreCalibration,
         "impact_stability_per_point",
         &HeavenlySwordCastSpec::swordCoreCalibrationPerPoint, 0.10f},
        {HeavenlySwordNodes::WorldsplitCore, "center_damage_per_point",
         &HeavenlySwordCastSpec::worldsplitCorePerPoint, 0.10f},
        {HeavenlySwordNodes::KingslayerIntent, "elite_impact_damage_per_point",
         &HeavenlySwordCastSpec::kingslayerEliteImpactPerPoint, 0.08f},
        {HeavenlySwordNodes::KingslayerIntent, "elite_field_damage_per_point",
         &HeavenlySwordCastSpec::kingslayerEliteFieldPerPoint, 0.04f},
        {HeavenlySwordNodes::SkyEdgeInfusion,
         "impact_damage_per_point_per_tier",
         &HeavenlySwordCastSpec::skyEdgeInfusionPerPointPerTier, 0.04f},
        {HeavenlySwordNodes::CycleOfAllForms, "impact_damage_mult",
         &HeavenlySwordCastSpec::cycleOfAllFormsDamageMult, 0.8f},
        {HeavenlySwordNodes::CelestialDomain, "field_radius_range_per_point",
         &HeavenlySwordCastSpec::celestialDomainPerPoint, 0.08f},
        {HeavenlySwordNodes::SkyPiercingFall, "field_radius_mult",
         &HeavenlySwordCastSpec::skyPiercingRangeMult, 0.7f},
        {HeavenlySwordNodes::SkyPiercingFall, "impact_damage_bonus",
         &HeavenlySwordCastSpec::skyPiercingDamageMult, 0.35f},
        {HeavenlySwordNodes::EdgeOffering, "field_damage_per_point_per_tier",
         &HeavenlySwordCastSpec::edgeOfferingPerPointPerTier, 0.04f},
        {HeavenlySwordNodes::EnduringHeaven, "duration_per_point",
         &HeavenlySwordCastSpec::enduringHeavenPerPoint, 0.5f},
        {HeavenlySwordNodes::ResidualPressure,
         "tick_interval_reduction_per_point",
         &HeavenlySwordCastSpec::residualPressurePerPoint, 0.05f},
        {HeavenlySwordNodes::FieldResonance,
         "tick_interval_reduction_per_point",
         &HeavenlySwordCastSpec::fieldResonancePerPoint, 0.08f},
        {HeavenlySwordNodes::SkyRendAftershock, "scar_damage_per_point",
         &HeavenlySwordCastSpec::skyRendScarPerPoint, 0.18f},
        {HeavenlySwordNodes::SpinningHeavens, "cadence_bonus_per_point",
         &HeavenlySwordCastSpec::spinningHeavensPerPoint, 0.15f},
        {HeavenlySwordNodes::ReturnToTheSheath, "empower_bonus_per_point",
         &HeavenlySwordCastSpec::returnToSheathPerPoint, 0.08f},
        {HeavenlySwordNodes::TideSpread, "afflicted_damage_per_point",
         &HeavenlySwordCastSpec::tideSpreadPerPoint, 0.10f},
        {HeavenlySwordNodes::ElementalRazing, "resist_shred_per_point",
         &HeavenlySwordCastSpec::elementalRazingPerPoint, 2.0f},
        {HeavenlySwordNodes::ElementalRazing, "resist_shred_cap",
         &HeavenlySwordCastSpec::elementalRazingCap, 12.0f},
        {HeavenlySwordNodes::SwordRainEcho, "echo_damage_per_tier",
         &HeavenlySwordCastSpec::swordRainEchoPerTier, 0.10f},
    }};

// 解析技能 11 的 SpecState：首个匹配槽位一次性填表（点数 + 点亮标志），
// 再按绑定表一次性读取机制系数；缺失键回退到迁移前字面量，保证行为等价。
[[nodiscard]] inline HeavenlySwordCastSpec
ResolveHeavenlySwordCastSpec(const entt::registry &registry,
                             const entt::entity owner) {
  HeavenlySwordCastSpec spec;
  if (const auto *active = registry.try_get<ActiveSkillsComponent>(owner)) {
    for (const auto &slot : active->specialized_slots) {
      if (slot.skill_id != kHeavenlySwordSkillId) {
        continue;
      }
      for (const auto &binding : kHeavenlySwordPointBindings) {
        spec.*(binding.points) = std::max(0, ReadPoints(slot, binding.node));
      }
      for (const auto &binding : kHeavenlySwordFlagBindings) {
        spec.*(binding.flag) = HasNode(slot, binding.node);
      }
      break;
    }
  }
  for (const auto &binding : kHeavenlySwordMechBindings) {
    spec.*(binding.coeff) = GetMech(kHeavenlySwordSkillId, binding.node,
                                    binding.key, binding.default_value);
  }
  return spec;
}

struct HeavenlySwordDescent : SkillBehaviorBase<HeavenlySwordDescent> {
  static constexpr uint32_t kSkillId = kHeavenlySwordSkillId;

  static void DoCast(entt::registry &registry, entt::entity owner,
                     SkillExecution &exec);
  static void UpdateField(entt::registry &registry, entt::entity entity,
                          HeavenlySwordFieldComponent &field, float dt,
                          const systems::SpatialHashGrid &grid);
  static void HandleLinkedHit(entt::registry &registry, const CombatEvent &evt);
};

void RegisterHeavenlySwordDescent();

} // namespace NoMoreDay::skills
