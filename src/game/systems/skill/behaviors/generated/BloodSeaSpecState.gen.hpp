// 本文件由 scripts/gen_skill_contracts.py --gen-specstate 生成，请勿手动编辑。
// 技能 12 效果层 SpecState（POD + 绑定表），由命名 descriptor 驱动。
// 输入：assets/data/skill_specstate/skill_12.json + talent_tree 结构。
#pragma once

#include "game/systems/skill/SpecStateTable.hpp"

#include <array>
#include <cstdint>

namespace NoMoreDay::skills {

// 技能 12 节点常量（talent_tree 全集，节点 id 升序）。
namespace BloodSeaNodesGen {
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
}  // namespace BloodSeaNodesGen

// 技能 12 效果层 SpecState（POD，A-01 D-A1）。
struct BloodSeaCastSpecGen {
  using PointBinding = SpecPointBinding<BloodSeaCastSpecGen>;
  using FlagBinding = SpecFlagBinding<BloodSeaCastSpecGen>;

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
  bool bottomlessPurgatory = false;
  bool triggerBurst = false;
  bool recoveryKeystone = false;
  bool sharedDevouring = false;
  bool voidKeystone = false;
  bool torrentForm = false;
  bool ringForm = false;
};

inline constexpr std::array<BloodSeaCastSpecGen::PointBinding, 18> kBloodSeaPointBindingsGen{{
    {BloodSeaNodesGen::BloodCurtainOpening, &BloodSeaCastSpecGen::bloodCurtainOpeningPoints},
    {BloodSeaNodesGen::PressureTideRise, &BloodSeaCastSpecGen::pressureTideRisePoints},
    {BloodSeaNodesGen::BloodthirstEdge, &BloodSeaCastSpecGen::bloodthirstEdgePoints},
    {BloodSeaNodesGen::BloodMistPursuit, &BloodSeaCastSpecGen::bloodMistPursuitPoints},
    {BloodSeaNodesGen::OppressiveEnd, &BloodSeaCastSpecGen::oppressiveEndPoints},
    {BloodSeaNodesGen::HuntingBloodTrail, &BloodSeaCastSpecGen::huntingBloodTrailPoints},
    {BloodSeaNodesGen::DyingEdge, &BloodSeaCastSpecGen::dyingEdgePoints},
    {BloodSeaNodesGen::SeveredVeinAftershock, &BloodSeaCastSpecGen::severedVeinAftershockPoints},
    {BloodSeaNodesGen::BloodDrinkingTide, &BloodSeaCastSpecGen::bloodDrinkingTidePoints},
    {BloodSeaNodesGen::DesperateReclaim, &BloodSeaCastSpecGen::desperateReclaimPoints},
    {BloodSeaNodesGen::BloodWaveRedrink, &BloodSeaCastSpecGen::bloodWaveRedrinkPoints},
    {BloodSeaNodesGen::LifeHuntReturn, &BloodSeaCastSpecGen::lifeHuntReturnPoints},
    {BloodSeaNodesGen::HuntingMiasma, &BloodSeaCastSpecGen::huntingMiasmaPoints},
    {BloodSeaNodesGen::BladeMistResonance, &BloodSeaCastSpecGen::bladeMistResonancePoints},
    {BloodSeaNodesGen::HuntingBloodPressure, &BloodSeaCastSpecGen::huntingBloodPressurePoints},
    {BloodSeaNodesGen::LingeringBloodMist, &BloodSeaCastSpecGen::lingeringBloodMistPoints},
    {BloodSeaNodesGen::BoneGnawingEmber, &BloodSeaCastSpecGen::boneGnawingEmberPoints},
    {BloodSeaNodesGen::MiasmaShred, &BloodSeaCastSpecGen::miasmaShredPoints},
}};

inline constexpr std::array<BloodSeaCastSpecGen::FlagBinding, 7> kBloodSeaFlagBindingsGen{{
    {BloodSeaNodesGen::BottomlessPurgatory, &BloodSeaCastSpecGen::bottomlessPurgatory},
    {BloodSeaNodesGen::FreshBloodReturn, &BloodSeaCastSpecGen::triggerBurst},
    {BloodSeaNodesGen::DrinkTheSeaAndLive, &BloodSeaCastSpecGen::recoveryKeystone},
    {BloodSeaNodesGen::PhantomDevour, &BloodSeaCastSpecGen::sharedDevouring},
    {BloodSeaNodesGen::VoidErosionMiasma, &BloodSeaCastSpecGen::voidKeystone},
    {BloodSeaNodesGen::CrimsonTorrent, &BloodSeaCastSpecGen::torrentForm},
    {BloodSeaNodesGen::BloodRingDevour, &BloodSeaCastSpecGen::ringForm},
}};

inline constexpr SpecStateTable<BloodSeaCastSpecGen> kBloodSeaTableGen{
    kBloodSeaPointBindingsGen, kBloodSeaFlagBindingsGen};

}  // namespace NoMoreDay::skills
