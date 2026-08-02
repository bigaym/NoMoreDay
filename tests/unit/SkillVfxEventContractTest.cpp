#include "doctest.h"

#include "engine/render/GPUSkillEffectSystem.hpp"
#include "engine/render/SkillVfxEvent.hpp"
#include "game/data/TagRegistry.hpp"
#include "game/systems/skill/SkillSystem.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <cstdint>
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

TEST_CASE("[Unit] SkillVfxEvent - Scalar Element Contract Field") {
  const NoMoreDay::SkillVfxEvent event = {};
  CHECK(sizeof(event.elementType) == 1u); // uint8_t scalar ABI

  NoMoreDay::SkillVfxEvent carrier = {};
  const std::array<std::pair<NoMoreDay::SkillVfxElementType, uint8_t>, 5>
      elementScalars = {{
          {NoMoreDay::SkillVfxElementType::Physical, 0u},
          {NoMoreDay::SkillVfxElementType::Fire, 1u},
          {NoMoreDay::SkillVfxElementType::Cold, 2u},
          {NoMoreDay::SkillVfxElementType::Lightning, 3u},
          {NoMoreDay::SkillVfxElementType::Void, 4u},
      }};
  for (const auto &[element, scalar] : elementScalars) {
    carrier.elementType = static_cast<uint8_t>(element);
    INFO("round-trip scalar=" << static_cast<uint32_t>(scalar));
    CHECK(carrier.elementType == scalar);
  }
}

TEST_CASE("[Unit] SkillVfxEvent - Element Scalar Normalization At Engine "
          "Boundary") {
  using NoMoreDay::NormalizeSkillVfxElementType;
  using E = NoMoreDay::SkillVfxElementType;
  // In-range scalars pass through unchanged.
  CHECK(NormalizeSkillVfxElementType(0u) ==
        static_cast<uint8_t>(E::Physical));
  CHECK(NormalizeSkillVfxElementType(1u) == static_cast<uint8_t>(E::Fire));
  CHECK(NormalizeSkillVfxElementType(2u) == static_cast<uint8_t>(E::Cold));
  CHECK(NormalizeSkillVfxElementType(3u) ==
        static_cast<uint8_t>(E::Lightning));
  CHECK(NormalizeSkillVfxElementType(4u) == static_cast<uint8_t>(E::Void));
  // Out-of-range scalars fall back to Physical (never clamped to Void, never
  // reinterpreted as a tag bit mask).
  CHECK(NormalizeSkillVfxElementType(5u) ==
        static_cast<uint8_t>(E::Physical));
  CHECK(NormalizeSkillVfxElementType(200u) ==
        static_cast<uint8_t>(E::Physical));
  CHECK(NormalizeSkillVfxElementType(0xFFu) ==
        static_cast<uint8_t>(E::Physical));
}

TEST_CASE("[Unit] SkillVfxEvent - Submission Boundary Normalizes Once") {
  NoMoreDay::SkillVfxEvent invalid = {};
  invalid.skillId = 1u;
  invalid.elementType = 0xFFu;

  const NoMoreDay::SkillVfxEvent normalized =
      NoMoreDay::systems::GPUSkillEffectSystem::NormalizeSkillVfxEvent(invalid);

  CHECK(invalid.elementType == 0xFFu);
  CHECK(normalized.elementType ==
        static_cast<uint8_t>(NoMoreDay::SkillVfxElementType::Physical));
  CHECK(normalized.skillId == invalid.skillId);
}

TEST_CASE("[Unit] SkillVfxEvent - Game Tag To Element Priority Contract") {
  using NoMoreDay::Tag;
  using E = NoMoreDay::SkillVfxElementType;

  // Single-element mapping.
  CHECK(NoMoreDay::SkillSystem::EncodeSkillVfxElementType(Tag::Physical) ==
        static_cast<uint8_t>(E::Physical));
  CHECK(NoMoreDay::SkillSystem::EncodeSkillVfxElementType(Tag::Fire) ==
        static_cast<uint8_t>(E::Fire));
  CHECK(NoMoreDay::SkillSystem::EncodeSkillVfxElementType(Tag::Cold) ==
        static_cast<uint8_t>(E::Cold));
  CHECK(NoMoreDay::SkillSystem::EncodeSkillVfxElementType(Tag::Lightning) ==
        static_cast<uint8_t>(E::Lightning));
  CHECK(NoMoreDay::SkillSystem::EncodeSkillVfxElementType(Tag::Void) ==
        static_cast<uint8_t>(E::Void));

  // Multi-element priority: Void > Lightning > Cold > Fire > Physical.
  CHECK(NoMoreDay::SkillSystem::EncodeSkillVfxElementType(Tag::Void | Tag::Fire) ==
        static_cast<uint8_t>(E::Void));
  CHECK(NoMoreDay::SkillSystem::EncodeSkillVfxElementType(Tag::Lightning | Tag::Cold) ==
        static_cast<uint8_t>(E::Lightning));
  CHECK(NoMoreDay::SkillSystem::EncodeSkillVfxElementType(Tag::Cold | Tag::Fire) ==
        static_cast<uint8_t>(E::Cold));
  CHECK(NoMoreDay::SkillSystem::EncodeSkillVfxElementType(Tag::Fire | Tag::Physical) ==
        static_cast<uint8_t>(E::Fire));
  CHECK(NoMoreDay::SkillSystem::EncodeSkillVfxElementType(
            Tag::Void | Tag::Lightning | Tag::Cold | Tag::Fire |
            Tag::Physical) == static_cast<uint8_t>(E::Void));

  // Non-element and high-bit state tags never produce an element.
  CHECK(NoMoreDay::SkillSystem::EncodeSkillVfxElementType(Tag::Melee | Tag::Attack) ==
        static_cast<uint8_t>(E::Physical));
  CHECK(NoMoreDay::SkillSystem::EncodeSkillVfxElementType(Tag::Hit | Tag::Critical) ==
        static_cast<uint8_t>(E::Physical));
  CHECK(NoMoreDay::SkillSystem::EncodeSkillVfxElementType(Tag::Boss) ==
        static_cast<uint8_t>(E::Physical)); // bit 55
  CHECK(NoMoreDay::SkillSystem::EncodeSkillVfxElementType(Tag::Stunned | Tag::Elite) ==
        static_cast<uint8_t>(E::Physical)); // bits 52/54
  CHECK(NoMoreDay::SkillSystem::EncodeSkillVfxElementType(Tag::Void | Tag::Boss) ==
        static_cast<uint8_t>(E::Void));

  // Shadow/Poison have no dedicated VFX recipe -> Physical fallback; an
  // explicit element tag still wins when combined with them.
  CHECK(NoMoreDay::SkillSystem::EncodeSkillVfxElementType(Tag::Shadow) ==
        static_cast<uint8_t>(E::Physical));
  CHECK(NoMoreDay::SkillSystem::EncodeSkillVfxElementType(Tag::Poison) ==
        static_cast<uint8_t>(E::Physical));
  CHECK(NoMoreDay::SkillSystem::EncodeSkillVfxElementType(Tag::Shadow | Tag::Poison) ==
        static_cast<uint8_t>(E::Physical));
  CHECK(NoMoreDay::SkillSystem::EncodeSkillVfxElementType(Tag::Shadow | Tag::Fire) ==
        static_cast<uint8_t>(E::Fire));

  // Transmuter override: a non-Physical transmuter element beats the effective
  // tags; a Physical transmuter does not override.
  CHECK(NoMoreDay::SkillSystem::ResolveSkillVfxElementTypeFromTags(Tag::Fire,
                                                                    Tag::Cold) ==
        static_cast<uint8_t>(E::Cold));
  CHECK(NoMoreDay::SkillSystem::ResolveSkillVfxElementTypeFromTags(Tag::Void,
                                                                    Tag::Cold) ==
        static_cast<uint8_t>(E::Cold));
  CHECK(NoMoreDay::SkillSystem::ResolveSkillVfxElementTypeFromTags(
                Tag::Fire, Tag::Physical) ==
        static_cast<uint8_t>(E::Fire));
  CHECK(NoMoreDay::SkillSystem::ResolveSkillVfxElementTypeFromTags(Tag::Fire,
                                                                    Tag::Shadow) ==
        static_cast<uint8_t>(E::Fire));
}

TEST_CASE("[Unit] SkillVfxEvent - Scalar Is Independent Of Tag Bit Positions") {
  using NoMoreDay::Tag;
  using E = NoMoreDay::SkillVfxElementType;

  // The scalar element ABI is 0..4 and is not a projection of the Tag layout.
  CHECK(static_cast<uint8_t>(E::Void) == 4u);

  // Mixing high-bit state/mechanism tags (bits 32-55) never changes the
  // resolved scalar: the mapping consults named tags semantically.
  const Tag noisy = Tag::Hit | Tag::Boss | Tag::Stunned | Tag::Elite;
  CHECK(NoMoreDay::SkillSystem::EncodeSkillVfxElementType(noisy) ==
        static_cast<uint8_t>(E::Physical));
  CHECK(NoMoreDay::SkillSystem::EncodeSkillVfxElementType(noisy | Tag::Cold) ==
        static_cast<uint8_t>(E::Cold));

  // Shadow/Poison resolve by named tag membership, never by bit adjacency.
  CHECK(NoMoreDay::SkillSystem::EncodeSkillVfxElementType(Tag::Shadow | Tag::Poison) ==
        static_cast<uint8_t>(E::Physical));
}

TEST_CASE("[Unit] SkillVfxEvent - Recipe Element Selectors Use Scalar ABI") {
  const std::filesystem::path recipePath =
      std::filesystem::path("assets") / "data" / "vfx" / "blade_ascendant_v3.json";
  std::ifstream input(recipePath);
  REQUIRE(input.good());

  const nlohmann::json root = nlohmann::json::parse(input);
  REQUIRE(root.contains("recipes"));
  REQUIRE(root["recipes"].is_array());

  int numericSelectors = 0;
  for (const auto &recipe : root["recipes"]) {
    if (!recipe.is_object() || !recipe.contains("selector")) {
      continue;
    }
    const auto &selector = recipe["selector"];
    if (!selector.is_object() || !selector.contains("elementType")) {
      continue;
    }
    const auto &elementType = selector["elementType"];
    if (elementType.is_string()) {
      // Wildcard selector is element-agnostic (Physical-safe).
      CHECK(elementType.get<std::string>() == "*");
      continue;
    }
    REQUIRE(elementType.is_number_integer());
    const int value = elementType.get<int>();
    ++numericSelectors;
    INFO("numeric elementType=" << value);
    CHECK(value >= static_cast<int>(NoMoreDay::SkillVfxElementType::Physical));
    CHECK(value <= static_cast<int>(NoMoreDay::SkillVfxElementType::Void));
    CHECK(NoMoreDay::NormalizeSkillVfxElementType(
              static_cast<uint8_t>(value)) == static_cast<uint8_t>(value));
  }
  // Fire (1) and Cold (2) transmutation selectors are covered separately.
  CHECK(numericSelectors > 0);
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

TEST_CASE("[Unit] SkillVfxEvent - Recipe Coverage Keystone Trigger Synergy") {
  const std::filesystem::path recipePath =
      std::filesystem::path("assets") / "data" / "vfx" / "blade_ascendant_v3.json";
  std::ifstream input(recipePath);
  REQUIRE(input.good());

  const nlohmann::json root = nlohmann::json::parse(input);
  REQUIRE(root.contains("recipes"));
  REQUIRE(root["recipes"].is_array());

  auto normalizeEventName = [](const nlohmann::json &eventType) -> std::string {
    if (eventType.is_string()) {
      return eventType.get<std::string>();
    }
    if (eventType.is_number_integer()) {
      const int eventId = eventType.get<int>();
      if (eventId == static_cast<int>(NoMoreDay::SkillVfxEventType::CastStart)) {
        return "CastStart";
      }
      if (eventId == static_cast<int>(NoMoreDay::SkillVfxEventType::CastImpact)) {
        return "CastImpact";
      }
      if (eventId == static_cast<int>(NoMoreDay::SkillVfxEventType::TriggerProc)) {
        return "TriggerProc";
      }
      if (eventId ==
          static_cast<int>(NoMoreDay::SkillVfxEventType::EmpoweredConsume)) {
        return "EmpoweredConsume";
      }
      if (eventId == static_cast<int>(NoMoreDay::SkillVfxEventType::BuffEnter)) {
        return "BuffEnter";
      }
      if (eventId == static_cast<int>(NoMoreDay::SkillVfxEventType::BuffExit)) {
        return "BuffExit";
      }
      if (eventId ==
          static_cast<int>(NoMoreDay::SkillVfxEventType::TransmuterSwitch)) {
        return "TransmuterSwitch";
      }
      if (eventId ==
          static_cast<int>(NoMoreDay::SkillVfxEventType::KeystoneActivate)) {
        return "KeystoneActivate";
      }
    }
    return {};
  };

  auto normalizeRoleMask = [](const nlohmann::json &selector) -> uint32_t {
    if (!selector.is_object() || !selector.contains("requiredNodeRoleMask")) {
      return NoMoreDay::SkillVfxNodeRoleMask::None;
    }
    const auto &value = selector["requiredNodeRoleMask"];
    if (value.is_number_unsigned()) {
      return value.get<uint32_t>();
    }
    if (!value.is_array()) {
      return NoMoreDay::SkillVfxNodeRoleMask::None;
    }
    uint32_t mask = NoMoreDay::SkillVfxNodeRoleMask::None;
    for (const auto &item : value) {
      if (!item.is_string()) {
        continue;
      }
      const std::string token = item.get<std::string>();
      if (token == "Keystone") {
        mask |= NoMoreDay::SkillVfxNodeRoleMask::Keystone;
      } else if (token == "Trigger") {
        mask |= NoMoreDay::SkillVfxNodeRoleMask::Trigger;
      } else if (token == "Synergy") {
        mask |= NoMoreDay::SkillVfxNodeRoleMask::Synergy;
      } else if (token == "Transmuter") {
        mask |= NoMoreDay::SkillVfxNodeRoleMask::Transmuter;
      }
    }
    return mask;
  };

  int keystoneActivateTemplates = 0;
  bool hasKeystoneSustain = false;
  bool hasTriggerRoleTriggerProc = false;
  bool hasSynergyRoleTriggerProc = false;

  for (const auto &recipe : root["recipes"]) {
    if (!recipe.is_object() || !recipe.contains("selector")) {
      continue;
    }
    const auto &selector = recipe["selector"];
    if (!selector.is_object()) {
      continue;
    }

    const std::string eventName =
        normalizeEventName(selector.value("eventType", nlohmann::json("*")));
    const uint32_t roleMask = normalizeRoleMask(selector);
    const bool isKeystone =
        NoMoreDay::HasSkillVfxNodeRole(roleMask, NoMoreDay::SkillVfxNodeRoleMask::Keystone);
    const bool isTrigger =
        NoMoreDay::HasSkillVfxNodeRole(roleMask, NoMoreDay::SkillVfxNodeRoleMask::Trigger);
    const bool isSynergy =
        NoMoreDay::HasSkillVfxNodeRole(roleMask, NoMoreDay::SkillVfxNodeRoleMask::Synergy);

    if (eventName == "KeystoneActivate" && isKeystone &&
        selector.contains("skillId") && selector["skillId"].is_number_integer()) {
      ++keystoneActivateTemplates;
    }
    if ((eventName == "BuffEnter" || eventName == "BuffExit") && isKeystone) {
      hasKeystoneSustain = true;
    }
    if (eventName == "TriggerProc" && isTrigger) {
      hasTriggerRoleTriggerProc = true;
    }
    if (eventName == "TriggerProc" && isSynergy) {
      hasSynergyRoleTriggerProc = true;
    }
  }

  CHECK(keystoneActivateTemplates >= 2);
  CHECK(hasKeystoneSustain);
  CHECK(hasTriggerRoleTriggerProc);
  CHECK(hasSynergyRoleTriggerProc);
}

TEST_CASE("[Unit] SkillVfxEvent - Recipe Coverage Global Systems") {
  const std::filesystem::path recipePath =
      std::filesystem::path("assets") / "data" / "vfx" / "blade_ascendant_v3.json";
  std::ifstream input(recipePath);
  REQUIRE(input.good());

  const nlohmann::json root = nlohmann::json::parse(input);
  REQUIRE(root.contains("recipes"));
  REQUIRE(root["recipes"].is_array());

  bool hasSkill1BuffEnter = false;
  bool hasSkill1BuffExit = false;
  bool hasResistOverlayAction = false;

  for (const auto &recipe : root["recipes"]) {
    if (!recipe.is_object() || !recipe.contains("selector")) {
      continue;
    }
    const auto &selector = recipe["selector"];
    if (!selector.is_object()) {
      continue;
    }

    const bool isSkill1 =
        selector.contains("skillId") && selector["skillId"].is_number_integer() &&
        selector["skillId"].get<int>() == 1;
    if (isSkill1 && selector.contains("eventType")) {
      const auto &eventType = selector["eventType"];
      if (eventType.is_string()) {
        const std::string eventName = eventType.get<std::string>();
        if (eventName == "BuffEnter") {
          hasSkill1BuffEnter = true;
        } else if (eventName == "BuffExit") {
          hasSkill1BuffExit = true;
        }
      } else if (eventType.is_number_integer()) {
        const int eventId = eventType.get<int>();
        if (eventId == static_cast<int>(NoMoreDay::SkillVfxEventType::BuffEnter)) {
          hasSkill1BuffEnter = true;
        } else if (eventId ==
                   static_cast<int>(NoMoreDay::SkillVfxEventType::BuffExit)) {
          hasSkill1BuffExit = true;
        }
      }
    }

    if (recipe.contains("actions") && recipe["actions"].is_array()) {
      for (const auto &action : recipe["actions"]) {
        if (!action.is_object() || !action.contains("kind") ||
            !action["kind"].is_string()) {
          continue;
        }
        if (action["kind"].get<std::string>() == "ResistOverlay") {
          hasResistOverlayAction = true;
        }
      }
    }
  }

  CHECK(hasSkill1BuffEnter);
  CHECK(hasSkill1BuffExit);
  CHECK(hasResistOverlayAction);
}
