#pragma once

#include "../src/components/ItemStats.hpp"

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