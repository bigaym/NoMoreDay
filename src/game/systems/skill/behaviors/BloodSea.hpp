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

// 技能 12（血海）节点 id：行为实现与单测共享的唯一定义，禁止在其他 TU 重复声明。
namespace BloodSeaNodes {
constexpr uint32_t BloodCurtainOpening = 1200;
constexpr uint32_t PressureTideRise = 1201;
constexpr uint32_t BloodthirstEdge = 1202;
constexpr uint32_t BloodMistPursuit = 1203;
constexpr uint32_t OppressiveEnd = 1204;
constexpr uint32_t HuntingBloodTrail = 1205;
constexpr uint32_t DyingEdge = 1206;
constexpr uint32_t BottomlessPurgatory = 1207;
constexpr uint32_t SeveredVeinAftershock = 1208;
constexpr uint32_t BloodDrinkingTide = 1209;
constexpr uint32_t DesperateReclaim = 1210;
constexpr uint32_t FreshBloodReturn = 1211;
constexpr uint32_t BloodWaveRedrink = 1212;
constexpr uint32_t DrinkTheSeaAndLive = 1213;
constexpr uint32_t LifeHuntReturn = 1214;
constexpr uint32_t HuntingMiasma = 1215;
constexpr uint32_t BladeMistResonance = 1216;
constexpr uint32_t PhantomDevour = 1217;
constexpr uint32_t HuntingBloodPressure = 1218;
constexpr uint32_t LingeringBloodMist = 1219;
constexpr uint32_t VoidErosionMiasma = 1220;
constexpr uint32_t CrimsonTorrent = 1221;
constexpr uint32_t BloodRingDevour = 1222;
constexpr uint32_t BoneGnawingEmber = 1223;
constexpr uint32_t MiasmaShred = 1224;
} // namespace BloodSeaNodes

inline constexpr uint32_t kBloodSeaSkillId = 12;

// 血海施法信封（POD）：DoCast 内一次性解析，其余运行期路径只读已写入
// BloodSeaFieldComponent 的运行时值，不再重复读点。
struct BloodSeaCastSpec {
  // 各节点已投入点数（ReadPoints，已 clamp 到 >= 0）。
  int bloodCurtainOpeningPoints = 0;
  int pressureTideRisePoints = 0;
  int bloodthirstEdgePoints = 0;
  int bloodMistPursuitPoints = 0;
  int oppressiveEndPoints = 0;
  int huntingBloodTrailPoints = 0;
  int dyingEdgePoints = 0;
  int severedVeinAftershockPoints = 0;
  int bloodDrinkingTidePoints = 0;
  int desperateReclaimPoints = 0;
  int bloodWaveRedrinkPoints = 0;
  int lifeHuntReturnPoints = 0;
  int huntingMiasmaPoints = 0;
  int bladeMistResonancePoints = 0;
  int huntingBloodPressurePoints = 0;
  int lingeringBloodMistPoints = 0;
  int boneGnawingEmberPoints = 0;
  int miasmaShredPoints = 0;
  // 节点点亮标志（HasNode 语义 = 已投入点数 > 0）。
  bool bottomlessPurgatory = false;
  bool triggerBurst = false;
  bool recoveryKeystone = false;
  // 1217 绝影共噬：点亮后由 DoCast 结合技能9 逆脉/免死窗口决定是否开启前 N 秒增伤/增疗。
  bool sharedDevouring = false;
  bool voidKeystone = false;
  bool torrentForm = false;
  bool ringForm = false;
  // 一次性解析的机制系数（GetMech，default_value = 迁移前字面量）。
  float lowLifeThreshold = 0.35f;
  float fieldDurationPerBloodthirst = 0.2f;
  float fieldRadiusPerBloodthirst = 6.0f;
  // 技能级参数回退（skills.json params 缺失时的兜底值）。默认值单源自 skills.json
  //（contract）；skill_mechanics.json 节点 0 不再重复同名键。
  float fieldDurationDefault = 4.8f;
  float fieldRadiusDefault = 120.0f;
  float fieldTickDefault = 0.25f;
  float bloodthirstDamageBonusDefault = 0.12f;
  float leechRatioDefault = 0.12f;
  float radiusPerPoint = 8.0f;
  float damagePerPoint = 0.06f;
  float damagePerPointPerBloodthirst = 0.025f;
  float moveSpeedPerPoint = 1.0f;
  float lowLifeDamagePerPoint = 0.08f;
  float pursuitDamagePerPoint = 0.08f;
  float lowLifePressureDamagePerPoint = 0.18f;
  float bottomlessDamageMult = 1.1f;
  float aftershockDamagePerPoint = 0.05f;
  float leechPerPointPerBloodthirst = 0.01f;
  float lowLifeLeechPerPoint = 0.025f;
  float burstBaseDamage = 16.0f;
  float burstDamagePerBloodthirst = 6.0f;
  float missingHealthHealRatio = 0.1f;
  float burstBloodthirstGain = 2.0f;
  float leechPerPoint = 0.02f;
  float recoveryLeechBonus = 0.08f;
  float empowerBonusPerPoint = 0.08f;
  // 1217 绝影共噬窗口系数（窗口时长 / 伤害增益 / 治疗增益），default_value = 设计值。
  float empowerDuration = 2.0f;
  float empowerDamageMult = 0.20f;
  float empowerHealMult = 0.20f;
  float closePressurePerPoint = 0.07f;
  float tickIntervalReductionPerPoint = 0.06f;
  float tickIntervalFloor = 0.7f;
  float linkedPressurePerPoint = 0.08f;
  float durationPerPoint = 0.6f;
  float voidDamageMult = 1.18f;
  float voidResistShredBonus = 4.0f;
  float torrentMoveSpeed = 14.0f;
  float torrentRadiusMult = 1.15f;
  float torrentTickIntervalMult = 0.85f;
  float ringRadiusMult = 0.8f;
  float ringLeechBonus = 0.1f;
  float ringDamageMult = 1.15f;
  float miasmaDurationPerPoint = 0.25f;
  float voidDamagePerPoint = 0.06f;
  float resistShredPerPoint = 2.0f;
};

// 静态绑定表：节点 id -> SpecState 成员指针。以表驱动替代逐字段赋值，
// 行为实现与单测共享同一张表；不含字符串比较或堆分配。
struct BloodSeaPointBinding {
  uint32_t node;
  int BloodSeaCastSpec::*points;
};

struct BloodSeaFlagBinding {
  uint32_t node;
  bool BloodSeaCastSpec::*flag;
};

// 机制系数的表驱动绑定：node/key/member/default 四元组，单次循环填充。
struct BloodSeaMechBinding {
  uint32_t node;
  const char *key;
  float BloodSeaCastSpec::*coeff;
  float default_value;
};

inline constexpr std::array<BloodSeaPointBinding, 18> kBloodSeaPointBindings{{
    {BloodSeaNodes::BloodCurtainOpening,
     &BloodSeaCastSpec::bloodCurtainOpeningPoints},
    {BloodSeaNodes::PressureTideRise, &BloodSeaCastSpec::pressureTideRisePoints},
    {BloodSeaNodes::BloodthirstEdge, &BloodSeaCastSpec::bloodthirstEdgePoints},
    {BloodSeaNodes::BloodMistPursuit, &BloodSeaCastSpec::bloodMistPursuitPoints},
    {BloodSeaNodes::OppressiveEnd, &BloodSeaCastSpec::oppressiveEndPoints},
    {BloodSeaNodes::HuntingBloodTrail,
     &BloodSeaCastSpec::huntingBloodTrailPoints},
    {BloodSeaNodes::DyingEdge, &BloodSeaCastSpec::dyingEdgePoints},
    {BloodSeaNodes::SeveredVeinAftershock,
     &BloodSeaCastSpec::severedVeinAftershockPoints},
    {BloodSeaNodes::BloodDrinkingTide,
     &BloodSeaCastSpec::bloodDrinkingTidePoints},
    {BloodSeaNodes::DesperateReclaim, &BloodSeaCastSpec::desperateReclaimPoints},
    {BloodSeaNodes::BloodWaveRedrink, &BloodSeaCastSpec::bloodWaveRedrinkPoints},
    {BloodSeaNodes::LifeHuntReturn, &BloodSeaCastSpec::lifeHuntReturnPoints},
    {BloodSeaNodes::HuntingMiasma, &BloodSeaCastSpec::huntingMiasmaPoints},
    {BloodSeaNodes::BladeMistResonance,
     &BloodSeaCastSpec::bladeMistResonancePoints},
    {BloodSeaNodes::HuntingBloodPressure,
     &BloodSeaCastSpec::huntingBloodPressurePoints},
    {BloodSeaNodes::LingeringBloodMist,
     &BloodSeaCastSpec::lingeringBloodMistPoints},
    {BloodSeaNodes::BoneGnawingEmber,
     &BloodSeaCastSpec::boneGnawingEmberPoints},
    {BloodSeaNodes::MiasmaShred, &BloodSeaCastSpec::miasmaShredPoints},
}};

inline constexpr std::array<BloodSeaFlagBinding, 7> kBloodSeaFlagBindings{{
    {BloodSeaNodes::BottomlessPurgatory, &BloodSeaCastSpec::bottomlessPurgatory},
    {BloodSeaNodes::FreshBloodReturn, &BloodSeaCastSpec::triggerBurst},
    {BloodSeaNodes::DrinkTheSeaAndLive, &BloodSeaCastSpec::recoveryKeystone},
    {BloodSeaNodes::PhantomDevour, &BloodSeaCastSpec::sharedDevouring},
    {BloodSeaNodes::VoidErosionMiasma, &BloodSeaCastSpec::voidKeystone},
    {BloodSeaNodes::CrimsonTorrent, &BloodSeaCastSpec::torrentForm},
    {BloodSeaNodes::BloodRingDevour, &BloodSeaCastSpec::ringForm},
}};

inline constexpr std::array<BloodSeaMechBinding, 40> kBloodSeaMechBindings{{
    {0u, "low_life_threshold", &BloodSeaCastSpec::lowLifeThreshold, 0.35f},
    {0u, "field_duration_per_bloodthirst",
     &BloodSeaCastSpec::fieldDurationPerBloodthirst, 0.2f},
    {0u, "field_radius_per_bloodthirst",
     &BloodSeaCastSpec::fieldRadiusPerBloodthirst, 6.0f},
    {BloodSeaNodes::BloodCurtainOpening, "radius_per_point",
     &BloodSeaCastSpec::radiusPerPoint, 8.0f},
    {BloodSeaNodes::PressureTideRise, "damage_per_point",
     &BloodSeaCastSpec::damagePerPoint, 0.06f},
    {BloodSeaNodes::BloodthirstEdge, "damage_per_point_per_bloodthirst",
     &BloodSeaCastSpec::damagePerPointPerBloodthirst, 0.025f},
    {BloodSeaNodes::BloodMistPursuit, "move_speed_per_point",
     &BloodSeaCastSpec::moveSpeedPerPoint, 1.0f},
    {BloodSeaNodes::OppressiveEnd, "low_life_damage_per_point",
     &BloodSeaCastSpec::lowLifeDamagePerPoint, 0.08f},
    {BloodSeaNodes::HuntingBloodTrail, "pursuit_damage_per_point",
     &BloodSeaCastSpec::pursuitDamagePerPoint, 0.08f},
    {BloodSeaNodes::DyingEdge, "low_life_pressure_damage_per_point",
     &BloodSeaCastSpec::lowLifePressureDamagePerPoint, 0.18f},
    {BloodSeaNodes::BottomlessPurgatory, "damage_mult",
     &BloodSeaCastSpec::bottomlessDamageMult, 1.1f},
    {BloodSeaNodes::SeveredVeinAftershock, "aftershock_damage_per_point",
     &BloodSeaCastSpec::aftershockDamagePerPoint, 0.05f},
    {BloodSeaNodes::BloodDrinkingTide, "leech_per_point_per_bloodthirst",
     &BloodSeaCastSpec::leechPerPointPerBloodthirst, 0.01f},
    {BloodSeaNodes::DesperateReclaim, "low_life_leech_per_point",
     &BloodSeaCastSpec::lowLifeLeechPerPoint, 0.025f},
    {BloodSeaNodes::FreshBloodReturn, "burst_base_damage",
     &BloodSeaCastSpec::burstBaseDamage, 16.0f},
    {BloodSeaNodes::FreshBloodReturn, "burst_damage_per_bloodthirst",
     &BloodSeaCastSpec::burstDamagePerBloodthirst, 6.0f},
    {BloodSeaNodes::FreshBloodReturn, "missing_health_heal_ratio",
     &BloodSeaCastSpec::missingHealthHealRatio, 0.1f},
    {BloodSeaNodes::FreshBloodReturn, "bloodthirst_gain",
     &BloodSeaCastSpec::burstBloodthirstGain, 2.0f},
    {BloodSeaNodes::BloodWaveRedrink, "leech_per_point",
     &BloodSeaCastSpec::leechPerPoint, 0.02f},
    {BloodSeaNodes::DrinkTheSeaAndLive, "leech_bonus",
     &BloodSeaCastSpec::recoveryLeechBonus, 0.08f},
    {BloodSeaNodes::LifeHuntReturn, "empower_bonus_per_point",
     &BloodSeaCastSpec::empowerBonusPerPoint, 0.08f},
    {BloodSeaNodes::HuntingMiasma, "close_pressure_per_point",
     &BloodSeaCastSpec::closePressurePerPoint, 0.07f},
    {BloodSeaNodes::BladeMistResonance, "tick_interval_reduction_per_point",
     &BloodSeaCastSpec::tickIntervalReductionPerPoint, 0.06f},
    {BloodSeaNodes::BladeMistResonance, "tick_interval_floor",
     &BloodSeaCastSpec::tickIntervalFloor, 0.7f},
    {BloodSeaNodes::HuntingBloodPressure, "linked_pressure_per_point",
     &BloodSeaCastSpec::linkedPressurePerPoint, 0.08f},
    {BloodSeaNodes::LingeringBloodMist, "duration_per_point",
     &BloodSeaCastSpec::durationPerPoint, 0.6f},
    {BloodSeaNodes::VoidErosionMiasma, "damage_mult",
     &BloodSeaCastSpec::voidDamageMult, 1.18f},
    {BloodSeaNodes::VoidErosionMiasma, "resist_shred_bonus",
     &BloodSeaCastSpec::voidResistShredBonus, 4.0f},
    {BloodSeaNodes::CrimsonTorrent, "move_speed",
     &BloodSeaCastSpec::torrentMoveSpeed, 14.0f},
    {BloodSeaNodes::CrimsonTorrent, "radius_mult",
     &BloodSeaCastSpec::torrentRadiusMult, 1.15f},
    {BloodSeaNodes::CrimsonTorrent, "tick_interval_mult",
     &BloodSeaCastSpec::torrentTickIntervalMult, 0.85f},
    {BloodSeaNodes::BloodRingDevour, "radius_mult",
     &BloodSeaCastSpec::ringRadiusMult, 0.8f},
    {BloodSeaNodes::BloodRingDevour, "leech_bonus",
     &BloodSeaCastSpec::ringLeechBonus, 0.1f},
    {BloodSeaNodes::BloodRingDevour, "damage_mult",
     &BloodSeaCastSpec::ringDamageMult, 1.15f},
    {BloodSeaNodes::BoneGnawingEmber, "miasma_duration_per_point",
     &BloodSeaCastSpec::miasmaDurationPerPoint, 0.25f},
    {BloodSeaNodes::BoneGnawingEmber, "void_damage_per_point",
     &BloodSeaCastSpec::voidDamagePerPoint, 0.06f},
    {BloodSeaNodes::MiasmaShred, "resist_shred_per_point",
     &BloodSeaCastSpec::resistShredPerPoint, 2.0f},
    {BloodSeaNodes::PhantomDevour, "empower_duration",
     &BloodSeaCastSpec::empowerDuration, 2.0f},
    {BloodSeaNodes::PhantomDevour, "empower_damage_mult",
     &BloodSeaCastSpec::empowerDamageMult, 0.20f},
    {BloodSeaNodes::PhantomDevour, "empower_heal_mult",
     &BloodSeaCastSpec::empowerHealMult, 0.20f},
}};

// 解析技能 12 的 SpecState：首个匹配槽位一次性填表（点数 + 点亮标志），
// 再按绑定表一次性读取机制系数；缺失键回退到迁移前字面量，保证行为等价。
[[nodiscard]] inline BloodSeaCastSpec
ResolveBloodSeaCastSpec(const entt::registry &registry, const entt::entity owner) {
  BloodSeaCastSpec spec;
  if (const auto *active = registry.try_get<ActiveSkillsComponent>(owner)) {
    for (const auto &slot : active->specialized_slots) {
      if (slot.skill_id != kBloodSeaSkillId) {
        continue;
      }
      for (const auto &binding : kBloodSeaPointBindings) {
        spec.*(binding.points) = std::max(0, ReadPoints(slot, binding.node));
      }
      for (const auto &binding : kBloodSeaFlagBindings) {
        spec.*(binding.flag) = HasNode(slot, binding.node);
      }
      break;
    }
  }
  for (const auto &binding : kBloodSeaMechBindings) {
    spec.*(binding.coeff) = GetMech(kBloodSeaSkillId, binding.node, binding.key,
                                    binding.default_value);
  }
  return spec;
}

struct BloodSea : SkillBehaviorBase<BloodSea> {
  static constexpr uint32_t kSkillId = kBloodSeaSkillId;

  static void DoCast(entt::registry &registry, entt::entity owner,
                     SkillExecution &exec);
  static void UpdateField(entt::registry &registry, entt::entity entity,
                          BloodSeaFieldComponent &field, float dt,
                          const systems::SpatialHashGrid &grid);
  static void HandleLinkedHit(entt::registry &registry, const CombatEvent &evt);
};

void RegisterBloodSea();

} // namespace NoMoreDay::skills
