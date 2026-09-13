// 技能 1（流云刺）后续跟进计划的集成回归用例。
// 覆盖 B2.2/D6：节点 153 饮血刃按流血 DoT 实际结算伤害的 30% 治疗施加者。
// 本用例不加载 skill_mechanics.json，专门验证 AilmentEngine 代码内的默认兜底
// 比例已由 100% 降为 30%（数据侧改动由 skill_mechanics.json 的 lifesteal_ratio 提供）。
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

// 153 饮血刃：流血 DoT tick 实际结算后，按 30%（而非 100%）治疗 DoT 施加者。
TEST_CASE("[Integration] Skill1Followup 153 Blood Drinker heals 30% of bleed tick") {
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
  REQUIRE(systems::AilmentApplier::Apply(registry, enemy, bleed));

  const float playerBefore = playerStats->health;
  const float enemyBefore = registry.get<HealthComponent>(enemy).current;

  systems::AilmentTickDriver::Tick(registry, 10.0f);

  const float healAmount = playerStats->health - playerBefore;
  const float enemyDamage = enemyBefore - registry.get<HealthComponent>(enemy).current;

  REQUIRE(enemyDamage > 0.0f);
  // 治疗量 = 实际流血伤害 × 30%
  CHECK(healAmount == doctest::Approx(enemyDamage * 0.30f));
  // 明确不再是 100% 吸血
  CHECK_FALSE(healAmount == doctest::Approx(enemyDamage));
}

} // namespace
} // namespace NoMoreDay
