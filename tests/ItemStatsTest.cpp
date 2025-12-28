#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../third_party/doctest/doctest.h"
#include "../src/components/ItemStats.hpp"

using namespace NoMoreDay;

TEST_CASE("ItemStats - Affix Descriptions") {
    SUBCASE("Primary Stats") {
        Affix affix;
        affix.type = AffixType::Strength;
        affix.value = 15.0f;
        CHECK(GetAffixDescription(affix) == "+15 Strength");
        
        affix.type = AffixType::Dexterity;
        CHECK(GetAffixDescription(affix) == "+15 Dexterity");
    }

    SUBCASE("Offensive Stats") {
        Affix affix;
        affix.type = AffixType::AttackSpeed;
        affix.value = 10.0f;
        CHECK(GetAffixDescription(affix) == "+10% Attack Speed");
        
        affix.type = AffixType::CritChance;
        affix.value = 5.0f;
        CHECK(GetAffixDescription(affix) == "+5% Crit Chance");
    }

    SUBCASE("Defensive Stats") {
        Affix affix;
        affix.type = AffixType::FlatHealth;
        affix.value = 50.0f;
        CHECK(GetAffixDescription(affix) == "+50 Health");
    }

    SUBCASE("Damage Affixes") {
        Affix affix;
        affix.type = AffixType::FlatFireDamage;
        affix.value = 20.0f;
        CHECK(GetAffixDescription(affix) == "+20 Fire Dmg");

        affix.type = AffixType::PercentPhysicalDamage;
        affix.value = 35.0f;
        CHECK(GetAffixDescription(affix) == "+35% Increased Physical Dmg");
    }

    SUBCASE("Resistances") {
        Affix affix;
        affix.type = AffixType::ResistAll;
        affix.value = 10.0f;
        CHECK(GetAffixDescription(affix) == "+10% All Resistances");

        affix.type = AffixType::ResistLightning;
        affix.value = 25.0f;
        CHECK(GetAffixDescription(affix) == "+25% Lightning Resistance");
    }
}
