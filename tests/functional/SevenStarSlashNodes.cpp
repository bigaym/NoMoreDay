// 技能 10（七星斩）节点功能测试（计划 A2-1）。
//
// 覆盖：生产 ResolveSpecState 的点读/点亮/转质映射，以及 VoidTread 节点经
// SkillMechanicsRegistry 落值的施法行为（证明外置系数接线）。完整施法链路与
// 其他节点行为已由 tests/functional/SkillBehaviors.cpp 与
// tests/unit/SkillBehaviorGuardTests.cpp 覆盖，本文件聚焦 A2-1 抽象信封。

#include "TestCommon.hpp"

#include "game/foundation/components/AdvancedAffixComponents.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include "game/systems/skill/behaviors/SevenStarSlashShared.hpp"

#include <cstdint>
#include <unordered_map>

namespace NoMoreDay {
namespace {

constexpr uint32_t kSkillId = skills::seven_star_shared::kSevenStarSlashSkillId;
namespace Nodes = skills::SevenStarSlashNodes;

void EnsureSkillMechanics() {
  data::SkillMechanicsRegistry::Get().ResetForTests();
  REQUIRE(data::SkillMechanicsRegistry::Get().LoadFromFile(
      "assets/data/skill_mechanics.json"));
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  SkillBehaviorRegistry::Initialize();
}

entt::entity CreateSpecOwner(
    entt::registry &registry,
    const std::unordered_map<uint32_t, int> &allocated_nodes = {}) {
  const entt::entity owner = registry.create();
  registry.emplace<Position>(owner, 100.0f, 100.0f);
  registry.emplace<HealthComponent>(owner, 1000.0f, 1000.0f);

  auto &stats = registry.emplace<CombatStats>(owner);
  stats.max_health = 1000.0f;
  stats.health = 1000.0f;
  stats.max_mana = 200.0f;
  stats.mana = 200.0f;
  stats.min_weapon_damage = 50.0f;
  stats.max_weapon_damage = 50.0f;

  auto &active = registry.emplace<ActiveSkillsComponent>(owner);
  active.slots[0].id = kSkillId;
  auto &spec = active.specialized_slots[0];
  spec.skill_id = kSkillId;
  spec.allocated_points = allocated_nodes;
  return owner;
}

void SetTransmuter(entt::registry &registry, entt::entity owner, uint32_t node) {
  auto &runtime = registry.get_or_emplace<SkillContractRuntimeComponent>(owner);
  runtime.active_transmuter_node_by_skill[kSkillId] = node;
}

void CastSevenStarSlash(entt::registry &registry, entt::entity owner,
                        Vector2 target_pos) {
  SkillExecution exec{};
  exec.skill_id = kSkillId;
  exec.owner = owner;
  exec.target_pos = target_pos;

  auto cast = SkillBehaviorRegistry::GetCast(kSkillId);
  REQUIRE(cast != nullptr);
  cast(registry, owner, exec);
}

} // namespace

TEST_CASE("[Functional] Skill 10 - SpecState resolves point/flag nodes and transmuters") {
  entt::registry registry;
  const entt::entity owner = CreateSpecOwner(
      registry, {{Nodes::TargetLock, 2}, {Nodes::CritChance, 3},
                 {Nodes::FinalSlash, 4}, {Nodes::VoidTread, 5},
                 {Nodes::SevenFocus, 1}, {Nodes::StarScarFollow, 1},
                 {Nodes::ReturningStep, 1}});

  const skills::SevenStarSlashSpecState state =
      skills::ResolveSpecState(registry, owner);

  CHECK(state.targetLockPoints == 2);
  CHECK(state.critChancePoints == 3);
  CHECK(state.finalSlashPoints == 4);
  CHECK(state.voidTreadPoints == 5);
  CHECK(state.sevenFocus == true);
  CHECK(state.starScarFollow == true);
  CHECK(state.returningStep == true);
  // 未分配节点保持默认。
  CHECK(state.quickStarPoints == 0);
  CHECK(state.endlessSeven == false);
  CHECK(state.poleStarOrbit == false);
  CHECK(state.starfall == false);

  SetTransmuter(registry, owner, Nodes::PoleStarOrbit);
  const skills::SevenStarSlashSpecState orbit =
      skills::ResolveSpecState(registry, owner);
  CHECK(orbit.poleStarOrbit == true);
  CHECK(orbit.starfall == false);

  SetTransmuter(registry, owner, Nodes::Starfall);
  const skills::SevenStarSlashSpecState starfall =
      skills::ResolveSpecState(registry, owner);
  CHECK(starfall.poleStarOrbit == false);
  CHECK(starfall.starfall == true);
}

TEST_CASE("[Functional] Skill 10 - VoidTread node scales cast invulnerability duration") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  SUBCASE("allocated VoidTread uses mechanics per-point coefficient") {
    entt::registry registry;
    const entt::entity owner = CreateSpecOwner(registry, {{Nodes::VoidTread, 4}});
    CastSevenStarSlash(registry, owner, {100.0f, 100.0f});

    const auto *invulnerable = registry.try_get<InvulnerableComponent>(owner);
    REQUIRE(invulnerable != nullptr);
    // 节点 1015（踏虚）清理 stat_modifiers 后仅保留 SKILL_DURATION_FLAT：基础 0.5s
    // + 每点 0.03s * 4 = 0.62s。闪避修饰符已随数据清理移除，故本行为层用例不含
    // 闪避断言（DodgeChance 不经技能行为路径）。
    CHECK(invulnerable->duration == doctest::Approx(0.62f));
  }

  SUBCASE("no VoidTread keeps base invulnerability duration") {
    entt::registry registry;
    const entt::entity owner = CreateSpecOwner(registry);
    CastSevenStarSlash(registry, owner, {100.0f, 100.0f});

    const auto *invulnerable = registry.try_get<InvulnerableComponent>(owner);
    REQUIRE(invulnerable != nullptr);
    CHECK(invulnerable->duration == doctest::Approx(0.5f));
  }
}

} // namespace NoMoreDay
