// 技能 10（七星斩）SpecState 映射单测（计划 A2-1 / 设计 §4.7 DoD#7）。
//
// 迁移后 SevenStarSlashSpecState、节点常量与 ResolveSpecState 均由生产头
// SevenStarSlashShared.hpp 导出，本文件不再镜像解析逻辑：直接调用生产
// ResolveSpecState，并用测试侧独立的「期望映射表」与生产绑定表互校，
// 从而在任一侧漂移时失败（旧手写镜像无法检出生产漂移）。
//
// 覆盖：points 点读语义、HasNode 点亮语义（ReadPoints > 0）、转质节点映射、
//       技能/槽位筛选与未知节点忽略。施法行为测试见 SkillBehaviorGuardTests /
//       SkillBehaviors / SevenStarSlashNodes.cpp。

#include "TestCommon.hpp"

#include "game/foundation/components/SkillDefs.hpp"
#include "game/systems/skill/behaviors/SevenStarSlashShared.hpp"

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace NoMoreDay {
namespace {

constexpr uint32_t kSkillId = skills::seven_star_shared::kSevenStarSlashSkillId;
using skills::SevenStarSlashSpecState;
namespace Nodes = skills::SevenStarSlashNodes;

// 测试侧独立期望表：node id -> SpecState 成员。与生产生成表
// kSevenStarSlashPointBindingsGen / kSevenStarSlashFlagBindingsGen 相互校验。
struct ExpectedPointMapping {
  uint32_t node;
  const char *label;
  int SevenStarSlashSpecState::*member;
};

struct ExpectedFlagMapping {
  uint32_t node;
  const char *label;
  bool SevenStarSlashSpecState::*member;
};

const std::array<ExpectedPointMapping, 17> kExpectedPoints = {{
    {Nodes::TargetLock, "TargetLock", &SevenStarSlashSpecState::targetLockPoints},
    {Nodes::CritChance, "CritChance", &SevenStarSlashSpecState::critChancePoints},
    {Nodes::FinalSlash, "FinalSlash", &SevenStarSlashSpecState::finalSlashPoints},
    {Nodes::QuickStar, "QuickStar", &SevenStarSlashSpecState::quickStarPoints},
    {Nodes::ExposedWeakness, "ExposedWeakness",
     &SevenStarSlashSpecState::exposedWeaknessPoints},
    {Nodes::PoJun, "PoJun", &SevenStarSlashSpecState::poJunPoints},
    {Nodes::ZhanJiang, "ZhanJiang", &SevenStarSlashSpecState::zhanJiangPoints},
    {Nodes::SolitaryStar, "SolitaryStar",
     &SevenStarSlashSpecState::solitaryStarPoints},
    {Nodes::FlowReturn, "FlowReturn", &SevenStarSlashSpecState::flowReturnPoints},
    {Nodes::RevolvingEdge, "RevolvingEdge",
     &SevenStarSlashSpecState::revolvingEdgePoints},
    {Nodes::ChaseStep, "ChaseStep", &SevenStarSlashSpecState::chaseStepPoints},
    {Nodes::VoidTread, "VoidTread", &SevenStarSlashSpecState::voidTreadPoints},
    {Nodes::StarVeil, "StarVeil", &SevenStarSlashSpecState::starVeilPoints},
    {Nodes::GateOfLife, "GateOfLife",
     &SevenStarSlashSpecState::gateOfLifePoints},
    {Nodes::LingeringScar, "LingeringScar",
     &SevenStarSlashSpecState::lingeringScarPoints},
    {Nodes::ShatteredConstellation, "ShatteredConstellation",
     &SevenStarSlashSpecState::shatteredConstellationPoints},
    {Nodes::ScarRuin, "ScarRuin", &SevenStarSlashSpecState::scarRuinPoints},
}};

const std::array<ExpectedFlagMapping, 6> kExpectedFlags = {{
    {Nodes::SevenFocus, "SevenFocus", &SevenStarSlashSpecState::sevenFocus},
    {Nodes::StarScarFollow, "StarScarFollow",
     &SevenStarSlashSpecState::starScarFollow},
    {Nodes::EndlessSeven, "EndlessSeven", &SevenStarSlashSpecState::endlessSeven},
    {Nodes::FallingStarSwitch, "FallingStarSwitch",
     &SevenStarSlashSpecState::fallingStarSwitch},
    {Nodes::SwordStepMirage, "SwordStepMirage",
     &SevenStarSlashSpecState::swordStepMirage},
    {Nodes::ReturningStep, "ReturningStep",
     &SevenStarSlashSpecState::returningStep},
}};

// 转质节点 1021/1022 在生成表中同样登记为 flag 绑定，但取值由运行时
// active_transmuter_node 覆盖（见 ResolveSpecState 尾部），并非由 allocated_points
// 点亮，因此单列期望表，避免与「投入点即点亮」语义混淆。
const std::array<ExpectedFlagMapping, 2> kExpectedTransmuterFlags = {{
    {Nodes::PoleStarOrbit, "PoleStarOrbit",
     &SevenStarSlashSpecState::poleStarOrbit},
    {Nodes::Starfall, "Starfall", &SevenStarSlashSpecState::starfall},
}};

// 全部成员（用于「仅目标字段被写入」的穷举检查）。
const std::array<int SevenStarSlashSpecState::*, 17> kAllIntMembers = {{
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

const std::array<bool SevenStarSlashSpecState::*, 8> kAllBoolMembers = {{
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

void SetTransmuter(entt::registry &registry, entt::entity owner, uint32_t node) {
  auto &runtime = registry.get_or_emplace<SkillContractRuntimeComponent>(owner);
  runtime.active_transmuter_node_by_skill[kSkillId] = node;
}

} // namespace

TEST_CASE("[Unit] Skill SpecStateMapping - production tables match expected mapping") {
  REQUIRE(skills::kSevenStarSlashPointBindingsGen.size() ==
          kExpectedPoints.size());
  REQUIRE(skills::kSevenStarSlashFlagBindingsGen.size() ==
          kExpectedFlags.size() + kExpectedTransmuterFlags.size());

  // 期望表中的每个 node/成员都必须存在于生产绑定表。
  size_t matched_points = 0;
  for (const auto &expected : kExpectedPoints) {
    for (const auto &binding : skills::kSevenStarSlashPointBindingsGen) {
      if (binding.node == expected.node &&
          binding.points == expected.member) {
        ++matched_points;
      }
    }
  }
  CHECK(matched_points == kExpectedPoints.size());

  size_t matched_flags = 0;
  for (const auto &expected : kExpectedFlags) {
    for (const auto &binding : skills::kSevenStarSlashFlagBindingsGen) {
      if (binding.node == expected.node && binding.flag == expected.member) {
        ++matched_flags;
      }
    }
  }
  for (const auto &expected : kExpectedTransmuterFlags) {
    for (const auto &binding : skills::kSevenStarSlashFlagBindingsGen) {
      if (binding.node == expected.node && binding.flag == expected.member) {
        ++matched_flags;
      }
    }
  }
  CHECK(matched_flags ==
        kExpectedFlags.size() + kExpectedTransmuterFlags.size());

  // 每个被消费的 node id 在表内不重复。
  std::vector<uint32_t> node_ids;
  node_ids.reserve(kExpectedPoints.size() + kExpectedFlags.size());
  for (const auto &binding : skills::kSevenStarSlashPointBindingsGen) {
    node_ids.push_back(binding.node);
  }
  for (const auto &binding : skills::kSevenStarSlashFlagBindingsGen) {
    node_ids.push_back(binding.node);
  }
  for (size_t i = 0; i < node_ids.size(); ++i) {
    for (size_t j = i + 1; j < node_ids.size(); ++j) {
      CAPTURE(node_ids[i], node_ids[j]);
      CHECK(node_ids[i] != node_ids[j]);
    }
  }
}

TEST_CASE("[Unit] Skill SpecStateMapping - all mapped nodes on (full table)") {
  entt::registry registry;
  const entt::entity owner = registry.create();

  std::vector<std::pair<uint32_t, int>> allocated;
  for (size_t i = 0; i < kExpectedPoints.size(); ++i) {
    // 每个 points 节点使用互不相同的点数，以便检出字段错位。
    allocated.emplace_back(kExpectedPoints[i].node, static_cast<int>(i + 1));
  }
  for (const auto &mapping : kExpectedFlags) {
    allocated.emplace_back(mapping.node, 1);
  }
  SetSpecialization(registry, owner, kSkillId, allocated);

  const SevenStarSlashSpecState state = skills::ResolveSpecState(registry, owner);

  for (size_t i = 0; i < kExpectedPoints.size(); ++i) {
    const auto &mapping = kExpectedPoints[i];
    const int actual = state.*(mapping.member);
    CAPTURE(mapping.label, mapping.node);
    CHECK(actual == static_cast<int>(i + 1));
  }
  for (const auto &mapping : kExpectedFlags) {
    const bool actual = state.*(mapping.member);
    CAPTURE(mapping.label, mapping.node);
    CHECK(actual == true);
  }
  // 转质标志位来自运行时选择，未设置时保持关闭。
  CHECK(state.poleStarOrbit == false);
  CHECK(state.starfall == false);
}

TEST_CASE("[Unit] Skill SpecStateMapping - empty allocation is all off") {
  entt::registry registry;
  const entt::entity owner = registry.create();
  SetSpecialization(registry, owner, kSkillId, {});

  const SevenStarSlashSpecState state = skills::ResolveSpecState(registry, owner);

  for (const auto member : kAllIntMembers) {
    CHECK(state.*member == 0);
  }
  for (const auto member : kAllBoolMembers) {
    CHECK(state.*member == false);
  }
}

TEST_CASE("[Unit] Skill SpecStateMapping - absent ActiveSkillsComponent is all off") {
  entt::registry registry;
  const entt::entity owner = registry.create();

  const SevenStarSlashSpecState state = skills::ResolveSpecState(registry, owner);

  for (const auto member : kAllIntMembers) {
    CHECK(state.*member == 0);
  }
  for (const auto member : kAllBoolMembers) {
    CHECK(state.*member == false);
  }
}

TEST_CASE("[Unit] Skill SpecStateMapping - points fields preserve 0 / non-zero value") {
  for (const auto &mapping : kExpectedPoints) {
    for (const int points : {0, 7}) {
      entt::registry registry;
      const entt::entity owner = registry.create();
      SetSpecialization(registry, owner, kSkillId, {{mapping.node, points}});

      const SevenStarSlashSpecState state =
          skills::ResolveSpecState(registry, owner);

      const int actual = state.*(mapping.member);
      CAPTURE(mapping.label, mapping.node, points);
      CHECK(actual == points);
      // 只有该字段被写入，其余整数字段保持默认。
      for (const auto member : kAllIntMembers) {
        if (member == mapping.member) {
          continue;
        }
        CHECK(state.*member == 0);
      }
    }
  }
}

TEST_CASE("[Unit] Skill SpecStateMapping - flag fields follow ReadPoints > 0") {
  // 生产点亮语义为 HasNode = ReadPoints > 0：0 点视为未点亮。
  for (const auto &mapping : kExpectedFlags) {
    for (const int points : {0, 1}) {
      entt::registry registry;
      const entt::entity owner = registry.create();
      SetSpecialization(registry, owner, kSkillId, {{mapping.node, points}});

      const SevenStarSlashSpecState state =
          skills::ResolveSpecState(registry, owner);

      const bool actual = state.*(mapping.member);
      CAPTURE(mapping.label, mapping.node, points);
      CHECK(actual == (points > 0));
      for (const auto member : kAllIntMembers) {
        CHECK(state.*member == 0);
      }
    }
  }
}

TEST_CASE("[Unit] Skill SpecStateMapping - transmuter node selects orbit / starfall") {
  const auto resolve_with_transmuter = [](uint32_t transmuter) {
    entt::registry registry;
    const entt::entity owner = registry.create();
    SetSpecialization(registry, owner, kSkillId, {});
    if (transmuter != 0) {
      SetTransmuter(registry, owner, transmuter);
    }
    return skills::ResolveSpecState(registry, owner);
  };

  const SevenStarSlashSpecState orbit = resolve_with_transmuter(Nodes::PoleStarOrbit);
  CHECK(orbit.poleStarOrbit == true);
  CHECK(orbit.starfall == false);

  const SevenStarSlashSpecState starfall = resolve_with_transmuter(Nodes::Starfall);
  CHECK(starfall.poleStarOrbit == false);
  CHECK(starfall.starfall == true);

  const SevenStarSlashSpecState none = resolve_with_transmuter(0);
  CHECK(none.poleStarOrbit == false);
  CHECK(none.starfall == false);

  const SevenStarSlashSpecState other = resolve_with_transmuter(9999);
  CHECK(other.poleStarOrbit == false);
  CHECK(other.starfall == false);

  // 1021/1022 作为 allocated_points 出现时不影响转质标志位（转质仅由运行时决定）。
  entt::registry registry;
  const entt::entity owner = registry.create();
  SetSpecialization(registry, owner, kSkillId,
                    {{Nodes::PoleStarOrbit, 1}, {Nodes::Starfall, 1}});
  const SevenStarSlashSpecState allocated_only =
      skills::ResolveSpecState(registry, owner);
  CHECK(allocated_only.poleStarOrbit == false);
  CHECK(allocated_only.starfall == false);
}

TEST_CASE("[Unit] Skill SpecStateMapping - skill id filter and slot selection") {
  SUBCASE("only the skill-10 slot is read") {
    entt::registry registry;
    const entt::entity owner = registry.create();
    SetSpecialization(registry, owner, 9, {{Nodes::TargetLock, 5}}, /*slot=*/0);
    SetSpecialization(registry, owner, kSkillId, {{Nodes::CritChance, 3}},
                      /*slot=*/1);

    const SevenStarSlashSpecState state =
        skills::ResolveSpecState(registry, owner);

    CHECK(state.targetLockPoints == 0);
    CHECK(state.critChancePoints == 3);
  }

  SUBCASE("first matching skill-10 slot wins") {
    entt::registry registry;
    const entt::entity owner = registry.create();
    SetSpecialization(registry, owner, kSkillId, {{Nodes::TargetLock, 2}},
                      /*slot=*/0);
    SetSpecialization(registry, owner, kSkillId, {{Nodes::TargetLock, 9}},
                      /*slot=*/1);

    const SevenStarSlashSpecState state =
        skills::ResolveSpecState(registry, owner);

    CHECK(state.targetLockPoints == 2);
  }

  SUBCASE("transmuter keyed by another skill id is ignored") {
    entt::registry registry;
    const entt::entity owner = registry.create();
    SetSpecialization(registry, owner, kSkillId, {});
    auto &runtime = registry.emplace<SkillContractRuntimeComponent>(owner);
    runtime.active_transmuter_node_by_skill[9] = Nodes::PoleStarOrbit;

    const SevenStarSlashSpecState state =
        skills::ResolveSpecState(registry, owner);

    CHECK(state.poleStarOrbit == false);
    CHECK(state.starfall == false);
  }
}

TEST_CASE("[Unit] Skill SpecStateMapping - unknown or unmapped nodes are ignored") {
  entt::registry registry;
  const entt::entity owner = registry.create();
  // 999 / 2000 非技能 10 节点；1021 / 1022 是转质节点而非字段映射节点。
  SetSpecialization(registry, owner, kSkillId,
                    {{999, 1}, {2000, 7}, {Nodes::PoleStarOrbit, 4}, {Nodes::Starfall, 4}});

  const SevenStarSlashSpecState state = skills::ResolveSpecState(registry, owner);

  for (const auto member : kAllIntMembers) {
    CHECK(state.*member == 0);
  }
  for (const auto member : kAllBoolMembers) {
    CHECK(state.*member == false);
  }
}

} // namespace NoMoreDay
