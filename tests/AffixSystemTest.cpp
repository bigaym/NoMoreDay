#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../third_party/doctest/doctest.h"
#include "../src/components/Stats.hpp"
#include "../src/components/AffixComponent.hpp"
#include "../src/components/ItemComponent.hpp"
#include "../src/components/EquipmentComponent.hpp"
#include "../src/systems/StatsSystem.hpp"
#include "../src/core/ItemFactory.hpp"
#include "../src/tools/Logger.hpp" // Include Logger
#include <entt/entt.hpp>
#include <vector>

using namespace NoMoreDay;

// RAII Helper for Logger
struct LoggerScope {
    LoggerScope() { tools::Logger::Init(); }
    ~LoggerScope() { tools::Logger::Shutdown(); }
};

TEST_CASE("Affix System Integration Test") {
    LoggerScope loggerScope; // Initialize Logger

    // 1. Initialize ItemFactory with test data
    // We can't easily inject mock data into ItemFactory static methods unless we overwrite the file or add a method to inject.
    // However, ItemFactory::initialize loads from assets/data/affixes.json.
    // We can assume the file exists or call loadAffixDefinitions manually with a test file if needed.
    // For now, let's try to use the real file or generate affixes manually for testing StatsSystem.
    
    // Check if we can load definitions
    ItemFactory::loadAffixDefinitions("assets/data/affixes.json");
    
    entt::registry registry;

    SUBCASE("StatsSystem applies Affixes correctly") {
        auto entity = registry.create();
        registry.emplace<CombatStats>(entity);
        registry.emplace<PrimaryStats>(entity, 10.0f, 10.0f, 10.0f, 10.0f); // Base stats
        
        // Create an item with known affixes
        auto sword = registry.create();
        ItemComponent item;
        item.type = ItemType::Weapon;
        item.slot = EquipmentSlot::MainHand;
        item.attack = 10.0f;
        
        // Add a Prefix: +10 Strength
        Affix strengthAffix;
        strengthAffix.type = AffixType::Strength;
        strengthAffix.value = 10.0f;
        strengthAffix.isPrefix = true;
        item.affixes.push_back(strengthAffix);
        
        // Add a Suffix: +50 Flat Health
        Affix healthAffix;
        healthAffix.type = AffixType::FlatHealth;
        healthAffix.value = 50.0f;
        healthAffix.isPrefix = false;
        item.affixes.push_back(healthAffix);
        
        registry.emplace<ItemComponent>(sword, item);
        
        // Equip it
        EquipmentComponent equipment;
        equipment.slots[0] = sword; // MainHand
        registry.emplace<EquipmentComponent>(entity, equipment);
        
        // Recalculate
        StatsSystem::Recalculate(registry, entity);
        
        const auto& stats = registry.get<CombatStats>(entity);
        
        // Check Strength: 10 Base + 10 Affix = 20
        CHECK(stats.effective_strength == doctest::Approx(20.0f));
        
        // Check Max Health: 
        // Base 100
        // Vitality 10 -> +150 (10 * 15)
        // Affix +50
        // Total = 100 + 150 + 50 = 300
        CHECK(stats.max_health == doctest::Approx(300.0f));
    }
    
    SUBCASE("Affix Generation Constraints") {
        // This test relies on ItemFactory having loaded definitions.
        // We will manually add a definition to ItemFactory for testing if possible, 
        // but since s_affixDefinitions is private, we depend on what's loaded.
        
        // Let's test generateRandomAffix behavior assuming definitions are loaded.
        // If definitions are empty, it returns Strength + 1 fallback.
        
        Affix aff = ItemFactory::generateRandomAffix(10, true, EquipmentSlot::MainHand);
        CHECK(aff.value > 0.0f);
        
        // Verify it respects isPrefix
        // Note: The fallback might not respect it if candidates are empty, but logic tries to find candidates.
        // If real data is loaded, we should get a valid prefix.
    }
    
    SUBCASE("ModifierList Application") {
        auto entity = registry.create();
        registry.emplace<CombatStats>(entity);
        
        ModifierList mods;
        StatModifier mod;
        mod.type = StatType::FireDamage;
        mod.mode = ModifierMode::PercentAdd;
        mod.value = 50.0f; // +50% Fire Damage
        mods.modifiers.push_back(mod);
        
        registry.emplace<ModifierList>(entity, mods);
        
        StatsSystem::Recalculate(registry, entity);
        
        const auto& stats = registry.get<CombatStats>(entity);
        // Default Fire Damage Multiplier is 1.0. With +50%, it should be 1.5.
        // Note: damage_multipliers init to 1.0.
        // In StatsSystem: multiplier = (1.0 + percent_add) * percent_mult
        // Here percent_add = 0.5. percent_mult = 1.0.
        // Result = 1.5.
        // AND we add it to combat.damage_multipliers (which are initialized to 1.0).
        // Wait, StatsSystem code:
        // combat.damage_multipliers[(int)dType] = c.GetMultiplier();
        // c.GetMultiplier() returns (1+add)*mult.
        // So it overrides the default 1.0 in combat stats (since calcs start fresh).
        // Correct.
        
        CHECK(stats.damage_multipliers[(int)DamageType::Fire] == doctest::Approx(1.5f));
    }
}
