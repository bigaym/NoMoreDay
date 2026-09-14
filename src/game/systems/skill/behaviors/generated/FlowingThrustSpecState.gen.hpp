// 本文件由 scripts/gen_skill_contracts.py --gen-specstate 生成，请勿手动编辑。
// 技能 1 效果层 SpecState（POD + 绑定表），由命名 descriptor 驱动。
// 输入：assets/data/skill_specstate/skill_01.json + talent_tree 结构。
#pragma once

#include "game/systems/skill/SpecStateTable.hpp"

#include <array>
#include <cstdint>

namespace NoMoreDay::skills {

// 技能 1 节点常量（talent_tree 全集，节点 id 升序）。
namespace FlowingThrustNodesGen {
constexpr uint32_t SwiftBlade = 100;
constexpr uint32_t Gather = 101;
constexpr uint32_t SwordHeart = 102;
constexpr uint32_t FlowingSpirit = 103;
constexpr uint32_t Pierce = 110;
constexpr uint32_t Chain = 111;
constexpr uint32_t Momentum = 112;
constexpr uint32_t Windwalker = 113;
constexpr uint32_t RidingTheWind = 114;
constexpr uint32_t Relentless = 115;
constexpr uint32_t Afterimage = 130;
constexpr uint32_t ShadowDomain = 131;
constexpr uint32_t ShadowStrike = 132;
constexpr uint32_t Swap = 133;
constexpr uint32_t ShadowBlitz = 134;
constexpr uint32_t PhantomShield = 135;
constexpr uint32_t WeakPoint = 150;
constexpr uint32_t DeepWounds = 151;
constexpr uint32_t ArterySever = 152;
constexpr uint32_t BloodDrinker = 153;
constexpr uint32_t AllIn = 154;
constexpr uint32_t SeverFate = 155;
constexpr uint32_t Hellfire = 170;
constexpr uint32_t InfernalPath = 171;
constexpr uint32_t FreezingWind = 172;
constexpr uint32_t BoneDeepFrost = 173;
constexpr uint32_t ElementalErosion = 174;
constexpr uint32_t ResidualElements = 175;
}  // namespace FlowingThrustNodesGen

// 技能 1 效果层 SpecState（POD，A-01 D-A1）。
struct FlowingThrustSpecStateGen {
  using PointBinding = SpecPointBinding<FlowingThrustSpecStateGen>;
  using FlagBinding = SpecFlagBinding<FlowingThrustSpecStateGen>;

  int momentumPoints = 0;
  int ridingTheWindPoints = 0;
  int relentlessPoints = 0;
  int phantomShieldPoints = 0;
  int deepWoundsPoints = 0;
  int arterySeverPoints = 0;
  int allInPoints = 0;
  int severFatePoints = 0;
  int infernalPathPoints = 0;
  int boneDeepFrostPoints = 0;
  int elementalErosionPoints = 0;
  int residualElementsPoints = 0;
  bool windwalker = false;
  bool afterimage = false;
  bool shadowStrike = false;
  bool swap = false;
};

inline constexpr std::array<FlowingThrustSpecStateGen::PointBinding, 12> kFlowingThrustPointBindingsGen{{
    {FlowingThrustNodesGen::Momentum, &FlowingThrustSpecStateGen::momentumPoints},
    {FlowingThrustNodesGen::RidingTheWind, &FlowingThrustSpecStateGen::ridingTheWindPoints},
    {FlowingThrustNodesGen::Relentless, &FlowingThrustSpecStateGen::relentlessPoints},
    {FlowingThrustNodesGen::PhantomShield, &FlowingThrustSpecStateGen::phantomShieldPoints},
    {FlowingThrustNodesGen::DeepWounds, &FlowingThrustSpecStateGen::deepWoundsPoints},
    {FlowingThrustNodesGen::ArterySever, &FlowingThrustSpecStateGen::arterySeverPoints},
    {FlowingThrustNodesGen::AllIn, &FlowingThrustSpecStateGen::allInPoints},
    {FlowingThrustNodesGen::SeverFate, &FlowingThrustSpecStateGen::severFatePoints},
    {FlowingThrustNodesGen::InfernalPath, &FlowingThrustSpecStateGen::infernalPathPoints},
    {FlowingThrustNodesGen::BoneDeepFrost, &FlowingThrustSpecStateGen::boneDeepFrostPoints},
    {FlowingThrustNodesGen::ElementalErosion, &FlowingThrustSpecStateGen::elementalErosionPoints},
    {FlowingThrustNodesGen::ResidualElements, &FlowingThrustSpecStateGen::residualElementsPoints},
}};

inline constexpr std::array<FlowingThrustSpecStateGen::FlagBinding, 4> kFlowingThrustFlagBindingsGen{{
    {FlowingThrustNodesGen::Windwalker, &FlowingThrustSpecStateGen::windwalker},
    {FlowingThrustNodesGen::Afterimage, &FlowingThrustSpecStateGen::afterimage},
    {FlowingThrustNodesGen::ShadowStrike, &FlowingThrustSpecStateGen::shadowStrike},
    {FlowingThrustNodesGen::Swap, &FlowingThrustSpecStateGen::swap},
}};

inline constexpr SpecStateTable<FlowingThrustSpecStateGen> kFlowingThrustTableGen{
    kFlowingThrustPointBindingsGen, kFlowingThrustFlagBindingsGen};

}  // namespace NoMoreDay::skills
