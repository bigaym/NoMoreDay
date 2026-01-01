#pragma once
#include "TestCommon.hpp"
#include "components/SkillSystem.hpp"

using namespace NoMoreDay;

TEST_CASE("Skill System Data Structures") {
    SUBCASE("DamagePool Operations") {
        DamagePool pool;
        pool.Add(Tag::Physical, 10.0f);
        pool.Add(Tag::Fire, 5.0f);
        
        CHECK(pool.Get(Tag::Physical) == 10.0f);
        CHECK(pool.Get(Tag::Fire) == 5.0f);
        CHECK(pool.Get(Tag::Cold) == 0.0f);
        
        DamagePool other;
        other.Add(Tag::Physical, 5.0f);
        other.Add(Tag::Lightning, 20.0f);
        
        pool.Merge(other);
        CHECK(pool.Get(Tag::Physical) == 15.0f);
        CHECK(pool.Get(Tag::Fire) == 5.0f);
        CHECK(pool.Get(Tag::Lightning) == 20.0f);
        
        pool.Clear();
        CHECK(pool.Get(Tag::Physical) == 0.0f);
    }
    
    SUBCASE("DamageModifier Definition") {
        DamageModifier mod;
        mod.source_tag = Tag::Physical | Tag::Melee;
        mod.value = 0.25f;
        mod.type = ModifierType::Increased;
        
        CHECK(HasTag(mod.source_tag, Tag::Physical));
        CHECK(HasTag(mod.source_tag, Tag::Melee));
        CHECK(mod.type == ModifierType::Increased);
    }
}
