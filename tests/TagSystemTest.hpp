#pragma once
#include "TestCommon.hpp"
#include "core/TagRegistry.hpp"
#include "core/SkillRegistry.hpp"
#include "systems/SkillSystem.hpp"
#include "components/SkillSystem.hpp"
#include <entt/entt.hpp>

using namespace NoMoreDay;

TEST_CASE("Tag Registry System") {
    SUBCASE("Bitmask Operations") {
        Tag t1 = Tag::Physical;
        Tag t2 = Tag::Fire;
        Tag t3 = Tag::Melee;

        // OR operation
        Tag combination = t1 | t2;
        CHECK(HasTag(combination, Tag::Physical));
        CHECK(HasTag(combination, Tag::Fire));
        CHECK_FALSE(HasTag(combination, Tag::Melee));

        // AND operation
        CHECK((combination & Tag::Physical) == Tag::Physical);
        CHECK((combination & Tag::Melee) == Tag::None);

        // Complex combination
        Tag complex = t1 | t2 | t3;
        CHECK(HasTag(complex, Tag::Physical));
        CHECK(HasTag(complex, Tag::Fire));
        CHECK(HasTag(complex, Tag::Melee));
    }

    SUBCASE("String Conversion") {
        CHECK(GetTagName(Tag::Physical) == "Physical");
        CHECK(GetTagName(Tag::Fire) == "Fire");
        CHECK(GetTagName(Tag::Shadow) == "Shadow");
        CHECK(GetTagName(Tag::Melee) == "Melee");
        CHECK(GetTagName(Tag::Hit) == "Hit");
        CHECK(GetTagName(Tag::Bleeding) == "Bleeding");
        
        // Test unknown/combined tag behavior (GetTagName handles single tags)
        // For combined tags, it should likely fall through to default or behavior undefined in current helper.
        // The current helper implementation uses switch/case, so a combined tag won't match any case unless explicitly handled or valid enum value.
        // It returns "Unknown" for combined values in current implementation.
        CHECK(GetTagName(Tag::Physical | Tag::Fire) == "Unknown"); 
    }

    SUBCASE("Value Verification") {
        uint64_t meleeVal = static_cast<uint64_t>(Tag::Melee);
        uint64_t projVal = static_cast<uint64_t>(Tag::Projectile);
        
        CHECK_MESSAGE(meleeVal == (1ULL << 16), "Melee Value: ", meleeVal);
        CHECK_MESSAGE(projVal == (1ULL << 17), "Projectile Value: ", projVal);
    }

    SUBCASE("Tag Categories") {
        // Verify values are distinct
        CHECK(Tag::Physical != Tag::Fire);
        CHECK(Tag::Physical != Tag::Melee);
        
        // Verify category ranges (sanity check on bit positions)
        // Physical is bit 0
        CHECK((static_cast<uint64_t>(Tag::Physical) & 0xFFFF) != 0); // DamageType range 0-15
        
        // Melee is bit 16
        CHECK((static_cast<uint64_t>(Tag::Melee) & 0xFFFF0000) != 0); // Form range 16-31
    }
}

// ============================================================================
// Phase 1 Tests: Tag String Parsing
// ============================================================================
TEST_CASE("Tag String Parsing (Phase 1)") {
    SUBCASE("TagFromString - Valid Tags") {
        auto physical = TagFromString("physical");
        REQUIRE(physical.has_value());
        CHECK(physical.value() == Tag::Physical);
        
        auto fire = TagFromString("fire");
        REQUIRE(fire.has_value());
        CHECK(fire.value() == Tag::Fire);
        
        auto melee = TagFromString("melee");
        REQUIRE(melee.has_value());
        CHECK(melee.value() == Tag::Melee);
        
        auto spell = TagFromString("spell");
        REQUIRE(spell.has_value());
        CHECK(spell.value() == Tag::Spell);
        
        auto swordriding = TagFromString("swordriding");
        REQUIRE(swordriding.has_value());
        CHECK(swordriding.value() == Tag::SwordRiding);
    }
    
    SUBCASE("TagFromString - Invalid Tags") {
        auto invalid = TagFromString("invalid_tag");
        CHECK_FALSE(invalid.has_value());
        
        auto empty = TagFromString("");
        CHECK_FALSE(empty.has_value());
        
        // Case sensitive test
        auto upperPhys = TagFromString("PHYSICAL");
        CHECK_FALSE(upperPhys.has_value());
    }
    
    SUBCASE("ParseTagList - Multiple Tags") {
        std::vector<std::string> tags = {"physical", "melee", "attack"};
        Tag result = ParseTagList(tags);
        
        CHECK(HasTag(result, Tag::Physical));
        CHECK(HasTag(result, Tag::Melee));
        CHECK(HasTag(result, Tag::Attack));
        CHECK_FALSE(HasTag(result, Tag::Spell));
        CHECK_FALSE(HasTag(result, Tag::Fire));
    }
    
    SUBCASE("ParseTagList - Empty List") {
        std::vector<std::string> empty;
        Tag result = ParseTagList(empty);
        CHECK(result == Tag::None);
    }
    
    SUBCASE("ParseTagList - Mixed Valid/Invalid") {
        std::vector<std::string> mixed = {"physical", "invalid", "fire", "unknown"};
        Tag result = ParseTagList(mixed);
        
        CHECK(HasTag(result, Tag::Physical));
        CHECK(HasTag(result, Tag::Fire));
        CHECK_FALSE(HasTag(result, Tag::Melee)); // Wasn't in list
    }
}

// ============================================================================
// Tag Metadata Lookup Tests
// ============================================================================
TEST_CASE("Tag Metadata Lookup") {
    SUBCASE("GetTagNameCN") {
        CHECK(GetTagNameCN(Tag::Physical) == "物理");
        CHECK(GetTagNameCN(Tag::Fire) == "火焰");
        CHECK(GetTagNameCN(Tag::Cold) == "冰霜");
        CHECK(GetTagNameCN(Tag::Melee) == "近战");
        CHECK(GetTagNameCN(Tag::Spell) == "法术");
        CHECK(GetTagNameCN(Tag::None) == "无");
    }
    
    SUBCASE("GetTagId") {
        CHECK(GetTagId(Tag::Physical) == "physical");
        CHECK(GetTagId(Tag::Fire) == "fire");
        CHECK(GetTagId(Tag::Melee) == "melee");
        CHECK(GetTagId(Tag::Spell) == "spell");
        CHECK(GetTagId(Tag::None) == "none");
    }
    
    SUBCASE("GetTagInfo") {
        const TagInfo* physInfo = GetTagInfo(Tag::Physical);
        REQUIRE(physInfo != nullptr);
        CHECK(physInfo->tag == Tag::Physical);
        CHECK(physInfo->id == "physical");
        CHECK(physInfo->name_cn == "物理");
        
        const TagInfo* noneInfo = GetTagInfo(Tag::None);
        CHECK(noneInfo == nullptr); // None is not in the table
    }
    
    SUBCASE("GetTagListString") {
        Tag tags = Tag::Physical | Tag::Melee | Tag::Attack;
        
        std::string cnList = GetTagListString(tags, true);
        CHECK(cnList.find("物理") != std::string::npos);
        CHECK(cnList.find("近战") != std::string::npos);
        CHECK(cnList.find("攻击") != std::string::npos);
        
        std::string enList = GetTagListString(tags, false);
        CHECK(enList.find("physical") != std::string::npos);
        CHECK(enList.find("melee") != std::string::npos);
        CHECK(enList.find("attack") != std::string::npos);
    }
}

// ============================================================================
// Phase 3 Tests: TalentNode Tag Modification
// ============================================================================
TEST_CASE("TalentNode Tag Modification") {
    SUBCASE("TalentNode Default Values") {
        TalentNode node;
        CHECK(node.add_tags == Tag::None);
        CHECK(node.remove_tags == Tag::None);
    }
    
    SUBCASE("TalentNode JSON Serialization - No Tags") {
        TalentNode original;
        original.id = 100;
        original.name_key = "test_talent";
        original.desc_key = "test_desc";
        original.max_points = 3;
        
        nlohmann::json j = original;
        
        // Should not contain add_tags/remove_tags if they are None
        CHECK_FALSE(j.contains("add_tags"));
        CHECK_FALSE(j.contains("remove_tags"));
        
        TalentNode parsed;
        from_json(j, parsed);
        CHECK(parsed.id == 100);
        CHECK(parsed.add_tags == Tag::None);
        CHECK(parsed.remove_tags == Tag::None);
    }
    
    SUBCASE("TalentNode JSON Serialization - With Tags") {
        TalentNode original;
        original.id = 200;
        original.name_key = "spell_conversion";
        original.desc_key = "converts_to_spell";
        original.add_tags = Tag::Spell | Tag::Fire;
        original.remove_tags = Tag::Attack;
        
        nlohmann::json j = original;
        
        CHECK(j.contains("add_tags"));
        CHECK(j.contains("remove_tags"));
        
        // Verify add_tags contains correct strings
        auto addTags = j["add_tags"].get<std::vector<std::string>>();
        CHECK(std::find(addTags.begin(), addTags.end(), "spell") != addTags.end());
        CHECK(std::find(addTags.begin(), addTags.end(), "fire") != addTags.end());
        
        // Verify remove_tags contains correct strings
        auto removeTags = j["remove_tags"].get<std::vector<std::string>>();
        CHECK(std::find(removeTags.begin(), removeTags.end(), "attack") != removeTags.end());
        
        // Verify deserialization
        TalentNode parsed;
        from_json(j, parsed);
        CHECK(HasTag(parsed.add_tags, Tag::Spell));
        CHECK(HasTag(parsed.add_tags, Tag::Fire));
        CHECK(HasTag(parsed.remove_tags, Tag::Attack));
    }
    
    SUBCASE("JSON to TalentNode - Parse Tags from String Array") {
        nlohmann::json j = {
            {"id", 300},
            {"name_key", "arcane_conversion"},
            {"desc_key", "desc"},
            {"add_tags", {"spell", "area"}},
            {"remove_tags", {"melee", "physical"}}
        };
        
        TalentNode node;
        from_json(j, node);
        
        CHECK(HasTag(node.add_tags, Tag::Spell));
        CHECK(HasTag(node.add_tags, Tag::Area));
        CHECK(HasTag(node.remove_tags, Tag::Melee));
        CHECK(HasTag(node.remove_tags, Tag::Physical));
    }
}

// ============================================================================
// Phase 3 Tests: GetEffectiveSkillTags
// ============================================================================
TEST_CASE("GetEffectiveSkillTags Integration") {
    // Note: This test requires the SkillRegistry to be loaded with test data
    // For unit testing, we can test the basic mechanics
    
    SUBCASE("Base Skill Tags - No Talents") {
        entt::registry registry;
        auto entity = registry.create();
        
        // No ActiveSkillsComponent = should return base tags from skill
        // This will return None if skill not found, so we test that path
        Tag tags = SkillSystem::GetEffectiveSkillTags(registry, entity, 999);
        CHECK(tags == Tag::None); // Skill doesn't exist
    }
    
    SUBCASE("Entity With ActiveSkillsComponent But No Allocations") {
        entt::registry registry;
        auto entity = registry.create();
        
        auto& active = registry.emplace<ActiveSkillsComponent>(entity);
        active.specialized_slots[0].skill_id = 1; // Flowing Thrust
        // No allocated_points = no tag modifications
        
        Tag tags = SkillSystem::GetEffectiveSkillTags(registry, entity, 1);
        // Should return base skill tags (or None if skill not loaded)
        // The actual tags depend on SkillRegistry being loaded
    }
    
    SUBCASE("Tag Modification Logic - Manual Test") {
        // Test the tag modification logic directly
        Tag baseTags = Tag::Physical | Tag::Melee | Tag::Attack;
        Tag addTags = Tag::Spell | Tag::Fire;
        Tag removeTags = Tag::Attack;
        
        // Apply modifications
        Tag result = baseTags;
        result = result | addTags;      // Add tags
        result = result & ~removeTags;  // Remove tags
        
        CHECK(HasTag(result, Tag::Physical));
        CHECK(HasTag(result, Tag::Melee));
        CHECK(HasTag(result, Tag::Spell));  // Added
        CHECK(HasTag(result, Tag::Fire));   // Added
        CHECK_FALSE(HasTag(result, Tag::Attack)); // Removed
    }
}
