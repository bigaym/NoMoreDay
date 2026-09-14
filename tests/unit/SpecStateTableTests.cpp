// A-01 Phase 2 单测：SpecStateTable<State> 模板（D3=b）的填表语义，以及
// 技能 8/10/11/12 生成式信封逐节点与运行期 ReadPoints / HasNode 的一致性。
//
// 与 SpecStateMappingTests.cpp 的分工：
//   - 后者校验技能 10「生产绑定表 vs 独立期望映射表」；
//   - 本文件校验「泛型模板 ResolveSpecState<State> 的解析值 == 运行期 ReadPoints /
//     HasNode」（L-2 直接断言，不依赖任何手写绑定表），并覆盖技能 8 的槽位选择。
//
// 覆盖：模板 point/flag 填充顺序、首槽位胜出、转质节点按 descriptor flag 占位、
//       缺槽位返回 POD 缺省值、技能 8/10/11/12 信封逐节点 == 运行期 ReadPoints /
//       HasNode。行为侧施法测试见 BladeBoomerangCatchTests。
// 注：技能 8/10/11/12 生产已切换为生成表（Track A-02），不再有手写绑定表。

#include "TestCommon.hpp"

#include "game/foundation/components/SkillDefs.hpp"
#include "game/systems/skill/SpecStateTable.hpp"
#include "game/systems/skill/behaviors/BloodSea.hpp"
#include "game/systems/skill/behaviors/HeavenlySwordDescent.hpp"
#include "game/systems/skill/behaviors/SevenStarSlashShared.hpp"
#include "game/systems/skill/behaviors/generated/BladeBoomerangSpecState.gen.hpp"
#include "game/systems/skill/behaviors/generated/BloodSeaSpecState.gen.hpp"
#include "game/systems/skill/behaviors/generated/HeavenlySwordDescentSpecState.gen.hpp"
#include "game/systems/skill/behaviors/generated/SevenStarSlashSpecState.gen.hpp"

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace NoMoreDay {
namespace {

using skills::BladeBoomerangSpecStateGen;
namespace BoomNodes = skills::BladeBoomerangNodesGen;

constexpr uint32_t kSkill8 = 8;
constexpr uint32_t kSkill9 = 9;
constexpr uint32_t kSkill10 = skills::seven_star_shared::kSevenStarSlashSkillId;

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

// 直接一致性断言（L-2 关闭）：以生成表逐节点分配点数（相异正值）与点亮（1），
// 用 4 参模板解析后，逐绑定断言成员值等于运行期 ReadPoints / HasNode。
// 不依赖任何手写绑定表，故生成表节点集合或成员指针的漂移会在此直接暴露。
template <typename State>
void CheckTableMatchesRuntime(uint32_t skillId,
                              const skills::SpecStateTable<State> &table) {
  entt::registry registry;
  const entt::entity owner = registry.create();
  std::vector<std::pair<uint32_t, int>> allocated;
  allocated.reserve(table.points.size() + table.flags.size());
  for (size_t i = 0; i < table.points.size(); ++i) {
    allocated.emplace_back(table.points[i].node, static_cast<int>(i + 1));
  }
  for (const auto &binding : table.flags) {
    allocated.emplace_back(binding.node, 1);
  }
  SetSpecialization(registry, owner, skillId, allocated);

  const auto &spec =
      registry.get<ActiveSkillsComponent>(owner).specialized_slots[0];
  const State state = skills::ResolveSpecState(registry, owner, skillId, table);
  for (const auto &binding : table.points) {
    CAPTURE(binding.node);
    CHECK((state.*(binding.points)) == skills::ReadPoints(spec, binding.node));
  }
  for (const auto &binding : table.flags) {
    CAPTURE(binding.node);
    CHECK((state.*(binding.flag)) == skills::HasNode(spec, binding.node));
  }
}

} // namespace

TEST_CASE("[Unit] Skill SpecStateTable - skill 10 table matches runtime ReadPoints") {
  CHECK(skills::kSevenStarSlashTableGen.points.size() == 17);
  CHECK(skills::kSevenStarSlashTableGen.flags.size() == 8);
  // 1021/1022（poleStarOrbit/starfall）现为 descriptor flag 绑定，模板仅按
  // HasNode 占位；「转质激活」语义由 2 参包装函数覆盖，不在本基元职责内。
  CheckTableMatchesRuntime<skills::SevenStarSlashSpecStateGen>(
      kSkill10, skills::kSevenStarSlashTableGen);
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
      CAPTURE(node); CAPTURE(pts);
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

TEST_CASE("[Unit] Skill SpecStateTable - skill 11 table matches runtime ReadPoints") {
  CHECK(skills::kHeavenlySwordDescentTableGen.points.size() == 16);
  CHECK(skills::kHeavenlySwordDescentTableGen.flags.size() == 9);
  CheckTableMatchesRuntime<skills::HeavenlySwordCastSpecGen>(
      skills::kHeavenlySwordSkillId, skills::kHeavenlySwordDescentTableGen);
}

TEST_CASE("[Unit] Skill SpecStateTable - skill 12 table matches runtime ReadPoints") {
  CHECK(skills::kBloodSeaTableGen.points.size() == 18);
  CHECK(skills::kBloodSeaTableGen.flags.size() == 7);
  CheckTableMatchesRuntime<skills::BloodSeaCastSpecGen>(
      skills::kBloodSeaSkillId, skills::kBloodSeaTableGen);
}

} // namespace NoMoreDay
