#include "doctest.h"

#include "game/components/SkillVfxEvent.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <string>

TEST_CASE("[Unit] SkillVfxEvent - Enum Value Stability") {
  CHECK(static_cast<uint8_t>(NoMoreDay::SkillVfxEventType::CastStart) == 0u);
  CHECK(static_cast<uint8_t>(NoMoreDay::SkillVfxEventType::CastImpact) == 1u);
  CHECK(static_cast<uint8_t>(NoMoreDay::SkillVfxEventType::TriggerProc) == 2u);
  CHECK(static_cast<uint8_t>(NoMoreDay::SkillVfxEventType::EmpoweredConsume) == 3u);
  CHECK(static_cast<uint8_t>(NoMoreDay::SkillVfxEventType::BuffEnter) == 4u);
  CHECK(static_cast<uint8_t>(NoMoreDay::SkillVfxEventType::BuffExit) == 5u);
  CHECK(static_cast<uint8_t>(NoMoreDay::SkillVfxEventType::TransmuterSwitch) == 6u);
  CHECK(static_cast<uint8_t>(NoMoreDay::SkillVfxEventType::KeystoneActivate) == 7u);

  CHECK(static_cast<uint8_t>(NoMoreDay::SkillVfxElementType::Physical) == 0u);
  CHECK(static_cast<uint8_t>(NoMoreDay::SkillVfxElementType::Fire) == 1u);
  CHECK(static_cast<uint8_t>(NoMoreDay::SkillVfxElementType::Cold) == 2u);
  CHECK(static_cast<uint8_t>(NoMoreDay::SkillVfxElementType::Lightning) == 3u);
  CHECK(static_cast<uint8_t>(NoMoreDay::SkillVfxElementType::Void) == 4u);

  CHECK(static_cast<uint8_t>(NoMoreDay::SkillVfxResistDebuffType::None) == 0u);
  CHECK(static_cast<uint8_t>(NoMoreDay::SkillVfxResistDebuffType::TypeA) == 1u);
  CHECK(static_cast<uint8_t>(NoMoreDay::SkillVfxResistDebuffType::TypeB) == 2u);
  CHECK(static_cast<uint8_t>(NoMoreDay::SkillVfxResistDebuffType::TypeC) == 3u);
  CHECK(static_cast<uint8_t>(NoMoreDay::SkillVfxResistDebuffType::TypeD) == 4u);
  CHECK(static_cast<uint8_t>(NoMoreDay::SkillVfxResistDebuffType::TypeE) == 5u);
}

TEST_CASE("[Unit] SkillVfxEvent - Default Compatibility Values") {
  const NoMoreDay::SkillVfxEvent event = {};

  CHECK(event.skillId == 0u);
  CHECK(event.castId == 0u);
  CHECK(event.type == NoMoreDay::SkillVfxEventType::CastStart);
  CHECK(event.nodeRoleMask == NoMoreDay::SkillVfxNodeRoleMask::None);
  CHECK(event.qualityTier == 1u);
  CHECK(event.intensity == doctest::Approx(1.0f));
  CHECK(event.elementType ==
        static_cast<uint8_t>(NoMoreDay::SkillVfxElementType::Physical));
  CHECK(event.resistDebuffType ==
        static_cast<uint8_t>(NoMoreDay::SkillVfxResistDebuffType::None));
}

TEST_CASE("[Unit] SkillVfxEvent - Role Mask Bits") {
  uint32_t mask = NoMoreDay::SkillVfxNodeRoleMask::None;
  mask |= NoMoreDay::SkillVfxNodeRoleMask::Keystone;
  mask |= NoMoreDay::SkillVfxNodeRoleMask::Transmuter;

  CHECK(NoMoreDay::HasSkillVfxNodeRole(mask, NoMoreDay::SkillVfxNodeRoleMask::Keystone));
  CHECK(NoMoreDay::HasSkillVfxNodeRole(mask, NoMoreDay::SkillVfxNodeRoleMask::Transmuter));
  CHECK(!NoMoreDay::HasSkillVfxNodeRole(mask, NoMoreDay::SkillVfxNodeRoleMask::Trigger));
}

TEST_CASE("[Unit] SkillVfxEvent - Recipe Smoke Coverage Skill1 Core Events") {
  const std::filesystem::path recipePath =
      std::filesystem::path("assets") / "data" / "vfx" / "blade_ascendant_v3.json";
  std::ifstream input(recipePath);
  REQUIRE(input.good());

  const nlohmann::json root = nlohmann::json::parse(input);
  REQUIRE(root.contains("recipes"));
  REQUIRE(root["recipes"].is_array());

  auto hasEventRecipe = [&](const std::string &eventName) {
    for (const auto &recipe : root["recipes"]) {
      if (!recipe.is_object() || !recipe.contains("selector")) {
        continue;
      }
      const auto &selector = recipe["selector"];
      if (!selector.is_object()) {
        continue;
      }
      if (!selector.contains("skillId") || selector["skillId"] != 1) {
        continue;
      }
      if (!selector.contains("eventType") || !selector["eventType"].is_string()) {
        continue;
      }
      if (selector["eventType"].get<std::string>() == eventName) {
        return true;
      }
    }
    return false;
  };

  CHECK(hasEventRecipe("CastStart"));
  CHECK(hasEventRecipe("CastImpact"));
  CHECK(hasEventRecipe("TriggerProc"));
  CHECK(hasEventRecipe("EmpoweredConsume"));
}
