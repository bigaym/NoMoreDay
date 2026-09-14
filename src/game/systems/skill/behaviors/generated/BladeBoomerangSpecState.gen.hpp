// 本文件由 scripts/gen_skill_contracts.py --gen-specstate 生成，请勿手动编辑。
// 技能 8 效果层 SpecState（POD + 绑定表），由命名 descriptor 驱动。
// 输入：assets/data/skill_specstate/skill_08.json + talent_tree 结构。
#pragma once

#include "game/systems/skill/SpecStateTable.hpp"

#include <array>
#include <cstdint>

namespace NoMoreDay::skills {

// 技能 8 节点常量（talent_tree 全集，节点 id 升序）。
namespace BladeBoomerangNodesGen {
constexpr uint32_t Lightweight = 800;
constexpr uint32_t VelocityNode = 801;
constexpr uint32_t Sharpness = 802;
constexpr uint32_t Feedback = 803;
constexpr uint32_t Hovering = 810;
constexpr uint32_t Bleed = 811;
constexpr uint32_t CritMulti = 812;
constexpr uint32_t SonicBoom = 813;
constexpr uint32_t BloodRip = 814;
constexpr uint32_t EyeOfStorm = 815;
constexpr uint32_t TriBlade = 830;
constexpr uint32_t BladeDance = 831;
constexpr uint32_t Catch = 832;
constexpr uint32_t Combo = 833;
constexpr uint32_t SwordStepCatch = 834;
constexpr uint32_t ReturnDance = 835;
constexpr uint32_t Magnet = 850;
constexpr uint32_t Area = 851;
constexpr uint32_t IntentGain = 852;
constexpr uint32_t MindIntent = 853;
constexpr uint32_t Giant = 854;
constexpr uint32_t ColossusEcho = 855;
constexpr uint32_t AshPath = 870;
constexpr uint32_t Wildfire = 871;
constexpr uint32_t Electro = 872;
constexpr uint32_t HighVoltage = 873;
constexpr uint32_t ElementalWake = 874;
constexpr uint32_t PathPen = 875;
constexpr uint32_t ElementShield = 876;
}  // namespace BladeBoomerangNodesGen

// 技能 8 效果层 SpecState（POD，A-01 D-A1）。
struct BladeBoomerangSpecStateGen {
  using PointBinding = SpecPointBinding<BladeBoomerangSpecStateGen>;
  using FlagBinding = SpecFlagBinding<BladeBoomerangSpecStateGen>;

  int sharpnessPoints = 0;
  int bleedPoints = 0;
  int sonicBoomPoints = 0;
  int intentGainPoints = 0;
  int giantPoints = 0;
};

inline constexpr std::array<BladeBoomerangSpecStateGen::PointBinding, 5> kBladeBoomerangPointBindingsGen{{
    {BladeBoomerangNodesGen::Sharpness, &BladeBoomerangSpecStateGen::sharpnessPoints},
    {BladeBoomerangNodesGen::Bleed, &BladeBoomerangSpecStateGen::bleedPoints},
    {BladeBoomerangNodesGen::SonicBoom, &BladeBoomerangSpecStateGen::sonicBoomPoints},
    {BladeBoomerangNodesGen::IntentGain, &BladeBoomerangSpecStateGen::intentGainPoints},
    {BladeBoomerangNodesGen::Giant, &BladeBoomerangSpecStateGen::giantPoints},
}};

inline constexpr std::array<BladeBoomerangSpecStateGen::FlagBinding, 0> kBladeBoomerangFlagBindingsGen{{
}};

inline constexpr SpecStateTable<BladeBoomerangSpecStateGen> kBladeBoomerangTableGen{
    kBladeBoomerangPointBindingsGen, kBladeBoomerangFlagBindingsGen};

}  // namespace NoMoreDay::skills
