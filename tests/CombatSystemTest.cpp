#define DOCTEST_CONFIG_IMPLEMENT
#include "../third_party/doctest/doctest.h"
#include "../src/components/Stats.hpp"
#include "../src/components/Combat.hpp"
#include "../src/systems/CombatSystem.hpp"
#include "../src/tools/Logger.hpp"

using namespace NoMoreDay;

int main(int argc, char** argv) {
    printf("Starting main...\n");
    try {
        tools::Logger::Init();
        printf("Logger initialized.\n");
    } catch (const std::exception& e) {
        printf("Logger Init failed: %s\n", e.what());
        return 1;
    }

    doctest::Context context;
    context.applyCommandLine(argc, argv);

    int res = context.run();

    tools::Logger::Shutdown();

    if (context.shouldExit()) 
        return res;
    
    return res;
}

TEST_CASE("Damage Calculation - Armor Mitigation") {
    printf("Test started\n");
    CombatStats attacker;
    CombatStats defender;
    
    // Reset stats
    attacker = CombatStats();
    defender = CombatStats();
    
    float baseDamage = 100.0f;
    
    // Case 1: 0 Armor -> 100 Damage
    defender.armor = 0.0f;
    float damage = CombatSystem::CalculateDamage(attacker, defender, baseDamage, DamageType::Physical);
    CHECK(damage == doctest::Approx(100.0f));
    
    // Case 2: 100 Armor. 
    // Formula: Reduction = Armor / (Armor + 100).
    // 100 / 200 = 0.5. Damage = 100 * 0.5 = 50.
    defender.armor = 100.0f;
    damage = CombatSystem::CalculateDamage(attacker, defender, baseDamage, DamageType::Physical);
    CHECK(damage == doctest::Approx(50.0f));
}

TEST_CASE("Damage Calculation - Resistance") {
    CombatStats attacker;
    CombatStats defender;
    
    attacker = CombatStats();
    defender = CombatStats();
    
    float baseDamage = 100.0f;
    
    // Case 1: 0 Res -> 100 Damage
    defender.resistances[(int)DamageType::Fire] = 0.0f;
    float damage = CombatSystem::CalculateDamage(attacker, defender, baseDamage, DamageType::Fire);
    CHECK(damage == doctest::Approx(100.0f));
    
    // Case 2: 50% Res -> 50 Damage
    defender.resistances[(int)DamageType::Fire] = 0.5f; // 50%
    damage = CombatSystem::CalculateDamage(attacker, defender, baseDamage, DamageType::Fire);
    CHECK(damage == doctest::Approx(50.0f));
    
    // Case 3: Cap check (75%)?
    // Spec says "Resistance caps".
    defender.resistances[(int)DamageType::Fire] = 0.9f; // 90%
    damage = CombatSystem::CalculateDamage(attacker, defender, baseDamage, DamageType::Fire);
    // Assuming hard cap at 75% for now, so reduction is 75%, dmg is 25.
    CHECK(damage == doctest::Approx(25.0f));
}
