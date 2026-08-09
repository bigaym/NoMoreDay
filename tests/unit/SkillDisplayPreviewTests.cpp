#include "doctest.h"
#include "game/systems/skill/SkillDisplayPreviewService.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Common.hpp"
#include <entt/entt.hpp>

namespace NoMoreDay {

TEST_CASE("[Unit] SkillDisplayPreview - duration preview uses static sources only") {
  entt::registry registry;
  const auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  auto& combat = registry.emplace<CombatStats>(player);
  combat.duration_scale = 1.25f;

  SkillData skill{.id = 990100, .name_key = "preview_skill", .desc_key = "desc",
                  .mana_cost = 0.0f, .cooldown = 0.0f, .base_damage = 40.0f,
                  .weapon_damage_mult = 1.0f};
  skill.params["field_duration"] = 4.0f;
  SkillRegistry::Get().RegisterSkill(skill);

  const auto preview = SkillDisplayPreviewService::Build(registry, player, 990100);
  CHECK(preview.has_duration);
  CHECK(preview.display_duration_seconds == doctest::Approx(5.0f));
}

TEST_CASE("[Unit] SkillDisplayPreview - duration preview with specialization modifiers") {
  entt::registry registry;
  const auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  auto& combat = registry.emplace<CombatStats>(player);
  combat.duration_scale = 1.0f;

  uint32_t skillId = 990101;
  SkillData skill{.id = skillId, .name_key = "preview_skill_spec", .desc_key = "desc",
                  .mana_cost = 0.0f, .cooldown = 0.0f, .base_damage = 40.0f,
                  .weapon_damage_mult = 1.0f};
  skill.params["field_duration"] = 5.0f;
  SkillRegistry::Get().RegisterSkill(skill);

  SkillTreeDefinition tree;
  tree.skill_id = skillId;
  TalentNode node;
  node.id = 1;
  node.max_points = 5;
  node.stat_modifiers.push_back({10.0f, StatType::DurationScale, ModifierMode::PercentAdd});
  tree.nodes[node.id] = node;
  SkillRegistry::Get().RegisterSkillTree(tree);

  auto& active = registry.emplace<ActiveSkillsComponent>(player);
  active.specialized_slots[0].skill_id = skillId;
  active.specialized_slots[0].allocated_points[1] = 2; // +20% duration

  const auto preview = SkillDisplayPreviewService::Build(registry, player, skillId);
  CHECK(preview.has_duration);
  // 5.0 * 1.0 (combat) + 5.0 * 10% * 2 (spec) = 6.0
  CHECK(preview.display_duration_seconds == doctest::Approx(6.0f));
}

TEST_CASE("[Unit] SkillDisplayPreview - duration preview with multiplicative specialization modifiers") {
  entt::registry registry;
  const auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  auto& combat = registry.emplace<CombatStats>(player);
  combat.duration_scale = 1.0f;

  uint32_t skillId = 990103;
  SkillData skill{.id = skillId, .name_key = "preview_skill_spec_mult", .desc_key = "desc",
                  .mana_cost = 0.0f, .cooldown = 0.0f, .base_damage = 40.0f,
                  .weapon_damage_mult = 1.0f};
  skill.params["field_duration"] = 5.0f;
  SkillRegistry::Get().RegisterSkill(skill);

  SkillTreeDefinition tree;
  tree.skill_id = skillId;
  TalentNode node;
  node.id = 1;
  node.max_points = 5;
  node.stat_modifiers.push_back({10.0f, StatType::DurationScale, ModifierMode::PercentMult});
  tree.nodes[node.id] = node;
  SkillRegistry::Get().RegisterSkillTree(tree);

  auto& active = registry.emplace<ActiveSkillsComponent>(player);
  active.specialized_slots[0].skill_id = skillId;
  active.specialized_slots[0].allocated_points[1] = 2; // x1.2 duration

  const auto preview = SkillDisplayPreviewService::Build(registry, player, skillId);
  CHECK(preview.has_duration);
  // 5.0 * 1.0 (combat) * 1.2 (spec) = 6.0
  CHECK(preview.display_duration_seconds == doctest::Approx(6.0f));
}

TEST_CASE("[Unit] SkillDisplayPreview - estimated damage uses average weapon damage") {
  entt::registry registry;
  const auto player = registry.create();
  registry.emplace<PlayerTag>(player);
  auto& combat = registry.emplace<CombatStats>(player);
  combat.min_weapon_damage = 10.0f;
  combat.max_weapon_damage = 20.0f;

  SkillData skill{.id = 990102, .name_key = "preview_skill_dmg", .desc_key = "desc",
                  .mana_cost = 0.0f, .cooldown = 0.0f, .base_damage = 40.0f,
                  .weapon_damage_mult = 1.0f};
  SkillRegistry::Get().RegisterSkill(skill);

  const auto preview = SkillDisplayPreviewService::Build(registry, player, 990102);
  CHECK(preview.has_estimated_damage);
  CHECK(preview.estimated_damage_value == doctest::Approx(15.0f));
  CHECK(preview.estimated_damage_mode == SkillDisplayDamageMode::Hit);
}

} // namespace NoMoreDay
