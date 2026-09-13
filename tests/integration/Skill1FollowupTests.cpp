// 技能 1（流云刺）后续跟进计划的集成回归用例。
// 覆盖 RD-08：节点 153 饮血刃仅对流云刺（技能1）造成的流血按实际结算伤害的
// 100% 治疗施加者；血海/回旋/引导等其他来源施加的流血不回血。
// 本用例不加载 skill_mechanics.json，专门验证 AilmentEngine 代码内的默认兜底
// 比例已为 100%（数据侧由 skill_mechanics.json 的 lifesteal_ratio 提供）。
#include "TestCommon.hpp"

#include "game/foundation/components/AIComponent.hpp"
#include "game/foundation/components/Buff.hpp"
#include "game/foundation/components/Combat.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/systems/combat/AilmentEngine.hpp"

#include <unordered_map>

namespace NoMoreDay {
namespace {

constexpr uint32_t kSkillId = 1;
constexpr uint32_t kBloodDrinkerNode = 153;

entt::entity MakePlayer(entt::registry &registry, float x, float y,
                        const std::unordered_map<uint32_t, int> &nodes) {
  auto player = registry.create();
  registry.emplace<Position>(player, x, y);
  auto &stats = registry.emplace<CombatStats>(player);
  stats.max_health = 100.0f;
  stats.health = 100.0f;
  stats.max_mana = 100.0f;
  stats.mana = 100.0f;
  stats.min_weapon_damage = 40.0f;
  stats.max_weapon_damage = 40.0f;
  stats.effective_dexterity = 50.0f;
  stats.crit_chance = 0.0f;
  stats.crit_damage = 1.5f;
  stats.cached_area_level = 1;
  registry.emplace<HealthComponent>(player, 100.0f, 100.0f);

  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  active.specialized_slots[0].skill_id = kSkillId;
  active.specialized_slots[0].allocated_points = nodes;
  return player;
}

entt::entity MakeEnemy(entt::registry &registry, float x, float y,
                       float health) {
  auto enemy = registry.create();
  registry.emplace<Position>(enemy, x, y);
  registry.emplace<EnemyTag>(enemy);
  auto &stats = registry.emplace<CombatStats>(enemy);
  stats.max_health = health;
  stats.health = health;
  stats.crit_chance = 0.0f;
  stats.crit_damage = 1.5f;
  stats.cached_area_level = 1;
  registry.emplace<HealthComponent>(enemy, health, health);
  return enemy;
}

// 153 饮血刃：流云刺（技能1）造成的流血 DoT tick 实际结算后，按 100% 治疗施加者。
TEST_CASE("[Integration] Skill1Followup 153 Blood Drinker heals 100% of skill1 bleed tick") {
  TestSetupScope scope;

  auto &ailments = systems::AilmentRegistry::Get();
  ailments.ResetForTests();
  REQUIRE(ailments.EnsureLoaded());

  entt::registry registry;

  // 施加者分配了 153，血量留出足够空间以避免 max_health 封顶。
  auto player = MakePlayer(registry, 0.0f, 0.0f, {{kBloodDrinkerNode, 1}});
  auto *playerStats = registry.try_get<CombatStats>(player);
  REQUIRE(playerStats != nullptr);
  playerStats->health = 20.0f;
  registry.get<HealthComponent>(player).current = 20.0f;

  auto enemy = MakeEnemy(registry, 100.0f, 0.0f, 100.0f);
  systems::AilmentApplyRequest bleed;
  bleed.ailment = AilmentType::Bleed;
  bleed.source = player;
  bleed.magnitude = 10.0f;
  bleed.duration = 4.0f;
  bleed.stacks = 1;
  // RD-08：标记来源为流云刺（技能1），该流血才参与 153 吸血结算。
  bleed.source_skill_id = 1;
  REQUIRE(systems::AilmentApplier::Apply(registry, enemy, bleed));

  const float playerBefore = playerStats->health;
  const float enemyBefore = registry.get<HealthComponent>(enemy).current;

  systems::AilmentTickDriver::Tick(registry, 10.0f);

  const float healAmount = playerStats->health - playerBefore;
  const float enemyDamage = enemyBefore - registry.get<HealthComponent>(enemy).current;

  REQUIRE(enemyDamage > 0.0f);
  // 治疗量 = 实际流血伤害 × 100%
  CHECK(healAmount == doctest::Approx(enemyDamage));
}

// RD-08：非流云刺来源施加的流血不回血——即使施加者分配了 153。
TEST_CASE("[Integration] Skill1Followup 153 Blood Drinker ignores non-skill1 bleed") {
  TestSetupScope scope;

  auto &ailments = systems::AilmentRegistry::Get();
  ailments.ResetForTests();
  REQUIRE(ailments.EnsureLoaded());

  entt::registry registry;

  auto player = MakePlayer(registry, 0.0f, 0.0f, {{kBloodDrinkerNode, 1}});
  auto *playerStats = registry.try_get<CombatStats>(player);
  REQUIRE(playerStats != nullptr);
  playerStats->health = 20.0f;
  registry.get<HealthComponent>(player).current = 20.0f;

  auto enemy = MakeEnemy(registry, 100.0f, 0.0f, 100.0f);

  // 来源技能保持默认 0（无归属），模拟血海/回旋/引导等非流云刺来源。
  systems::AilmentApplyRequest bleed;
  bleed.ailment = AilmentType::Bleed;
  bleed.source = player;
  bleed.magnitude = 10.0f;
  bleed.duration = 4.0f;
  bleed.stacks = 1;
  REQUIRE(systems::AilmentApplier::Apply(registry, enemy, bleed));

  const float playerBefore = playerStats->health;
  const float enemyBefore = registry.get<HealthComponent>(enemy).current;

  systems::AilmentTickDriver::Tick(registry, 10.0f);

  REQUIRE(enemyBefore - registry.get<HealthComponent>(enemy).current > 0.0f);
  CHECK((playerStats->health - playerBefore) == doctest::Approx(0.0f));

  // 显式非 1 的来源技能同样零回血，确认门控是精确匹配。
  systems::AilmentApplyRequest otherBleed;
  otherBleed.ailment = AilmentType::Bleed;
  otherBleed.source = player;
  otherBleed.magnitude = 10.0f;
  otherBleed.duration = 4.0f;
  otherBleed.stacks = 1;
  otherBleed.source_skill_id = 999;
  REQUIRE(systems::AilmentApplier::Apply(registry, enemy, otherBleed));

  const float playerBefore2 = playerStats->health;
  const float enemyBefore2 = registry.get<HealthComponent>(enemy).current;
  systems::AilmentTickDriver::Tick(registry, 10.0f);

  REQUIRE(enemyBefore2 - registry.get<HealthComponent>(enemy).current > 0.0f);
  CHECK((playerStats->health - playerBefore2) == doctest::Approx(0.0f));
}

} // namespace
} // namespace NoMoreDay
