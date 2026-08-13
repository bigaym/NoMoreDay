#include "doctest.h"

#include "game/foundation/components/Combat.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/SerializationSystem.hpp"

#include <entt/entt.hpp>

#include <filesystem>
#include <fstream>

namespace NoMoreDay {
namespace {

// 测试专用技能 id：与既有测试（990001/990002 等）错开，避免单例污染。
constexpr uint32_t kSanitizeValidSkillId = 990301;
// 永不注册的未知技能 id，模拟旧存档注入（如 test_power）。
constexpr uint32_t kSanitizeUnknownSkillId = 999999;

void RegisterSanitizeFixtureSkill() {
  SkillData skill;
  skill.id = kSanitizeValidSkillId;
  skill.name_key = "unit_sanitize_valid";
  skill.desc_key = "unit fixture skill for slot sanitize tests";
  skill.mana_cost = 0.0f;
  skill.cooldown = 1.0f;
  skill.base_damage = 1.0f;
  skill.weapon_damage_mult = 1.0f;
  SkillRegistry::Get().RegisterSkill(skill);
}

// 构造含未知 id（活跃槽 + 专精槽）与有效 id 混排的 ActiveSkills JSON。
nlohmann::json MakeActiveSkillsJson() {
  return nlohmann::json{
      {"slots", nlohmann::json::array({
           nlohmann::json{{"id", 0}, {"cooldown", 0.0f}, {"current_charges", 0}},
           nlohmann::json{{"id", kSanitizeValidSkillId}, {"cooldown", 1.5f}, {"current_charges", 2}},
           nlohmann::json{{"id", kSanitizeUnknownSkillId}, {"cooldown", 3.0f}, {"current_charges", 5}},
           nlohmann::json{{"id", 0}, {"cooldown", 0.0f}, {"current_charges", 0}},
           nlohmann::json{{"id", 0}, {"cooldown", 0.0f}, {"current_charges", 0}},
       })},
      {"specialized_slots", nlohmann::json::array({
            nlohmann::json{{"skill_id", kSanitizeUnknownSkillId},
                           {"bonus_levels", 3},
                           {"allocated_points", {{900101, 2}, {900102, 1}}}},
            nlohmann::json{{"skill_id", kSanitizeValidSkillId},
                           {"bonus_levels", 1},
                           {"allocated_points", {{900201, 1}}}},
            nlohmann::json{{"skill_id", INVALID_SKILL_ID},
                           {"bonus_levels", 0},
                           {"allocated_points", nlohmann::json::array()}},
            nlohmann::json{{"skill_id", 0},
                           {"bonus_levels", 0},
                           {"allocated_points", nlohmann::json::array()}},
            nlohmann::json{{"skill_id", INVALID_SKILL_ID},
                           {"bonus_levels", 0},
                           {"allocated_points", nlohmann::json::array()}},
       })},
      {"available_talent_points", 7},
  };
}

TEST_CASE("[Unit] SkillRegistry - SanitizeLoadedSkillSlots resets unknown ids and keeps valid slots") {
  RegisterSanitizeFixtureSkill();

  ActiveSkillsComponent active = MakeActiveSkillsJson().get<ActiveSkillsComponent>();
  REQUIRE(active.slots[2].id == kSanitizeUnknownSkillId);
  REQUIRE(active.specialized_slots[0].skill_id == kSanitizeUnknownSkillId);

  SkillRegistry::Get().SanitizeLoadedSkillSlots(active);

  // 未知活跃槽完全复位为空槽（id==0，cooldown/charges 清零）。
  CHECK(active.slots[2].id == 0);
  CHECK(active.slots[2].cooldown == doctest::Approx(0.0f));
  CHECK(active.slots[2].current_charges == 0);

  // 有效活跃槽原样保留。
  CHECK(active.slots[1].id == kSanitizeValidSkillId);
  CHECK(active.slots[1].cooldown == doctest::Approx(1.5f));
  CHECK(active.slots[1].current_charges == 2);

  // 空活跃槽（id==0）不受影响。
  CHECK(active.slots[0].id == 0);

  // 未知专精槽完全复位：skill_id 回到空哨兵 INVALID_SKILL_ID，bonus_levels 与天赋分配清空。
  CHECK(active.specialized_slots[0].skill_id == INVALID_SKILL_ID);
  CHECK(active.specialized_slots[0].bonus_levels == 0);
  CHECK(active.specialized_slots[0].allocated_points.empty());

  // 有效专精槽原样保留（bonus_levels 与天赋分配完整）。
  CHECK(active.specialized_slots[1].skill_id == kSanitizeValidSkillId);
  CHECK(active.specialized_slots[1].bonus_levels == 1);
  REQUIRE(active.specialized_slots[1].allocated_points.size() == 1);
  CHECK(active.specialized_slots[1].allocated_points.at(900201) == 1);

  // 空专精槽（INVALID_SKILL_ID）与 skill_id==0 的槽均不重置。
  CHECK(active.specialized_slots[2].skill_id == INVALID_SKILL_ID);
  CHECK(active.specialized_slots[3].skill_id == 0);

  // 天赋点数不受影响。
  CHECK(active.available_talent_points == 7);
}

TEST_CASE("[Unit] SerializationSystem - Load sanitizes unknown skill slots from save json") {
  RegisterSanitizeFixtureSkill();

  const auto savePath = std::filesystem::temp_directory_path() / "nmd_unit_skill_sanitize_save.json";
  {
    nlohmann::json root;
    root["entities"] = nlohmann::json::array();
    nlohmann::json entityJson;
    entityJson["uuid"] = 42u;
    entityJson["ActiveSkills"] = MakeActiveSkillsJson();
    root["entities"].push_back(entityJson);

    std::ofstream file(savePath);
    REQUIRE(file.is_open());
    file << root.dump(4);
  }

  entt::registry registry;
  SerializationSystem::Load(registry, savePath);

  std::error_code ec;
  std::filesystem::remove(savePath, ec);

  // 按 uuid 定位加载出的实体。
  entt::entity player = entt::null;
  for (auto [entity, id] : registry.view<IDComponent>().each()) {
    if (id.uuid == 42u) {
      player = entity;
      break;
    }
  }
  REQUIRE(player != static_cast<entt::entity>(entt::null));
  REQUIRE(registry.all_of<ActiveSkillsComponent>(player));

  const auto &active = registry.get<ActiveSkillsComponent>(player);
  // 未知活跃槽被清理为空槽。
  CHECK(active.slots[2].id == 0);
  CHECK(active.slots[2].current_charges == 0);
  // 有效活跃槽原样保留。
  CHECK(active.slots[1].id == kSanitizeValidSkillId);
  CHECK(active.slots[1].current_charges == 2);
  // 未知专精槽被清理（天赋分配清空）。
  CHECK(active.specialized_slots[0].skill_id == INVALID_SKILL_ID);
  CHECK(active.specialized_slots[0].allocated_points.empty());
  // 有效专精槽保留天赋分配。
  REQUIRE(active.specialized_slots[1].allocated_points.size() == 1);
  CHECK(active.specialized_slots[1].allocated_points.at(900201) == 1);
  // 天赋点数不受影响。
  CHECK(active.available_talent_points == 7);
}

}  // namespace
}  // namespace NoMoreDay
