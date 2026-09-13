// 技能 10（七星斩）收尾回归测试（计划 A2 / 新-1）。
//
// 覆盖两条收尾要求：
// 1) 原 getModifier 的 trigger 系数已单源到 skill_mechanics.json，且数值与技能契约一致；
// 2) slash_count 机制键驱动普通形态 7 次斩击、星落形态固定 4 次。
// 完整施法链路与其余节点行为由 tests/functional/SkillBehaviors.cpp 与
// tests/functional/SevenStarSlashNodes.cpp 覆盖，本文件聚焦单源迁移回归。

#include "TestCommon.hpp"

#include "game/contracts/impl/CombatEventDispatcher.hpp"
#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/AdvancedAffixComponents.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/SkillPointAccess.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillMechanicsRegistry.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/skill/behaviors/SkillBehaviorRegistry.hpp"
#include "game/systems/skill/behaviors/SevenStarSlashShared.hpp"

#include <cstdint>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
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

entt::entity CreateTarget(entt::registry &registry, Vector2 position,
                          float health = 1000.0f) {
  const entt::entity target = registry.create();
  registry.emplace<Position>(target, position.x, position.y);
  registry.emplace<EnemyTag>(target);
  auto &combat = registry.emplace<CombatStats>(target);
  combat.max_health = health;
  combat.health = health;
  registry.emplace<HealthComponent>(target, health, health);
  return target;
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

// 事件处理器 RAII 守卫：SUBCASE 内断言失败会提前离开作用域，析构统一注销，
// 避免处理器残留到后续用例（与 SkillBehaviors.cpp 同型）。
class CombatEventHandlerScope {
public:
  CombatEventHandlerScope(CombatEventType type,
                          CombatEventDispatcher::Handler handler)
      : type_(type), id_(CombatEventDispatcher::Register(type, handler)) {}
  ~CombatEventHandlerScope() {
    if (id_ != 0u) {
      CombatEventDispatcher::Unregister(type_, id_);
    }
  }
  CombatEventHandlerScope(const CombatEventHandlerScope &) = delete;
  CombatEventHandlerScope &operator=(const CombatEventHandlerScope &) = delete;

private:
  CombatEventType type_;
  uint32_t id_ = 0u;
};

} // namespace

TEST_CASE("[Functional] Skill 10 - migrated trigger coefficients are single-sourced") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  std::ifstream mechanics_file("assets/data/skill_mechanics.json", std::ios::binary);
  REQUIRE(mechanics_file.is_open());
  nlohmann::json mechanics;
  mechanics_file >> mechanics;
  REQUIRE(mechanics.contains("10"));
  const auto &skill_json = mechanics.at("10");

  struct MigratedCoefficient {
    uint32_t node;
    const char *key;
    bool is_range_mult;
  };
  // 迁移后的系数：node/key 必须与 SevenStarSlash.cpp 生产读点一致。
  const MigratedCoefficient kCoefficients[] = {
      {Nodes::TargetLock, "acquisition_radius_per_point", true},
      {Nodes::StarScarFollow, "hit_radius_mult", true},
      {Nodes::StarScarFollow, "follow_up_damage_ratio", false},
      {Nodes::FinalSlash, "final_slash_damage_per_point", false},
      {Nodes::ExposedWeakness, "exposed_weakness_damage_per_point", false},
      {Nodes::PoJun, "po_jun_damage_per_point", false},
      {Nodes::SolitaryStar, "solitary_star_damage_per_point", false},
      {Nodes::ZhanJiang, "zhan_jiang_crit_damage_per_point", false},
      {Nodes::FlowReturn, "flow_return_chance_per_point", false},
  };

  for (const auto &coeff : kCoefficients) {
    const auto *contract = SkillRegistry::Get().GetNodeContract(kSkillId, coeff.node);
    REQUIRE(contract != nullptr);
    // 迁移前 getModifier 的来源即 trigger.effectiveness / trigger.range_mult。
    const float trigger_value =
        coeff.is_range_mult ? contract->trigger.range_mult : contract->trigger.effectiveness;

    const std::string node_key = std::to_string(coeff.node);
    REQUIRE_MESSAGE(skill_json.contains(node_key),
                    ("skill_mechanics.json missing node " + node_key).c_str());
    const auto &node_json = skill_json.at(node_key);
    REQUIRE_MESSAGE(
        node_json.contains(coeff.key),
        ("skill_mechanics.json missing key 10/" + node_key + "/" + coeff.key).c_str());
    // 机制表落值必须与技能契约数值一致（数值等价）。
    CHECK(node_json.at(coeff.key).get<float>() == doctest::Approx(trigger_value));
    // 有效读点值也必须等于契约来源（键缺失时由默认值兜底，仍须等价）。
    CHECK(skills::GetMech(kSkillId, coeff.node, coeff.key, trigger_value) ==
          doctest::Approx(trigger_value));
  }

  // slash_count 迁移到技能级节点 0（惯例同 InfiniteBlades 的 0 号节点）。
  REQUIRE(skill_json.contains("0"));
  REQUIRE(skill_json.at("0").contains("slash_count"));
  CHECK(skill_json.at("0").at("slash_count").get<float>() == doctest::Approx(7.0f));
  CHECK(skills::GetMech(kSkillId, 0, "slash_count", 7.0f) == doctest::Approx(7.0f));
}

TEST_CASE("[Functional] Skill 10 - slash_count drives normal and starfall slash counts") {
  TestSetupScope setup;
  EnsureSkillMechanics();

  SUBCASE("normal form uses mechanics slash_count (7 slashes)") {
    entt::registry registry;
    const entt::entity owner = CreateSpecOwner(registry);
    // 高血量目标：确保 7/4 段斩击期间目标不被击杀，命中事件数只反映斩击段数。
    (void)CreateTarget(registry, {124.0f, 100.0f}, 1.0e9f);

    int damage_events = 0;
    CombatEventHandlerScope hit_scope(
        CombatEventType::OnDealDamage,
        [&](entt::registry &, const CombatEvent &evt) {
          if (evt.skill_id == kSkillId) {
            ++damage_events;
          }
        });

    CastSevenStarSlash(registry, owner, {124.0f, 100.0f});
    // 普通形态 7 段，每段命中单一目标 1 次。
    CHECK(damage_events == 7);
  }

  SUBCASE("starfall form keeps four slashes regardless of slash_count") {
    entt::registry registry;
    const entt::entity owner = CreateSpecOwner(registry);
    SetTransmuter(registry, owner, Nodes::Starfall);
    // 高血量目标：确保 7/4 段斩击期间目标不被击杀，命中事件数只反映斩击段数。
    (void)CreateTarget(registry, {124.0f, 100.0f}, 1.0e9f);

    int damage_events = 0;
    CombatEventHandlerScope hit_scope(
        CombatEventType::OnDealDamage,
        [&](entt::registry &, const CombatEvent &evt) {
          if (evt.skill_id == kSkillId) {
            ++damage_events;
          }
        });

    CastSevenStarSlash(registry, owner, {124.0f, 100.0f});
    // 星落形态固定 4 段（3 环绕 + 1 收束），不读 slash_count。
    CHECK(damage_events == 4);
  }
}

} // namespace NoMoreDay
