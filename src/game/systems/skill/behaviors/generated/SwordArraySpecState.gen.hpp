// 本文件由 scripts/gen_skill_contracts.py --gen-specstate 生成，请勿手动编辑。
// 技能 6 效果层 SpecState（POD + 绑定表），由命名 descriptor 驱动。
// 输入：assets/data/skill_specstate/skill_06.json + talent_tree 结构。
#pragma once

#include "game/systems/skill/SpecStateTable.hpp"

#include <array>
#include <cstdint>

namespace NoMoreDay::skills {

// 技能 6 节点常量（talent_tree 全集，节点 id 升序）。
namespace SwordArrayNodesGen {
constexpr uint32_t Duration = 600;
constexpr uint32_t Expansion = 601;
constexpr uint32_t Torment = 602;
constexpr uint32_t Structure = 603;
constexpr uint32_t TwinArrays = 610;
constexpr uint32_t TriFormation = 611;
constexpr uint32_t Resonance = 612;
constexpr uint32_t Connection = 613;
constexpr uint32_t DashTrigger = 614;
constexpr uint32_t SwordStepArray = 615;
constexpr uint32_t SlowPressure = 630;
constexpr uint32_t ArmorIntent = 631;
constexpr uint32_t Weaken = 632;
constexpr uint32_t ExecuteField = 633;
constexpr uint32_t Cage = 634;
constexpr uint32_t ArrayEcho = 635;
constexpr uint32_t Core = 650;
constexpr uint32_t ManaSpring = 651;
constexpr uint32_t MindUnity = 652;
constexpr uint32_t MobileAura = 653;
constexpr uint32_t CooldownRecovery = 654;
constexpr uint32_t ArrayWard = 655;
constexpr uint32_t FireField = 670;
constexpr uint32_t InfernalGround = 671;
constexpr uint32_t LightningField = 672;
constexpr uint32_t ChainThunder = 673;
constexpr uint32_t ArrayCorrosion = 674;
constexpr uint32_t Relocate = 675;
}  // namespace SwordArrayNodesGen

// 技能 6 效果层 SpecState（POD，A-01 D-A1）。
struct SwordArraySpecStateGen {
  using PointBinding = SpecPointBinding<SwordArraySpecStateGen>;
  using FlagBinding = SpecFlagBinding<SwordArraySpecStateGen>;

  int resonancePoints = 0;
  int swordStepArrayPoints = 0;
  int slowPressurePoints = 0;
  int armorIntentPoints = 0;
  int weakenPoints = 0;
  int corePoints = 0;
  int manaSpringPoints = 0;
  int mindUnityPoints = 0;
  int cooldownRecoveryPoints = 0;
  int arrayWardPoints = 0;
  int infernalGroundPoints = 0;
  int chainThunderPoints = 0;
  int arrayCorrosionPoints = 0;
  bool twinArrays = false;
  bool triFormation = false;
  bool connection = false;
  bool dashTrigger = false;
  bool executeField = false;
  bool cage = false;
  bool mobileAura = false;
  bool fireField = false;
  bool lightningField = false;
  bool relocate = false;
};

inline constexpr std::array<SwordArraySpecStateGen::PointBinding, 13> kSwordArrayPointBindingsGen{{
    {SwordArrayNodesGen::Resonance, &SwordArraySpecStateGen::resonancePoints},
    {SwordArrayNodesGen::SwordStepArray, &SwordArraySpecStateGen::swordStepArrayPoints},
    {SwordArrayNodesGen::SlowPressure, &SwordArraySpecStateGen::slowPressurePoints},
    {SwordArrayNodesGen::ArmorIntent, &SwordArraySpecStateGen::armorIntentPoints},
    {SwordArrayNodesGen::Weaken, &SwordArraySpecStateGen::weakenPoints},
    {SwordArrayNodesGen::Core, &SwordArraySpecStateGen::corePoints},
    {SwordArrayNodesGen::ManaSpring, &SwordArraySpecStateGen::manaSpringPoints},
    {SwordArrayNodesGen::MindUnity, &SwordArraySpecStateGen::mindUnityPoints},
    {SwordArrayNodesGen::CooldownRecovery, &SwordArraySpecStateGen::cooldownRecoveryPoints},
    {SwordArrayNodesGen::ArrayWard, &SwordArraySpecStateGen::arrayWardPoints},
    {SwordArrayNodesGen::InfernalGround, &SwordArraySpecStateGen::infernalGroundPoints},
    {SwordArrayNodesGen::ChainThunder, &SwordArraySpecStateGen::chainThunderPoints},
    {SwordArrayNodesGen::ArrayCorrosion, &SwordArraySpecStateGen::arrayCorrosionPoints},
}};

inline constexpr std::array<SwordArraySpecStateGen::FlagBinding, 10> kSwordArrayFlagBindingsGen{{
    {SwordArrayNodesGen::TwinArrays, &SwordArraySpecStateGen::twinArrays},
    {SwordArrayNodesGen::TriFormation, &SwordArraySpecStateGen::triFormation},
    {SwordArrayNodesGen::Connection, &SwordArraySpecStateGen::connection},
    {SwordArrayNodesGen::DashTrigger, &SwordArraySpecStateGen::dashTrigger},
    {SwordArrayNodesGen::ExecuteField, &SwordArraySpecStateGen::executeField},
    {SwordArrayNodesGen::Cage, &SwordArraySpecStateGen::cage},
    {SwordArrayNodesGen::MobileAura, &SwordArraySpecStateGen::mobileAura},
    {SwordArrayNodesGen::FireField, &SwordArraySpecStateGen::fireField},
    {SwordArrayNodesGen::LightningField, &SwordArraySpecStateGen::lightningField},
    {SwordArrayNodesGen::Relocate, &SwordArraySpecStateGen::relocate},
}};

inline constexpr SpecStateTable<SwordArraySpecStateGen> kSwordArrayTableGen{
    kSwordArrayPointBindingsGen, kSwordArrayFlagBindingsGen};

}  // namespace NoMoreDay::skills
