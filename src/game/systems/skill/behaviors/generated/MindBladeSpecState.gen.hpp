// 本文件由 scripts/gen_skill_contracts.py --gen-specstate 生成，请勿手动编辑。
// 技能 7 效果层 SpecState（POD + 绑定表），由命名 descriptor 驱动。
// 输入：assets/data/skill_specstate/skill_07.json + talent_tree 结构。
#pragma once

#include "game/systems/skill/SpecStateTable.hpp"

#include <array>
#include <cstdint>

namespace NoMoreDay::skills {

// 技能 7 节点常量（talent_tree 全集，节点 id 升序）。
namespace MindBladeNodesGen {
constexpr uint32_t SpiritualFocus = 700;
constexpr uint32_t ShadowlessForm = 701;
constexpr uint32_t Rift = 702;
constexpr uint32_t MindMapping = 703;
constexpr uint32_t MindFlowStack = 710;
constexpr uint32_t SpaceBurst = 711;
constexpr uint32_t VoidTraction = 712;
constexpr uint32_t MentalOverdraw = 713;
constexpr uint32_t Oblivion = 714;
constexpr uint32_t WeaknessInsight = 715;
constexpr uint32_t MindSplit = 730;
constexpr uint32_t ThousandFaces = 731;
constexpr uint32_t ShadowStep = 732;
constexpr uint32_t SwordFlight = 733;
constexpr uint32_t SpiritDisengage = 734;
constexpr uint32_t PreciseCut = 735;
constexpr uint32_t GravityCollapse = 750;
constexpr uint32_t SpaceShatter = 751;
constexpr uint32_t AbyssErosion = 752;
constexpr uint32_t MindStorm = 753;
constexpr uint32_t SwordIntentVoid = 754;
constexpr uint32_t MindFeedback = 755;
constexpr uint32_t GlacialShards = 770;
constexpr uint32_t FrostboneShatter = 771;
constexpr uint32_t OrbitalStrike = 772;
constexpr uint32_t HeavenlyTribulation = 773;
constexpr uint32_t AnomalyCut = 774;
constexpr uint32_t MindResistBreak = 775;
}  // namespace MindBladeNodesGen

// 技能 7 效果层 SpecState（POD，A-01 D-A1）。
struct MindBladeSpecStateGen {
  using PointBinding = SpecPointBinding<MindBladeSpecStateGen>;
  using FlagBinding = SpecFlagBinding<MindBladeSpecStateGen>;

  bool glacialShards = false;
  bool orbitalStrike = false;
};

inline constexpr std::array<MindBladeSpecStateGen::PointBinding, 0> kMindBladePointBindingsGen{{
}};

inline constexpr std::array<MindBladeSpecStateGen::FlagBinding, 2> kMindBladeFlagBindingsGen{{
    {MindBladeNodesGen::GlacialShards, &MindBladeSpecStateGen::glacialShards},
    {MindBladeNodesGen::OrbitalStrike, &MindBladeSpecStateGen::orbitalStrike},
}};

inline constexpr SpecStateTable<MindBladeSpecStateGen> kMindBladeTableGen{
    kMindBladePointBindingsGen, kMindBladeFlagBindingsGen};

}  // namespace NoMoreDay::skills
