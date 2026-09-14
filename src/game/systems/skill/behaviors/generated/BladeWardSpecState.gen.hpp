// 本文件由 scripts/gen_skill_contracts.py --gen-specstate 生成，请勿手动编辑。
// 技能 4 效果层 SpecState（POD + 绑定表），由命名 descriptor 驱动。
// 输入：assets/data/skill_specstate/skill_04.json + talent_tree 结构。
#pragma once

#include "game/systems/skill/SpecStateTable.hpp"

#include <array>
#include <cstdint>

namespace NoMoreDay::skills {

// 技能 4 节点常量（talent_tree 全集，节点 id 升序）。
namespace BladeWardNodesGen {
constexpr uint32_t GoldenBell = 400;
constexpr uint32_t Deflection = 401;
constexpr uint32_t Persist = 402;
constexpr uint32_t Repel = 403;
constexpr uint32_t IronGuard = 410;
constexpr uint32_t FiveGuard = 411;
constexpr uint32_t Mountain = 412;
constexpr uint32_t LastStand = 413;
constexpr uint32_t SwordWardBarrier = 414;
constexpr uint32_t Vitality = 415;
constexpr uint32_t IntentBlock = 430;
constexpr uint32_t Unstoppable = 431;
constexpr uint32_t ShieldBarrier = 432;
constexpr uint32_t BloodBarrier = 433;
constexpr uint32_t PerfectParry = 434;
constexpr uint32_t IntentProc = 435;
constexpr uint32_t PhantomStep = 450;
constexpr uint32_t CounterSpeed = 451;
constexpr uint32_t BlinkCounter = 452;
constexpr uint32_t Aftermath = 453;
constexpr uint32_t SwordStep = 454;
constexpr uint32_t AttackDefend = 455;
constexpr uint32_t CounterBlade = 470;
constexpr uint32_t Vengeance = 471;
constexpr uint32_t StaticField = 472;
constexpr uint32_t ThunderCascade = 473;
constexpr uint32_t FrostArmor = 474;
constexpr uint32_t Permafrost = 475;
constexpr uint32_t Exposure = 476;
}  // namespace BladeWardNodesGen

// 技能 4 效果层 SpecState（POD，A-01 D-A1）。
struct BladeWardSpecStateGen {
  using PointBinding = SpecPointBinding<BladeWardSpecStateGen>;
  using FlagBinding = SpecFlagBinding<BladeWardSpecStateGen>;

  int deflectionPoints = 0;
  int persistPoints = 0;
  int repelPoints = 0;
  int ironGuardPoints = 0;
  int fiveGuardPoints = 0;
  int lastStandPoints = 0;
  int vitalityPoints = 0;
  int intentBlockPoints = 0;
  int unstoppablePoints = 0;
  int shieldBarrierPoints = 0;
  int perfectParryPoints = 0;
  int intentProcPoints = 0;
  int counterSpeedPoints = 0;
  int aftermathPoints = 0;
  int swordStepPoints = 0;
  int vengeancePoints = 0;
  int thunderCascadePoints = 0;
  int permafrostPoints = 0;
  int exposurePoints = 0;
  bool mountain = false;
  bool bloodBarrier = false;
  bool attackDefend = false;
  bool counterBlade = false;
  bool staticField = false;
  bool frostArmor = false;
};

inline constexpr std::array<BladeWardSpecStateGen::PointBinding, 19> kBladeWardPointBindingsGen{{
    {BladeWardNodesGen::Deflection, &BladeWardSpecStateGen::deflectionPoints},
    {BladeWardNodesGen::Persist, &BladeWardSpecStateGen::persistPoints},
    {BladeWardNodesGen::Repel, &BladeWardSpecStateGen::repelPoints},
    {BladeWardNodesGen::IronGuard, &BladeWardSpecStateGen::ironGuardPoints},
    {BladeWardNodesGen::FiveGuard, &BladeWardSpecStateGen::fiveGuardPoints},
    {BladeWardNodesGen::LastStand, &BladeWardSpecStateGen::lastStandPoints},
    {BladeWardNodesGen::Vitality, &BladeWardSpecStateGen::vitalityPoints},
    {BladeWardNodesGen::IntentBlock, &BladeWardSpecStateGen::intentBlockPoints},
    {BladeWardNodesGen::Unstoppable, &BladeWardSpecStateGen::unstoppablePoints},
    {BladeWardNodesGen::ShieldBarrier, &BladeWardSpecStateGen::shieldBarrierPoints},
    {BladeWardNodesGen::PerfectParry, &BladeWardSpecStateGen::perfectParryPoints},
    {BladeWardNodesGen::IntentProc, &BladeWardSpecStateGen::intentProcPoints},
    {BladeWardNodesGen::CounterSpeed, &BladeWardSpecStateGen::counterSpeedPoints},
    {BladeWardNodesGen::Aftermath, &BladeWardSpecStateGen::aftermathPoints},
    {BladeWardNodesGen::SwordStep, &BladeWardSpecStateGen::swordStepPoints},
    {BladeWardNodesGen::Vengeance, &BladeWardSpecStateGen::vengeancePoints},
    {BladeWardNodesGen::ThunderCascade, &BladeWardSpecStateGen::thunderCascadePoints},
    {BladeWardNodesGen::Permafrost, &BladeWardSpecStateGen::permafrostPoints},
    {BladeWardNodesGen::Exposure, &BladeWardSpecStateGen::exposurePoints},
}};

inline constexpr std::array<BladeWardSpecStateGen::FlagBinding, 6> kBladeWardFlagBindingsGen{{
    {BladeWardNodesGen::Mountain, &BladeWardSpecStateGen::mountain},
    {BladeWardNodesGen::BloodBarrier, &BladeWardSpecStateGen::bloodBarrier},
    {BladeWardNodesGen::AttackDefend, &BladeWardSpecStateGen::attackDefend},
    {BladeWardNodesGen::CounterBlade, &BladeWardSpecStateGen::counterBlade},
    {BladeWardNodesGen::StaticField, &BladeWardSpecStateGen::staticField},
    {BladeWardNodesGen::FrostArmor, &BladeWardSpecStateGen::frostArmor},
}};

inline constexpr SpecStateTable<BladeWardSpecStateGen> kBladeWardTableGen{
    kBladeWardPointBindingsGen, kBladeWardFlagBindingsGen};

}  // namespace NoMoreDay::skills
