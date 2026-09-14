// 本文件由 scripts/gen_skill_contracts.py --gen-specstate 生成，请勿手动编辑。
// 技能 5 效果层 SpecState（POD + 绑定表），由命名 descriptor 驱动。
// 输入：assets/data/skill_specstate/skill_05.json + talent_tree 结构。
#pragma once

#include "game/systems/skill/SpecStateTable.hpp"

#include <array>
#include <cstdint>

namespace NoMoreDay::skills {

// 技能 5 节点常量（talent_tree 全集，节点 id 升序）。
namespace InfiniteBladesNodesGen {
constexpr uint32_t Rainfall = 500;
constexpr uint32_t Resonance = 501;
constexpr uint32_t MeteoricIron = 502;
constexpr uint32_t Fluidity = 503;
constexpr uint32_t MindLock = 510;
constexpr uint32_t NoEscape = 511;
constexpr uint32_t FateMark = 512;
constexpr uint32_t Execution = 513;
constexpr uint32_t BladeStorm = 514;
constexpr uint32_t BladesToArray = 515;
constexpr uint32_t Composure = 530;
constexpr uint32_t SteeledBody = 531;
constexpr uint32_t AbundantQi = 532;
constexpr uint32_t ColossalBlades = 533;
constexpr uint32_t SwordGod = 534;
constexpr uint32_t Shockwave = 535;
constexpr uint32_t WalkThePath = 550;
constexpr uint32_t SwordStepChannel = 551;
constexpr uint32_t FollowingShadow = 552;
constexpr uint32_t IntentSiphon = 553;
constexpr uint32_t IntentBurst = 554;
constexpr uint32_t Multiplier = 555;
constexpr uint32_t MeteorShower = 570;
constexpr uint32_t DoomsdayAsh = 571;
constexpr uint32_t Blizzard = 572;
constexpr uint32_t AbsoluteZero = 573;
constexpr uint32_t ElementalAttunement = 574;
constexpr uint32_t Catastrophe = 575;
}  // namespace InfiniteBladesNodesGen

// 技能 5 效果层 SpecState（POD，A-01 D-A1）。
struct InfiniteBladesSpecStateGen {
  using PointBinding = SpecPointBinding<InfiniteBladesSpecStateGen>;
  using FlagBinding = SpecFlagBinding<InfiniteBladesSpecStateGen>;

  int meteoricIronPoints = 0;
  int fateMarkPoints = 0;
  int bladeStormPoints = 0;
  int swordStepChannelPoints = 0;
  int intentSiphonPoints = 0;
  int doomsdayAshPoints = 0;
  int absoluteZeroPoints = 0;
  int catastrophePoints = 0;
  bool mindLock = false;
  bool intentBurst = false;
  bool multiplier = false;
  bool blizzard = false;
};

inline constexpr std::array<InfiniteBladesSpecStateGen::PointBinding, 8> kInfiniteBladesPointBindingsGen{{
    {InfiniteBladesNodesGen::MeteoricIron, &InfiniteBladesSpecStateGen::meteoricIronPoints},
    {InfiniteBladesNodesGen::FateMark, &InfiniteBladesSpecStateGen::fateMarkPoints},
    {InfiniteBladesNodesGen::BladeStorm, &InfiniteBladesSpecStateGen::bladeStormPoints},
    {InfiniteBladesNodesGen::SwordStepChannel, &InfiniteBladesSpecStateGen::swordStepChannelPoints},
    {InfiniteBladesNodesGen::IntentSiphon, &InfiniteBladesSpecStateGen::intentSiphonPoints},
    {InfiniteBladesNodesGen::DoomsdayAsh, &InfiniteBladesSpecStateGen::doomsdayAshPoints},
    {InfiniteBladesNodesGen::AbsoluteZero, &InfiniteBladesSpecStateGen::absoluteZeroPoints},
    {InfiniteBladesNodesGen::Catastrophe, &InfiniteBladesSpecStateGen::catastrophePoints},
}};

inline constexpr std::array<InfiniteBladesSpecStateGen::FlagBinding, 4> kInfiniteBladesFlagBindingsGen{{
    {InfiniteBladesNodesGen::MindLock, &InfiniteBladesSpecStateGen::mindLock},
    {InfiniteBladesNodesGen::IntentBurst, &InfiniteBladesSpecStateGen::intentBurst},
    {InfiniteBladesNodesGen::Multiplier, &InfiniteBladesSpecStateGen::multiplier},
    {InfiniteBladesNodesGen::Blizzard, &InfiniteBladesSpecStateGen::blizzard},
}};

inline constexpr SpecStateTable<InfiniteBladesSpecStateGen> kInfiniteBladesTableGen{
    kInfiniteBladesPointBindingsGen, kInfiniteBladesFlagBindingsGen};

}  // namespace NoMoreDay::skills
