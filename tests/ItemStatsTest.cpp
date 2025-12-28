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

    SUBCASE("Utility") {
        Affix affix;
        affix.type = AffixType::MoveSpeed;
        affix.value = 12.0f;
        CHECK(GetAffixDescription(affix) == "+12% Move Speed");
    }
}
