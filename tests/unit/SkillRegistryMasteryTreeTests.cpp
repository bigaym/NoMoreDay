#include "TestCommon.hpp"

#include "game/data/SkillRegistry.hpp"

namespace NoMoreDay {

TEST_CASE("[Unit] Skill Registry - mastery trees expose stable mastery identity") {
  auto &registry = SkillRegistry::Get();
  registry.LoadFromJson("assets/data/skills.json");

  const SkillTreeDefinition *mastery_tree = registry.GetSkillTree(10);
  REQUIRE(mastery_tree != nullptr);

  CHECK(mastery_tree->mastery_id == BladeMasteryId::SwordSaint);

  const SkillTreeDefinition *regular_tree = registry.GetSkillTree(1);
  REQUIRE(regular_tree != nullptr);

  CHECK(regular_tree->mastery_id == BladeMasteryId::None);
}

TEST_CASE("[Unit] Skill Registry - mastery tree identity survives JSON round-trip") {
  SkillTreeDefinition tree;
  tree.skill_id = 10;
  tree.mastery_id = BladeMasteryId::SwordSaint;

  const nlohmann::json serialized = tree;
  const SkillTreeDefinition restored = serialized.get<SkillTreeDefinition>();

  CHECK(restored.skill_id == 10);
  CHECK(restored.mastery_id == BladeMasteryId::SwordSaint);
}

TEST_CASE("[Unit] Skill Registry - Blood Sea specialization preview data matches runtime-authored values") {
  auto &registry = SkillRegistry::Get();
  registry.LoadFromJson("assets/data/skills.json");

  const SkillData *bloodSea = registry.GetSkill(12);
  REQUIRE(bloodSea != nullptr);
  CHECK(bloodSea->GetParam("field_duration", 0.0f) > 0.0f);

  const SkillTreeDefinition *tree = registry.GetSkillTree(12);
  REQUIRE(tree != nullptr);

  const TalentNode &openingNode = tree->nodes.at(1200);
  CHECK(openingNode.stat_modifiers.empty());
  REQUIRE(openingNode.display_lines.size() == 1);
  CHECK(openingNode.display_lines[0].label == "范围");
  CHECK(openingNode.display_lines[0].per_point == doctest::Approx(8.0f));
  CHECK_FALSE(openingNode.display_lines[0].is_percent);
  CHECK(openingNode.desc_key.find("8/16/24/32") != std::string::npos);

  const TalentNode &lingeringNode = tree->nodes.at(1219);
  CHECK(lingeringNode.stat_modifiers.empty());
  REQUIRE(lingeringNode.display_lines.size() == 1);
  CHECK(lingeringNode.display_lines[0].label == "持续时间");
  CHECK(lingeringNode.display_lines[0].per_point == doctest::Approx(0.6f));
  CHECK_FALSE(lingeringNode.display_lines[0].is_percent);
  CHECK(lingeringNode.desc_key.find("0.6/1.2/1.8") != std::string::npos);
}

} // namespace NoMoreDay
