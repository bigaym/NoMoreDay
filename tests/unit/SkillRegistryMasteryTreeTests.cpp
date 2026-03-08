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

} // namespace NoMoreDay
