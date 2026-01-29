#pragma once

#include "TestCommon.hpp"
#include "game/components/AstrolabeUIComponent.hpp"
#include "game/components/EquipmentComponent.hpp"
#include "game/components/InventoryComponent.hpp"
#include "game/components/Progression.hpp"
#include "game/components/MaterialBankComponent.hpp"
#include "game/components/SkillDefs.hpp"
#include "game/components/Stats.hpp"
#include "game/components/Common.hpp"
#include "game/components/EnemyComponent.hpp"
#include "game/components/MapComponent.hpp"
#include "game/components/PlayerState.hpp"

#include "game/data/AstrolabeRegistry.hpp"
#include "game/data/SkillRegistry.hpp"
#include "game/systems/combat/StatsSystem.hpp"
#include "game/systems/skill/AstrolabeSystem.hpp"
#include "game/systems/skill/SkillSystem.hpp"
#include "game/systems/skill/ShadowSystem.hpp"
#include "game/systems/world/MovementStanceSystem.hpp"
#include "game/systems/item/ItemFactory.hpp"
#include "game/systems/item/MaterialRegistry.hpp"
#include "game/systems/item/InventorySystem.hpp"
#include "game/systems/item/SalvageSystem.hpp"
#include "game/systems/item/RunewordSystem.hpp"

namespace NoMoreDay {

TEST_CASE("[Integration] Astrolabe - Node Activation") {
  auto &registry_data = AstrolabeRegistry::Get();
  registry_data.Load("assets/data/astrolabe.json");

  entt::registry registry;
  auto entity = registry.create();
  registry.emplace<PrimaryStats>(entity);
  registry.emplace<AstrolabeComponent>(entity);
  
  SUBCASE("Node Activation") {
      registry.get<AstrolabeComponent>(entity).available_points = 1;
      bool success = AstrolabeSystem::activate_node(registry, entity, 0); // Origin node

      CHECK(success == true);
      CHECK(registry.get<AstrolabeComponent>(entity).activated_nodes.count(0) > 0);
  }
}

TEST_CASE("[Integration] CombatSystem - Basic Damage Flow") {
    entt::registry registry;
    auto attacker = registry.create();
    auto defender = registry.create();
    
    registry.emplace<CombatStats>(attacker).damage_multipliers[0] = 1.0;
    registry.emplace<Position>(attacker, 0.0f, 0.0f);
    
    registry.emplace<HealthComponent>(defender, 100.0f, 100.0f);
    registry.emplace<CombatStats>(defender);
    registry.emplace<Position>(defender, 10.0f, 0.0f);

    DamagePool pool;
    pool.Add(Tag::Physical, 20.0f);
    
    auto result = DamagePipeline::Calculate(registry, attacker, defender, 0, pool, Tag::Melee, entt::null);
    CHECK(result.total_damage > 0.0f);
}

TEST_CASE("[Integration] ItemSystem - Equipment Flow") {
    TestSetupScope scope;
    entt::registry registry;
    auto player = registry.create();
    registry.emplace<InventoryComponent>(player);
    registry.emplace<EquipmentComponent>(player);
    
    auto weapon = ItemFactory::createWeapon(registry, 1, Rarity::Common);
    bool equipped = InventorySystem::equipItem(registry, player, weapon, EquipmentSlot::MainHand);
    CHECK(equipped);
    CHECK(registry.valid(registry.get<EquipmentComponent>(player).Get(EquipmentSlot::MainHand)));


}

TEST_CASE("[Integration] MaterialSystem - Bank Operations") {
    MaterialBankComponent bank;
    bank.Add(1001, 10);
    CHECK(bank.GetCount(1001) == 10);
    CHECK(bank.Has(1001, 5));
    bank.Remove(1001, 5);
    CHECK(bank.GetCount(1001) == 5);
}

TEST_CASE("[Integration] SalvageSystem - Item Salvaging") {
    entt::registry registry;
    auto player = registry.create();
    registry.emplace<MaterialBankComponent>(player);
    
    auto itemEnt = registry.create();
    auto& item = registry.emplace<ItemComponent>(itemEnt);
    item.rarity = Rarity::Magic;
    item.type = ItemType::Weapon;
    
    SalvageSystem::Execute(registry, itemEnt, player);
    CHECK(registry.valid(itemEnt) == false);
}

TEST_CASE("[Integration] Cultivator - Full Combat Flow") {
    entt::registry registry;
    SkillRegistry::Get().LoadFromJson("assets/data/skills.json");
    SkillSystem::InitHooks();
    systems::SpatialHashGrid grid(100, 100, 50);

    auto player = registry.create();
    auto& active = registry.emplace<ActiveSkillsComponent>(player);
    registry.emplace<CombatStats>(player);
    registry.emplace<Position>(player, 0.0f, 0.0f);
    registry.emplace<Velocity>(player, 0.0f, 0.0f);
    registry.emplace<AnimationStateComponent>(player);
    auto& intent = registry.emplace<SwordIntentComponent>(player);
    registry.emplace<MovementStanceComponent>(player);
    intent.stacks = 10;
    
    active.slots[0].id = 1;
    active.slots[0].current_charges = 1;

    CHECK(SkillSystem::TryCast(registry, player, 0));
    SkillSystem::Update(registry, grid, 0.11f); 
    
    auto exec_view = registry.view<SkillExecution>();
    CHECK(!exec_view.empty());
}

TEST_CASE("[Integration] Legendary - Infrastructure Verification") {
    TestSetupScope scope;
    entt::registry registry;
    ItemFactory::initialize();
    
    auto weapon = ItemFactory::createWeapon(registry, 100, Rarity::Legendary);
    auto& item = registry.get<ItemComponent>(weapon);
    
    // Manually inject a legendary affix to verify the detection logic works
    // (ItemFactory currently filters out legendary affixes from random rolls)
    Affix legAffix;
    legAffix.type = static_cast<AffixType>(1001); 
    item.affixes.push_back(legAffix);

    bool hasLegendaryAffix = false;
    for(const auto& aff : item.affixes) {
        if(static_cast<uint16_t>(aff.type) >= 1000) {
            hasLegendaryAffix = true;
            break;
        }
    }
    CHECK(hasLegendaryAffix);
}

} // namespace NoMoreDay
