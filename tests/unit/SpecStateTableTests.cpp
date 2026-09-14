// A-01 Phase 2 单测：SpecStateTable<State> 模板（D3=b）与手写信封的语义等价，
// 以及技能 8（御剑·回旋）生成式信封的逐节点读点一致性。
//
// 与 SpecStateMappingTests.cpp 的分工：
//   - 后者校验技能 10「生产绑定表 vs 独立期望映射表」；
//   - 本文件校验「泛型模板 ResolveSpecState<State> == 技能 10 手写 ResolveSpecState」，
//     并用生产绑定表转成模板绑定，避免再手抄一份成员名。
//
// 覆盖：模板 point/flag 填充顺序、首槽位胜出、transmuter 不入表（表循环外叠加
//       由各技能自理，模板侧保持缺省）、缺槽位返回 POD 缺省值、技能 8 信封
//       逐节点 == 运行期 ReadPoints。行为侧施法测试见 BladeBoomerangCatchTests。
// 注：技能 8 生产已切换为生成表 `kBladeBoomerangTableGen`（M-1 收口），不再有手写信封。

#include "TestCommon.hpp"

#include "game/foundation/components/SkillDefs.hpp"
#include "game/systems/skill/SpecStateTable.hpp"
#include "game/systems/skill/behaviors/BloodSea.hpp"
#include "game/systems/skill/behaviors/HeavenlySwordDescent.hpp"
#include "game/systems/skill/behaviors/SevenStarSlashShared.hpp"
#include "game/systems/skill/behaviors/generated/BladeBoomerangSpecState.gen.hpp"

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace NoMoreDay {
namespace {

using skills::BladeBoomerangSpecStateGen;
using skills::SevenStarSlashSpecState;
namespace BoomNodes = skills::BladeBoomerangNodesGen;
namespace Nodes = skills::SevenStarSlashNodes;

constexpr uint32_t kSkill8 = 8;
constexpr uint32_t kSkill9 = 9;
constexpr uint32_t kSkill10 = skills::seven_star_shared::kSevenStarSlashSkillId;

// 技能 10 全成员清单（与 SpecStateMappingTests.cpp 一致，用于逐字段对拍）。
const std::array<int SevenStarSlashSpecState::*, 17> kSkill10IntMembers = {{
    &SevenStarSlashSpecState::targetLockPoints,
    &SevenStarSlashSpecState::critChancePoints,
    &SevenStarSlashSpecState::finalSlashPoints,
    &SevenStarSlashSpecState::quickStarPoints,
    &SevenStarSlashSpecState::exposedWeaknessPoints,
    &SevenStarSlashSpecState::poJunPoints,
    &SevenStarSlashSpecState::zhanJiangPoints,
    &SevenStarSlashSpecState::solitaryStarPoints,
    &SevenStarSlashSpecState::flowReturnPoints,
    &SevenStarSlashSpecState::revolvingEdgePoints,
    &SevenStarSlashSpecState::chaseStepPoints,
    &SevenStarSlashSpecState::voidTreadPoints,
    &SevenStarSlashSpecState::starVeilPoints,
    &SevenStarSlashSpecState::gateOfLifePoints,
    &SevenStarSlashSpecState::lingeringScarPoints,
    &SevenStarSlashSpecState::shatteredConstellationPoints,
    &SevenStarSlashSpecState::scarRuinPoints,
}};

const std::array<bool SevenStarSlashSpecState::*, 8> kSkill10BoolMembers = {{
    &SevenStarSlashSpecState::sevenFocus,
    &SevenStarSlashSpecState::starScarFollow,
    &SevenStarSlashSpecState::endlessSeven,
    &SevenStarSlashSpecState::fallingStarSwitch,
    &SevenStarSlashSpecState::swordStepMirage,
    &SevenStarSlashSpecState::poleStarOrbit,
    &SevenStarSlashSpecState::starfall,
    &SevenStarSlashSpecState::returningStep,
}};

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

// 由生产绑定表构造模板绑定：只做 {node, member} 搬运，成员指针由编译器校验。
skills::SpecStateTable<SevenStarSlashSpecState> MakeSkill10Table() {
  static const auto points = [] {
    std::vector<skills::SpecPointBinding<SevenStarSlashSpecState>> result;
    result.reserve(skills::kSevenStarSlashPointBindings.size());
    for (const auto &binding : skills::kSevenStarSlashPointBindings) {
      result.push_back({binding.node, binding.points});
    }
    return result;
  }();
  static const auto flags = [] {
    std::vector<skills::SpecFlagBinding<SevenStarSlashSpecState>> result;
    result.reserve(skills::kSevenStarSlashFlagBindings.size());
    for (const auto &binding : skills::kSevenStarSlashFlagBindings) {
      result.push_back({binding.node, binding.flag});
    }
    return result;
  }();
  return skills::SpecStateTable<SevenStarSlashSpecState>{points, flags};
}

// 通用对拍：把某技能的生产绑定表搬进模板，逐绑定与手写解析器结果比较。
// 只对拍点数/点亮字段；机制系数由手写解析器额外填充，模板不承载，故不比较。
template <typename State, typename PointBindings, typename FlagBindings,
          typename HandWrittenResolver>
void CheckTemplateAgainstHandWritten(uint32_t skillId,
                                     const PointBindings &pointBindings,
                                     const FlagBindings &flagBindings,
                                     HandWrittenResolver resolveHandWritten) {
  std::vector<skills::SpecPointBinding<State>> points;
  std::vector<skills::SpecFlagBinding<State>> flags;
  points.reserve(pointBindings.size());
  flags.reserve(flagBindings.size());
  for (const auto &binding : pointBindings) {
    points.push_back({binding.node, binding.points});
  }
  for (const auto &binding : flagBindings) {
    flags.push_back({binding.node, binding.flag});
  }
  const skills::SpecStateTable<State> table{points, flags};

  entt::registry registry;
  const entt::entity owner = registry.create();
  // 每个点数节点取各不相同的正值，每个点亮节点取 1，另加一个未知节点。
  std::vector<std::pair<uint32_t, int>> allocated;
  for (size_t i = 0; i < points.size(); ++i) {
    allocated.emplace_back(points[i].node, static_cast<int>(i + 1));
  }
  for (const auto &binding : flags) {
    allocated.emplace_back(binding.node, 1);
  }
  allocated.emplace_back(999999, 3);
  SetSpecialization(registry, owner, skillId, allocated);

  const State handWritten = resolveHandWritten(registry, owner);
  const State viaTemplate =
      skills::ResolveSpecState(registry, owner, skillId, table);
  for (const auto &binding : pointBindings) {
    CAPTURE(binding.node);
    CHECK((viaTemplate.*(binding.points)) == (handWritten.*(binding.points)));
  }
  for (const auto &binding : flagBindings) {
    CAPTURE(binding.node);
    CHECK((viaTemplate.*(binding.flag)) == (handWritten.*(binding.flag)));
  }
}

} // namespace

TEST_CASE("[Unit] Skill SpecStateTable - template matches hand-written skill 10 resolver") {
  const auto table = MakeSkill10Table();

  const std::vector<std::vector<std::pair<uint32_t, int>>> cases = {
      {},
      {{Nodes::TargetLock, 3}},
      {{Nodes::CritChance, 5}, {Nodes::FinalSlash, 2}},
      // 转质节点（1021/1022）不入模板表：两侧都应保持缺省 false。
      {{Nodes::PoleStarOrbit, 1}, {Nodes::Starfall, 1}},
      {{999, 4}}, // 未知节点被忽略
  };

  for (size_t caseIndex = 0; caseIndex < cases.size(); ++caseIndex) {
    entt::registry registry;
    const entt::entity owner = registry.create();
    SetSpecialization(registry, owner, kSkill10, cases[caseIndex], /*slot=*/0);
    // 异技能槽位在前：两侧都必须跳过，读槽位 1 的技能 10。
    SetSpecialization(registry, owner, kSkill9, {{Nodes::TargetLock, 9}}, /*slot=*/1);

    const SevenStarSlashSpecState handWritten = skills::ResolveSpecState(registry, owner);
    const SevenStarSlashSpecState viaTemplate =
        skills::ResolveSpecState(registry, owner, kSkill10, table);

    for (const auto member : kSkill10IntMembers) {
      CAPTURE(caseIndex);
      CHECK((viaTemplate.*member) == (handWritten.*member));
    }
    for (const auto member : kSkill10BoolMembers) {
      CAPTURE(caseIndex);
      CHECK((viaTemplate.*member) == (handWritten.*member));
    }
  }

  // 直接断言：模板表逐节点 == 运行期 ReadPoints（D-A8 手写黄金样本形态）。
  entt::registry registry;
  const entt::entity owner = registry.create();
  std::vector<std::pair<uint32_t, int>> allocated;
  for (size_t i = 0; i < skills::kSevenStarSlashPointBindings.size(); ++i) {
    allocated.emplace_back(skills::kSevenStarSlashPointBindings[i].node,
                           static_cast<int>(i + 1));
  }
  for (const auto &binding : skills::kSevenStarSlashFlagBindings) {
    allocated.emplace_back(binding.node, 1);
  }
  SetSpecialization(registry, owner, kSkill10, allocated);

  const auto &spec = registry.get<ActiveSkillsComponent>(owner).specialized_slots[0];
  const SevenStarSlashSpecState single = skills::ResolveSpecState(registry, owner, kSkill10, table);
  for (size_t i = 0; i < skills::kSevenStarSlashPointBindings.size(); ++i) {
    const auto &binding = skills::kSevenStarSlashPointBindings[i];
    CAPTURE(binding.node);
    CHECK((single.*(binding.points)) == skills::ReadPoints(spec, binding.node));
  }
  for (const auto &binding : skills::kSevenStarSlashFlagBindings) {
    CAPTURE(binding.node);
    CHECK((single.*(binding.flag)) == skills::HasNode(spec, binding.node));
  }
}

TEST_CASE("[Unit] Skill SpecStateTable - skill 8 table matches runtime ReadPoints") {
  // 信封节点集合固定：新增/删除绑定须显式改测试，防漂移。
  REQUIRE(skills::kBladeBoomerangPointBindingsGen.size() == 5);
  CHECK(skills::kBladeBoomerangTableGen.points.size() == 5);
  CHECK(skills::kBladeBoomerangTableGen.flags.empty());

  const std::array<uint32_t, 5> boundNodes = {
      BoomNodes::Sharpness, BoomNodes::Bleed, BoomNodes::SonicBoom,
      BoomNodes::IntentGain, BoomNodes::Giant};
  for (const uint32_t expectedNode : boundNodes) {
    bool found = false;
    for (const auto &binding : skills::kBladeBoomerangPointBindingsGen) {
      found = found || binding.node == expectedNode;
    }
    CAPTURE(expectedNode);
    CHECK(found);
  }

  for (const uint32_t node : boundNodes) {
    for (const int pts : {0, 5}) {
      entt::registry registry;
      const entt::entity owner = registry.create();
      SetSpecialization(registry, owner, kSkill8, {{node, pts}});

      const auto &spec = registry.get<ActiveSkillsComponent>(owner).specialized_slots[0];
      const BladeBoomerangSpecStateGen state =
          skills::ResolveSpecState(registry, owner, kSkill8, skills::kBladeBoomerangTableGen);

      const auto expected = [&](uint32_t other) { return other == node ? pts : 0; };
      CAPTURE(node, pts);
      CHECK(state.sharpnessPoints == expected(BoomNodes::Sharpness));
      CHECK(state.bleedPoints == expected(BoomNodes::Bleed));
      CHECK(state.sonicBoomPoints == expected(BoomNodes::SonicBoom));
      CHECK(state.intentGainPoints == expected(BoomNodes::IntentGain));
      CHECK(state.giantPoints == expected(BoomNodes::Giant));

      // 逐字段与运行期 ReadPoints 对齐（模板不应做 clamp/换算）。
      CHECK(state.sharpnessPoints == skills::ReadPoints(spec, BoomNodes::Sharpness));
      CHECK(state.bleedPoints == skills::ReadPoints(spec, BoomNodes::Bleed));
      CHECK(state.sonicBoomPoints == skills::ReadPoints(spec, BoomNodes::SonicBoom));
      CHECK(state.intentGainPoints == skills::ReadPoints(spec, BoomNodes::IntentGain));
      CHECK(state.giantPoints == skills::ReadPoints(spec, BoomNodes::Giant));
    }
  }
}

TEST_CASE("[Unit] Skill SpecStateTable - skill 8 slot selection and absent component") {
  SUBCASE("absent component yields default state") {
    entt::registry registry;
    const entt::entity owner = registry.create();

    const BladeBoomerangSpecStateGen state =
        skills::ResolveSpecState(registry, owner, kSkill8, skills::kBladeBoomerangTableGen);
    CHECK(state.sharpnessPoints == 0);
    CHECK(state.bleedPoints == 0);
    CHECK(state.sonicBoomPoints == 0);
    CHECK(state.intentGainPoints == 0);
    CHECK(state.giantPoints == 0);
  }

  SUBCASE("only the skill-8 slot is read") {
    entt::registry registry;
    const entt::entity owner = registry.create();
    SetSpecialization(registry, owner, kSkill9, {{BoomNodes::Bleed, 4}}, /*slot=*/0);
    SetSpecialization(registry, owner, kSkill8, {{BoomNodes::SonicBoom, 3}}, /*slot=*/1);

    const BladeBoomerangSpecStateGen state =
        skills::ResolveSpecState(registry, owner, kSkill8, skills::kBladeBoomerangTableGen);
    CHECK(state.bleedPoints == 0);
    CHECK(state.sonicBoomPoints == 3);
  }

  SUBCASE("first matching skill-8 slot wins") {
    entt::registry registry;
    const entt::entity owner = registry.create();
    SetSpecialization(registry, owner, kSkill8, {{BoomNodes::Bleed, 2}}, /*slot=*/0);
    SetSpecialization(registry, owner, kSkill8, {{BoomNodes::Bleed, 7}}, /*slot=*/1);

    const BladeBoomerangSpecStateGen state =
        skills::ResolveSpecState(registry, owner, kSkill8, skills::kBladeBoomerangTableGen);
    CHECK(state.bleedPoints == 2);
  }
}

TEST_CASE("[Unit] Skill SpecStateTable - template matches hand-written skill 11 resolver") {
  CheckTemplateAgainstHandWritten<skills::HeavenlySwordCastSpec>(
      skills::kHeavenlySwordSkillId, skills::kHeavenlySwordPointBindings,
      skills::kHeavenlySwordFlagBindings,
      [](const entt::registry &registry, entt::entity owner) {
        return skills::ResolveHeavenlySwordCastSpec(registry, owner);
      });
}

TEST_CASE("[Unit] Skill SpecStateTable - template matches hand-written skill 12 resolver") {
  CheckTemplateAgainstHandWritten<skills::BloodSeaCastSpec>(
      skills::kBloodSeaSkillId, skills::kBloodSeaPointBindings,
      skills::kBloodSeaFlagBindings,
      [](const entt::registry &registry, entt::entity owner) {
        return skills::ResolveBloodSeaCastSpec(registry, owner);
      });
}

} // namespace NoMoreDay
