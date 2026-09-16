#include "doctest.h"

#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/ItemStats.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/modifier/EquipmentModifierAdapter.hpp"
#include "game/systems/modifier/ModifierContext.hpp"
#include "game/systems/modifier/ModifierRuntimeRegistry.hpp"
#include "game/systems/modifier/ModifierRuntimeTypes.hpp"

#include <entt/entt.hpp>

#include <cstdint>
#include <vector>

namespace {

template <typename T>
void AppendStruct(std::vector<uint8_t> &out, const T &value) {
  const auto *bytes = reinterpret_cast<const uint8_t *>(&value);
  out.insert(out.end(), bytes, bytes + sizeof(T));
}

std::vector<uint8_t> BuildSkillLevelRuntimeBlob() {
  NoMoreDay::ModifierRuntimeHeader header;
  header.record_count = 1;
  header.filter_count = 1;
  header.op_count = 1;
  header.index_count = 1;
  header.records_offset = sizeof(NoMoreDay::ModifierRuntimeHeader);
  header.filters_offset =
      header.records_offset + sizeof(NoMoreDay::ModifierRuntimeRecord);
  header.ops_offset =
      header.filters_offset + sizeof(NoMoreDay::ModifierRuntimeFilter);
  header.index_offset = header.ops_offset + sizeof(NoMoreDay::ModifierRuntimeOp);
  header.crc32 = 0;

  NoMoreDay::ModifierRuntimeRecord record;
  record.id = 1001001u;
  record.filter_index = 0;
  record.op_offset = 0;
  record.op_count = 1;

  NoMoreDay::ModifierRuntimeFilter filter;
  filter.required_skill_tags_all = static_cast<uint64_t>(NoMoreDay::Tag::Hit);
  filter.skill_whitelist_offset = 0;
  filter.skill_whitelist_count = 1;

  NoMoreDay::ModifierRuntimeOp op;
  op.opcode = static_cast<uint16_t>(NoMoreDay::ModifierOpCode::ADD_SKILL_LEVEL);
  op.param_u32 = 1u;
  op.param_f32 = 2.0f;

  const uint32_t skillWhitelist = 1u;

  std::vector<uint8_t> blob;
  blob.reserve(sizeof(header) + sizeof(record) + sizeof(filter) + sizeof(op) +
               sizeof(skillWhitelist));
  AppendStruct(blob, header);
  AppendStruct(blob, record);
  AppendStruct(blob, filter);
  AppendStruct(blob, op);
  AppendStruct(blob, skillWhitelist);
  return blob;
}

// 构造与数据表 record 1001001 同形的运行时词缀：MANA_COST_MULT、skill_id 白名单
// [1]、required_skill_tags_all=Hit、equip_slot_mask=2、param_u32=1、param_f32=0.9。
std::vector<uint8_t> BuildManaCostRuntimeBlob() {
  NoMoreDay::ModifierRuntimeHeader header;
  header.record_count = 1;
  header.filter_count = 1;
  header.op_count = 1;
  header.index_count = 1;
  header.records_offset = sizeof(NoMoreDay::ModifierRuntimeHeader);
  header.filters_offset =
      header.records_offset + sizeof(NoMoreDay::ModifierRuntimeRecord);
  header.ops_offset =
      header.filters_offset + sizeof(NoMoreDay::ModifierRuntimeFilter);
  header.index_offset = header.ops_offset + sizeof(NoMoreDay::ModifierRuntimeOp);
  header.crc32 = 0;

  NoMoreDay::ModifierRuntimeRecord record;
  record.id = 1001001u;
  record.filter_index = 0;
  record.op_offset = 0;
  record.op_count = 1;

  NoMoreDay::ModifierRuntimeFilter filter;
  filter.required_skill_tags_all =
      static_cast<uint64_t>(NoMoreDay::Tag::Hit);
  filter.equip_slot_mask = 2u; // 主手槽位；注意 ctx.equip_slot_mask 目前不被填充（保持通配），本用例不覆盖槽位过滤
  filter.skill_whitelist_offset = 0;
  filter.skill_whitelist_count = 1;

  NoMoreDay::ModifierRuntimeOp op;
  op.opcode = static_cast<uint16_t>(NoMoreDay::ModifierOpCode::MANA_COST_MULT);
  op.param_u32 = 1u; // 目标技能 1
  op.param_f32 = 0.9f;

  const uint32_t skillWhitelist = 1u;

  std::vector<uint8_t> blob;
  blob.reserve(sizeof(header) + sizeof(record) + sizeof(filter) + sizeof(op) +
               sizeof(skillWhitelist));
  AppendStruct(blob, header);
  AppendStruct(blob, record);
  AppendStruct(blob, filter);
  AppendStruct(blob, op);
  AppendStruct(blob, skillWhitelist);
  return blob;
}

void EquipMainHandWithRecord(entt::registry &registry, const entt::entity player,
                             const uint32_t recordId) {
  auto &equipment =
      registry.get_or_emplace<NoMoreDay::EquipmentComponent>(player);
  const entt::entity itemEntity = registry.create();
  auto &item = registry.emplace<NoMoreDay::ItemComponent>(itemEntity);

  NoMoreDay::Affix affix;
  affix.type = NoMoreDay::AffixType::Strength;
  affix.modifier_record_ids = {recordId};
  item.affixes.push_back(affix);

  equipment.Set(NoMoreDay::EquipmentSlot::MainHand, itemEntity);
}

} // namespace

TEST_CASE("[Unit] EquipmentModifierAdapter - collects runtime modifier record ids") {
  entt::registry registry;
  const entt::entity player = registry.create();
  auto &equipment = registry.emplace<NoMoreDay::EquipmentComponent>(player);

  const entt::entity itemEntity = registry.create();
  auto &item = registry.emplace<NoMoreDay::ItemComponent>(itemEntity);
  item.id = 42u;

  NoMoreDay::Affix affix;
  affix.type = NoMoreDay::AffixType::PlusFlowingThrust;
  affix.modifier_record_ids = {1001001u};
  affix.value = 2.0f;
  affix.required_tags = NoMoreDay::Tag::Hit;
  item.affixes.push_back(affix);

  equipment.Set(NoMoreDay::EquipmentSlot::MainHand, itemEntity);

  const auto ids =
      NoMoreDay::EquipmentModifierAdapter::CollectEquippedRecordIds(registry, player);
  REQUIRE(ids.size() == 1u);
  CHECK(ids[0] == 1001001u);
}

TEST_CASE("[Unit] EquipmentModifierAdapter - collection ignores affix type without runtime ids") {
  entt::registry registry;
  const entt::entity player = registry.create();
  auto &equipment = registry.emplace<NoMoreDay::EquipmentComponent>(player);

  const entt::entity itemEntity = registry.create();
  auto &item = registry.emplace<NoMoreDay::ItemComponent>(itemEntity);

  NoMoreDay::Affix legacyOnly;
  legacyOnly.type = NoMoreDay::AffixType::PlusFlowingThrust;
  legacyOnly.value = 2.0f;
  item.affixes.push_back(legacyOnly);

  NoMoreDay::Affix runtimeMapped;
  runtimeMapped.type = NoMoreDay::AffixType::Strength;
  runtimeMapped.modifier_record_ids = {1001001u, 1001002u};
  item.affixes.push_back(runtimeMapped);

  equipment.Set(NoMoreDay::EquipmentSlot::MainHand, itemEntity);

  const auto ids =
      NoMoreDay::EquipmentModifierAdapter::CollectEquippedRecordIds(registry, player);
  REQUIRE(ids.size() == 2u);
  CHECK(ids[0] == 1001001u);
  CHECK(ids[1] == 1001002u);
}

TEST_CASE("[Unit] EquipmentModifierAdapter - runtime evaluator gates and applies skill levels") {
  auto &runtimeRegistry = NoMoreDay::ModifierRuntimeRegistry::Get();
  const auto blob = BuildSkillLevelRuntimeBlob();
  REQUIRE(runtimeRegistry.LoadFromBytes(blob));

  NoMoreDay::SkillData skill;
  skill.id = 1u;
  skill.tags = NoMoreDay::Tag::None;
  NoMoreDay::SkillRegistry::Get().RegisterSkill(skill);

  entt::registry registry;
  const entt::entity player = registry.create();
  auto &equipment = registry.emplace<NoMoreDay::EquipmentComponent>(player);
  auto &activeSkills = registry.emplace<NoMoreDay::ActiveSkillsComponent>(player);
  activeSkills.specialized_slots[0].skill_id = 1u;

  const entt::entity itemEntity = registry.create();
  auto &item = registry.emplace<NoMoreDay::ItemComponent>(itemEntity);

  NoMoreDay::Affix affix;
  affix.type = NoMoreDay::AffixType::Strength;
  affix.modifier_record_ids = {1001001u};
  item.affixes.push_back(affix);
  equipment.Set(NoMoreDay::EquipmentSlot::MainHand, itemEntity);

  NoMoreDay::EquipmentModifierAdapter::ApplyEquippedSkillLevelBonuses(registry,
                                                                       player);
  CHECK(activeSkills.specialized_slots[0].bonus_levels == 0);

  skill.tags = NoMoreDay::Tag::Hit;
  NoMoreDay::SkillRegistry::Get().RegisterSkill(skill);
  activeSkills.specialized_slots[0].bonus_levels = 0;

  NoMoreDay::EquipmentModifierAdapter::ApplyEquippedSkillLevelBonuses(registry,
                                                                       player);
  CHECK(activeSkills.specialized_slots[0].bonus_levels == 2);
}

TEST_CASE("[Unit] EquipmentModifierAdapter - equipped mana cost multiplier applies record") {
  auto &runtimeRegistry = NoMoreDay::ModifierRuntimeRegistry::Get();
  REQUIRE(runtimeRegistry.LoadFromBytes(BuildManaCostRuntimeBlob()));

  entt::registry registry;
  const entt::entity player = registry.create();
  EquipMainHandWithRecord(registry, player, 1001001u);

  const float mult =
      NoMoreDay::EquipmentModifierAdapter::GetEquippedManaCostMultiplier(
          registry, player, 1u, NoMoreDay::Tag::Hit);
  CHECK(mult == doctest::Approx(0.9f));
}

TEST_CASE("[Unit] EquipmentModifierAdapter - unequipped mana cost multiplier is neutral") {
  auto &runtimeRegistry = NoMoreDay::ModifierRuntimeRegistry::Get();
  REQUIRE(runtimeRegistry.LoadFromBytes(BuildManaCostRuntimeBlob()));

  SUBCASE("no equipment component") {
    entt::registry registry;
    const entt::entity player = registry.create();
    const float mult =
        NoMoreDay::EquipmentModifierAdapter::GetEquippedManaCostMultiplier(
            registry, player, 1u, NoMoreDay::Tag::Hit);
    CHECK(mult == doctest::Approx(1.0f));
  }

  SUBCASE("equipment component without affixes") {
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<NoMoreDay::EquipmentComponent>(player);
    const float mult =
        NoMoreDay::EquipmentModifierAdapter::GetEquippedManaCostMultiplier(
            registry, player, 1u, NoMoreDay::Tag::Hit);
    CHECK(mult == doctest::Approx(1.0f));
  }

  SUBCASE("equipped record id missing from runtime registry") {
    entt::registry registry;
    const entt::entity player = registry.create();
    EquipMainHandWithRecord(registry, player, 9999999u);
    const float mult =
        NoMoreDay::EquipmentModifierAdapter::GetEquippedManaCostMultiplier(
            registry, player, 1u, NoMoreDay::Tag::Hit);
    CHECK(mult == doctest::Approx(1.0f));
  }

  SUBCASE("skill id filtered out by whitelist") {
    entt::registry registry;
    const entt::entity player = registry.create();
    EquipMainHandWithRecord(registry, player, 1001001u);
    const float mult =
        NoMoreDay::EquipmentModifierAdapter::GetEquippedManaCostMultiplier(
            registry, player, 2u, NoMoreDay::Tag::Hit);
    CHECK(mult == doctest::Approx(1.0f));
  }

  SUBCASE("skill tags filtered out by required tags") {
    entt::registry registry;
    const entt::entity player = registry.create();
    EquipMainHandWithRecord(registry, player, 1001001u);
    const float mult =
        NoMoreDay::EquipmentModifierAdapter::GetEquippedManaCostMultiplier(
            registry, player, 1u, NoMoreDay::Tag::None);
    CHECK(mult == doctest::Approx(1.0f));
  }
}
