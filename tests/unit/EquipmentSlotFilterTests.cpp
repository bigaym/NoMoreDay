#include "doctest.h"

#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/ItemStats.hpp"
#include "game/foundation/components/Progression.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/foundation/data/TalentData.hpp"
#include "game/systems/modifier/EquipmentModifierAdapter.hpp"
#include "game/systems/modifier/ModifierContext.hpp"
#include "game/systems/modifier/ModifierRuntimeRegistry.hpp"
#include "game/systems/modifier/ModifierRuntimeTypes.hpp"

#include <entt/entt.hpp>

#include <cstdint>
#include <vector>

namespace {

using NoMoreDay::Affix;
using NoMoreDay::AstrolabeComponent;
using NoMoreDay::EquipmentComponent;
using NoMoreDay::EquipmentModifierAdapter;
using NoMoreDay::EquipmentSlot;
using NoMoreDay::ItemComponent;
using NoMoreDay::ModifierOp;
using NoMoreDay::ModifierOpCode;
using NoMoreDay::ModifierRuntimeFilter;
using NoMoreDay::ModifierRuntimeHeader;
using NoMoreDay::ModifierRuntimeOp;
using NoMoreDay::ModifierRuntimeRecord;
using NoMoreDay::ModifierRuntimeRegistry;
using NoMoreDay::ProfessionID;
using NoMoreDay::SkillData;
using NoMoreDay::SkillRegistry;
using NoMoreDay::Tag;
using NoMoreDay::WeaponSubtype;

template <typename T>
void AppendStruct(std::vector<uint8_t> &out, const T &value) {
  const auto *bytes = reinterpret_cast<const uint8_t *>(&value);
  out.insert(out.end(), bytes, bytes + sizeof(T));
}

// 构造单记录单算子运行时 blob。index 表布局为 [nodeWhitelist..., skillWhitelist...]，
// crc32 = 0 表示跳过 CRC 校验（单测合成数据的既有约定）。
std::vector<uint8_t> BuildSingleRecordRuntimeBlob(
    const uint32_t recordId, ModifierRuntimeFilter filter,
    const ModifierRuntimeOp &op, const std::vector<uint32_t> &skillWhitelist,
    const std::vector<uint32_t> &nodeWhitelist = {}) {
  filter.node_whitelist_offset = 0;
  filter.node_whitelist_count = static_cast<uint32_t>(nodeWhitelist.size());
  filter.skill_whitelist_offset = static_cast<uint32_t>(nodeWhitelist.size());
  filter.skill_whitelist_count = static_cast<uint32_t>(skillWhitelist.size());

  ModifierRuntimeHeader header;
  header.record_count = 1;
  header.filter_count = 1;
  header.op_count = 1;
  header.index_count =
      static_cast<uint32_t>(nodeWhitelist.size() + skillWhitelist.size());
  header.records_offset = sizeof(ModifierRuntimeHeader);
  header.filters_offset = header.records_offset + sizeof(ModifierRuntimeRecord);
  header.ops_offset = header.filters_offset + sizeof(ModifierRuntimeFilter);
  header.index_offset = header.ops_offset + sizeof(ModifierRuntimeOp);
  header.crc32 = 0;

  ModifierRuntimeRecord record;
  record.id = recordId;
  record.filter_index = 0;
  record.op_offset = 0;
  record.op_count = 1;

  std::vector<uint8_t> blob;
  AppendStruct(blob, header);
  AppendStruct(blob, record);
  AppendStruct(blob, filter);
  AppendStruct(blob, op);
  for (const uint32_t nodeId : nodeWhitelist) {
    AppendStruct(blob, nodeId);
  }
  for (const uint32_t skillId : skillWhitelist) {
    AppendStruct(blob, skillId);
  }
  return blob;
}

// 在指定槽位放置一件带单条运行时记录词缀的物品，返回物品实体。
entt::entity EquipItemWithRecord(entt::registry &registry,
                                 const entt::entity player,
                                 const EquipmentSlot slot,
                                 const WeaponSubtype weaponSubtype,
                                 const uint32_t recordId) {
  const entt::entity item = registry.create();
  auto &itemComponent = registry.emplace<ItemComponent>(item);
  itemComponent.weaponSubtype = weaponSubtype;

  Affix affix;
  affix.modifier_record_ids = {recordId};
  itemComponent.affixes.push_back(affix);

  registry.get<EquipmentComponent>(player).Set(slot, item);
  return item;
}

void RegisterSkillWithNoTags(const uint32_t skillId) {
  SkillData skill{};
  skill.id = skillId;
  skill.tags = Tag::None;
  SkillRegistry::Get().RegisterSkill(skill);
}

} // namespace

TEST_CASE("[Unit] EquipmentSlotFilter - Head affix restricted to head slot") {
  constexpr uint32_t kSkillId = 9101u;
  constexpr uint32_t kRecordId = 8101u;

  ModifierRuntimeFilter filter;
  filter.equip_slot_mask = 1u << static_cast<uint32_t>(EquipmentSlot::Head);

  ModifierRuntimeOp op;
  op.opcode = static_cast<uint16_t>(ModifierOpCode::ADD_SKILL_LEVEL);
  op.param_u32 = kSkillId;
  op.param_f32 = 2.0f;

  REQUIRE(ModifierRuntimeRegistry::Get().LoadFromBytes(
      BuildSingleRecordRuntimeBlob(kRecordId, filter, op, {kSkillId})));
  RegisterSkillWithNoTags(kSkillId);

  const auto bonusInSlot = [&](const EquipmentSlot slot) {
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<EquipmentComponent>(player);
    registry.emplace<NoMoreDay::ActiveSkillsComponent>(player);

    EquipItemWithRecord(registry, player, slot, WeaponSubtype::None, kRecordId);
    registry.get<NoMoreDay::ActiveSkillsComponent>(player)
        .specialized_slots[0]
        .skill_id = kSkillId;

    EquipmentModifierAdapter::ApplyEquippedSkillLevelBonuses(registry, player);
    // 每次重新取组件，避免跨创建实体持有组件指针。
    return registry.get<NoMoreDay::ActiveSkillsComponent>(player)
        .specialized_slots[0]
        .bonus_levels;
  };

  // 头部词缀放在胸甲槽：槽位掩码不匹配，不授予加成。
  CHECK(bonusInSlot(EquipmentSlot::Chest) == 0);
  // 同一词缀放在头部槽：命中，授予 2 级。
  CHECK(bonusInSlot(EquipmentSlot::Head) == 2);
}

TEST_CASE(
    "[Unit] EquipmentSlotFilter - weapon class mask filters by subtype") {
  constexpr uint32_t kSkillId = 9102u;
  constexpr uint32_t kRecordId = 8102u;

  ModifierRuntimeFilter filter;
  filter.weapon_class_mask = 1u << static_cast<uint32_t>(WeaponSubtype::Sword);

  ModifierRuntimeOp op;
  op.opcode = static_cast<uint16_t>(ModifierOpCode::MANA_COST_MULT);
  op.param_u32 = kSkillId;
  op.param_f32 = 0.5f;

  REQUIRE(ModifierRuntimeRegistry::Get().LoadFromBytes(
      BuildSingleRecordRuntimeBlob(kRecordId, filter, op, {kSkillId})));

  const auto multForWeapon = [&](const WeaponSubtype subtype) {
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<EquipmentComponent>(player);

    EquipItemWithRecord(registry, player, EquipmentSlot::MainHand, subtype,
                        kRecordId);
    return EquipmentModifierAdapter::GetEquippedManaCostMultiplier(
        registry, player, kSkillId, Tag::None);
  };

  // 主手为剑：weapon_class_mask 命中，法力消耗折扣生效。
  CHECK(multForWeapon(WeaponSubtype::Sword) == doctest::Approx(0.5f));
  // 主手为斧：类别不匹配，记录被过滤，保持中性 1.0。
  CHECK(multForWeapon(WeaponSubtype::Axe) == doctest::Approx(1.0f));
}

TEST_CASE(
    "[Unit] EquipmentSlotFilter - unsworn profession keeps bit 0 records") {
  constexpr uint32_t kSkillId = 9103u;
  constexpr uint32_t kRecordId = 8103u;

  ModifierRuntimeFilter filter;
  filter.profession_mask = 1ull; // bit 0：未誓约（profession_id = 0）通配位

  ModifierRuntimeOp op;
  op.opcode = static_cast<uint16_t>(ModifierOpCode::MANA_COST_MULT);
  op.param_u32 = kSkillId;
  op.param_f32 = 0.5f;

  REQUIRE(ModifierRuntimeRegistry::Get().LoadFromBytes(
      BuildSingleRecordRuntimeBlob(kRecordId, filter, op, {kSkillId})));

  const auto multForPlayer = [&](const bool addAstrolabe,
                                 const int mainProfession = -1) {
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<EquipmentComponent>(player);
    if (addAstrolabe) {
      // 默认 mainProfession = -1，代表未誓约；显式传入即为已誓约职业。
      registry.emplace<AstrolabeComponent>(player).mainProfession =
          mainProfession;
    }

    EquipItemWithRecord(registry, player, EquipmentSlot::MainHand,
                        WeaponSubtype::None, kRecordId);
    return EquipmentModifierAdapter::GetEquippedManaCostMultiplier(
        registry, player, kSkillId, Tag::None);
  };

  // AstrolabeComponent 缺失：profession_id 保持 0，命中 bit 0。
  CHECK(multForPlayer(false) == doctest::Approx(0.5f));
  // mainProfession = -1：同样保持 0，命中 bit 0。
  CHECK(multForPlayer(true) == doctest::Approx(0.5f));
  // 反面用例：已誓约法师（mainProfession = Mage => profession_id = 1）不命中
  // bit 0，记录被过滤并返回中性 1.0；若 profession_id 从未被填充，此断言会失败。
  CHECK(multForPlayer(true, static_cast<int>(ProfessionID::Mage)) ==
        doctest::Approx(1.0f));
}

TEST_CASE("[Unit] EquipmentSlotFilter - unarmed keeps 65535 weapon wildcard") {
  constexpr uint32_t kSkillId = 9104u;
  constexpr uint32_t kRecordId = 8104u;

  ModifierRuntimeFilter filter;
  // 65535 为引擎约定的通配掩码；徒手时 ctx.weapon_class_mask 置
  // WeaponSubtype::None 位，记录仍应生效。
  filter.weapon_class_mask = 65535u;

  ModifierRuntimeOp op;
  op.opcode = static_cast<uint16_t>(ModifierOpCode::MANA_COST_MULT);
  op.param_u32 = kSkillId;
  op.param_f32 = 0.4f;

  REQUIRE(ModifierRuntimeRegistry::Get().LoadFromBytes(
      BuildSingleRecordRuntimeBlob(kRecordId, filter, op, {kSkillId})));

  entt::registry registry;
  const entt::entity player = registry.create();
  registry.emplace<EquipmentComponent>(player);

  // 记录挂在头部护甲上，角色未装备任何武器（徒手）。
  EquipItemWithRecord(registry, player, EquipmentSlot::Head, WeaponSubtype::None,
                      kRecordId);

  CHECK(EquipmentModifierAdapter::GetEquippedManaCostMultiplier(
            registry, player, kSkillId, Tag::None) == doctest::Approx(0.4f));
}

TEST_CASE(
    "[Unit] EquipmentSlotFilter - zero and all-bits weapon masks are wildcards") {
  constexpr uint32_t kSkillId = 9105u;

  ModifierRuntimeOp op;
  op.opcode = static_cast<uint16_t>(ModifierOpCode::MANA_COST_MULT);
  op.param_u32 = kSkillId;
  op.param_f32 = 0.5f;

  // armed=true 时主手装备剑（持械上下文）；否则记录挂头部、主副手无武器（徒手）。
  const auto multForMask = [&](const uint32_t filterMask, const uint32_t recordId, const bool armed) {
    ModifierRuntimeFilter filter;
    filter.weapon_class_mask = filterMask;
    REQUIRE(ModifierRuntimeRegistry::Get().LoadFromBytes(
        BuildSingleRecordRuntimeBlob(recordId, filter, op, {kSkillId})));

    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<EquipmentComponent>(player);
    if (armed) {
      EquipItemWithRecord(registry, player, EquipmentSlot::MainHand,
                          WeaponSubtype::Sword, recordId);
    } else {
      EquipItemWithRecord(registry, player, EquipmentSlot::Head,
                          WeaponSubtype::None, recordId);
    }
    return EquipmentModifierAdapter::GetEquippedManaCostMultiplier(
        registry, player, kSkillId, Tag::None);
  };

  // 掩码 0 视为通配：持械与徒手都应命中。
  CHECK(multForMask(0u, 8105u, true) == doctest::Approx(0.5f));
  CHECK(multForMask(0u, 8105u, false) == doctest::Approx(0.5f));

  // 掩码 0xFFFFFFFF 视为通配：持械与徒手都应命中。
  CHECK(multForMask(0xFFFFFFFFu, 8106u, true) == doctest::Approx(0.5f));
  CHECK(multForMask(0xFFFFFFFFu, 8106u, false) == doctest::Approx(0.5f));

  // 掩码 1（bit0 = WeaponSubtype::None）表示「仅徒手」：徒手命中，持械被过滤。
  CHECK(multForMask(1u, 8107u, false) == doctest::Approx(0.5f));
  CHECK(multForMask(1u, 8107u, true) == doctest::Approx(1.0f));
}

TEST_CASE(
    "[Unit] EquipmentSlotFilter - equipment path ignores skill-delivery opcodes") {
  constexpr uint32_t kSkillId = 9106u;
  constexpr uint32_t kRecordId = 8108u;

  ModifierRuntimeFilter filter; // 技能白名单以外全部通配
  ModifierRuntimeOp op;
  // SKILL_MANA_COST_MULT 写入 mana_cost_mult 容器：若装备路径误施加，法耗乘算会变成
  // 1 - 0.15 = 0.85。装备路径按类别掩码排除 SkillDelivery，故应保持中性 1.0。
  op.opcode = static_cast<uint16_t>(ModifierOpCode::SKILL_MANA_COST_MULT);
  op.param_u32 = kSkillId;
  op.param_f32 = 0.15f;

  REQUIRE(ModifierRuntimeRegistry::Get().LoadFromBytes(
      BuildSingleRecordRuntimeBlob(kRecordId, filter, op, {kSkillId})));

  entt::registry registry;
  const entt::entity player = registry.create();
  registry.emplace<EquipmentComponent>(player);
  EquipItemWithRecord(registry, player, EquipmentSlot::MainHand,
                      WeaponSubtype::Sword, kRecordId);

  CHECK(EquipmentModifierAdapter::GetEquippedManaCostMultiplier(
            registry, player, kSkillId, Tag::None) == doctest::Approx(1.0f));
}

TEST_CASE(
    "[Unit] EquipmentSlotFilter - refs carry slot bit and are deduped/sorted") {
  entt::registry registry;
  const entt::entity player = registry.create();
  registry.emplace<EquipmentComponent>(player);

  // 头部物品：affix 记录 900，implicit 记录 900（重复）与 901。
  const entt::entity headItem =
      EquipItemWithRecord(registry, player, EquipmentSlot::Head,
                          WeaponSubtype::None, 900u);
  Affix implicitAffix;
  implicitAffix.modifier_record_ids = {900u, 901u};
  registry.get<ItemComponent>(headItem).implicits.push_back(implicitAffix);

  // 胸甲物品：记录 902。
  EquipItemWithRecord(registry, player, EquipmentSlot::Chest, WeaponSubtype::None,
                      902u);

  const uint32_t headBit = 1u << static_cast<uint32_t>(EquipmentSlot::Head);
  const uint32_t chestBit = 1u << static_cast<uint32_t>(EquipmentSlot::Chest);

  const auto refs =
      EquipmentModifierAdapter::CollectEquippedRecordRefs(registry, player);
  REQUIRE(refs.size() == 3u);
  // 按 (slot_bit, record_id) 排序，头部 bit 小于胸甲 bit。
  CHECK(refs[0].slot_bit == headBit);
  CHECK(refs[0].record_id == 900u);
  CHECK(refs[1].slot_bit == headBit);
  CHECK(refs[1].record_id == 901u);
  CHECK(refs[2].slot_bit == chestBit);
  CHECK(refs[2].record_id == 902u);

  // 仅 ID 视图：跨槽去重后按 ID 升序。
  const auto ids =
      EquipmentModifierAdapter::CollectEquippedRecordIds(registry, player);
  REQUIRE(ids.size() == 3u);
  CHECK(ids[0] == 900u);
  CHECK(ids[1] == 901u);
  CHECK(ids[2] == 902u);
}

TEST_CASE(
    "[Unit] EquipmentSlotFilter - ring slot aliases normalize to a shared mask") {
  constexpr uint32_t kSkillId = 9107u;

  ModifierRuntimeOp op;
  op.opcode = static_cast<uint16_t>(ModifierOpCode::MANA_COST_MULT);
  op.param_u32 = kSkillId;
  op.param_f32 = 0.5f;

  // 记录按给定槽位掩码过滤，物品实际戴在 equippedSlot。
  const auto multFor = [&](const uint32_t filterSlotMask,
                           const uint32_t recordId,
                           const EquipmentSlot equippedSlot) {
    ModifierRuntimeFilter filter;
    filter.equip_slot_mask = filterSlotMask;
    REQUIRE(ModifierRuntimeRegistry::Get().LoadFromBytes(
        BuildSingleRecordRuntimeBlob(recordId, filter, op, {kSkillId})));

    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<EquipmentComponent>(player);
    EquipItemWithRecord(registry, player, equippedSlot, WeaponSubtype::None,
                        recordId);
    return EquipmentModifierAdapter::GetEquippedManaCostMultiplier(
        registry, player, kSkillId, Tag::None);
  };

  const uint32_t ring1Bit = 1u << static_cast<uint32_t>(EquipmentSlot::Ring1);
  const uint32_t ringBit = 1u << static_cast<uint32_t>(EquipmentSlot::Ring);
  const uint32_t headBit = 1u << static_cast<uint32_t>(EquipmentSlot::Head);

  // 记录限定 Ring1，实际戴在 Ring2：归一化后互为别名，生效。
  CHECK(multFor(ring1Bit, 9107u, EquipmentSlot::Ring2) ==
        doctest::Approx(0.5f));
  // 记录限定通用 Ring(bit 12)，实际戴在 Ring1：同样命中。
  CHECK(multFor(ringBit, 9108u, EquipmentSlot::Ring1) ==
        doctest::Approx(0.5f));
  // 非戒指槽位记录（头部）不匹配戒指槽，过滤后保持中性。
  CHECK(multFor(headBit, 9109u, EquipmentSlot::Ring2) ==
        doctest::Approx(1.0f));
}
