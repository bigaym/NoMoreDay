#include "TestCommon.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/ItemStats.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/skill/SkillDisplayPreviewService.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/foundation/stats/AttributePipeline.hpp"
#include <type_traits>

namespace NoMoreDay {

TEST_CASE("[Unit] ItemSkillModifier - Layout and JSON Serialization") {
  static_assert(std::is_standard_layout_v<ItemSkillModifier>);
  static_assert(std::is_standard_layout_v<PayloadDefinition>);
  static_assert(std::is_standard_layout_v<BakedSkillProfile>);

  ItemSkillModifier mod;
  mod.target_skill_id = 1;
  mod.flat_cooldown_delta = -1.2f;
  mod.mana_cost_delta = -4.0f;
  mod.extra_projectiles = 2;
  mod.area_radius_mult = 1.35f;
  mod.convert_from = Tag::Physical;
  mod.convert_to = Tag::Cold;
  mod.conversion_ratio = 1.0f;
  mod.inject_ailment_id = 42;
  mod.inject_ailment_chance = 0.8f;

  nlohmann::json j = mod;
  ItemSkillModifier loaded = j.get<ItemSkillModifier>();

  CHECK(loaded.target_skill_id == 1);
  CHECK(loaded.flat_cooldown_delta == doctest::Approx(-1.2f));
  CHECK(loaded.mana_cost_delta == doctest::Approx(-4.0f));
  CHECK(loaded.extra_projectiles == 2);
  CHECK(loaded.area_radius_mult == doctest::Approx(1.35f));
  CHECK(loaded.convert_from == Tag::Physical);
  CHECK(loaded.convert_to == Tag::Cold);
  CHECK(loaded.conversion_ratio == doctest::Approx(1.0f));
  CHECK(loaded.inject_ailment_id == 42);
  CHECK(loaded.inject_ailment_chance == doctest::Approx(0.8f));
  CHECK(loaded == mod);

  // ItemComponent integration
  ItemComponent item;
  item.id = 1001;
  item.name = "Glacial Greatsword";
  item.skill_modifiers.push_back(mod);

  nlohmann::json itemJson = item;
  ItemComponent loadedItem = itemJson.get<ItemComponent>();
  REQUIRE(loadedItem.skill_modifiers.size() == 1);
  CHECK(loadedItem.skill_modifiers[0] == mod);
}

TEST_CASE("[Unit] ItemSkillModifier - RebakeSkillProfiles and Preview") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;

  const auto player = registry.create();
  registry.emplace<Position>(player, 100.0f, 100.0f);
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  auto &equipment = registry.emplace<EquipmentComponent>(player);
  auto &combatStats = registry.emplace<CombatStats>(player);
  combatStats.min_weapon_damage = 10.0f;
  combatStats.max_weapon_damage = 20.0f;

  // Equip Flowing Thrust (ID 1) in slot 0
  active.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 1};

  const auto *skillData = SkillRegistry::Get().GetSkill(1);
  REQUIRE(skillData != nullptr);

  // Initial rebake without gear
  SkillSystem::RebakeSkillProfiles(registry, player);
  const auto *initialProfile = SkillSystem::GetBakedSkillProfile(registry, player, 1);
  REQUIRE(initialProfile != nullptr);
  CHECK(initialProfile->skill_id == 1);
  CHECK(initialProfile->effective_cooldown == doctest::Approx(skillData->cooldown));
  CHECK(initialProfile->effective_mana_cost == doctest::Approx(skillData->mana_cost));
  CHECK(initialProfile->projectile_count == 1);
  CHECK(initialProfile->injected_count == 0);

  // Create an equipped weapon with ItemSkillModifier
  const auto weapon = registry.create();
  auto &itemComp = registry.emplace<ItemComponent>(weapon);
  itemComp.id = 5001;
  itemComp.name = "Sword of the Frost Piercer";
  itemComp.slot = EquipmentSlot::MainHand;

  ItemSkillModifier mod;
  mod.target_skill_id = 1;
  mod.flat_cooldown_delta = -1.5f;
  mod.mana_cost_delta = -5.0f;
  mod.extra_projectiles = 3;
  mod.area_radius_mult = 1.4f;
  mod.convert_from = Tag::Physical;
  mod.convert_to = Tag::Cold;
  mod.conversion_ratio = 1.0f;
  mod.inject_ailment_id = 77;
  mod.inject_ailment_chance = 0.6f;
  itemComp.skill_modifiers.push_back(mod);

  equipment.Set(EquipmentSlot::MainHand, weapon);

  // Rebake with gear equipped
  SkillSystem::RebakeSkillProfiles(registry, player);
  const auto *modifiedProfile = SkillSystem::GetBakedSkillProfile(registry, player, 1);
  REQUIRE(modifiedProfile != nullptr);
  CHECK(modifiedProfile->effective_cooldown == doctest::Approx(std::max(0.0f, skillData->cooldown - 1.5f)));
  CHECK(modifiedProfile->effective_mana_cost == doctest::Approx(std::max(0.0f, skillData->mana_cost - 5.0f)));
  CHECK(modifiedProfile->projectile_count == 4); // 1 base + 3 extra
  CHECK(modifiedProfile->area_radius == doctest::Approx(1.4f));
  CHECK(HasTag(modifiedProfile->effective_tags, Tag::Cold));
  CHECK_FALSE(HasTag(modifiedProfile->effective_tags, Tag::Physical));
  REQUIRE(modifiedProfile->injected_count == 1);
  CHECK(modifiedProfile->injected_payloads[0].type == PayloadType::Ailment);
  CHECK(modifiedProfile->injected_payloads[0].ailment_id == 77);
  CHECK(modifiedProfile->injected_payloads[0].value_mult == doctest::Approx(0.6f));

  // Verify preview reflects baked profile
  const auto preview = SkillDisplayPreviewService::Build(registry, player, 1);
  CHECK(preview.display_mana_cost == modifiedProfile->effective_mana_cost);
  CHECK(preview.display_cooldown == modifiedProfile->effective_cooldown);
  CHECK(preview.display_projectiles == modifiedProfile->projectile_count);
  CHECK(preview.display_tags == modifiedProfile->effective_tags);

  // Unequip weapon and rebake
  equipment.Unequip(EquipmentSlot::MainHand);
  SkillSystem::RebakeSkillProfiles(registry, player);

  const auto *restoredProfile = SkillSystem::GetBakedSkillProfile(registry, player, 1);
  REQUIRE(restoredProfile != nullptr);
  CHECK(restoredProfile->effective_cooldown == doctest::Approx(skillData->cooldown));
  CHECK(restoredProfile->effective_mana_cost == doctest::Approx(skillData->mana_cost));
  CHECK(restoredProfile->projectile_count == 1);
  CHECK(restoredProfile->injected_count == 0);
  CHECK(HasTag(restoredProfile->effective_tags, Tag::Physical));
}

TEST_CASE("[Unit] ItemSkillModifier - PlusSkillLevelGeneric and PlusAllSkills with AttributePipeline") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");

  entt::registry registry;

  const auto player = registry.create();
  registry.emplace<Position>(player, 0.0f, 0.0f);
  auto &active = registry.emplace<ActiveSkillsComponent>(player);
  auto &equipment = registry.emplace<EquipmentComponent>(player);
  registry.emplace<CombatStats>(player);
  registry.emplace<PrimaryStats>(player);

  active.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 1};
  active.specialized_slots[0] = SpecializedSkill{.skill_id = 1, .bonus_levels = 0};

  // Calculate with no gear
  AttributePipeline::Calculate(registry, player);
  const auto *initialProfile = SkillSystem::GetBakedSkillProfile(registry, player, 1);
  REQUIRE(initialProfile != nullptr);
  CHECK(initialProfile->effective_level == 1);

  // Equip weapon with PlusAllSkills (+2) and PlusSkillLevelGeneric (+3 for Physical)
  const auto weapon = registry.create();
  auto &itemComp = registry.emplace<ItemComponent>(weapon);
  itemComp.id = 6001;
  itemComp.name = "Blade of Mastery";
  itemComp.slot = EquipmentSlot::MainHand;

  Affix allSkillsAffix;
  allSkillsAffix.type = AffixType::PlusAllSkills;
  allSkillsAffix.value = 2.0f;
  itemComp.affixes.push_back(allSkillsAffix);

  Affix genericSkillAffix;
  genericSkillAffix.type = AffixType::PlusSkillLevelGeneric;
  genericSkillAffix.value = 3.0f;
  genericSkillAffix.required_tags = Tag::Physical; // Flowing thrust has physical tag
  itemComp.affixes.push_back(genericSkillAffix);

  equipment.Set(EquipmentSlot::MainHand, weapon);

  // Calculate through pipeline: should apply both affixes and rebake
  AttributePipeline::Calculate(registry, player);

  const auto *bakedProfile = SkillSystem::GetBakedSkillProfile(registry, player, 1);
  REQUIRE(bakedProfile != nullptr);
  // Base 1 + 2 (PlusAllSkills) + 3 (PlusSkillLevelGeneric) = 6!
  CHECK(bakedProfile->effective_level == 6);
}

} // namespace NoMoreDay
