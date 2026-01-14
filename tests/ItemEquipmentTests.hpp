#pragma once
#include "TestCommon.hpp"
#include "game/components/ItemComponent.hpp"
#include "game/components/ItemStats.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "game/systems/item/CraftingSystem.hpp"
#include "game/systems/item/InventorySystem.hpp"
#include "game/systems/item/DropSystem.hpp"
#include "game/systems/item/LootFilter.hpp"
#include "game/systems/combat/StatsSystem.hpp"
#include "game/components/EquipmentComponent.hpp"
#include "game/components/InventoryComponent.hpp"
#include "game/components/Stats.hpp"
#include "game/components/Common.hpp"
#include "game/components/PlayerState.hpp"
#include "game/components/EnemyComponent.hpp"
#include <entt/entt.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdio>

namespace NoMoreDay {

TEST_CASE("ItemComponent Serialization") {
    ItemComponent item;
    item.id = 100;
    item.name = "Test Sword";
    item.type = ItemType::Weapon;
    item.slot = EquipmentSlot::MainHand;
    item.rarity = Rarity::Rare;
    item.value = 500.0f;
    item.attack = 10.0f;
    item.isTwoHanded = true;
    item.textureId = 12345;

    nlohmann::json j = item;
    ItemComponent deserialized = j.get<ItemComponent>();

    CHECK(deserialized.id == 100);
    CHECK(deserialized.name == "Test Sword");
    CHECK(deserialized.type == ItemType::Weapon);
    CHECK(deserialized.slot == EquipmentSlot::MainHand);
    CHECK(deserialized.rarity == Rarity::Rare);
    CHECK(deserialized.value == 500.0f);
    CHECK(deserialized.attack == 10.0f);
    CHECK(deserialized.isTwoHanded == true);
    CHECK(deserialized.textureId == 12345);
}

TEST_CASE("ItemFactory Texture Assignment") {
    entt::registry registry;
    ItemFactory::initialize();
    
    // Weapon
    auto weaponEntity = ItemFactory::createWeapon(registry, 10, Rarity::Common);
    const auto& weapon = registry.get<ItemComponent>(weaponEntity);
    CHECK(weapon.textureId != 0);
    
    // Armor (Chest)
    auto armorEntity = ItemFactory::createArmor(registry, 10, Rarity::Common, EquipmentSlot::Chest);
    const auto& armor = registry.get<ItemComponent>(armorEntity);
    CHECK(armor.textureId != 0);
}

TEST_CASE("EquipmentSlot Enum") {
    CHECK(static_cast<int>(EquipmentSlot::None) == 0);
    CHECK(static_cast<int>(EquipmentSlot::Ring2) == 11);
    CHECK(static_cast<int>(EquipmentSlot::Count) == 13);
}

TEST_CASE("ItemStats - Affix Descriptions") {
    SUBCASE("Primary Stats") {
        Affix affix;
        affix.type = AffixType::Strength;
        affix.value = 15.0f;
        CHECK(GetAffixDescription(affix, false) == "+15 力量");
        
        affix.type = AffixType::Dexterity;
        CHECK(GetAffixDescription(affix, false) == "+15 敏捷");
    }

    SUBCASE("Offensive Stats") {
        Affix affix;
        affix.type = AffixType::AttackSpeed;
        affix.value = 10.0f;
        CHECK(GetAffixDescription(affix, false) == "+10% 攻击速度");
        
        affix.type = AffixType::CritChance;
        affix.value = 5.0f;
        CHECK(GetAffixDescription(affix, false) == "+5% 暴击率");
    }

    SUBCASE("Defensive Stats") {
        Affix affix;
        affix.type = AffixType::FlatHealth;
        affix.value = 50.0f;
        CHECK(GetAffixDescription(affix, false) == "+50 生命");
    }

    SUBCASE("Damage Affixes") {
        Affix affix;
        affix.type = AffixType::FlatFireDamage;
        affix.value = 20.0f;
        CHECK(GetAffixDescription(affix, false) == "+20 火焰伤害");

        affix.type = AffixType::PercentPhysicalDamage;
        affix.value = 35.0f;
        CHECK(GetAffixDescription(affix, false) == "+35% 物理伤害");
    }

    SUBCASE("Resistances") {
        Affix affix;
        affix.type = AffixType::ResistAll;
        affix.value = 10.0f;
        CHECK(GetAffixDescription(affix, false) == "+10% 全抗性");

        affix.type = AffixType::ResistLightning;
        affix.value = 25.0f;
        CHECK(GetAffixDescription(affix, false) == "+25% 闪电抗性");
    }
}

TEST_CASE("Item Modification System Test") {
    ItemFactory::initialize();
    entt::registry registry;

    SUBCASE("Refinement: Affix Values") {
        ItemComponent item;
        item.name = "Test Sword";
        item.forgingPotential = 50;
        
        Affix aff;
        aff.type = AffixType::Strength;
        aff.tier = 1;
        aff.value = 1.0f;
        item.affixes.push_back(aff);
        
        auto result = CraftingSystem::refineAffixValues(item, 0);
        CHECK(result == CraftingResult::Success);
        CHECK(item.affixes[0].value != 1.0f);
        CHECK(item.forgingPotential < 50);
    }

    SUBCASE("Refinement: Base Stats") {
        ItemComponent item;
        item.type = ItemType::Weapon;
        item.name = "锈蚀铁剑";
        item.attack = 1.0f;
        item.forgingPotential = 50;
        
        auto result = CraftingSystem::refineBaseStats(item);
        CHECK(result == CraftingResult::Success);
        CHECK(item.attack >= 5.0f);
        CHECK(item.attack <= 8.0f);
        CHECK(item.forgingPotential < 50);
    }

    SUBCASE("Rune Socketing and Stats Integration") {
        auto player = registry.create();
        registry.emplace<CombatStats>(player);
        registry.emplace<PrimaryStats>(player);
        
        auto swordEntity = registry.create();
        ItemComponent item;
        item.type = ItemType::Weapon;
        item.slot = EquipmentSlot::MainHand;
        item.name = "Socketed Sword";
        item.socketCount = 2;
        item.sockets.resize(2, entt::null);
        registry.emplace<ItemComponent>(swordEntity, item);
        
        auto runeEntity = registry.create();
        ItemComponent runeItem;
        runeItem.name = "Fire Rune";
        runeItem.type = ItemType::Material;
        registry.emplace<ItemComponent>(runeEntity, runeItem);
        
        RuneComponent runeComp;
        Affix fireDmg;
        fireDmg.type = AffixType::FlatFireDamage;
        fireDmg.value = 10.0f;
        runeComp.weaponEffects.push_back(fireDmg);
        registry.emplace<RuneComponent>(runeEntity, runeComp);
        
        auto result = CraftingSystem::socketRune(registry, swordEntity, runeEntity, 0);
        CHECK(result == CraftingResult::Success);
        
        auto& itemInReg = registry.get<ItemComponent>(swordEntity);
        CHECK(itemInReg.sockets[0] == runeEntity);
        CHECK(registry.all_of<RuneComponent>(runeEntity));

        EquipmentComponent equipment;
        equipment.slots[0] = swordEntity;
        registry.emplace<EquipmentComponent>(player, equipment);
        
        StatsSystem::Recalculate(registry, player);
        const auto& stats = registry.get<CombatStats>(player);
        CHECK(stats.flat_damage[(int)DamageType::Fire] == doctest::Approx(10.0f));
        
        auto result2 = CraftingSystem::unsocketRune(registry, swordEntity, 0);
        CHECK(result2 == CraftingResult::Success);
        CHECK(itemInReg.sockets[0] == entt::entity{entt::null});

        StatsSystem::Recalculate(registry, player);
        const auto& stats2 = registry.get<CombatStats>(player);
        CHECK(stats2.flat_damage[(int)DamageType::Fire] == doctest::Approx(0.0f));
    }
}

TEST_CASE("EquipmentComponent Logic") {
    entt::registry registry;
    EquipmentComponent eq;
    
    SUBCASE("Initialization") {
        for (int i = 1; i < static_cast<int>(EquipmentSlot::Count); ++i) {
            CHECK(eq.Get(static_cast<EquipmentSlot>(i)) == entt::entity{entt::null});
        }
    }

    SUBCASE("Direct Set/Get") {
        entt::entity itemEntity = registry.create();
        ItemComponent& item = registry.emplace<ItemComponent>(itemEntity);
        item.name = "Sword";
        item.slot = EquipmentSlot::MainHand;

        eq.Set(EquipmentSlot::MainHand, itemEntity);
        CHECK(eq.Get(EquipmentSlot::MainHand) == itemEntity);

        entt::entity removed = eq.Unequip(EquipmentSlot::MainHand);
        CHECK(removed == itemEntity);
        CHECK(eq.Get(EquipmentSlot::MainHand) == entt::entity{entt::null});
    }
}

TEST_CASE("InventorySystem Equip Logic") {
    entt::registry registry;
    entt::entity player = registry.create();
    
    registry.emplace<InventoryComponent>(player);
    registry.emplace<EquipmentComponent>(player);
    registry.emplace<StatsDirty>(player);
    registry.emplace<CombatStats>(player);

    entt::entity sword = registry.create();
    ItemComponent& swordComp = registry.emplace<ItemComponent>(sword);
    swordComp.name = "Test Sword";
    swordComp.slot = EquipmentSlot::MainHand;
    swordComp.type = ItemType::Weapon;
    
    InventorySystem::pickUpItem(registry, player, sword);
    CHECK(InventorySystem::hasItem(registry, player, swordComp.id));

    bool success = InventorySystem::equipItem(registry, player, sword);
    CHECK(success);
    
    const auto& equip = registry.get<EquipmentComponent>(player);
    CHECK(equip.Get(EquipmentSlot::MainHand) == sword);
    
    auto& inv = registry.get<InventoryComponent>(player);
    bool foundInInv = false;
    for(auto item : inv.items) {
        if (item == sword) foundInInv = true;
    }
    CHECK_FALSE(foundInInv);

    success = InventorySystem::unequipItem(registry, player, EquipmentSlot::MainHand);
    CHECK(success);
    
    foundInInv = false;
    for(auto item : inv.items) {
        if (item == sword) foundInInv = true;
    }
    CHECK(foundInInv);
    CHECK(equip.Get(EquipmentSlot::MainHand) == entt::entity{entt::null});
}

TEST_CASE("DropSystem Tests") {
    entt::registry registry;

    SUBCASE("Basic Gold Drop") {
        registry.clear();
        auto player = registry.create();
        registry.emplace<PlayerTag>(player);
        registry.emplace<CombatStats>(player);
        
        auto victim = registry.create();
        registry.emplace<Position>(victim, 100.0f, 200.0f);
        
        LootPool goldPool;
        goldPool.id = 1;
        goldPool.name = "Gold Only";
        goldPool.entries = { { LootEntryType::Gold, 0, 10, 10, 100.0f } };
        ItemFactory::addLootPool(1, goldPool);
        
        registry.emplace<DropTableComponent>(victim, 1, 1.0f, 1, 1);
        registry.emplace<KilledTag>(victim, player);
        
        DropSystem::update(registry, 1);
        
        auto goldView = registry.view<GoldComponent, Position>();
        bool found = false;
        for (auto entity : goldView) {
            const auto& gold = goldView.get<GoldComponent>(entity);
            const auto& pos = goldView.get<Position>(entity);
            if (gold.amount == 10 && pos.x == 100.0f && pos.y == 200.0f) {
                found = true;
            }
        }
        CHECK(found);
    }

    SUBCASE("Magic Find Rarity Shift") {
        registry.clear();
        auto player = registry.create();
        registry.emplace<PlayerTag>(player);
        auto& stats = registry.emplace<CombatStats>(player);
        stats.magic_find = 1000.0f;
        
        auto victim = registry.create();
        registry.emplace<Position>(victim, 0.0f, 0.0f);
        
        LootPool itemPool;
        itemPool.id = 2;
        itemPool.name = "Item Only";
        itemPool.entries = { { LootEntryType::Item, 0, 1, 1, 100.0f } };
        ItemFactory::addLootPool(2, itemPool);
        
        registry.emplace<DropTableComponent>(victim, 2, 1.0f, 10, 10);
        registry.emplace<KilledTag>(victim, player);
        
        DropSystem::update(registry, 1);
        
        auto itemView = registry.view<ItemComponent>();
        int totalItems = 0;
        int legendaryCount = 0;
        for (auto entity : itemView) {
            totalItems++;
            if (itemView.get<ItemComponent>(entity).rarity == Rarity::Legendary) {
                legendaryCount++;
            }
        }
        CHECK(totalItems == 10);
        CHECK(legendaryCount > 0);
    }
}

TEST_CASE("LootFilter - Loading and Evaluation") {
    auto writeTempFilter = [](const std::string& filename, const std::string& content) {
        std::ofstream out(filename);
        out << content;
        out.close();
    };

    std::string filterJson = R"({
        "name": "Test Filter",
        "rules": [
            {
                "name": "Hide Low Level Common",
                "action": "HIDE",
                "conditions": {
                    "max_rarity": "COMMON",
                    "max_level": 5
                }
            },
            {
                "name": "Highlight Legendaries",
                "action": "EMPHASIZE",
                "color": [255, 0, 0],
                "scale": 1.5,
                "conditions": {
                    "min_rarity": "LEGENDARY"
                }
            },
            {
                "name": "Show Swords",
                "action": "SHOW",
                "conditions": {
                    "base_name": "Sword"
                }
            }
        ]
    })";
    
    std::string tempPath = "test_filter.json";
    writeTempFilter(tempPath, filterJson);
    
    LootFilter::load(tempPath);
    CHECK(LootFilter::getCurrentProfile().name == "Test Filter");

    ItemComponent itemA;
    itemA.name = "Rusty Dagger";
    itemA.rarity = Rarity::Common;
    CHECK(LootFilter::evaluate(itemA, 1).type == FilterActionType::HIDE);
    CHECK(LootFilter::evaluate(itemA, 10).type == FilterActionType::SHOW);

    ItemComponent itemC;
    itemC.name = "Godslayer";
    itemC.rarity = Rarity::Legendary;
    CHECK(LootFilter::evaluate(itemC, 50).type == FilterActionType::EMPHASIZE);

    std::remove(tempPath.c_str());
}

TEST_CASE("Affix System Integration Test") {
    ItemFactory::loadAffixDefinitions("assets/data/affixes.json");
    entt::registry registry;

    SUBCASE("StatsSystem applies Affixes correctly") {
        auto entity = registry.create();
        registry.emplace<PlayerTag>(entity);
        registry.emplace<CombatStats>(entity);
        registry.emplace<PrimaryStats>(entity, 10.0f, 10.0f, 10.0f, 10.0f);
        
        auto sword = registry.create();
        ItemComponent item;
        item.type = ItemType::Weapon;
        item.slot = EquipmentSlot::MainHand;
        item.attack = 10.0f;
        
        Affix strengthAffix;
        strengthAffix.type = AffixType::Strength;
        strengthAffix.value = 10.0f;
        strengthAffix.isPrefix = true;
        item.affixes.push_back(strengthAffix);
        
        registry.emplace<ItemComponent>(sword, item);
        EquipmentComponent equipment;
        equipment.slots[0] = sword;
        registry.emplace<EquipmentComponent>(entity, equipment);
        
        StatsSystem::Recalculate(registry, entity);
        const auto& stats = registry.get<CombatStats>(entity);
        CHECK(stats.effective_strength == doctest::Approx(20.0f));
    }
}


TEST_CASE("Legendary Merging System", "[Item][Crafting]") {
    entt::registry registry;
    ItemFactory::initialize();

    SUBCASE("Validation Failures") {
        auto base = registry.create();
        auto fodder = registry.create();
        auto catalyst = registry.create();

        // Empty components
        CHECK(CraftingSystem::fuseLegendary(registry, base, fodder, catalyst, 0) == CraftingResult::Failure);
    }

    SUBCASE("Successful Fusion - 1LP") {
        // Base: Unique Sword (1LP)
        auto baseEntity = registry.create();
        ItemComponent baseItem;
        baseItem.name = "Unique Sword";
        baseItem.rarity = Rarity::Mythic;
        baseItem.legendaryPotential = 1;
        baseItem.slot = EquipmentSlot::MainHand;
        registry.emplace<ItemComponent>(baseEntity, baseItem);

        // Fodder: Exalted Sword (4 Affixes)
        auto fodderEntity = registry.create();
        ItemComponent fodderItem;
        fodderItem.name = "Exalted Sword";
        fodderItem.rarity = Rarity::Uncommon; 
        fodderItem.slot = EquipmentSlot::MainHand;
        for(int i=0; i<4; ++i) {
            Affix aff;
            aff.type = (AffixType)i;
            aff.tier = 6;
            aff.name = "T6 Affix " + std::to_string(i);
            fodderItem.affixes.push_back(aff);
        }
        registry.emplace<ItemComponent>(fodderEntity, fodderItem);

        // Catalyst: Legendary Core
        auto catalystEntity = registry.create();
        ItemComponent catalystItem;
        catalystItem.name = "Legendary Core";
        catalystItem.quantity = 1;
        registry.emplace<ItemComponent>(catalystEntity, catalystItem);

        // Fuse select 2nd affix (index 1)
        auto result = CraftingSystem::fuseLegendary(registry, baseEntity, fodderEntity, catalystEntity, 1);
        CHECK(result == CraftingResult::Success);

        auto& fusedBase = registry.get<ItemComponent>(baseEntity);
        CHECK(fusedBase.rarity == Rarity::Ancient);
        CHECK(fusedBase.legendaryPotential == 0);
        REQUIRE(fusedBase.affixes.size() == 1);
        CHECK(fusedBase.affixes[0].type == (AffixType)1);
        CHECK(fusedBase.affixes[0].isLegendary == true);

        CHECK_FALSE(registry.valid(fodderEntity)); // Consumed
        CHECK_FALSE(registry.valid(catalystEntity)); // Consumed (qty 1->0)
    }

    SUBCASE("Successful Fusion - 2LP") {
        // Base: Unique Sword (2LP)
        auto baseEntity = registry.create();
        ItemComponent baseItem;
        baseItem.name = "Unique Sword";
        baseItem.rarity = Rarity::Mythic;
        baseItem.legendaryPotential = 2;
        baseItem.slot = EquipmentSlot::MainHand;
        registry.emplace<ItemComponent>(baseEntity, baseItem);

        // Fodder
        auto fodderEntity = registry.create();
        ItemComponent fodderItem;
        fodderItem.name = "Exalted Sword";
        fodderItem.rarity = Rarity::Rare;
        fodderItem.slot = EquipmentSlot::MainHand;
        for(int i=0; i<4; ++i) {
            Affix aff;
            aff.type = (AffixType)i;
            fodderItem.affixes.push_back(aff);
        }
        registry.emplace<ItemComponent>(fodderEntity, fodderItem);

        // Catalyst
        auto catalystEntity = registry.create();
        ItemComponent catalystItem;
        catalystItem.name = "Legendary Core";
        catalystItem.quantity = 5;
        registry.emplace<ItemComponent>(catalystEntity, catalystItem);

        // Fuse select index 0
        auto result = CraftingSystem::fuseLegendary(registry, baseEntity, fodderEntity, catalystEntity, 0);
        CHECK(result == CraftingResult::Success);

        auto& fusedBase = registry.get<ItemComponent>(baseEntity);
        REQUIRE(fusedBase.affixes.size() == 2);
        
        // One must be index 0
        bool foundSelected = false;
        for(const auto& aff : fusedBase.affixes) {
            if (aff.type == (AffixType)0) foundSelected = true;
            CHECK(aff.isLegendary);
        }
        CHECK(foundSelected);

        CHECK(registry.valid(catalystEntity)); 
        CHECK(registry.get<ItemComponent>(catalystEntity).quantity == 4);
    }
}

} // namespace NoMoreDay
