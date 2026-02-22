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

TEST_CASE("[Unit] SkillVfxEvent - Recipe Smoke Coverage Base Forms Core Events") {
  const std::filesystem::path recipePath =
      std::filesystem::path("assets") / "data" / "vfx" / "blade_ascendant_v3.json";
  std::ifstream input(recipePath);
  REQUIRE(input.good());

  const nlohmann::json root = nlohmann::json::parse(input);
  REQUIRE(root.contains("recipes"));
  REQUIRE(root["recipes"].is_array());

  const std::array<std::pair<std::string, int>, 8> eventMap = {{
      {"CastStart", 0},
      {"CastImpact", 1},
      {"TriggerProc", 2},
      {"EmpoweredConsume", 3},
      {"BuffEnter", 4},
      {"BuffExit", 5},
      {"TransmuterSwitch", 6},
      {"KeystoneActivate", 7},
  }};

  auto hasEventRecipe = [&](const int skillId, const std::string &eventName) {
    for (const auto &recipe : root["recipes"]) {
      if (!recipe.is_object() || !recipe.contains("selector")) {
        continue;
      }
      const auto &selector = recipe["selector"];
      if (!selector.is_object()) {
        continue;
      }
      if (!selector.contains("skillId") || !selector["skillId"].is_number_integer() ||
          selector["skillId"].get<int>() != skillId) {
        continue;
      }
      if (!selector.contains("eventType")) {
        continue;
      }
      const auto &eventType = selector["eventType"];
      if (eventType.is_string() && eventType.get<std::string>() == eventName) {
        return true;
      }
      if (eventType.is_number_integer()) {
        const int eventId = eventType.get<int>();
        for (const auto &[name, id] : eventMap) {
          if (name == eventName && id == eventId) {
            return true;
          }
        }
      }
    }
    return false;
  };

  const std::array<std::pair<int, std::array<const char *, 5>>, 9> coverage = {{
      {1, {"CastStart", "CastImpact", "TriggerProc", "EmpoweredConsume", ""}},
      {2, {"CastImpact", "TriggerProc", "", "", ""}},
      {3, {"CastStart", "CastImpact", "TriggerProc", "BuffEnter", ""}},
      {4, {"CastStart", "CastImpact", "TriggerProc", "BuffEnter", ""}},
      {5, {"CastStart", "CastImpact", "EmpoweredConsume", "", ""}},
      {6, {"CastStart", "TriggerProc", "BuffEnter", "", ""}},
      {7, {"CastImpact", "TriggerProc", "", "", ""}},
      {8, {"CastStart", "CastImpact", "TriggerProc", "BuffExit", ""}},
      {9, {"CastStart", "CastImpact", "TriggerProc", "BuffEnter", "BuffExit"}},
  }};

  for (const auto &[skillId, events] : coverage) {
    for (const char *eventName : events) {
      if (eventName[0] == '\0') {
        continue;
      }
      INFO("missing recipe for skillId=" << skillId << " event=" << eventName);
      CHECK(hasEventRecipe(skillId, eventName));
    }
  }
}

TEST_CASE("[Unit] SkillVfxEvent - Recipe Coverage Transmutation Elements") {
  const std::filesystem::path recipePath =
      std::filesystem::path("assets") / "data" / "vfx" / "blade_ascendant_v3.json";
  std::ifstream input(recipePath);
  REQUIRE(input.good());

  const nlohmann::json root = nlohmann::json::parse(input);
  REQUIRE(root.contains("recipes"));
  REQUIRE(root["recipes"].is_array());

  auto hasElementRecipe = [&](const int skillId, const int elementType) {
    for (const auto &recipe : root["recipes"]) {
      if (!recipe.is_object() || !recipe.contains("selector")) {
        continue;
      }
      const auto &selector = recipe["selector"];
      if (!selector.is_object()) {
        continue;
      }
      if (!selector.contains("skillId") || !selector["skillId"].is_number_integer() ||
          selector["skillId"].get<int>() != skillId) {
        continue;
      }
      if (!selector.contains("elementType") ||
          !selector["elementType"].is_number_integer()) {
        continue;
      }
      if (selector["elementType"].get<int>() == elementType) {
        return true;
      }
    }
    return false;
  };

  for (int skillId = 1; skillId <= 9; ++skillId) {
    INFO("missing fire transmutation recipe for skillId=" << skillId);
    CHECK(hasElementRecipe(skillId, 1));
    INFO("missing cold transmutation recipe for skillId=" << skillId);
    CHECK(hasElementRecipe(skillId, 2));
  }
}
