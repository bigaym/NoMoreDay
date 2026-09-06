#include "TestCommon.hpp"
#include "game/foundation/components/Common.hpp"
#include "game/foundation/components/EquipmentComponent.hpp"
#include "game/foundation/components/ItemComponent.hpp"
#include "game/foundation/components/ItemStats.hpp"
#include "game/foundation/components/SkillDefs.hpp"
#include "game/foundation/components/Stats.hpp"
#include "game/foundation/data/SkillRegistry.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "game/systems/item/storage/ItemPersistenceCodec.hpp"
#include "game/systems/item/storage/ItemStorageService.hpp"
#include "game/systems/item/storage/ItemTemplateRegistry.hpp"
#include "game/systems/skill/SkillDisplayPreviewService.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include <sstream>
#include <vector>

namespace NoMoreDay {

TEST_CASE("[Integration] ItemSkillModifier and PlusSkillLevelGeneric End-to-End Pipeline") {
  TestSetupScope scope;
  SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
  ItemTemplateRegistry::Instance().initializeDefaults();
  ItemFactory::initialize();

  // =========================================================================
  // Phase 1: Generation & Drop Entry Points
  // =========================================================================
  SUBCASE("1. Generation: PlusSkillLevelGeneric (type 52) and ItemSkillModifier") {
    // Test Affix definition loading and creation for PlusSkillLevelGeneric
    const Affix genericAffix = ItemFactory::createAffix(AffixType::PlusSkillLevelGeneric, 3);
    CHECK(genericAffix.type == AffixType::PlusSkillLevelGeneric);
    CHECK(genericAffix.tier == 3);
    CHECK(genericAffix.value >= 1.0f);
    CHECK(genericAffix.isPrefix == true);

    const char *affixDesc = GetAffixDescriptionRef(genericAffix, true);
    REQUIRE(affixDesc != nullptr);
    CHECK(std::string(affixDesc).find("技能等级") != std::string::npos);

    // Test ItemFactory item generation and skill modifier rolling
    ItemComponent testItem{};
    testItem.type = ItemType::Weapon;
    testItem.rarity = Rarity::Legendary;
    testItem.name = "Legendary Claymore";

    ItemFactory::rollSkillModifiers(testItem, 50);
    // Since chance is 60% for legendary, roll again in loop if empty to guarantee generation
    int attempts = 0;
    while (testItem.skill_modifiers.empty() && attempts < 20) {
      ItemFactory::rollSkillModifiers(testItem, 50);
      ++attempts;
    }
    REQUIRE(!testItem.skill_modifiers.empty());
    const auto &rolledMod = testItem.skill_modifiers.front();
    CHECK(rolledMod.target_skill_id > 0);
  }

  // =========================================================================
  // Phase 2: Binary Persistence (Section 13 - ItemSkillModifiers)
  // =========================================================================
  SUBCASE("2. Serialization & Deserialization: Section 13 Round-Trip") {
    ItemStorageService service;

    ItemInstance weaponProto{};
    weaponProto.instanceId = 70001;
    weaponProto.baseId = 1001;
    weaponProto.quantity = 1;
    weaponProto.itemLevel = 50;
    weaponProto.rarity = 4; // Legendary
    weaponProto.affixCount = 1;
    weaponProto.affixes[0] = CompactAffix{AffixType::PlusSkillLevelGeneric, 3, true, 2.0f};

    const ItemHandle weaponHandle = service.getStoreMutable().create(weaponProto);
    REQUIRE(weaponHandle);

    // Attach ItemSkillModifier to side table
    ItemSideTableData sideData;
    ItemSkillModifier skillMod{};
    skillMod.target_skill_id = 1; // Flowing Thrust
    skillMod.flat_cooldown_delta = -1.2f;
    skillMod.mana_cost_delta = -5.0f;
    skillMod.extra_projectiles = 3;
    skillMod.area_radius_mult = 1.35f;
    skillMod.inject_ailment_id = 55;
    skillMod.inject_ailment_chance = 0.75f;
    sideData.skill_modifiers.push_back(skillMod);

    service.getStoreMutable().setSideTable(weaponHandle, sideData);

    // Place into Equipment MainHand slot
    CHECK(service.setSlotHandle(
        SlotRef{ContainerKind::Equipment, static_cast<uint8_t>(EquipmentSlot::MainHand), 0, 0},
        weaponHandle));

    // Encode to binary stream
    std::stringstream binaryStream(std::ios::in | std::ios::out | std::ios::binary);
    const bool encodeOk = ItemPersistenceCodec::encode(
        service, binaryStream, nullptr, ContainerDirtyFlags::All);
    REQUIRE(encodeOk);

    // Decode from binary stream
    ItemStorageService restoredService;
    binaryStream.seekg(0, std::ios::beg);
    const bool decodeOk = ItemPersistenceCodec::decode(binaryStream, restoredService);
    REQUIRE(decodeOk);

    // Verify restored side table and skill modifier
    const ItemHandle restoredHandle = restoredService.getSlotHandle(
        SlotRef{ContainerKind::Equipment, static_cast<uint8_t>(EquipmentSlot::MainHand), 0, 0});
    REQUIRE(restoredHandle);

    const auto *restoredSide = restoredService.getStore().getSideTable(restoredHandle);
    REQUIRE(restoredSide != nullptr);
    REQUIRE(restoredSide->skill_modifiers.size() == 1);

    const auto &restoredMod = restoredSide->skill_modifiers[0];
    CHECK(restoredMod.target_skill_id == 1);
    CHECK(restoredMod.flat_cooldown_delta == doctest::Approx(-1.2f));
    CHECK(restoredMod.mana_cost_delta == doctest::Approx(-5.0f));
    CHECK(restoredMod.extra_projectiles == 3);
    CHECK(restoredMod.area_radius_mult == doctest::Approx(1.35f));
    CHECK(restoredMod.inject_ailment_id == 55);
    CHECK(restoredMod.inject_ailment_chance == doctest::Approx(0.75f));

    // =========================================================================
    // Phase 3: Rebaking (SkillSystem::RebakeSkillProfiles)
    // =========================================================================
    entt::registry registry;
    const auto player = registry.create();
    registry.emplace<Position>(player, 0.0f, 0.0f);
    auto &active = registry.emplace<ActiveSkillsComponent>(player);
    auto &equipment = registry.emplace<EquipmentComponent>(player);
    auto &combatStats = registry.emplace<CombatStats>(player);
    combatStats.min_weapon_damage = 15.0f;
    combatStats.max_weapon_damage = 30.0f;

    // Equip Flowing Thrust (ID 1)
    active.slots[0] = SkillSlot{.id = 1, .cooldown = 0.0f, .current_charges = 1};

    const auto *skillData = SkillRegistry::Get().GetSkill(1);
    REQUIRE(skillData != nullptr);

    // Instantiate equipped item with the deserialized skill modifiers
    const auto weaponEntity = registry.create();
    auto &itemComp = registry.emplace<ItemComponent>(weaponEntity);
    itemComp.id = 70001;
    itemComp.name = "Restored Ancient Blade";
    itemComp.slot = EquipmentSlot::MainHand;
    itemComp.skill_modifiers = restoredSide->skill_modifiers;

    equipment.Set(EquipmentSlot::MainHand, weaponEntity);

    // Rebake skill profiles
    SkillSystem::RebakeSkillProfiles(registry, player);

    const auto *profile = SkillSystem::GetBakedSkillProfile(registry, player, 1);
    REQUIRE(profile != nullptr);
    CHECK(profile->skill_id == 1);
    CHECK(profile->effective_cooldown == doctest::Approx(std::max(0.0f, skillData->cooldown - 1.2f)));
    CHECK(profile->effective_mana_cost == doctest::Approx(std::max(0.0f, skillData->mana_cost - 5.0f)));
    CHECK(profile->projectile_count == 4); // 1 base + 3 extra
    CHECK(profile->area_radius == doctest::Approx(1.35f));
    REQUIRE(profile->injected_count == 1);
    CHECK(profile->injected_payloads[0].type == PayloadType::Ailment);
    CHECK(profile->injected_payloads[0].ailment_id == 55);
    CHECK(profile->injected_payloads[0].value_mult == doctest::Approx(0.75f));

    // =========================================================================
    // Phase 4: Display (SkillDisplayPreviewService)
    // =========================================================================
    const auto preview = SkillDisplayPreviewService::Build(registry, player, 1);
    CHECK(preview.display_mana_cost == profile->effective_mana_cost);
    CHECK(preview.display_cooldown == profile->effective_cooldown);
    CHECK(preview.display_projectiles == profile->projectile_count);
    CHECK(preview.display_tags == profile->effective_tags);
  }
}

} // namespace NoMoreDay
