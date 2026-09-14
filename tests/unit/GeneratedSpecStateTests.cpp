// A-01 Phase 3 单测：--gen-specstate 生成物与手写 SpecState 绑定表的语义等价。
//
// 生成器（scripts/gen_skill_contracts.py --gen-specstate）从「talent_tree 结构 +
// 命名 descriptor」产出 generated/*.gen.hpp（POD + 绑定表），本文件把生成表与
// 各技能手写的生产绑定表放在同一专精分配下解析，逐绑定比较节点 id 与解析值，
// 确保 P3.3 对拍成立，且生成物与手写表不产生语义漂移。
//
// 覆盖：技能 10/11/12（point + flag，手写绑定表仍在）；技能 1/5/6/7/8/9（手写
// 绑定表已删除或收口，故与本文件测试侧独立期望表对拍）。机制系数不在 SpecState 内
//（计划 §6.2），故不参与对拍。行为侧施法测试见 BladeBoomerangCatchTests /
// BloodSeaTests / SwordArrayTests / InfiniteBladesTests / FlowingThrustTests 等。

#include "TestCommon.hpp"

#include "game/foundation/components/SkillDefs.hpp"
#include "game/systems/skill/SpecStateTable.hpp"
#include "game/systems/skill/behaviors/BloodSea.hpp"
#include "game/systems/skill/behaviors/HeavenlySwordDescent.hpp"
#include "game/systems/skill/behaviors/SevenStarSlashShared.hpp"
#include "game/systems/skill/behaviors/generated/BladeBoomerangSpecState.gen.hpp"
#include "game/systems/skill/behaviors/generated/BloodSeaSpecState.gen.hpp"
#include "game/systems/skill/behaviors/generated/HeavenlySwordDescentSpecState.gen.hpp"
#include "game/systems/skill/behaviors/generated/InfiniteBladesSpecState.gen.hpp"
#include "game/systems/skill/behaviors/generated/SevenStarSlashSpecState.gen.hpp"
#include "game/systems/skill/behaviors/generated/SwordArraySpecState.gen.hpp"
#include "game/systems/skill/behaviors/generated/BladeFormationSpecState.gen.hpp"
#include "game/systems/skill/behaviors/generated/BladeWardSpecState.gen.hpp"
#include "game/systems/skill/behaviors/generated/FlowingThrustSpecState.gen.hpp"
#include "game/systems/skill/behaviors/generated/MindBladeSpecState.gen.hpp"
#include "game/systems/skill/behaviors/generated/PhantomTranceSpecState.gen.hpp"
#include "game/systems/skill/behaviors/generated/RendingWaveSpecState.gen.hpp"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

namespace NoMoreDay {
namespace {

void SetSpecialization(entt::registry &registry, entt::entity owner,
                       uint32_t skill_id,
                       const std::vector<std::pair<uint32_t, int>> &points,
                       size_t slot = 0) {
  auto &active = registry.get_or_emplace<ActiveSkillsComponent>(owner);
  auto &spec = active.specialized_slots[slot];
  spec.skill_id = skill_id;
  spec.allocated_points.clear();
  for (const auto &[node_id, pts] : points) {
    spec.allocated_points[node_id] = pts;
  }
}

// 把生成表与手写生产绑定表放在同一专精分配下解析，逐绑定比较。
// 手写绑定结构（如 SevenStarSlashPointBinding）与模板绑定结构字段同名，搬运即可。
template <typename GenState, typename HandState, typename HandPointArray,
          typename HandFlagArray>
void CheckGeneratedMatchesHandWritten(
    uint32_t skillId, const skills::SpecStateTable<GenState> &generated,
    const HandPointArray &handPointBindings,
    const HandFlagArray &handFlagBindings) {
  std::vector<skills::SpecPointBinding<HandState>> handPoints;
  std::vector<skills::SpecFlagBinding<HandState>> handFlags;
  handPoints.reserve(handPointBindings.size());
  handFlags.reserve(handFlagBindings.size());
  for (const auto &binding : handPointBindings) {
    handPoints.push_back({binding.node, binding.points});
  }
  for (const auto &binding : handFlagBindings) {
    handFlags.push_back({binding.node, binding.flag});
  }
  const skills::SpecStateTable<HandState> handTable{handPoints, handFlags};

  entt::registry registry;
  const entt::entity owner = registry.create();
  // 生成表为准分配：点数节点取各不相同的正值，点亮节点取 1，另加未知节点。
  std::vector<std::pair<uint32_t, int>> allocated;
  allocated.reserve(generated.points.size() + generated.flags.size() + 1);
  for (size_t i = 0; i < generated.points.size(); ++i) {
    allocated.emplace_back(generated.points[i].node, static_cast<int>(i + 1));
  }
  for (const auto &binding : generated.flags) {
    allocated.emplace_back(binding.node, 1);
  }
  allocated.emplace_back(999999, 7);
  SetSpecialization(registry, owner, skillId, allocated);

  const GenState generatedState =
      skills::ResolveSpecState(registry, owner, skillId, generated);
  const HandState handWrittenState =
      skills::ResolveSpecState(registry, owner, skillId, handTable);

  REQUIRE(generated.points.size() == handPointBindings.size());
  REQUIRE(generated.flags.size() == handFlagBindings.size());
  for (size_t i = 0; i < handPointBindings.size(); ++i) {
    CAPTURE(handPointBindings[i].node);
    CHECK(generated.points[i].node == handPointBindings[i].node);
    CHECK((generatedState.*(generated.points[i].points)) ==
          (handWrittenState.*(handPointBindings[i].points)));
  }
  for (size_t i = 0; i < handFlagBindings.size(); ++i) {
    CAPTURE(handFlagBindings[i].node);
    CHECK(generated.flags[i].node == handFlagBindings[i].node);
    CHECK((generatedState.*(generated.flags[i].flag)) ==
          (handWrittenState.*(handFlagBindings[i].flag)));
  }
}

// 生成物对拍（技能 5/6 手写绑定表已随 Phase 4a 删除）：把生成绑定与测试侧
// 独立期望表按节点 id 对齐，校验成员指针一致，并在同一专精分配下比较解析值。
template <typename GenState, size_t NPoint, size_t NFlag>
void CheckGeneratedMatchesExpected(
    uint32_t skillId, const skills::SpecStateTable<GenState> &generated,
    const std::array<skills::SpecPointBinding<GenState>, NPoint> &expectedPoints,
    const std::array<skills::SpecFlagBinding<GenState>, NFlag> &expectedFlags) {
  REQUIRE(generated.points.size() == NPoint);
  REQUIRE(generated.flags.size() == NFlag);

  std::vector<skills::SpecPointBinding<GenState>> handPoints(expectedPoints.begin(),
                                                            expectedPoints.end());
  std::vector<skills::SpecFlagBinding<GenState>> handFlags(expectedFlags.begin(),
                                                          expectedFlags.end());
  const skills::SpecStateTable<GenState> expectedTable{handPoints, handFlags};

  entt::registry registry;
  const entt::entity owner = registry.create();
  // 期望表为准分配：点数节点取各不相同的正值，点亮节点取 1，另加未知节点。
  std::vector<std::pair<uint32_t, int>> allocated;
  allocated.reserve(NPoint + NFlag + 1);
  for (size_t i = 0; i < NPoint; ++i) {
    allocated.emplace_back(expectedPoints[i].node, static_cast<int>(i + 1));
  }
  for (const auto &binding : expectedFlags) {
    allocated.emplace_back(binding.node, 1);
  }
  allocated.emplace_back(999999, 7);
  SetSpecialization(registry, owner, skillId, allocated);

  const GenState generatedState =
      skills::ResolveSpecState(registry, owner, skillId, generated);
  const GenState expectedState =
      skills::ResolveSpecState(registry, owner, skillId, expectedTable);

  for (const auto &binding : generated.points) {
    CAPTURE(binding.node);
    const auto it = std::find_if(expectedPoints.begin(), expectedPoints.end(),
                                 [&](const auto &expected) {
                                   return expected.node == binding.node;
                                 });
    REQUIRE(it != expectedPoints.end());
    CHECK(binding.points == it->points);
    CHECK((generatedState.*(binding.points)) == (expectedState.*(it->points)));
  }
  for (const auto &binding : generated.flags) {
    CAPTURE(binding.node);
    const auto it = std::find_if(expectedFlags.begin(), expectedFlags.end(),
                                 [&](const auto &expected) {
                                   return expected.node == binding.node;
                                 });
    REQUIRE(it != expectedFlags.end());
    CHECK(binding.flag == it->flag);
    CHECK((generatedState.*(binding.flag)) == (expectedState.*(it->flag)));
  }
}

} // namespace

TEST_CASE("[Unit] Skill SpecState generated - skill 8 matches expected mapping") {
  using State = skills::BladeBoomerangSpecStateGen;
  CHECK(skills::kBladeBoomerangTableGen.points.size() == 5);
  CHECK(skills::kBladeBoomerangTableGen.flags.empty());

  CHECK(skills::BladeBoomerangNodesGen::Sharpness == 802);
  CHECK(skills::BladeBoomerangNodesGen::Bleed == 811);
  CHECK(skills::BladeBoomerangNodesGen::SonicBoom == 813);
  CHECK(skills::BladeBoomerangNodesGen::IntentGain == 852);
  CHECK(skills::BladeBoomerangNodesGen::Giant == 854);

  const std::array<skills::SpecPointBinding<State>, 5> expectedPoints{{
      {802, &State::sharpnessPoints},
      {811, &State::bleedPoints},
      {813, &State::sonicBoomPoints},
      {852, &State::intentGainPoints},
      {854, &State::giantPoints}}};
  const std::array<skills::SpecFlagBinding<State>, 0> expectedFlags{};
  CheckGeneratedMatchesExpected<State>(8, skills::kBladeBoomerangTableGen,
                                       expectedPoints, expectedFlags);
}

TEST_CASE("[Unit] Skill SpecState generated - skill 10 matches hand-written table") {
  CHECK(skills::kSevenStarSlashTableGen.points.size() == 17);
  CHECK(skills::kSevenStarSlashTableGen.flags.size() == 6);
  CheckGeneratedMatchesHandWritten<skills::SevenStarSlashSpecStateGen,
                                   skills::SevenStarSlashSpecState>(
      skills::seven_star_shared::kSevenStarSlashSkillId,
      skills::kSevenStarSlashTableGen,
      skills::kSevenStarSlashPointBindings,
      skills::kSevenStarSlashFlagBindings);
}

TEST_CASE("[Unit] Skill SpecState generated - skill 11 matches hand-written table") {
  CHECK(skills::kHeavenlySwordDescentTableGen.points.size() == 16);
  CHECK(skills::kHeavenlySwordDescentTableGen.flags.size() == 9);
  CheckGeneratedMatchesHandWritten<skills::HeavenlySwordCastSpecGen,
                                   skills::HeavenlySwordCastSpec>(
      skills::kHeavenlySwordSkillId, skills::kHeavenlySwordDescentTableGen,
      skills::kHeavenlySwordPointBindings,
      skills::kHeavenlySwordFlagBindings);
}

TEST_CASE("[Unit] Skill SpecState generated - skill 12 matches hand-written table") {
  CHECK(skills::kBloodSeaTableGen.points.size() == 18);
  CHECK(skills::kBloodSeaTableGen.flags.size() == 7);
  CheckGeneratedMatchesHandWritten<skills::BloodSeaCastSpecGen,
                                   skills::BloodSeaCastSpec>(
      skills::kBloodSeaSkillId, skills::kBloodSeaTableGen,
      skills::kBloodSeaPointBindings, skills::kBloodSeaFlagBindings);
}

// 技能 5/6 于 Phase 4a 迁移到生成式信封（手写绑定表已删除），故与测试侧独立
// 期望表对拍：节点常量命名空间仍锚定历史节点 id，绑定成员指针须与生成物一致。
TEST_CASE("[Unit] Skill SpecState generated - skill 5 matches expected mapping") {
  using State = skills::InfiniteBladesSpecStateGen;
  CHECK(skills::kInfiniteBladesTableGen.points.size() == 8);
  CHECK(skills::kInfiniteBladesTableGen.flags.size() == 4);

  CHECK(skills::InfiniteBladesNodesGen::MeteoricIron == 502);
  CHECK(skills::InfiniteBladesNodesGen::FateMark == 512);
  CHECK(skills::InfiniteBladesNodesGen::BladeStorm == 514);
  CHECK(skills::InfiniteBladesNodesGen::SwordStepChannel == 551);
  CHECK(skills::InfiniteBladesNodesGen::IntentSiphon == 553);
  CHECK(skills::InfiniteBladesNodesGen::DoomsdayAsh == 571);
  CHECK(skills::InfiniteBladesNodesGen::AbsoluteZero == 573);
  CHECK(skills::InfiniteBladesNodesGen::Catastrophe == 575);
  CHECK(skills::InfiniteBladesNodesGen::MindLock == 510);
  CHECK(skills::InfiniteBladesNodesGen::IntentBurst == 554);
  CHECK(skills::InfiniteBladesNodesGen::Multiplier == 555);
  CHECK(skills::InfiniteBladesNodesGen::Blizzard == 572);

  const std::array<skills::SpecPointBinding<State>, 8> expectedPoints{{
      {502, &State::meteoricIronPoints},
      {512, &State::fateMarkPoints},
      {514, &State::bladeStormPoints},
      {551, &State::swordStepChannelPoints},
      {553, &State::intentSiphonPoints},
      {571, &State::doomsdayAshPoints},
      {573, &State::absoluteZeroPoints},
      {575, &State::catastrophePoints}}};
  const std::array<skills::SpecFlagBinding<State>, 4> expectedFlags{{
      {510, &State::mindLock},
      {554, &State::intentBurst},
      {555, &State::multiplier},
      {572, &State::blizzard}}};
  CheckGeneratedMatchesExpected<State>(
      5, skills::kInfiniteBladesTableGen, expectedPoints, expectedFlags);
}

TEST_CASE("[Unit] Skill SpecState generated - skill 6 matches expected mapping") {
  using State = skills::SwordArraySpecStateGen;
  CHECK(skills::kSwordArrayTableGen.points.size() == 13);
  CHECK(skills::kSwordArrayTableGen.flags.size() == 10);

  CHECK(skills::SwordArrayNodesGen::Resonance == 612);
  CHECK(skills::SwordArrayNodesGen::SwordStepArray == 615);
  CHECK(skills::SwordArrayNodesGen::SlowPressure == 630);
  CHECK(skills::SwordArrayNodesGen::ArmorIntent == 631);
  CHECK(skills::SwordArrayNodesGen::Weaken == 632);
  CHECK(skills::SwordArrayNodesGen::Core == 650);
  CHECK(skills::SwordArrayNodesGen::ManaSpring == 651);
  CHECK(skills::SwordArrayNodesGen::MindUnity == 652);
  CHECK(skills::SwordArrayNodesGen::CooldownRecovery == 654);
  CHECK(skills::SwordArrayNodesGen::ArrayWard == 655);
  CHECK(skills::SwordArrayNodesGen::InfernalGround == 671);
  CHECK(skills::SwordArrayNodesGen::ChainThunder == 673);
  CHECK(skills::SwordArrayNodesGen::ArrayCorrosion == 674);
  CHECK(skills::SwordArrayNodesGen::TwinArrays == 610);
  CHECK(skills::SwordArrayNodesGen::TriFormation == 611);
  CHECK(skills::SwordArrayNodesGen::Connection == 613);
  CHECK(skills::SwordArrayNodesGen::DashTrigger == 614);
  CHECK(skills::SwordArrayNodesGen::ExecuteField == 633);
  CHECK(skills::SwordArrayNodesGen::Cage == 634);
  CHECK(skills::SwordArrayNodesGen::MobileAura == 653);
  CHECK(skills::SwordArrayNodesGen::FireField == 670);
  CHECK(skills::SwordArrayNodesGen::LightningField == 672);
  CHECK(skills::SwordArrayNodesGen::Relocate == 675);

  const std::array<skills::SpecPointBinding<State>, 13> expectedPoints{{
      {612, &State::resonancePoints},
      {615, &State::swordStepArrayPoints},
      {630, &State::slowPressurePoints},
      {631, &State::armorIntentPoints},
      {632, &State::weakenPoints},
      {650, &State::corePoints},
      {651, &State::manaSpringPoints},
      {652, &State::mindUnityPoints},
      {654, &State::cooldownRecoveryPoints},
      {655, &State::arrayWardPoints},
      {671, &State::infernalGroundPoints},
      {673, &State::chainThunderPoints},
      {674, &State::arrayCorrosionPoints}}};
  const std::array<skills::SpecFlagBinding<State>, 10> expectedFlags{{
      {610, &State::twinArrays},
      {611, &State::triFormation},
      {613, &State::connection},
      {614, &State::dashTrigger},
      {633, &State::executeField},
      {634, &State::cage},
      {653, &State::mobileAura},
      {670, &State::fireField},
      {672, &State::lightningField},
      {675, &State::relocate}}};
  CheckGeneratedMatchesExpected<State>(
      6, skills::kSwordArrayTableGen, expectedPoints, expectedFlags);
}

// 技能 2/3/4 于 Phase 4b 迁移到生成式信封（手写绑定表与手写节点命名空间已删除），
// 故与测试侧独立期望表对拍：节点常量命名空间仍锚定历史节点 id，绑定成员指针须与
// 生成物一致，且同一专精分配下解析值相同。
TEST_CASE("[Unit] Skill SpecState generated - skill 2 matches expected mapping") {
  using State = skills::RendingWaveSpecStateGen;
  CHECK(skills::kRendingWaveTableGen.points.size() == 12);
  CHECK(skills::kRendingWaveTableGen.flags.size() == 12);

  CHECK(skills::RendingWaveNodesGen::QiBurst == 203);
  CHECK(skills::RendingWaveNodesGen::MultiWave == 210);
  CHECK(skills::RendingWaveNodesGen::ChainReaction == 212);
  CHECK(skills::RendingWaveNodesGen::DoubleHit == 231);
  CHECK(skills::RendingWaveNodesGen::AbyssEdge == 233);
  CHECK(skills::RendingWaveNodesGen::SwordStepGravity == 235);
  CHECK(skills::RendingWaveNodesGen::QiBrand == 250);
  CHECK(skills::RendingWaveNodesGen::Bottomless == 252);
  CHECK(skills::RendingWaveNodesGen::IntentRecovery == 255);
  CHECK(skills::RendingWaveNodesGen::ShatterCascade == 271);
  CHECK(skills::RendingWaveNodesGen::StaticConduction == 273);
  CHECK(skills::RendingWaveNodesGen::Proliferation == 275);
  CHECK(skills::RendingWaveNodesGen::Fracture == 211);
  CHECK(skills::RendingWaveNodesGen::Scatter == 213);
  CHECK(skills::RendingWaveNodesGen::Orbit == 214);
  CHECK(skills::RendingWaveNodesGen::SpiritPursuit == 215);
  CHECK(skills::RendingWaveNodesGen::Boomerang == 230);
  CHECK(skills::RendingWaveNodesGen::GravityWell == 232);
  CHECK(skills::RendingWaveNodesGen::TimeLock == 234);
  CHECK(skills::RendingWaveNodesGen::IntentBurst == 251);
  CHECK(skills::RendingWaveNodesGen::ObliterationWave == 253);
  CHECK(skills::RendingWaveNodesGen::EchoedSlash == 254);
  CHECK(skills::RendingWaveNodesGen::FrostForm == 270);
  CHECK(skills::RendingWaveNodesGen::LightningForm == 272);

  const std::array<skills::SpecPointBinding<State>, 12> expectedPoints{{
      {203, &State::qiBurstPoints},
      {210, &State::multiWavePoints},
      {212, &State::chainReactionPoints},
      {231, &State::doubleHitPoints},
      {233, &State::abyssEdgePoints},
      {235, &State::swordStepGravityPoints},
      {250, &State::qiBrandPoints},
      {252, &State::bottomlessPoints},
      {255, &State::intentRecoveryPoints},
      {271, &State::shatterCascadePoints},
      {273, &State::staticConductionPoints},
      {275, &State::proliferationPoints}}};
  const std::array<skills::SpecFlagBinding<State>, 12> expectedFlags{{
      {211, &State::fracture},
      {213, &State::scatter},
      {214, &State::orbit},
      {215, &State::spiritPursuit},
      {230, &State::boomerang},
      {232, &State::gravityWell},
      {234, &State::timeLock},
      {251, &State::intentBurst},
      {253, &State::obliterationWave},
      {254, &State::echoedSlash},
      {270, &State::frostForm},
      {272, &State::lightningForm}}};
  CheckGeneratedMatchesExpected<State>(
      2, skills::kRendingWaveTableGen, expectedPoints, expectedFlags);
}

TEST_CASE("[Unit] Skill SpecState generated - skill 3 matches expected mapping") {
  using State = skills::BladeFormationSpecStateGen;
  CHECK(skills::kBladeFormationTableGen.points.size() == 17);
  CHECK(skills::kBladeFormationTableGen.flags.size() == 10);

  CHECK(skills::BladeFormationNodesGen::SwordPool == 300);
  CHECK(skills::BladeFormationNodesGen::SwiftIntent == 301);
  CHECK(skills::BladeFormationNodesGen::EdgedSpirit == 302);
  CHECK(skills::BladeFormationNodesGen::ElementalCore == 303);
  CHECK(skills::BladeFormationNodesGen::SearchRadius == 310);
  CHECK(skills::BladeFormationNodesGen::Network == 312);
  CHECK(skills::BladeFormationNodesGen::SwordStepResonance == 315);
  CHECK(skills::BladeFormationNodesGen::WeakPointCrit == 331);
  CHECK(skills::BladeFormationNodesGen::DeadlyEdge == 332);
  CHECK(skills::BladeFormationNodesGen::SwordPressure == 333);
  CHECK(skills::BladeFormationNodesGen::Crush == 334);
  CHECK(skills::BladeFormationNodesGen::Ward == 350);
  CHECK(skills::BladeFormationNodesGen::RetaliationWeb == 352);
  CHECK(skills::BladeFormationNodesGen::BlazingDance == 371);
  CHECK(skills::BladeFormationNodesGen::ArcChain == 373);
  CHECK(skills::BladeFormationNodesGen::SpiritCorrosion == 374);
  CHECK(skills::BladeFormationNodesGen::Charge == 375);
  CHECK(skills::BladeFormationNodesGen::InfiniteSheath == 311);
  CHECK(skills::BladeFormationNodesGen::Godspeed == 313);
  CHECK(skills::BladeFormationNodesGen::Concentrate == 314);
  CHECK(skills::BladeFormationNodesGen::GiantSword == 330);
  CHECK(skills::BladeFormationNodesGen::BladeOrbit == 351);
  CHECK(skills::BladeFormationNodesGen::Immortality == 353);
  CHECK(skills::BladeFormationNodesGen::SpellEcho == 354);
  CHECK(skills::BladeFormationNodesGen::ArrayResonance == 355);
  CHECK(skills::BladeFormationNodesGen::ElementFire == 370);
  CHECK(skills::BladeFormationNodesGen::ElementLightning == 372);
  // 335 巨剑裂空为 talent_tree 中未被读取的节点，命名由 name_key 机械补全。
  CHECK(skills::BladeFormationNodesGen::GiantSwordRift == 335);

  const std::array<skills::SpecPointBinding<State>, 17> expectedPoints{{
      {300, &State::swordPoolPoints},
      {301, &State::swiftIntentPoints},
      {302, &State::edgedSpiritPoints},
      {303, &State::elementalCorePoints},
      {310, &State::searchRadiusPoints},
      {312, &State::networkPoints},
      {315, &State::swordStepResonancePoints},
      {331, &State::weakPointCritPoints},
      {332, &State::deadlyEdgePoints},
      {333, &State::swordPressurePoints},
      {334, &State::crushPoints},
      {350, &State::wardPoints},
      {352, &State::retaliationWebPoints},
      {371, &State::blazingDancePoints},
      {373, &State::arcChainPoints},
      {374, &State::spiritCorrosionPoints},
      {375, &State::chargePoints}}};
  const std::array<skills::SpecFlagBinding<State>, 10> expectedFlags{{
      {311, &State::infiniteSheath},
      {313, &State::godspeed},
      {314, &State::concentrate},
      {330, &State::giantSword},
      {351, &State::bladeOrbit},
      {353, &State::immortality},
      {354, &State::spellEcho},
      {355, &State::arrayResonance},
      {370, &State::elementFire},
      {372, &State::elementLightning}}};
  CheckGeneratedMatchesExpected<State>(
      3, skills::kBladeFormationTableGen, expectedPoints, expectedFlags);
}

TEST_CASE("[Unit] Skill SpecState generated - skill 4 matches expected mapping") {
  using State = skills::BladeWardSpecStateGen;
  CHECK(skills::kBladeWardTableGen.points.size() == 19);
  CHECK(skills::kBladeWardTableGen.flags.size() == 6);

  CHECK(skills::BladeWardNodesGen::Deflection == 401);
  CHECK(skills::BladeWardNodesGen::Persist == 402);
  CHECK(skills::BladeWardNodesGen::Repel == 403);
  CHECK(skills::BladeWardNodesGen::IronGuard == 410);
  CHECK(skills::BladeWardNodesGen::FiveGuard == 411);
  CHECK(skills::BladeWardNodesGen::LastStand == 413);
  CHECK(skills::BladeWardNodesGen::Vitality == 415);
  CHECK(skills::BladeWardNodesGen::IntentBlock == 430);
  CHECK(skills::BladeWardNodesGen::Unstoppable == 431);
  CHECK(skills::BladeWardNodesGen::ShieldBarrier == 432);
  CHECK(skills::BladeWardNodesGen::PerfectParry == 434);
  CHECK(skills::BladeWardNodesGen::IntentProc == 435);
  CHECK(skills::BladeWardNodesGen::CounterSpeed == 451);
  CHECK(skills::BladeWardNodesGen::Aftermath == 453);
  CHECK(skills::BladeWardNodesGen::SwordStep == 454);
  CHECK(skills::BladeWardNodesGen::Vengeance == 471);
  CHECK(skills::BladeWardNodesGen::ThunderCascade == 473);
  CHECK(skills::BladeWardNodesGen::Permafrost == 475);
  CHECK(skills::BladeWardNodesGen::Exposure == 476);
  CHECK(skills::BladeWardNodesGen::Mountain == 412);
  CHECK(skills::BladeWardNodesGen::BloodBarrier == 433);
  CHECK(skills::BladeWardNodesGen::AttackDefend == 455);
  CHECK(skills::BladeWardNodesGen::CounterBlade == 470);
  CHECK(skills::BladeWardNodesGen::StaticField == 472);
  CHECK(skills::BladeWardNodesGen::FrostArmor == 474);
  // 414/450 为 talent_tree 中未被读取的节点，命名由 name_key 机械补全。
  CHECK(skills::BladeWardNodesGen::SwordWardBarrier == 414);
  CHECK(skills::BladeWardNodesGen::PhantomStep == 450);

  const std::array<skills::SpecPointBinding<State>, 19> expectedPoints{{
      {401, &State::deflectionPoints},
      {402, &State::persistPoints},
      {403, &State::repelPoints},
      {410, &State::ironGuardPoints},
      {411, &State::fiveGuardPoints},
      {413, &State::lastStandPoints},
      {415, &State::vitalityPoints},
      {430, &State::intentBlockPoints},
      {431, &State::unstoppablePoints},
      {432, &State::shieldBarrierPoints},
      {434, &State::perfectParryPoints},
      {435, &State::intentProcPoints},
      {451, &State::counterSpeedPoints},
      {453, &State::aftermathPoints},
      {454, &State::swordStepPoints},
      {471, &State::vengeancePoints},
      {473, &State::thunderCascadePoints},
      {475, &State::permafrostPoints},
      {476, &State::exposurePoints}}};
  const std::array<skills::SpecFlagBinding<State>, 6> expectedFlags{{
      {412, &State::mountain},
      {433, &State::bloodBarrier},
      {455, &State::attackDefend},
      {470, &State::counterBlade},
      {472, &State::staticField},
      {474, &State::frostArmor}}};
  CheckGeneratedMatchesExpected<State>(
      4, skills::kBladeWardTableGen, expectedPoints, expectedFlags);
}

TEST_CASE("[Unit] Skill SpecState generated - skill 1 matches expected mapping") {
  using State = skills::FlowingThrustSpecStateGen;
  CHECK(skills::kFlowingThrustTableGen.points.size() == 12);
  CHECK(skills::kFlowingThrustTableGen.flags.size() == 4);

  CHECK(skills::FlowingThrustNodesGen::SwiftBlade == 100);
  CHECK(skills::FlowingThrustNodesGen::Gather == 101);
  CHECK(skills::FlowingThrustNodesGen::SwordHeart == 102);
  CHECK(skills::FlowingThrustNodesGen::FlowingSpirit == 103);
  CHECK(skills::FlowingThrustNodesGen::Pierce == 110);
  CHECK(skills::FlowingThrustNodesGen::Chain == 111);
  CHECK(skills::FlowingThrustNodesGen::Momentum == 112);
  CHECK(skills::FlowingThrustNodesGen::Windwalker == 113);
  CHECK(skills::FlowingThrustNodesGen::RidingTheWind == 114);
  CHECK(skills::FlowingThrustNodesGen::Relentless == 115);
  CHECK(skills::FlowingThrustNodesGen::Afterimage == 130);
  CHECK(skills::FlowingThrustNodesGen::ShadowDomain == 131);
  CHECK(skills::FlowingThrustNodesGen::ShadowStrike == 132);
  CHECK(skills::FlowingThrustNodesGen::Swap == 133);
  CHECK(skills::FlowingThrustNodesGen::ShadowBlitz == 134);
  CHECK(skills::FlowingThrustNodesGen::PhantomShield == 135);
  CHECK(skills::FlowingThrustNodesGen::WeakPoint == 150);
  CHECK(skills::FlowingThrustNodesGen::DeepWounds == 151);
  CHECK(skills::FlowingThrustNodesGen::ArterySever == 152);
  CHECK(skills::FlowingThrustNodesGen::BloodDrinker == 153);
  CHECK(skills::FlowingThrustNodesGen::AllIn == 154);
  CHECK(skills::FlowingThrustNodesGen::SeverFate == 155);
  CHECK(skills::FlowingThrustNodesGen::Hellfire == 170);
  CHECK(skills::FlowingThrustNodesGen::InfernalPath == 171);
  CHECK(skills::FlowingThrustNodesGen::FreezingWind == 172);
  CHECK(skills::FlowingThrustNodesGen::BoneDeepFrost == 173);
  CHECK(skills::FlowingThrustNodesGen::ElementalErosion == 174);
  CHECK(skills::FlowingThrustNodesGen::ResidualElements == 175);

  const std::array<skills::SpecPointBinding<State>, 12> expectedPoints{{
      {112, &State::momentumPoints},
      {114, &State::ridingTheWindPoints},
      {115, &State::relentlessPoints},
      {135, &State::phantomShieldPoints},
      {151, &State::deepWoundsPoints},
      {152, &State::arterySeverPoints},
      {154, &State::allInPoints},
      {155, &State::severFatePoints},
      {171, &State::infernalPathPoints},
      {173, &State::boneDeepFrostPoints},
      {174, &State::elementalErosionPoints},
      {175, &State::residualElementsPoints}}};
  const std::array<skills::SpecFlagBinding<State>, 4> expectedFlags{{
      {113, &State::windwalker},
      {130, &State::afterimage},
      {132, &State::shadowStrike},
      {133, &State::swap}}};
  CheckGeneratedMatchesExpected<State>(
      1, skills::kFlowingThrustTableGen, expectedPoints, expectedFlags);
}

TEST_CASE("[Unit] Skill SpecState generated - skill 7 matches expected mapping") {
  using State = skills::MindBladeSpecStateGen;
  CHECK(skills::kMindBladeTableGen.points.size() == 0);
  CHECK(skills::kMindBladeTableGen.flags.size() == 2);

  CHECK(skills::MindBladeNodesGen::SpiritualFocus == 700);
  CHECK(skills::MindBladeNodesGen::ShadowlessForm == 701);
  CHECK(skills::MindBladeNodesGen::Rift == 702);
  CHECK(skills::MindBladeNodesGen::MindMapping == 703);
  CHECK(skills::MindBladeNodesGen::MindFlowStack == 710);
  CHECK(skills::MindBladeNodesGen::SpaceBurst == 711);
  CHECK(skills::MindBladeNodesGen::VoidTraction == 712);
  CHECK(skills::MindBladeNodesGen::MentalOverdraw == 713);
  CHECK(skills::MindBladeNodesGen::Oblivion == 714);
  CHECK(skills::MindBladeNodesGen::WeaknessInsight == 715);
  CHECK(skills::MindBladeNodesGen::MindSplit == 730);
  CHECK(skills::MindBladeNodesGen::ThousandFaces == 731);
  CHECK(skills::MindBladeNodesGen::ShadowStep == 732);
  CHECK(skills::MindBladeNodesGen::SwordFlight == 733);
  CHECK(skills::MindBladeNodesGen::SpiritDisengage == 734);
  CHECK(skills::MindBladeNodesGen::PreciseCut == 735);
  CHECK(skills::MindBladeNodesGen::GravityCollapse == 750);
  CHECK(skills::MindBladeNodesGen::SpaceShatter == 751);
  CHECK(skills::MindBladeNodesGen::AbyssErosion == 752);
  CHECK(skills::MindBladeNodesGen::MindStorm == 753);
  CHECK(skills::MindBladeNodesGen::SwordIntentVoid == 754);
  CHECK(skills::MindBladeNodesGen::MindFeedback == 755);
  CHECK(skills::MindBladeNodesGen::GlacialShards == 770);
  CHECK(skills::MindBladeNodesGen::FrostboneShatter == 771);
  CHECK(skills::MindBladeNodesGen::OrbitalStrike == 772);
  CHECK(skills::MindBladeNodesGen::HeavenlyTribulation == 773);
  CHECK(skills::MindBladeNodesGen::AnomalyCut == 774);
  CHECK(skills::MindBladeNodesGen::MindResistBreak == 775);

  const std::array<skills::SpecPointBinding<State>, 0> expectedPoints{};
  const std::array<skills::SpecFlagBinding<State>, 2> expectedFlags{{
      {770, &State::glacialShards},
      {772, &State::orbitalStrike}}};
  CheckGeneratedMatchesExpected<State>(
      7, skills::kMindBladeTableGen, expectedPoints, expectedFlags);
}

TEST_CASE("[Unit] Skill SpecState generated - skill 9 matches expected mapping") {
  using State = skills::PhantomTranceSpecStateGen;
  CHECK(skills::kPhantomTranceTableGen.points.size() == 0);
  CHECK(skills::kPhantomTranceTableGen.flags.size() == 29);

  CHECK(skills::PhantomTranceNodesGen::LightAsSwallow == 902);
  CHECK(skills::PhantomTranceNodesGen::SpiritFlowPierce == 913);
  CHECK(skills::PhantomTranceNodesGen::VoidRealmGift == 914);
  CHECK(skills::PhantomTranceNodesGen::SwordFollowsMind == 934);
  CHECK(skills::PhantomTranceNodesGen::FateBacklash == 935);
  CHECK(skills::PhantomTranceNodesGen::TimeReversal == 954);
  CHECK(skills::PhantomTranceNodesGen::FullFocus == 955);
  CHECK(skills::PhantomTranceNodesGen::SkyThunder == 972);
  CHECK(skills::PhantomTranceNodesGen::OverloadShield == 973);
  CHECK(skills::PhantomTranceNodesGen::ClearMind == 974);
  CHECK(skills::PhantomTranceNodesGen::Longevity == 975);
  CHECK(skills::PhantomTranceNodesGen::CycloneBurst == 976);
  CHECK(skills::PhantomTranceNodesGen::DeathDefiance == 977);
  CHECK(skills::PhantomTranceNodesGen::BloodRebirth == 978);
  CHECK(skills::PhantomTranceNodesGen::ShadowArmor == 979);
  CHECK(skills::PhantomTranceNodesGen::VoidBody == 980);
  CHECK(skills::PhantomTranceNodesGen::ReverseMeridian == 981);
  CHECK(skills::PhantomTranceNodesGen::DesperateGambit == 982);
  CHECK(skills::PhantomTranceNodesGen::DeathSpiral == 983);
  CHECK(skills::PhantomTranceNodesGen::Bloodthirst == 984);
  CHECK(skills::PhantomTranceNodesGen::BlinkStrike == 985);
  CHECK(skills::PhantomTranceNodesGen::GroundShrink == 986);
  CHECK(skills::PhantomTranceNodesGen::IntentFollowsSpirit == 987);
  CHECK(skills::PhantomTranceNodesGen::SwordShadow == 988);
  CHECK(skills::PhantomTranceNodesGen::SnowVeil == 989);
  CHECK(skills::PhantomTranceNodesGen::WinterEnchant == 990);
  CHECK(skills::PhantomTranceNodesGen::MindPierce == 991);
  CHECK(skills::PhantomTranceNodesGen::SpiritFeedback == 992);
  CHECK(skills::PhantomTranceNodesGen::ShadowEcho == 993);

  const std::array<skills::SpecPointBinding<State>, 0> expectedPoints{};
  const std::array<skills::SpecFlagBinding<State>, 29> expectedFlags{{
      {902, &State::lightAsSwallow},
      {913, &State::spiritFlowPierce},
      {914, &State::voidRealmGift},
      {934, &State::swordFollowsMind},
      {935, &State::fateBacklash},
      {954, &State::timeReversal},
      {955, &State::fullFocus},
      {972, &State::skyThunder},
      {973, &State::overloadShield},
      {974, &State::clearMind},
      {975, &State::longevity},
      {976, &State::cycloneBurst},
      {977, &State::deathDefiance},
      {978, &State::bloodRebirth},
      {979, &State::shadowArmor},
      {980, &State::voidBody},
      {981, &State::reverseMeridian},
      {982, &State::desperateGambit},
      {983, &State::deathSpiral},
      {984, &State::bloodthirst},
      {985, &State::blinkStrike},
      {986, &State::groundShrink},
      {987, &State::intentFollowsSpirit},
      {988, &State::swordShadow},
      {989, &State::snowVeil},
      {990, &State::winterEnchant},
      {991, &State::mindPierce},
      {992, &State::spiritFeedback},
      {993, &State::shadowEcho}}};
  CheckGeneratedMatchesExpected<State>(
      9, skills::kPhantomTranceTableGen, expectedPoints, expectedFlags);
}

} // namespace NoMoreDay
