// 本文件由 scripts/gen_skill_contracts.py --gen-specstate 生成，请勿手动编辑。
// 技能 9 效果层 SpecState（POD + 绑定表），由命名 descriptor 驱动。
// 输入：assets/data/skill_specstate/skill_09.json + talent_tree 结构。
#pragma once

#include "game/systems/skill/SpecStateTable.hpp"

#include <array>
#include <cstdint>

namespace NoMoreDay::skills {

// 技能 9 节点常量（talent_tree 全集，节点 id 升序）。
namespace PhantomTranceNodesGen {
constexpr uint32_t LightAsSwallow = 902;
constexpr uint32_t SpiritFlowPierce = 913;
constexpr uint32_t VoidRealmGift = 914;
constexpr uint32_t SwordFollowsMind = 934;
constexpr uint32_t FateBacklash = 935;
constexpr uint32_t TimeReversal = 954;
constexpr uint32_t FullFocus = 955;
constexpr uint32_t SkyThunder = 972;
constexpr uint32_t OverloadShield = 973;
constexpr uint32_t ClearMind = 974;
constexpr uint32_t Longevity = 975;
constexpr uint32_t CycloneBurst = 976;
constexpr uint32_t DeathDefiance = 977;
constexpr uint32_t BloodRebirth = 978;
constexpr uint32_t ShadowArmor = 979;
constexpr uint32_t VoidBody = 980;
constexpr uint32_t ReverseMeridian = 981;
constexpr uint32_t DesperateGambit = 982;
constexpr uint32_t DeathSpiral = 983;
constexpr uint32_t Bloodthirst = 984;
constexpr uint32_t BlinkStrike = 985;
constexpr uint32_t GroundShrink = 986;
constexpr uint32_t IntentFollowsSpirit = 987;
constexpr uint32_t SwordShadow = 988;
constexpr uint32_t SnowVeil = 989;
constexpr uint32_t WinterEnchant = 990;
constexpr uint32_t MindPierce = 991;
constexpr uint32_t SpiritFeedback = 992;
constexpr uint32_t ShadowEcho = 993;
}  // namespace PhantomTranceNodesGen

// 技能 9 效果层 SpecState（POD，A-01 D-A1）。
struct PhantomTranceSpecStateGen {
  using PointBinding = SpecPointBinding<PhantomTranceSpecStateGen>;
  using FlagBinding = SpecFlagBinding<PhantomTranceSpecStateGen>;

  bool lightAsSwallow = false;
  bool spiritFlowPierce = false;
  bool voidRealmGift = false;
  bool swordFollowsMind = false;
  bool fateBacklash = false;
  bool timeReversal = false;
  bool fullFocus = false;
  bool skyThunder = false;
  bool overloadShield = false;
  bool clearMind = false;
  bool longevity = false;
  bool cycloneBurst = false;
  bool deathDefiance = false;
  bool bloodRebirth = false;
  bool shadowArmor = false;
  bool voidBody = false;
  bool reverseMeridian = false;
  bool desperateGambit = false;
  bool deathSpiral = false;
  bool bloodthirst = false;
  bool blinkStrike = false;
  bool groundShrink = false;
  bool intentFollowsSpirit = false;
  bool swordShadow = false;
  bool snowVeil = false;
  bool winterEnchant = false;
  bool mindPierce = false;
  bool spiritFeedback = false;
  bool shadowEcho = false;
};

inline constexpr std::array<PhantomTranceSpecStateGen::PointBinding, 0> kPhantomTrancePointBindingsGen{{
}};

inline constexpr std::array<PhantomTranceSpecStateGen::FlagBinding, 29> kPhantomTranceFlagBindingsGen{{
    {PhantomTranceNodesGen::LightAsSwallow, &PhantomTranceSpecStateGen::lightAsSwallow},
    {PhantomTranceNodesGen::SpiritFlowPierce, &PhantomTranceSpecStateGen::spiritFlowPierce},
    {PhantomTranceNodesGen::VoidRealmGift, &PhantomTranceSpecStateGen::voidRealmGift},
    {PhantomTranceNodesGen::SwordFollowsMind, &PhantomTranceSpecStateGen::swordFollowsMind},
    {PhantomTranceNodesGen::FateBacklash, &PhantomTranceSpecStateGen::fateBacklash},
    {PhantomTranceNodesGen::TimeReversal, &PhantomTranceSpecStateGen::timeReversal},
    {PhantomTranceNodesGen::FullFocus, &PhantomTranceSpecStateGen::fullFocus},
    {PhantomTranceNodesGen::SkyThunder, &PhantomTranceSpecStateGen::skyThunder},
    {PhantomTranceNodesGen::OverloadShield, &PhantomTranceSpecStateGen::overloadShield},
    {PhantomTranceNodesGen::ClearMind, &PhantomTranceSpecStateGen::clearMind},
    {PhantomTranceNodesGen::Longevity, &PhantomTranceSpecStateGen::longevity},
    {PhantomTranceNodesGen::CycloneBurst, &PhantomTranceSpecStateGen::cycloneBurst},
    {PhantomTranceNodesGen::DeathDefiance, &PhantomTranceSpecStateGen::deathDefiance},
    {PhantomTranceNodesGen::BloodRebirth, &PhantomTranceSpecStateGen::bloodRebirth},
    {PhantomTranceNodesGen::ShadowArmor, &PhantomTranceSpecStateGen::shadowArmor},
    {PhantomTranceNodesGen::VoidBody, &PhantomTranceSpecStateGen::voidBody},
    {PhantomTranceNodesGen::ReverseMeridian, &PhantomTranceSpecStateGen::reverseMeridian},
    {PhantomTranceNodesGen::DesperateGambit, &PhantomTranceSpecStateGen::desperateGambit},
    {PhantomTranceNodesGen::DeathSpiral, &PhantomTranceSpecStateGen::deathSpiral},
    {PhantomTranceNodesGen::Bloodthirst, &PhantomTranceSpecStateGen::bloodthirst},
    {PhantomTranceNodesGen::BlinkStrike, &PhantomTranceSpecStateGen::blinkStrike},
    {PhantomTranceNodesGen::GroundShrink, &PhantomTranceSpecStateGen::groundShrink},
    {PhantomTranceNodesGen::IntentFollowsSpirit, &PhantomTranceSpecStateGen::intentFollowsSpirit},
    {PhantomTranceNodesGen::SwordShadow, &PhantomTranceSpecStateGen::swordShadow},
    {PhantomTranceNodesGen::SnowVeil, &PhantomTranceSpecStateGen::snowVeil},
    {PhantomTranceNodesGen::WinterEnchant, &PhantomTranceSpecStateGen::winterEnchant},
    {PhantomTranceNodesGen::MindPierce, &PhantomTranceSpecStateGen::mindPierce},
    {PhantomTranceNodesGen::SpiritFeedback, &PhantomTranceSpecStateGen::spiritFeedback},
    {PhantomTranceNodesGen::ShadowEcho, &PhantomTranceSpecStateGen::shadowEcho},
}};

inline constexpr SpecStateTable<PhantomTranceSpecStateGen> kPhantomTranceTableGen{
    kPhantomTrancePointBindingsGen, kPhantomTranceFlagBindingsGen};

}  // namespace NoMoreDay::skills
