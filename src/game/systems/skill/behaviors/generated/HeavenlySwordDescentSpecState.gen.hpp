// 本文件由 scripts/gen_skill_contracts.py --gen-specstate 生成，请勿手动编辑。
// 技能 11 效果层 SpecState（POD + 绑定表），由命名 descriptor 驱动。
// 输入：assets/data/skill_specstate/skill_11.json + talent_tree 结构。
#pragma once

#include "game/systems/skill/SpecStateTable.hpp"

#include <array>
#include <cstdint>

namespace NoMoreDay::skills {

// 技能 11 节点常量（talent_tree 全集，节点 id 升序）。
namespace HeavenlySwordDescentNodesGen {
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
}  // namespace HeavenlySwordDescentNodesGen

// 技能 11 效果层 SpecState（POD，A-01 D-A1）。
struct HeavenlySwordCastSpecGen {
  using PointBinding = SpecPointBinding<HeavenlySwordCastSpecGen>;
  using FlagBinding = SpecFlagBinding<HeavenlySwordCastSpecGen>;

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
  bool cycleOfAllForms = false;
  bool skyPiercingFall = false;
  bool swordRainEcho = false;
  bool domainLock = false;
  bool arraySynchrony = false;
  bool attunementPolarization = false;
  bool lightningTribunal = false;
  bool frozenDominion = false;
  bool solarIncineration = false;
};

inline constexpr std::array<HeavenlySwordCastSpecGen::PointBinding, 16> kHeavenlySwordDescentPointBindingsGen{{
    {HeavenlySwordDescentNodesGen::SwordCoreCalibration, &HeavenlySwordCastSpecGen::swordCoreCalibrationPoints},
    {HeavenlySwordDescentNodesGen::CelestialDomain, &HeavenlySwordCastSpecGen::celestialDomainPoints},
    {HeavenlySwordDescentNodesGen::SkyEdgeInfusion, &HeavenlySwordCastSpecGen::skyEdgeInfusionPoints},
    {HeavenlySwordDescentNodesGen::ResidualPressure, &HeavenlySwordCastSpecGen::residualPressurePoints},
    {HeavenlySwordDescentNodesGen::WorldsplitCore, &HeavenlySwordCastSpecGen::worldsplitCorePoints},
    {HeavenlySwordDescentNodesGen::KingslayerIntent, &HeavenlySwordCastSpecGen::kingslayerIntentPoints},
    {HeavenlySwordDescentNodesGen::MeteorCore, &HeavenlySwordCastSpecGen::meteorCorePoints},
    {HeavenlySwordDescentNodesGen::SkyRendAftershock, &HeavenlySwordCastSpecGen::skyRendAftershockPoints},
    {HeavenlySwordDescentNodesGen::EdgeOffering, &HeavenlySwordCastSpecGen::edgeOfferingPoints},
    {HeavenlySwordDescentNodesGen::OverflowingTiers, &HeavenlySwordCastSpecGen::overflowingTiersPoints},
    {HeavenlySwordDescentNodesGen::SpinningHeavens, &HeavenlySwordCastSpecGen::spinningHeavensPoints},
    {HeavenlySwordDescentNodesGen::ReturnToTheSheath, &HeavenlySwordCastSpecGen::returnToTheSheathPoints},
    {HeavenlySwordDescentNodesGen::FieldResonance, &HeavenlySwordCastSpecGen::fieldResonancePoints},
    {HeavenlySwordDescentNodesGen::TideSpread, &HeavenlySwordCastSpecGen::tideSpreadPoints},
    {HeavenlySwordDescentNodesGen::EnduringHeaven, &HeavenlySwordCastSpecGen::enduringHeavenPoints},
    {HeavenlySwordDescentNodesGen::ElementalRazing, &HeavenlySwordCastSpecGen::elementalRazingPoints},
}};

inline constexpr std::array<HeavenlySwordCastSpecGen::FlagBinding, 9> kHeavenlySwordDescentFlagBindingsGen{{
    {HeavenlySwordDescentNodesGen::CycleOfAllForms, &HeavenlySwordCastSpecGen::cycleOfAllForms},
    {HeavenlySwordDescentNodesGen::SkyPiercingFall, &HeavenlySwordCastSpecGen::skyPiercingFall},
    {HeavenlySwordDescentNodesGen::SwordRainEcho, &HeavenlySwordCastSpecGen::swordRainEcho},
    {HeavenlySwordDescentNodesGen::DomainLock, &HeavenlySwordCastSpecGen::domainLock},
    {HeavenlySwordDescentNodesGen::ArraySynchrony, &HeavenlySwordCastSpecGen::arraySynchrony},
    {HeavenlySwordDescentNodesGen::AttunementPolarization, &HeavenlySwordCastSpecGen::attunementPolarization},
    {HeavenlySwordDescentNodesGen::LightningTribunal, &HeavenlySwordCastSpecGen::lightningTribunal},
    {HeavenlySwordDescentNodesGen::FrozenDominion, &HeavenlySwordCastSpecGen::frozenDominion},
    {HeavenlySwordDescentNodesGen::SolarIncineration, &HeavenlySwordCastSpecGen::solarIncineration},
}};

inline constexpr SpecStateTable<HeavenlySwordCastSpecGen> kHeavenlySwordDescentTableGen{
    kHeavenlySwordDescentPointBindingsGen, kHeavenlySwordDescentFlagBindingsGen};

}  // namespace NoMoreDay::skills
