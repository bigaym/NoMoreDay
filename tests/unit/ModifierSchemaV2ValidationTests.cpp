#include "doctest.h"

#include "game/systems/modifier/ModifierSchemaV2Validation.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>
#include <string>

namespace {

bool HasCatalogEntry(const nlohmann::json &entries, const std::string &domain,
                     const std::string &path) {
  for (const auto &entry : entries) {
    if (entry.value("domain", "") == domain && entry.value("path", "") == path) {
      return true;
    }
  }
  return false;
}

} // namespace

TEST_CASE("[Unit] ModifierSchemaV2 - Required top-level fields") {
  const std::string bad = R"({"domain":"equipment"})";
  CHECK_THROWS_WITH_AS(NoMoreDay::ValidateModifierSchemaV2Json(bad),
                       doctest::Contains("schema_version"), std::runtime_error);
}

TEST_CASE("[Unit] ModifierSchemaV2 - Accepts valid minimal record") {
  const std::string good = R"(
{
  "schema_version": 2,
  "domain": "skill_spec",
  "records": [
    {
      "id": 2002103,
      "domain": "skill_spec",
      "priority": 200,
      "filters": {
        "profession_mask": 1,
        "skill_id_whitelist": [2],
        "required_skill_tags_all": 1,
        "forbidden_skill_tags_any": 0,
        "weapon_class_mask": 65535,
        "equip_slot_mask": 0,
        "node_id_whitelist": [213]
      },
      "constraints": {
        "exclusive_group": 0,
        "max_active": 0
      },
      "ops": [
        {
          "opcode": "ADD_STAT_PERCENT_MULT",
          "target": "damage",
          "param_u32": 9,
          "param_f32": 0.22
        }
      ]
    }
  ]
}
  )";

  CHECK_NOTHROW(NoMoreDay::ValidateModifierSchemaV2Json(good));
}

TEST_CASE("[Unit] ModifierSchemaV2 - Rejects missing nested record fields") {
  const std::string bad = R"(
{
  "schema_version": 2,
  "domain": "skill_spec",
  "records": [
    {
      "id": 2002103,
      "domain": "skill_spec",
      "priority": 200,
      "constraints": {
        "exclusive_group": 0,
        "max_active": 0
      },
      "ops": []
    }
  ]
}
  )";

  CHECK_THROWS_WITH_AS(NoMoreDay::ValidateModifierSchemaV2Json(bad),
                       doctest::Contains("filters"), std::runtime_error);
}

TEST_CASE("[Unit] ModifierSchemaV2 - Rejects malformed ops payload") {
  const std::string bad = R"(
{
  "schema_version": 2,
  "domain": "skill_spec",
  "records": [
    {
      "id": 2002103,
      "domain": "skill_spec",
      "priority": 200,
      "filters": {
        "profession_mask": 1,
        "skill_id_whitelist": [2],
        "required_skill_tags_all": 1,
        "forbidden_skill_tags_any": 0,
        "weapon_class_mask": 65535,
        "equip_slot_mask": 0,
        "node_id_whitelist": [213]
      },
      "constraints": {
        "exclusive_group": 0,
        "max_active": 0
      },
      "ops": [
        {
          "opcode": "ADD_STAT_PERCENT_MULT",
          "target": "damage",
          "param_u32": "wrong",
          "param_f32": 0.22
        }
      ]
    }
  ]
}
  )";

  CHECK_THROWS_WITH_AS(NoMoreDay::ValidateModifierSchemaV2Json(bad),
                       doctest::Contains("param_u32"), std::runtime_error);
}

TEST_CASE("[Unit] ModifierSchemaV2 - Catalog includes map and monster domains") {
  std::ifstream fileStream("assets/data/modifier_v2/modifier_catalog.json");
  REQUIRE(fileStream.is_open());

  const auto root = nlohmann::json::parse(fileStream);
  const auto &entries = root.at("entries");

  CHECK(HasCatalogEntry(entries, "map", "map_modifiers.json"));
  CHECK(HasCatalogEntry(entries, "monster", "monster_modifiers.json"));
}
