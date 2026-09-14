// 本文件由 scripts/gen_skill_contracts.py --gen-specstate 生成，请勿手动编辑。
// 技能 3 效果层 SpecState（POD + 绑定表），由命名 descriptor 驱动。
// 输入：assets/data/skill_specstate/skill_03.json + talent_tree 结构。
#pragma once

#include "game/systems/skill/SpecStateTable.hpp"

#include <array>
#include <cstdint>

namespace NoMoreDay::skills {

// 技能 3 节点常量（talent_tree 全集，节点 id 升序）。
namespace BladeFormationNodesGen {
constexpr uint32_t SwordPool = 300;
constexpr uint32_t SwiftIntent = 301;
constexpr uint32_t EdgedSpirit = 302;
constexpr uint32_t ElementalCore = 303;
constexpr uint32_t SearchRadius = 310;
constexpr uint32_t InfiniteSheath = 311;
constexpr uint32_t Network = 312;
constexpr uint32_t Godspeed = 313;
constexpr uint32_t Concentrate = 314;
constexpr uint32_t SwordStepResonance = 315;
constexpr uint32_t GiantSword = 330;
constexpr uint32_t WeakPointCrit = 331;
constexpr uint32_t DeadlyEdge = 332;
constexpr uint32_t SwordPressure = 333;
constexpr uint32_t Crush = 334;
constexpr uint32_t GiantSwordRift = 335;
constexpr uint32_t Ward = 350;
constexpr uint32_t BladeOrbit = 351;
constexpr uint32_t RetaliationWeb = 352;
constexpr uint32_t Immortality = 353;
constexpr uint32_t SpellEcho = 354;
constexpr uint32_t ArrayResonance = 355;
constexpr uint32_t ElementFire = 370;
constexpr uint32_t BlazingDance = 371;
constexpr uint32_t ElementLightning = 372;
constexpr uint32_t ArcChain = 373;
constexpr uint32_t SpiritCorrosion = 374;
constexpr uint32_t Charge = 375;
}  // namespace BladeFormationNodesGen

// 技能 3 效果层 SpecState（POD，A-01 D-A1）。
struct BladeFormationSpecStateGen {
  using PointBinding = SpecPointBinding<BladeFormationSpecStateGen>;
  using FlagBinding = SpecFlagBinding<BladeFormationSpecStateGen>;

  int swordPoolPoints = 0;
  int swiftIntentPoints = 0;
  int edgedSpiritPoints = 0;
  int elementalCorePoints = 0;
  int searchRadiusPoints = 0;
  int networkPoints = 0;
  int swordStepResonancePoints = 0;
  int weakPointCritPoints = 0;
  int deadlyEdgePoints = 0;
  int swordPressurePoints = 0;
  int crushPoints = 0;
  int wardPoints = 0;
  int retaliationWebPoints = 0;
  int blazingDancePoints = 0;
  int arcChainPoints = 0;
  int spiritCorrosionPoints = 0;
  int chargePoints = 0;
  bool infiniteSheath = false;
  bool godspeed = false;
  bool concentrate = false;
  bool giantSword = false;
  bool bladeOrbit = false;
  bool immortality = false;
  bool spellEcho = false;
  bool arrayResonance = false;
  bool elementFire = false;
  bool elementLightning = false;
};

inline constexpr std::array<BladeFormationSpecStateGen::PointBinding, 17> kBladeFormationPointBindingsGen{{
    {BladeFormationNodesGen::SwordPool, &BladeFormationSpecStateGen::swordPoolPoints},
    {BladeFormationNodesGen::SwiftIntent, &BladeFormationSpecStateGen::swiftIntentPoints},
    {BladeFormationNodesGen::EdgedSpirit, &BladeFormationSpecStateGen::edgedSpiritPoints},
    {BladeFormationNodesGen::ElementalCore, &BladeFormationSpecStateGen::elementalCorePoints},
    {BladeFormationNodesGen::SearchRadius, &BladeFormationSpecStateGen::searchRadiusPoints},
    {BladeFormationNodesGen::Network, &BladeFormationSpecStateGen::networkPoints},
    {BladeFormationNodesGen::SwordStepResonance, &BladeFormationSpecStateGen::swordStepResonancePoints},
    {BladeFormationNodesGen::WeakPointCrit, &BladeFormationSpecStateGen::weakPointCritPoints},
    {BladeFormationNodesGen::DeadlyEdge, &BladeFormationSpecStateGen::deadlyEdgePoints},
    {BladeFormationNodesGen::SwordPressure, &BladeFormationSpecStateGen::swordPressurePoints},
    {BladeFormationNodesGen::Crush, &BladeFormationSpecStateGen::crushPoints},
    {BladeFormationNodesGen::Ward, &BladeFormationSpecStateGen::wardPoints},
    {BladeFormationNodesGen::RetaliationWeb, &BladeFormationSpecStateGen::retaliationWebPoints},
    {BladeFormationNodesGen::BlazingDance, &BladeFormationSpecStateGen::blazingDancePoints},
    {BladeFormationNodesGen::ArcChain, &BladeFormationSpecStateGen::arcChainPoints},
    {BladeFormationNodesGen::SpiritCorrosion, &BladeFormationSpecStateGen::spiritCorrosionPoints},
    {BladeFormationNodesGen::Charge, &BladeFormationSpecStateGen::chargePoints},
}};

inline constexpr std::array<BladeFormationSpecStateGen::FlagBinding, 10> kBladeFormationFlagBindingsGen{{
    {BladeFormationNodesGen::InfiniteSheath, &BladeFormationSpecStateGen::infiniteSheath},
    {BladeFormationNodesGen::Godspeed, &BladeFormationSpecStateGen::godspeed},
    {BladeFormationNodesGen::Concentrate, &BladeFormationSpecStateGen::concentrate},
    {BladeFormationNodesGen::GiantSword, &BladeFormationSpecStateGen::giantSword},
    {BladeFormationNodesGen::BladeOrbit, &BladeFormationSpecStateGen::bladeOrbit},
    {BladeFormationNodesGen::Immortality, &BladeFormationSpecStateGen::immortality},
    {BladeFormationNodesGen::SpellEcho, &BladeFormationSpecStateGen::spellEcho},
    {BladeFormationNodesGen::ArrayResonance, &BladeFormationSpecStateGen::arrayResonance},
    {BladeFormationNodesGen::ElementFire, &BladeFormationSpecStateGen::elementFire},
    {BladeFormationNodesGen::ElementLightning, &BladeFormationSpecStateGen::elementLightning},
}};

inline constexpr SpecStateTable<BladeFormationSpecStateGen> kBladeFormationTableGen{
    kBladeFormationPointBindingsGen, kBladeFormationFlagBindingsGen};

}  // namespace NoMoreDay::skills
