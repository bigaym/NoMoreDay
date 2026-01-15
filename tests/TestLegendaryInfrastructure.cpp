#include "TestCommon.hpp"
#include "game/components/Stats.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/components/InventoryComponent.hpp"
#include "game/components/EquipmentComponent.hpp"
#include "game/components/Common.hpp"
#include "game/systems/combat/StatsSystem.hpp"
#include "game/systems/combat/DamagePipeline.hpp"
#include "game/systems/item/InventorySystem.hpp"
#include "game/systems/combat/CombatEventDispatcher.hpp"
#include "game/data/TagRegistry.hpp"
#include "doctest.h"

using namespace NoMoreDay;

TEST_CASE("Legendary Infrastructure: Stat Conversion") {
    entt::registry registry;
    auto entity = registry.create();
    registry.emplace<PlayerTag>(entity);
    registry.emplace<CombatStats>(entity);
    auto& primary = registry.emplace<PrimaryStats>(entity);
    primary.intelligence = 100.0f;

    auto& convComp = registry.emplace<StatConversionComponent>(entity);
    // Convert 50% of Intelligence to Armor
    convComp.conversions.push_back({StatType::Intelligence, StatType::Armor, 0.5f});

    StatsSystem::Recalculate(registry, entity);
    auto& stats = registry.get<CombatStats>(entity);

    // Intelligence gives 1 Mana per point by default (check Attribute constants)
    // Here we check Armor. Base Armor is 0. 
    // 100 Int * 0.5 = 50 Armor.
    CHECK(stats.armor == doctest::Approx(50.0f));
}

TEST_CASE("Legendary Infrastructure: Titan's Grip via Affix") {
    entt::registry registry;
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<EquipmentComponent>(player);
    registry.emplace<InventoryComponent>(player);
    registry.emplace<CombatStats>(player);
    registry.emplace<PrimaryStats>(player);

    // Create gloves with TitanGrip affix
    ItemComponent gloves;
    gloves.name = "Titan's Mitts";
    gloves.type = ItemType::Armor;
    gloves.slot = EquipmentSlot::Hands;
    gloves.rarity = Rarity::Legendary;
    gloves.affixes.push_back({AffixType::TitanGrip, 1.0f, 7});
    
    auto g_ent = registry.create();
    registry.emplace<ItemComponent>(g_ent, gloves);

    // Equip gloves
    InventorySystem::equipItem(registry, player, g_ent);
    
    // StatsSystem::update should be called or Recalculate
    StatsSystem::Recalculate(registry, player);

    CHECK(registry.all_of<TitanGripTrait>(player));

    // Now test if we can equip two 2H weapons
    ItemComponent sword1;
    sword1.name = "Giant Sword 1";
    sword1.type = ItemType::Weapon;
    sword1.slot = EquipmentSlot::MainHand;
    sword1.isTwoHanded = true;
    auto e1 = registry.create();
    registry.emplace<ItemComponent>(e1, sword1);

    ItemComponent sword2;
    sword2.name = "Giant Sword 2";
    sword2.type = ItemType::Weapon;
    sword2.slot = EquipmentSlot::MainHand;
    sword2.isTwoHanded = true;
    auto e2 = registry.create();
    registry.emplace<ItemComponent>(e2, sword2);

    CHECK(InventorySystem::equipItem(registry, player, e1, EquipmentSlot::MainHand));
    CHECK(InventorySystem::equipItem(registry, player, e2, EquipmentSlot::OffHand));

    auto& equipment = registry.get<EquipmentComponent>(player);
    CHECK(equipment.get(EquipmentSlot::MainHand) == e1);
    CHECK(equipment.get(EquipmentSlot::OffHand) == e2);
}

TEST_CASE("Legendary Infrastructure: Global Damage Conversion via Item") {
    entt::registry registry;
    auto player = registry.create();
    registry.emplace<PlayerTag>(player);
    registry.emplace<EquipmentComponent>(player);
    registry.emplace<InventoryComponent>(player);
    registry.emplace<CombatStats>(player);
    registry.emplace<PrimaryStats>(player);

    // Create a ring that converts Physical to Lightning
    ItemComponent ring;
    ring.name = "Storm Circle";
    ring.type = ItemType::Jewelry;
    ring.slot = EquipmentSlot::Ring;
    ring.damage_modifiers.push_back({Tag::Physical, Tag::Lightning, 1.0f, ModifierType::Convert});

    auto r_ent = registry.create();
    registry.emplace<ItemComponent>(r_ent, ring);

    // Equip ring
    InventorySystem::equipItem(registry, player, r_ent);
    StatsSystem::Recalculate(registry, player);

    // Verify DamagePipeline uses it
    auto defender = registry.create();
    registry.emplace<CombatStats>(defender);

    DamagePool base;
    base.Add(Tag::Physical, 100.0f);

    auto result = DamagePipeline::Calculate(registry, player, defender, 0, base, Tag::Hit);

    // Physical should be 0, Lightning should be > 0
    CHECK(result.final_pool.Get(Tag::Physical) == 0.0f);
    CHECK(result.final_pool.Get(Tag::Lightning) > 99.0f);
}

TEST_CASE("Legendary Infrastructure: Combat Events (Potion)") {
    entt::registry registry;
    auto player = registry.create();
    
    bool eventFired = false;
    uint32_t id = CombatEventDispatcher::Register(CombatEventType::OnUsePotion, [&](entt::registry& reg, const CombatEvent& evt) {
        if (evt.source == player) {
            eventFired = true;
        }
    });

    // Manually dispatch to verify listener works
    CombatEventDispatcher::Dispatch(registry, CombatEventFactory::CreateOnUsePotion(player, 123, 50.0f));
    CHECK(eventFired == true);

    CombatEventDispatcher::Unregister(CombatEventType::OnUsePotion, id);
}

TEST_CASE("Legendary Infrastructure: Damage Pipeline Conversion") {
    entt::registry registry;
    auto attacker = registry.create();
    auto defender = registry.create();
    registry.emplace<CombatStats>(attacker);
    registry.emplace<CombatStats>(defender);

    auto& global_mods = registry.emplace<GlobalModifierComponent>(attacker);
    // Convert 100% Physical to Fire
    global_mods.modifiers.push_back({Tag::Physical, Tag::Fire, 1.0f, ModifierType::Convert});

    DamagePool base;
    base.Add(Tag::Physical, 100.0f);

    auto result = DamagePipeline::Calculate(registry, attacker, defender, 0, base, Tag::Hit);

    // Result should be pure Fire damage
    CHECK(result.final_pool.Get(Tag::Physical) == 0.0f); 
    CHECK(result.final_pool.Get(Tag::Fire) > 99.0f);  
}

TEST_CASE("Legendary Infrastructure: Movement Accumulator") {
    entt::registry registry;
    auto player = registry.create();
    auto& acc = registry.emplace<MovementAccumulator>(player);
    acc.threshold = 100.0f;
    
    int distanceEvents = 0;
    uint32_t id = CombatEventDispatcher::Register(CombatEventType::OnMoveDistance, [&](entt::registry& reg, const CombatEvent& evt) {
        if (evt.source == player) {
            distanceEvents++;
        }
    });

    // We simulate the system logic here:
    acc.distance += 150.0f;
    if (acc.distance >= acc.threshold) {
        acc.distance -= acc.threshold;
        CombatEventDispatcher::Dispatch(registry, CombatEventFactory::CreateMoveDistance(player, acc.threshold));
    }
    
    CHECK(distanceEvents == 1);
    CHECK(acc.distance == 50.0f);

    CombatEventDispatcher::Unregister(CombatEventType::OnMoveDistance, id);
}
