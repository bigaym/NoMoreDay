// 本文件由 scripts/gen_skill_contracts.py --gen-specstate 生成，请勿手动编辑。
// 技能 2 效果层 SpecState（POD + 绑定表），由命名 descriptor 驱动。
// 输入：assets/data/skill_specstate/skill_02.json + talent_tree 结构。
#pragma once

#include "game/systems/skill/SpecStateTable.hpp"

#include <array>
#include <cstdint>

namespace NoMoreDay::skills {

// 技能 2 节点常量（talent_tree 全集，节点 id 升序）。
namespace RendingWaveNodesGen {
constexpr uint32_t WaveExpansion = 200;
constexpr uint32_t Focus = 201;
constexpr uint32_t Edge = 202;
constexpr uint32_t QiBurst = 203;
constexpr uint32_t MultiWave = 210;
constexpr uint32_t Fracture = 211;
constexpr uint32_t ChainReaction = 212;
constexpr uint32_t Scatter = 213;
constexpr uint32_t Orbit = 214;
constexpr uint32_t SpiritPursuit = 215;
constexpr uint32_t Boomerang = 230;
constexpr uint32_t DoubleHit = 231;
constexpr uint32_t GravityWell = 232;
constexpr uint32_t AbyssEdge = 233;
constexpr uint32_t TimeLock = 234;
constexpr uint32_t SwordStepGravity = 235;
constexpr uint32_t QiBrand = 250;
constexpr uint32_t IntentBurst = 251;
constexpr uint32_t Bottomless = 252;
constexpr uint32_t ObliterationWave = 253;
constexpr uint32_t EchoedSlash = 254;
constexpr uint32_t IntentRecovery = 255;
constexpr uint32_t FrostForm = 270;
constexpr uint32_t ShatterCascade = 271;
constexpr uint32_t LightningForm = 272;
constexpr uint32_t StaticConduction = 273;
constexpr uint32_t ElementalAffinity = 274;
constexpr uint32_t Proliferation = 275;
}  // namespace RendingWaveNodesGen

// 技能 2 效果层 SpecState（POD，A-01 D-A1）。
struct RendingWaveSpecStateGen {
  using PointBinding = SpecPointBinding<RendingWaveSpecStateGen>;
  using FlagBinding = SpecFlagBinding<RendingWaveSpecStateGen>;

  int qiBurstPoints = 0;
  int multiWavePoints = 0;
  int chainReactionPoints = 0;
  int doubleHitPoints = 0;
  int abyssEdgePoints = 0;
  int swordStepGravityPoints = 0;
  int qiBrandPoints = 0;
  int bottomlessPoints = 0;
  int intentRecoveryPoints = 0;
  int shatterCascadePoints = 0;
  int staticConductionPoints = 0;
  int proliferationPoints = 0;
  bool fracture = false;
  bool scatter = false;
  bool orbit = false;
  bool spiritPursuit = false;
  bool boomerang = false;
  bool gravityWell = false;
  bool timeLock = false;
  bool intentBurst = false;
  bool obliterationWave = false;
  bool echoedSlash = false;
  bool frostForm = false;
  bool lightningForm = false;
};

inline constexpr std::array<RendingWaveSpecStateGen::PointBinding, 12> kRendingWavePointBindingsGen{{
    {RendingWaveNodesGen::QiBurst, &RendingWaveSpecStateGen::qiBurstPoints},
    {RendingWaveNodesGen::MultiWave, &RendingWaveSpecStateGen::multiWavePoints},
    {RendingWaveNodesGen::ChainReaction, &RendingWaveSpecStateGen::chainReactionPoints},
    {RendingWaveNodesGen::DoubleHit, &RendingWaveSpecStateGen::doubleHitPoints},
    {RendingWaveNodesGen::AbyssEdge, &RendingWaveSpecStateGen::abyssEdgePoints},
    {RendingWaveNodesGen::SwordStepGravity, &RendingWaveSpecStateGen::swordStepGravityPoints},
    {RendingWaveNodesGen::QiBrand, &RendingWaveSpecStateGen::qiBrandPoints},
    {RendingWaveNodesGen::Bottomless, &RendingWaveSpecStateGen::bottomlessPoints},
    {RendingWaveNodesGen::IntentRecovery, &RendingWaveSpecStateGen::intentRecoveryPoints},
    {RendingWaveNodesGen::ShatterCascade, &RendingWaveSpecStateGen::shatterCascadePoints},
    {RendingWaveNodesGen::StaticConduction, &RendingWaveSpecStateGen::staticConductionPoints},
    {RendingWaveNodesGen::Proliferation, &RendingWaveSpecStateGen::proliferationPoints},
}};

inline constexpr std::array<RendingWaveSpecStateGen::FlagBinding, 12> kRendingWaveFlagBindingsGen{{
    {RendingWaveNodesGen::Fracture, &RendingWaveSpecStateGen::fracture},
    {RendingWaveNodesGen::Scatter, &RendingWaveSpecStateGen::scatter},
    {RendingWaveNodesGen::Orbit, &RendingWaveSpecStateGen::orbit},
    {RendingWaveNodesGen::SpiritPursuit, &RendingWaveSpecStateGen::spiritPursuit},
    {RendingWaveNodesGen::Boomerang, &RendingWaveSpecStateGen::boomerang},
    {RendingWaveNodesGen::GravityWell, &RendingWaveSpecStateGen::gravityWell},
    {RendingWaveNodesGen::TimeLock, &RendingWaveSpecStateGen::timeLock},
    {RendingWaveNodesGen::IntentBurst, &RendingWaveSpecStateGen::intentBurst},
    {RendingWaveNodesGen::ObliterationWave, &RendingWaveSpecStateGen::obliterationWave},
    {RendingWaveNodesGen::EchoedSlash, &RendingWaveSpecStateGen::echoedSlash},
    {RendingWaveNodesGen::FrostForm, &RendingWaveSpecStateGen::frostForm},
    {RendingWaveNodesGen::LightningForm, &RendingWaveSpecStateGen::lightningForm},
}};

inline constexpr SpecStateTable<RendingWaveSpecStateGen> kRendingWaveTableGen{
    kRendingWavePointBindingsGen, kRendingWaveFlagBindingsGen};

}  // namespace NoMoreDay::skills
